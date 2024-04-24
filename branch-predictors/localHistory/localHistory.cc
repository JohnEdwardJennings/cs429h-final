/*
This is the complete C++ interface for branch predictors in ChampSim.
Here is the source for a clear explanation of the interface functions and parameters:
https://github.com/ChampSim/ChampSim/blob/2b8d3fc28abb6072d7675228418fa7cfe862d4dc/docs/src/Modules.rst#branch-predictors
*/

// implementing K-bit local history (basically for each branch, i'll store its own K-bit history and for each of the possible K-bit histories for each distinct branch, i'll store a 2-bit saturating counter) since champ sim only has global history implemented

// 5% accuracy diff for 100k instructions when using K=14 vs K=20

#include <algorithm>
#include <array>
#include <vector>
#include <bitset>
#include <map>

#include "msl/fwcounter.h"
#include "ooo_cpu.h"

namespace
{
constexpr std::size_t LOCAL_HISTORY_LENGTH = 2;
std::map<O3_CPU*, std::bitset<LOCAL_HISTORY_LENGTH>> prevBits; // stores prev N bits

constexpr std::size_t COUNTER_BITS = 2; // saturated counter length
constexpr std::size_t firstKBitsOfIP = 0x3FFF; // (right now K=14...ask if i can change it to whole PC; as index for local history table

std::map<O3_CPU*, std::map<size_t, std::array<champsim::msl::fwcounter<COUNTER_BITS>,
        (1 << LOCAL_HISTORY_LENGTH)>>> localHistories; // stores every branch's PC and corresponding local history

} // namespace....ask what is namespace and why not just make a helper method outside namespace...why does it need to be in namespace

void O3_CPU::initialize_branch_predictor() {}

uint8_t O3_CPU::predict_branch(uint64_t ip)
{
    size_t hashVal = ip & ::firstKBitsOfIP;
    auto satCounter = ::localHistories[this][hashVal][prevBits[this].to_ullong()]; // should return the appropriate saturated counter
    return satCounter.value() >= (satCounter.maximum / 2); // uses saturated counter of computed 2-bit local history to predict
}

// ensure this ip is the same as the predict_branch's ip...
void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
    size_t hashVal = ip & ::firstKBitsOfIP;
    ::localHistories[this][hashVal][prevBits[this].to_ullong()] += taken ? 1 : -1;

    // update branch history vector
    ::prevBits[this] <<= 1;
    ::prevBits[this][0] = taken;
}