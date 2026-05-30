#include "modules/diagnostics/HardwareProbe.h"

#include <Windows.h>
#include <dxgi.h>
#include <wrl/client.h>

#include <cstdio>

#pragma comment(lib, "dxgi.lib")

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

ULONGLONG ToU64(const FILETIME& ft) {
    return (static_cast<ULONGLONG>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
}

}  // namespace

std::wstring FormatBytes(uint64_t bytes) {
    const double gb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    wchar_t buf[32];
    if (gb >= 1.0) { swprintf_s(buf, L"%.1f GB", gb); return buf; }
    const double mb = static_cast<double>(bytes) / (1024.0 * 1024.0);
    swprintf_s(buf, L"%.0f MB", mb);
    return buf;
}

SystemSpecs ProbeSystem() {
    SystemSpecs s;

    s.cpuName = RegString(HKEY_LOCAL_MACHINE,
        L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"ProcessorNameString");
    if (!s.cpuName.empty()) {
        // trim trailing spaces
        while (!s.cpuName.empty() && s.cpuName.back() == L' ') s.cpuName.pop_back();
    }

    SYSTEM_INFO si{};
    ::GetSystemInfo(&si);
    s.logicalCores = static_cast<int>(si.dwNumberOfProcessors);

    MEMORYSTATUSEX mem{sizeof(mem)};
    ::GlobalMemoryStatusEx(&mem);
    s.ramTotalBytes = mem.ullTotalPhys;

    // GPU via DXGI (primary adapter).
    Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
    if (SUCCEEDED(::CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        if (SUCCEEDED(factory->EnumAdapters1(0, &adapter))) {
            DXGI_ADAPTER_DESC1 desc{};
            if (SUCCEEDED(adapter->GetDesc1(&desc))) {
                s.gpuName = desc.Description;
                s.vramBytes = desc.DedicatedVideoMemory;
            }
        }
    }

    std::wstring product = RegString(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"ProductName");
    std::wstring display = RegString(HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", L"DisplayVersion");
    s.osName = display.empty() ? product : (product + L" (" + display + L")");

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

}  // namespace superwin
