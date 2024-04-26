#include <map>

#include "msl/fwcounter.h"
#include "ooo_cpu.h"

namespace
{
constexpr std::size_t BIMODAL_TABLE_SIZE = 1 << 14; // og: 1 << 14
constexpr std::size_t BIMODAL_PRIME = 16381; // dont use since not feasible in hardware (even though prime division improves accuracy by a lot)
constexpr std::size_t COUNTER_BITS = 2;

std::map<O3_CPU*, std::array<champsim::msl::fwcounter<COUNTER_BITS>, BIMODAL_TABLE_SIZE>> bimodal_table;
} // namespace

void O3_CPU::initialize_branch_predictor() {}

uint8_t O3_CPU::predict_branch(uint64_t ip)
{
  auto hash = ip % ::BIMODAL_TABLE_SIZE;
  auto value = ::bimodal_table[this][hash];

  return value.value() >= (value.maximum / 2);
}

void O3_CPU::last_branch_result(uint64_t ip, uint64_t branch_target, uint8_t taken, uint8_t branch_type)
{
  auto hash = ip % ::BIMODAL_TABLE_SIZE;
  ::bimodal_table[this][hash] += taken ? 1 : -1;
}
