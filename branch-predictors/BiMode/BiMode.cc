/*
This is the complete C++ interface for branch predictors in ChampSim.
Here is the source for a clear explanation of the interface functions and parameters:
https://github.com/ChampSim/ChampSim/blob/2b8d3fc28abb6072d7675228418fa7cfe862d4dc/docs/src/Modules.rst#branch-predictors
*/

// note: gselect uses leftmost k bits of PC and rightmost n-k bits of Global history (assume n is length of global history)...

// bi-mode is supposed to be another way of making global history better (an alternative to gselect and gshare)
// note: PHT = pattern history table (basically stores a 2-bit saturated counter given the 14-bit history pattern)

#include <algorithm>
#include <array>
#include <bitset>
#include <map>

#include "msl/fwcounter.h"
#include "ooo_cpu.h"

namespace
{
constexpr std::size_t GLOBAL_HISTORY_LENGTH = 13;
constexpr std::size_t PC_TAG_LENGTH = 13; // determines the number of rightmost bits of the PC used for indexing
constexpr std::size_t PC_TAG_TABLE_SIZE = 1 << PC_TAG_LENGTH; 
constexpr std::size_t firstKBitsOfHistory = GLOBAL_HISTORY_LENGTH >> 1;
constexpr std::size_t COUNTER_BITS = 2; // saturated counter length
constexpr std::size_t GS_HISTORY_TABLE_SIZE = 1 << GLOBAL_HISTORY_LENGTH;

bool prediction = false;
std::map<O3_CPU*, std::bitset<GLOBAL_HISTORY_LENGTH>> branch_history_vector;
std::map<O3_CPU*, std::array<champsim::msl::fwcounter<COUNTER_BITS>, PC_TAG_TABLE_SIZE>> choice_predictor;
std::map<O3_CPU*, std::array<champsim::msl::fwcounter<COUNTER_BITS>, GS_HISTORY_TABLE_SIZE>> history_table_1;
std::map<O3_CPU*, std::array<champsim::msl::fwcounter<COUNTER_BITS>, GS_HISTORY_TABLE_SIZE>> history_table_2;

std::size_t getAppropriateTable(O3_CPU* cpu, uint64_t ip)
{
    auto saturatedCounter = ::choice_predictor[cpu][ip % PC_TAG_TABLE_SIZE];
    return saturatedCounter.value() >= (saturatedCounter.maximum / 2);
}

std::size_t gselect_table_hash(uint64_t ip, std::bitset<GLOBAL_HISTORY_LENGTH> bh_vector)
{
  std::size_t hash = bh_vector.to_ullong();
  return hash % GS_HISTORY_TABLE_SIZE;
}

}

void O3_CPU::initialize_branch_predictor() {}

// to check if predict is incorrect..store global boolean flag that is set to true if prediction occured...
// then check that value with the lastbranchtaken's ("taken") variable
uint8_t O3_CPU::predict_branch(uint64_t ip)
{
    auto tableNum = ::getAppropriateTable(this, ip);
    auto gs_hash = ::gselect_table_hash(ip, ::branch_history_vector[this]);

    if (tableNum) { // use table 2
        auto value = ::history_table_2[this][gs_hash];
        return value.value() >= (value.maximum / 2);
    }

    auto value = ::history_table_1[this][gs_hash];
    ::prediction = value.value() >= (value.maximum / 2);
    return value.value() >= (value.maximum / 2);
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
    auto gs_hash = gselect_table_hash(ip, ::branch_history_vector[this]);
    auto tableNum = ::getAppropriateTable(this, ip);
    if (tableNum) { // use table 2
        ::history_table_2[this][gs_hash] += taken ? 1 : -1;

        // determines if choice predictor predicted branch incorrectly
        auto incorrectChoicePred = (prediction != taken);
        if (incorrectChoicePred) {
            ::choice_predictor[this][ip % PC_TAG_TABLE_SIZE] += taken ? 1 : -1;
        }
    } else {
        ::history_table_1[this][gs_hash] += taken ? 1 : -1;

        // determines if choice predictor predicted branch incorrectly
        auto incorrectChoicePred = (prediction != taken);
        if (incorrectChoicePred) {
            ::choice_predictor[this][ip % PC_TAG_TABLE_SIZE] += taken ? 1 : -1;
        }
    }

    // update branch history vector
    ::branch_history_vector[this] <<= 1;
    ::branch_history_vector[this][0] = taken;
}