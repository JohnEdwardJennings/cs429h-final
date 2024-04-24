/*
This is the complete C++ interface for branch predictors in ChampSim.
Here is the source for a clear explanation of the interface functions and parameters:
https://github.com/ChampSim/ChampSim/blob/2b8d3fc28abb6072d7675228418fa7cfe862d4dc/docs/src/Modules.rst#branch-predictors
*/

// note: gselect concatenates rightmost k bits of PC with rightmost n-k bits of Global history (assume n is length of global history)...also..bimodal predictor = saturated counter....

// also apparently optimal gshare doesn't just XOR PC with global history...it does some extra XORs/has multiple pattern history tables (where a 14-bit history corresponds to multiple predictions, which are selected by other saturated counters..ie in bi-mode)

// ask john if we should try multiple PHTs for gselect as well (in addition to multiple values of "K")...and if
// we should change global history bit length or not (14 is champ sim's default)...

// implementing gselect so that we can benchmark with already implement gshare (and will be used for hybrid predictors that we will implement as well)

#include <algorithm>
#include <array>
#include <bitset>
#include <map>

#include "msl/fwcounter.h"
#include "ooo_cpu.h"

namespace
{
constexpr std::size_t GLOBAL_HISTORY_LENGTH = 14; // look up what ispurpose of "const expr"?
constexpr std::size_t firstKBitsOfHistory = GLOBAL_HISTORY_LENGTH >> 1;
constexpr std::size_t COUNTER_BITS = 2; // saturated counter length
constexpr std::size_t GS_HISTORY_TABLE_SIZE = 16384;

// ask if saturated counter is set to weakly taken or not (01)
std::map<O3_CPU*, std::bitset<GLOBAL_HISTORY_LENGTH>> branch_history_vector;
std::map<O3_CPU*, std::array<champsim::msl::fwcounter<COUNTER_BITS>, GS_HISTORY_TABLE_SIZE>> gs_history_table;

std::size_t gselect_table_hash(uint64_t ip, std::bitset<GLOBAL_HISTORY_LENGTH> bh_vector)
{
  std::size_t hash = bh_vector.to_ullong(); // ask john what bh_vector is...is it the global history?

  // concatenates rightmost K bitsof PC with rightmost K bits of global history (is it worth modifying K for extra statistical analysis???)
  hash = ((ip % (1 << firstKBitsOfHistory)) << firstKBitsOfHistory) | (hash % (1 << firstKBitsOfHistory)); 

  return hash % GS_HISTORY_TABLE_SIZE;
}
} // namespace....ask what is namespace and why not just make a helper method outside namespace...why does it need to be in namespace

void O3_CPU::initialize_branch_predictor() {}

uint8_t O3_CPU::predict_branch(uint64_t ip)
{
  auto gs_hash = ::gselect_table_hash(ip, ::branch_history_vector[this]); // ask why we have :: without anything before it?
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