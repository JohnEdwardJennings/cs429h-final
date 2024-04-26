/*
This is the complete C++ interface for branch predictors in ChampSim.
Here is the source for a clear explanation of the interface functions and parameters:
https://github.com/ChampSim/ChampSim/blob/2b8d3fc28abb6072d7675228418fa7cfe862d4dc/docs/src/Modules.rst#branch-predictors
*/

#include <algorithm>
#include <array>
#include <bitset>
#include <map>

//#include "msl/fwcounter.h"
#include "SatCounter.h"
#include "LoopPredictor.h"
#include "ooo_cpu.h"


// using 9 tables (including bimodal) for now
namespace
{
// original paper parameters (5 tables): https://jilp.org/vol7/v7paper10.pdf
// constexpr std::size_t GLOBAL_HISTORY_LENGTHS[] = {0, 10, 20, 40, 80}; // follows a geometric series
// constexpr std::size_t TABLE_INDEX_SIZES[] = {12, 10, 10, 10, 10}; // change as necessary
// constexpr std::size_t TAG_SIZES[] = {0, 8, 8, 8, 8}; // change as necessary
// constexpr std::size_t NUM_TABLES = 5;

constexpr std::size_t GLOBAL_HISTORY_LENGTHS[] = {0, 5, 8, 13, 20, 33, 53, 85, 136}; // follows a geometric series (ratio = 1.6)
constexpr std::size_t TABLE_INDEX_SIZES[] = {12, 10, 10, 11, 11, 11, 11, 12, 12}; // change as necessary
constexpr std::size_t TAG_SIZES[] = {0, 12, 12, 12, 11, 11, 11, 10, 10}; // change as necessary
constexpr std::size_t NUM_TABLES = 9; // includes bimodal table
constexpr std::size_t MAX_TABLE_INDEX_SIZE = 12;
constexpr std::size_t MAX_GLOBAL_HISTORY_LENGTH = 140; // CHANGE WHEN NECESSARY
constexpr std::size_t MAX_PATH_HISTORY_LENGTH = 32; // CHANGE WHEN NECESSARY
constexpr std::size_t COUNTER_BITS = 3; // saturated counter length (TAGE paper says to use 3)
constexpr std::size_t GS_HISTORY_TABLE_SIZE = 1 << MAX_TABLE_INDEX_SIZE;
constexpr std::size_t PERIODIC_RESET = 256000; // to reset useful counter after N branches
constexpr std::size_t PRIME_LENGTH = 4093; // closest prime number near 2^12 (not using in actual prgoram since not hardware applicable)
std::size_t totalBranches = 0;

typedef struct table_entry {
    SatCounter<COUNTER_BITS> counter;  // 3-bit saturated counter
    uint64_t tag;       // 8-bit tag for identifying the entry
    SatCounter<2> useful;        // Flag to determine if this entry should be considered or not
} table_entry;

SatCounter<4> useAltPred; // determines which of the two predictions to use
bool isNotWeak = false; // determines if prediction's saturated counter is "strong" ("not weak")
std::size_t tableUsedForPred = 0; // flags the table that was used for last prediction
std::size_t altTableUsedForPred = 0; // alternative table depending on 4-bit saturated counter

uint8_t mainPrediction = 0; // prediction from main table
uint8_t altPrediction = 0; // prediction from alternative table
uint8_t actualPrediction;

std::map<O3_CPU*, std::bitset<MAX_GLOBAL_HISTORY_LENGTH>> branch_history_vector;
std::map<O3_CPU*, std::bitset<MAX_PATH_HISTORY_LENGTH>> path_history_vector;
std::map<O3_CPU*, std::array<std::array<table_entry,
        GS_HISTORY_TABLE_SIZE>, NUM_TABLES>> prediction_table; // stores every branch's PC and corresponding local history

std::map<O3_CPU*, LoopPredictor> loop_predictor;

// note: rightmost bits are the most recent ones for history
// utilizeIP should be false if computing tag, else true 
std::size_t fold_global_history(bool utilizeIP, uint64_t ip, uint64_t tableNum, uint64_t hist_len, uint64_t 
        output_len, std::bitset<MAX_GLOBAL_HISTORY_LENGTH> bh_vector,
        std::bitset<MAX_PATH_HISTORY_LENGTH> ph_vector)
{
    std::size_t tempHistory = 0;
    uint64_t foldedHistory = 0;
    if (tableNum == 0) {
        return ip & ((1 << TABLE_INDEX_SIZES[0]) - 1);
    }

    for (uint64_t i = 0; i < hist_len; i += output_len) {
        std::size_t history = 0;
        for (uint64_t j = i; j < std::min(hist_len, output_len + i); j++) {
            history |= (((uint64_t) bh_vector[0]) << (j - i));
            bh_vector >>= 1;
        }
        tempHistory = history;
        foldedHistory ^= tempHistory;
    }

    if (utilizeIP) {
        foldedHistory ^= (ip & ((1 << output_len) - 1));
        ip = ip >> output_len;
        foldedHistory ^= (ip & ((1 << output_len) - 1));
    }

    return foldedHistory;
}

// Used same 2 shift registers principle from the paper: https://jilp.org/vol7/v7paper10.pdf
std::size_t computeTag(uint64_t ip, uint64_t tableNum, uint64_t hist_len, uint64_t output_len, std::bitset<MAX_GLOBAL_HISTORY_LENGTH> bh_vector, std::bitset<MAX_PATH_HISTORY_LENGTH> ph_vector) {
    
    auto tagHash = ::fold_global_history(false, ip, tableNum, ::GLOBAL_HISTORY_LENGTHS[tableNum], 
            TAG_SIZES[tableNum], bh_vector, ph_vector); // shift register 1
    tagHash ^= ::fold_global_history(false, ip, tableNum, ::GLOBAL_HISTORY_LENGTHS[tableNum], 
            TAG_SIZES[tableNum] - 1, bh_vector, ph_vector); // shift register 2

    return (tagHash ^ ip) & ((1 << TAG_SIZES[tableNum]) - 1);
}

void update(O3_CPU* cpu, uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type) {
    auto index = fold_global_history(true, ip, tableUsedForPred, GLOBAL_HISTORY_LENGTHS[tableUsedForPred], 
            TABLE_INDEX_SIZES[tableUsedForPred], branch_history_vector[cpu], 
            path_history_vector[cpu]);

    auto altIndex = fold_global_history(true, ip, altTableUsedForPred, GLOBAL_HISTORY_LENGTHS
            [altTableUsedForPred], TABLE_INDEX_SIZES[altTableUsedForPred], branch_history_vector[cpu], 
            path_history_vector[cpu]);

    // update useful and saturated counters based on main/alt predictions vs actual result
    if (tableUsedForPred == 0) { 
        prediction_table[cpu][0][index].counter.update(taken); // means bimodal prediction was used
    } else {
        table_entry *tempEntry = &prediction_table[cpu][tableUsedForPred][index];
        table_entry *alt_entry = &prediction_table[cpu][altTableUsedForPred][altIndex];

        if (!isNotWeak && mainPrediction != altPrediction && mainPrediction != taken) {
            useAltPred.update(1); // increment if main prediction was incorrect
        } else if (!isNotWeak && mainPrediction != altPrediction && mainPrediction == taken) {
            useAltPred.update(-1); // decrement if main prediction was correct
        }

        if(useAltPred.predictTaken() && !isNotWeak) { // means main entry is "strongly useless"
            alt_entry->counter.update(taken); // if main prediction was "useless", update the alt entry
        }

        // update useful counter (useful is used for replacing entries...basically cache replacement policy)
        if (mainPrediction != altPrediction) {
            if (mainPrediction == taken) {
                tempEntry->useful.update(1); // correct main prediction
            }
            else {
                if(useAltPred.predictTaken() == taken) { // incorrect main prediction...need to decrement useful counter
                    tempEntry->useful.update(-1);
                } 
            }
        }

        tempEntry->counter.update(taken);
    }

    // misprediction occured
    if (actualPrediction != taken)
    {
        std::size_t startTable = tableUsedForPred + 1;

        // determining if a useless entry exists
        bool uselessTableExists = false;
        for (std::size_t i = startTable; i < NUM_TABLES; i++)
        {
            auto tempInd = fold_global_history(true, ip, i, GLOBAL_HISTORY_LENGTHS[i],TABLE_INDEX_SIZES[i], 
                    branch_history_vector[cpu], path_history_vector[cpu]);
            table_entry *tempIndEntry = &prediction_table[cpu][i][tempInd];
            if (tempIndEntry->useful.val == 0) {
                uselessTableExists = true;
            }
        }
        
        // deals with setting an entry as useless if there are none
        if (!uselessTableExists && startTable < NUM_TABLES) {
            auto tempInd = fold_global_history(true, ip, startTable , GLOBAL_HISTORY_LENGTHS[startTable],
                    TABLE_INDEX_SIZES[startTable], branch_history_vector[cpu], path_history_vector[cpu]);
            prediction_table[cpu][startTable][tempInd].useful.reset();
        }
        auto temporaryInd = fold_global_history(true, ip, NUM_TABLES - 1 , GLOBAL_HISTORY_LENGTHS[NUM_TABLES - 1],
                    TABLE_INDEX_SIZES[NUM_TABLES - 1], branch_history_vector[cpu], path_history_vector[cpu]);

        if (startTable == NUM_TABLES && prediction_table[cpu][NUM_TABLES - 1][temporaryInd].useful.val == 0) {
            startTable = startTable - 1;
        }
        
        // replace some "useless entry" with current ip's tag and set saturated counter to weakly taken
        for (std::size_t i = startTable; i < NUM_TABLES; i++)
        {
            auto tempInd = fold_global_history(true, ip, i, GLOBAL_HISTORY_LENGTHS[i],TABLE_INDEX_SIZES[i], 
                    branch_history_vector[cpu], path_history_vector[cpu]);
            table_entry *tempIndEntry = &prediction_table[cpu][i][tempInd];

            if (tempIndEntry->useful.val == 0 && !(rand() % 5)) // uses random sampling to not always evict first useless entry
            {
                tempIndEntry->tag = computeTag(ip, i, GLOBAL_HISTORY_LENGTHS[i],
                    TABLE_INDEX_SIZES[i], branch_history_vector[cpu], path_history_vector[cpu]);
                tempIndEntry->counter.val = (tempIndEntry->counter.maximum >> 1) + 1;
                break;
            }
        }
    }

}

} 

