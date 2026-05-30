// Hardware/OS facts plus lightweight live counters for the Diagnostics page.
#pragma once

#include <cstdint>
#include <string>

namespace superwin {

struct SystemSpecs {
    std::wstring cpuName;
    int          logicalCores = 0;
    std::wstring gpuName;
    uint64_t     vramBytes = 0;
    uint64_t     ramTotalBytes = 0;
    std::wstring osName;
};

SystemSpecs ProbeSystem();

// Live counters (delta-based; call periodically).
double   CpuUsagePercent();
double   RamUsagePercent();
uint64_t RamUsedBytes();

std::wstring FormatBytes(uint64_t bytes);  // e.g. "16.0 GB"

}  // namespace superwin
