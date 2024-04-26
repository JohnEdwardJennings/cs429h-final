/*
This is the complete C++ interface for branch predictors in ChampSim.
Here is the source for a clear explanation of the interface functions and parameters:
https://github.com/ChampSim/ChampSim/blob/2b8d3fc28abb6072d7675228418fa7cfe862d4dc/docs/src/Modules.rst#branch-predictors
*/

// note: gselect concatenates rightmost k bits of PC with rightmost n-k bits of Global history (assume n is length of global history)...

// implementing gselect so that we can benchmark with already implement gshare

#include <algorithm>
#include <array>
#include <bitset>
#include <map>

#include "msl/fwcounter.h"
#include "ooo_cpu.h"

namespace
{
constexpr std::size_t GLOBAL_HISTORY_LENGTH = 14;
constexpr std::size_t firstKBitsOfHistory = GLOBAL_HISTORY_LENGTH >> 1;
constexpr std::size_t COUNTER_BITS = 2; // saturated counter length
constexpr std::size_t GS_HISTORY_TABLE_SIZE = 16384;

std::map<O3_CPU*, std::bitset<GLOBAL_HISTORY_LENGTH>> branch_history_vector;
std::map<O3_CPU*, std::array<champsim::msl::fwcounter<COUNTER_BITS>, GS_HISTORY_TABLE_SIZE>> gs_history_table;

std::size_t gselect_table_hash(uint64_t ip, std::bitset<GLOBAL_HISTORY_LENGTH> bh_vector)
{
  std::size_t hash = bh_vector.to_ullong();

  // concatenates rightmost K bits of PC with rightmost K bits of global history
  hash = ((ip % (1 << firstKBitsOfHistory)) << firstKBitsOfHistory) | (hash % (1 << firstKBitsOfHistory)); 

  return hash % GS_HISTORY_TABLE_SIZE;
}
}

void O3_CPU::initialize_branch_predictor() {}

uint8_t O3_CPU::predict_branch(uint64_t ip)
{
  auto gs_hash = ::gselect_table_hash(ip, ::branch_history_vector[this]);
  auto value = ::gs_history_table[this][gs_hash];
  return value.value() >= (value.maximum / 2); // uses saturated counter of computed 14-bit history to predict
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  auto gs_hash = gselect_table_hash(ip, ::branch_history_vector[this]);
  ::gs_history_table[this][gs_hash] += taken ? 1 : -1;

  // update branch history vector
  ::branch_history_vector[this] <<= 1;
  ::branch_history_vector[this][0] = taken;
}