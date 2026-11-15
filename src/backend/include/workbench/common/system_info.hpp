#pragma once

#include <cstdint>
#include <string>

namespace workbench {
namespace system_info {

int logical_cpu_count();
int physical_cpu_count();
uint64_t total_memory_bytes();
std::string cpu_model();

} // namespace system_info
} // namespace workbench
