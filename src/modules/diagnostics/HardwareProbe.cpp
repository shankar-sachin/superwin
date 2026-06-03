#include "modules/diagnostics/HardwareProbe.h"

#include <Windows.h>
#include <dxgi.h>
#include <psapi.h>
#include <wrl/client.h>

#include <cstdio>
#include <vector>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "psapi.lib")

namespace superwin {
namespace {

std::wstring RegString(HKEY root, const wchar_t* sub, const wchar_t* value) {
    HKEY key;
    if (::RegOpenKeyExW(root, sub, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return {};
    wchar_t buf[512];
    DWORD size = sizeof(buf), type = 0;
    std::wstring out;
    if (::RegQueryValueExW(key, value, nullptr, &type, reinterpret_cast<LPBYTE>(buf), &size)
            == ERROR_SUCCESS && type == REG_SZ) {
        out.assign(buf, size / sizeof(wchar_t));
        if (!out.empty() && out.back() == L'\0') out.pop_back();
    }
    ::RegCloseKey(key);
    return out;
}

DWORD RegDword(HKEY root, const wchar_t* sub, const wchar_t* value) {
    HKEY key;
    if (::RegOpenKeyExW(root, sub, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return 0;
    DWORD data = 0, size = sizeof(data), type = 0;
    if (::RegQueryValueExW(key, value, nullptr, &type, reinterpret_cast<LPBYTE>(&data), &size)
            != ERROR_SUCCESS || type != REG_DWORD) {
        data = 0;
    }
    ::RegCloseKey(key);
    return data;
}

ULONGLONG ToU64(const FILETIME& ft) {
    return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

void TrimTrailingSpaces(std::wstring& s) {
    while (!s.empty() && s.back() == L' ') s.pop_back();
}

std::wstring ArchName(WORD arch) {
    switch (arch) {
        case PROCESSOR_ARCHITECTURE_AMD64: return L"x64";
        case PROCESSOR_ARCHITECTURE_ARM64: return L"ARM64";
        case PROCESSOR_ARCHITECTURE_ARM:   return L"ARM";
        case PROCESSOR_ARCHITECTURE_INTEL: return L"x86";
        default:                           return L"Unknown";
    }
}

int CountPhysicalCores() {
    DWORD len = 0;
    ::GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &len);
    if (len == 0) return 0;
    std::vector<BYTE> buf(len);
    if (!::GetLogicalProcessorInformationEx(RelationProcessorCore,
            reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buf.data()), &len)) {
        return 0;
    }
    int cores = 0;
    for (DWORD off = 0; off < len;) {
        auto* info = reinterpret_cast<SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(buf.data() + off);
        if (info->Relationship == RelationProcessorCore) ++cores;
        off += info->Size;
    }
    return cores;
}

}  // namespace

std::wstring FormatBytes(uint64_t bytes) {
    const double tb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0 * 1024.0);
    wchar_t buf[32];
    if (tb >= 1.0) { swprintf_s(buf, L"%.2f TB", tb); return buf; }
    const double gb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    if (gb >= 1.0) { swprintf_s(buf, L"%.1f GB", gb); return buf; }
    const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    swprintf_s(buf, L"%.0f MB", mb);
    return buf;
}

std::wstring FormatDuration(uint64_t secs) {
    const uint64_t d = secs / 86400;
    const uint64_t h = (secs % 86400) / 3600;
    const uint64_t m = (secs % 3600) / 60;
    wchar_t buf[48];
    if (d > 0)      swprintf_s(buf, L"%llud %02lluh %02llum", d, h, m);
    else if (h > 0) swprintf_s(buf, L"%lluh %02llum", h, m);
    else            swprintf_s(buf, L"%llum %02llus", m, secs % 60);
    return buf;
}

SystemSpecs ProbeSystem() {
    SystemSpecs s;

    // --- CPU ---
    const wchar_t* kCpuKey = L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
    s.cpuName = RegString(HKEY_LOCAL_MACHINE, kCpuKey, L"ProcessorNameString");
    TrimTrailingSpaces(s.cpuName);
    s.cpuVendor = RegString(HKEY_LOCAL_MACHINE, kCpuKey, L"VendorIdentifier");
    s.cpuBaseMHz = static_cast<int>(RegDword(HKEY_LOCAL_MACHINE, kCpuKey, L"~MHz"));

    SYSTEM_INFO si{};
    ::GetNativeSystemInfo(&si);
    s.logicalCores = static_cast<int>(si.dwNumberOfProcessors);
    s.physicalCores = CountPhysicalCores();
    s.cpuArch = ArchName(si.wProcessorArchitecture);

    // --- Memory ---
    MEMORYSTATUSEX mem{sizeof(mem)};
    ::GlobalMemoryStatusEx(&mem);
    s.ramTotalBytes = mem.ullTotalPhys;

    // --- GPU(s) via DXGI ---
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    if (SUCCEEDED(::CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc{};
            if (SUCCEEDED(adapter->GetDesc1(&desc))) {
                // Skip the Microsoft Basic Render Driver (software adapter).
                if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
                std::wstring entry = std::wstring(desc.Description) + L"  (" +
                                     FormatBytes(desc.DedicatedVideoMemory) + L")";
                s.gpus.push_back(entry);
                if (s.gpuName.empty()) {
                    s.gpuName = desc.Description;
                    s.vramBytes = desc.DedicatedVideoMemory;
                }
            }
            adapter.Reset();
        }
    }

    // --- OS / identity ---
    const wchar_t* kVer = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    std::wstring product = RegString(HKEY_LOCAL_MACHINE, kVer, L"ProductName");
    std::wstring display = RegString(HKEY_LOCAL_MACHINE, kVer, L"DisplayVersion");
    std::wstring buildStr = RegString(HKEY_LOCAL_MACHINE, kVer, L"CurrentBuildNumber");
    const DWORD ubr = RegDword(HKEY_LOCAL_MACHINE, kVer, L"UBR");
    const int build = _wtoi(buildStr.c_str());

    // The registry's ProductName still says "Windows 10" on Windows 11. Builds
    // 22000 and above are Windows 11 -- correct the label.
    if (build >= 22000) {
        auto pos = product.find(L"Windows 10");
        if (pos != std::wstring::npos) product.replace(pos, 10, L"Windows 11");
    }
    s.osName = display.empty() ? product : (product + L" (" + display + L")");
    if (!buildStr.empty()) {
        s.osBuild = buildStr;
        if (ubr) s.osBuild += L"." + std::to_wstring(ubr);
    }
    s.osArch = ArchName(si.wProcessorArchitecture);

    wchar_t name[256]; DWORD nameLen = 256;
    if (::GetComputerNameExW(ComputerNameDnsHostname, name, &nameLen)) s.computerName.assign(name, nameLen);
    nameLen = 256;
    if (::GetUserNameW(name, &nameLen) && nameLen > 0) s.userName.assign(name, nameLen - 1);

    // --- Firmware / board ---
    const wchar_t* kBios = L"HARDWARE\\DESCRIPTION\\System\\BIOS";
    s.systemManufacturer = RegString(HKEY_LOCAL_MACHINE, kBios, L"SystemManufacturer");
    s.systemProduct = RegString(HKEY_LOCAL_MACHINE, kBios, L"SystemProductName");
    std::wstring biosVendor = RegString(HKEY_LOCAL_MACHINE, kBios, L"BIOSVendor");
    std::wstring biosVer = RegString(HKEY_LOCAL_MACHINE, kBios, L"BIOSVersion");
    s.biosVersion = biosVendor.empty() ? biosVer
                  : (biosVer.empty() ? biosVendor : biosVendor + L" " + biosVer);
    std::wstring boardMfr = RegString(HKEY_LOCAL_MACHINE, kBios, L"BaseBoardManufacturer");
    std::wstring boardProd = RegString(HKEY_LOCAL_MACHINE, kBios, L"BaseBoardProduct");
    s.baseBoard = (boardMfr + L" " + boardProd);
    TrimTrailingSpaces(s.baseBoard);

    // --- Storage (fixed drives) ---
    wchar_t drives[256];
    const DWORD dlen = ::GetLogicalDriveStringsW(255, drives);
    for (const wchar_t* p = drives; p < drives + dlen && *p; p += wcslen(p) + 1) {
        if (::GetDriveTypeW(p) != DRIVE_FIXED) continue;
        ULARGE_INTEGER freeForCaller{}, total{}, freeTotal{};
        if (::GetDiskFreeSpaceExW(p, &freeForCaller, &total, &freeTotal)) {
            wchar_t fsName[32] = L"";
            ::GetVolumeInformationW(p, nullptr, 0, nullptr, nullptr, nullptr, fsName, 32);
            std::wstring letter(p);
            if (!letter.empty() && letter.back() == L'\\') letter.pop_back();
            std::wstring entry = letter + L"  " + FormatBytes(freeTotal.QuadPart) +
                                 L" free of " + FormatBytes(total.QuadPart);
            if (fsName[0]) entry += std::wstring(L" (") + fsName + L")";
            s.disks.push_back(entry);
        }
    }

    // --- Displays ---
    const int monitors = ::GetSystemMetrics(SM_CMONITORS);
    const int cx = ::GetSystemMetrics(SM_CXSCREEN);
    const int cy = ::GetSystemMetrics(SM_CYSCREEN);
    s.displayInfo = std::to_wstring(monitors) + (monitors == 1 ? L" display  -  " : L" displays  -  ") +
                    std::to_wstring(cx) + L"x" + std::to_wstring(cy) + L" primary";

    return s;
}

double CpuUsagePercent() {
    static ULONGLONG prevIdle = 0, prevKernel = 0, prevUser = 0;
    FILETIME idle, kernel, user;
    if (!::GetSystemTimes(&idle, &kernel, &user)) return 0;
    const ULONGLONG i = ToU64(idle), k = ToU64(kernel), u = ToU64(user);
    const ULONGLONG dIdle = i - prevIdle, dKernel = k - prevKernel, dUser = u - prevUser;
    prevIdle = i; prevKernel = k; prevUser = u;
    const ULONGLONG total = dKernel + dUser;  // kernel already includes idle
    if (total == 0) return 0;
    return (1.0 - static_cast<double>(dIdle) / static_cast<double>(total)) * 100.0;
}

double RamUsagePercent() {
    MEMORYSTATUSEX mem{sizeof(mem)};
    ::GlobalMemoryStatusEx(&mem);
    return mem.dwMemoryLoad;
}

uint64_t RamUsedBytes() {
    MEMORYSTATUSEX mem{sizeof(mem)};
    ::GlobalMemoryStatusEx(&mem);
    return mem.ullTotalPhys - mem.ullAvailPhys;
}

LiveStats SampleLive() {
    LiveStats s;
    s.cpuPercent = CpuUsagePercent();

    MEMORYSTATUSEX mem{sizeof(mem)};
    ::GlobalMemoryStatusEx(&mem);
    s.ramPercent = mem.dwMemoryLoad;
    s.ramTotalBytes = mem.ullTotalPhys;
    s.ramUsedBytes = mem.ullTotalPhys - mem.ullAvailPhys;

    PERFORMANCE_INFORMATION pi{sizeof(pi)};
    if (::GetPerformanceInfo(&pi, sizeof(pi))) {
        const uint64_t page = pi.PageSize;
        s.commitUsedBytes = static_cast<uint64_t>(pi.CommitTotal) * page;
        s.commitLimitBytes = static_cast<uint64_t>(pi.CommitLimit) * page;
        s.cachedBytes = static_cast<uint64_t>(pi.SystemCache) * page;
        s.processCount = pi.ProcessCount;
        s.threadCount = pi.ThreadCount;
        s.handleCount = pi.HandleCount;
    }

    s.uptimeSeconds = ::GetTickCount64() / 1000;
    return s;
}

}  // namespace superwin
