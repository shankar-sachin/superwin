// Hardware/OS facts plus lightweight live counters for the Diagnostics page.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace superwin {

// Static facts gathered once at startup.
struct SystemSpecs {
    // CPU
    std::wstring cpuName;
    std::wstring cpuVendor;
    std::wstring cpuArch;       // e.g. "x64", "ARM64"
    int          physicalCores = 0;
    int          logicalCores = 0;
    int          cpuBaseMHz = 0;

    // GPU(s)
    std::wstring gpuName;       // primary adapter
    uint64_t     vramBytes = 0;
    std::vector<std::wstring> gpus;  // every adapter, formatted "name  (vram)"

    // Memory
    uint64_t     ramTotalBytes = 0;

    // OS / identity
    std::wstring osName;        // Win11-corrected product name
    std::wstring osBuild;       // e.g. "22631.4317"
    std::wstring osArch;
    std::wstring computerName;
    std::wstring userName;

    // Firmware / board
    std::wstring systemManufacturer;
    std::wstring systemProduct;
    std::wstring biosVersion;
    std::wstring baseBoard;

    // Storage / displays
    std::vector<std::wstring> disks;  // "C:  120.0 GB free of 512.0 GB (NTFS)"
    std::wstring displayInfo;         // "2 displays  -  1920x1080 primary"
};

SystemSpecs ProbeSystem();

// A snapshot of everything that moves; sample once per tick.
struct LiveStats {
    double   cpuPercent = 0;
    double   ramPercent = 0;
    uint64_t ramUsedBytes = 0;
    uint64_t ramTotalBytes = 0;
    uint64_t commitUsedBytes = 0;
    uint64_t commitLimitBytes = 0;
    uint64_t cachedBytes = 0;
    uint32_t processCount = 0;
    uint32_t threadCount = 0;
    uint32_t handleCount = 0;
    uint64_t uptimeSeconds = 0;

    // GPU (utilization via PDH, VRAM via DXGI).
    bool     gpuAvailable = false;
    double   gpuPercent = 0;
    uint64_t vramUsedBytes = 0;
    uint64_t vramBudgetBytes = 0;

    // Physical disk activity + throughput (PDH, _Total instance).
    bool     diskAvailable = false;
    double   diskActivePercent = 0;
    uint64_t diskReadBytesPerSec = 0;
    uint64_t diskWriteBytesPerSec = 0;
};

LiveStats SampleLive();

// Live counters (delta-based; call periodically). Retained for simple callers.
double   CpuUsagePercent();
double   RamUsagePercent();
uint64_t RamUsedBytes();

std::wstring FormatBytes(uint64_t bytes);   // e.g. "16.0 GB"
std::wstring FormatDuration(uint64_t secs); // e.g. "3d 04h 12m"

}  // namespace superwin
