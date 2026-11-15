#include "workbench/common/system_info.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#elif defined(__APPLE__)
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/utsname.h>
#elif defined(__linux__)
#include <sys/utsname.h>
#endif

namespace workbench {
namespace system_info {

namespace {

int fallback_physical_count() {
    return logical_cpu_count();
}

#if defined(__linux__)
std::set<std::pair<int, int>> linux_core_ids() {
    std::ifstream input("/proc/cpuinfo");
    std::set<std::pair<int, int>> cores;
    std::string line;
    int package_id = -1;
    int core_id = -1;

    const auto commit_core = [&]() {
        if (core_id >= 0) cores.emplace(package_id < 0 ? 0 : package_id, core_id);
        package_id = -1;
        core_id = -1;
    };

    while (std::getline(input, line)) {
        if (line.empty()) {
            commit_core();
            continue;
        }
        const size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        std::string key = line.substr(0, colon);
        while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back()))) key.pop_back();
        std::istringstream value_stream(line.substr(colon + 1));
        if (key == "physical id") value_stream >> package_id;
        else if (key == "core id") value_stream >> core_id;
    }
    commit_core();
    return cores;
}
#endif

} // namespace

int logical_cpu_count() {
    return std::max(1u, std::thread::hardware_concurrency());
}

int physical_cpu_count() {
#if defined(_WIN32)
    DWORD buffer_size = 0;
    GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &buffer_size);
    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || buffer_size == 0) return fallback_physical_count();
    std::vector<unsigned char> buffer(buffer_size);
    auto* record = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data());
    if (!GetLogicalProcessorInformationEx(RelationProcessorCore, record, &buffer_size)) return fallback_physical_count();
    int cores = 0;
    DWORD offset = 0;
    while (offset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX) <= buffer_size) {
        record = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buffer.data() + offset);
        if (record->Size == 0 || offset + record->Size > buffer_size) break;
        if (record->Relationship == RelationProcessorCore) ++cores;
        offset += record->Size;
    }
    return cores > 0 ? cores : fallback_physical_count();
#elif defined(__APPLE__)
    int cores = 0;
    size_t size = sizeof(cores);
    if (sysctlbyname("hw.physicalcpu", &cores, &size, nullptr, 0) == 0 && cores > 0) return cores;
    return fallback_physical_count();
#elif defined(__linux__)
    const auto cores = linux_core_ids();
    return cores.empty() ? fallback_physical_count() : static_cast<int>(cores.size());
#else
    return fallback_physical_count();
#endif
}

uint64_t total_memory_bytes() {
#if defined(_WIN32)
    MEMORYSTATUSEX status{};
    status.dwLength = static_cast<DWORD>(sizeof(status));
    return GlobalMemoryStatusEx(&status) ? status.ullTotalPhys : 0;
#elif defined(__APPLE__)
    uint64_t memory_bytes = 0;
    size_t size = sizeof(memory_bytes);
    return sysctlbyname("hw.memsize", &memory_bytes, &size, nullptr, 0) == 0 ? memory_bytes : 0;
#elif defined(__linux__)
    std::ifstream input("/proc/meminfo");
    std::string line;
    while (std::getline(input, line)) {
        std::istringstream fields(line);
        std::string key;
        uint64_t value = 0;
        fields >> key >> value;
        if (key == "MemTotal:") return value * 1024;
    }
    return 0;
#else
    return 0;
#endif
}

std::string cpu_model() {
#if defined(_WIN32)
    const char* identifier = std::getenv("PROCESSOR_IDENTIFIER");
    return identifier && *identifier ? identifier : "Windows processor";
#elif defined(__APPLE__)
    size_t size = 0;
    if (sysctlbyname("machdep.cpu.brand_string", nullptr, &size, nullptr, 0) == 0 && size > 1) {
        std::string model(size, '\0');
        if (sysctlbyname("machdep.cpu.brand_string", model.data(), &size, nullptr, 0) == 0) {
            if (!model.empty() && model.back() == '\0') model.pop_back();
            if (!model.empty()) return model;
        }
    }
    struct utsname info {};
    if (uname(&info) == 0) return std::string("Apple processor (") + info.machine + ")";
    return "Apple processor";
#elif defined(__linux__)
    std::ifstream input("/proc/cpuinfo");
    std::string line;
    while (std::getline(input, line)) {
        if (line.rfind("model name", 0) == 0) {
            const size_t colon = line.find(':');
            if (colon != std::string::npos) {
                const size_t first = line.find_first_not_of(" \t", colon + 1);
                if (first != std::string::npos) return line.substr(first);
            }
        }
    }
    struct utsname info {};
    if (uname(&info) == 0) return std::string(info.machine) + " processor";
    return "Linux processor";
#else
    return "Unknown processor";
#endif
}

} // namespace system_info
} // namespace workbench