void O3_CPU::initialize_branch_predictor() {
    ::useAltPred.val = 8; // start off with choosing alternate predictor
    loop_predictor[this].initializeLoopPredictor();
    for (std::size_t i = 0; i < ::NUM_TABLES; i++) {
        for (std::size_t j = 0; j < ::GS_HISTORY_TABLE_SIZE; j++) {
            ::prediction_table[this][i][j].counter.val = 3;
            ::prediction_table[this][i][j].tag = 0;
            ::prediction_table[this][i][j].useful.val = 0; // sets "useful" to "strongly useless"
        }
    }
}

uint8_t O3_CPU::predict_branch(uint64_t ip)
{
    uint8_t loopPredictionResult = ::loop_predictor[this].predict(ip);
    if (::loop_predictor[this].validLoopPredictionOccured) {
        ::actualPrediction = loopPredictionResult;
        return loopPredictionResult;
    }

    // chooses which table to use for prediction (0-indexed)
    bool mainPredSet = false;
    bool altPredSet = false;
    std::size_t indexToUse = 0;
    std::size_t altIndexToUse = 0;
    for (int i = ::NUM_TABLES - 1; i >= 0; i--) {
        auto index = ::fold_global_history(true, ip, i, ::GLOBAL_HISTORY_LENGTHS[i], ::TABLE_INDEX_SIZES[i], 
                ::branch_history_vector[this], ::path_history_vector[this]);
        if (i > 0) {
            // compute 8-bit "Tag index here" and then see if the computed tag index is equal to the tag stored in the prediction table
            std::size_t tag = computeTag(ip, i, ::GLOBAL_HISTORY_LENGTHS[i], ::TABLE_INDEX_SIZES[i],
                    ::branch_history_vector[this], ::path_history_vector[this]);
            if (::prediction_table[this][i][index].tag == tag) {
                if (mainPredSet && !altPredSet) {
                    altPredSet = true;
                    ::altTableUsedForPred = i;
                    altIndexToUse = index;
                } else if (!mainPredSet) {
                    mainPredSet = true;
                    ::tableUsedForPred = i;
                    indexToUse = index;
                }
            }
        }
    }
    
    if (!mainPredSet) { // use bimodal prediction
        ::tableUsedForPred = 0;
        indexToUse = ::fold_global_history(true, ip, 0, ::GLOBAL_HISTORY_LENGTHS[0], ::TABLE_INDEX_SIZES[0], 
                ::branch_history_vector[this], ::path_history_vector[this]);
    }

    if (!altPredSet) {
        ::altTableUsedForPred = 0; 
        altIndexToUse = ::fold_global_history(true, ip, 0, ::GLOBAL_HISTORY_LENGTHS[0], ::TABLE_INDEX_SIZES[0], 
                ::branch_history_vector[this], ::path_history_vector[this]);
    }

    ::table_entry correspondingEntry = ::prediction_table[this][::tableUsedForPred][indexToUse];
    ::table_entry altEntry = ::prediction_table[this][::tableUsedForPred][altIndexToUse];
    bool useAlt = useAltPred.predictTaken() && !correspondingEntry.counter.notWeak(); // use alt if counter >= 8 and main prediction is not "strong"
    ::isNotWeak = correspondingEntry.counter.notWeak(); // same thing as "is the saturated counter strong"
    ::table_entry actualEntry = (useAlt) ? altEntry : correspondingEntry;

    ::mainPrediction = correspondingEntry.counter.predictTaken();
    ::altPrediction = altEntry.counter.predictTaken();

    // determining whether to use prediction or not
    auto value = actualEntry.counter;
    ::actualPrediction = value.predictTaken();
    return ::actualPrediction;
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
    ::loop_predictor[this].update(ip, taken, actualPrediction);
    ::update(this, ip, branch_target, taken, branch_type);

    // reset useful counters in table if necessary
    ::totalBranches++;
    if (::totalBranches == ::PERIODIC_RESET) {
        ::totalBranches = 0;
        for (std::size_t i = 0; i < ::NUM_TABLES; i++) {
            for (std::size_t j = 0; j < ::GS_HISTORY_TABLE_SIZE; j++) {
                ::prediction_table[this][i][j].useful.val >>= 1;
            }
        }   
    }
    // update branch/path history vectors
    ::branch_history_vector[this] <<= 1;
    ::branch_history_vector[this][0] = taken;

    ::path_history_vector[this] <<= 1;
    ::path_history_vector[this][0] = ip & 1;

}
