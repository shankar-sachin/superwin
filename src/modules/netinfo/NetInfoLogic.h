// Network adapter enumeration + a simple ICMP ping, over the Windows IP Helper
// and Winsock APIs. Logic lives in superwin_core (no UI).
#pragma once

#include <string>
#include <vector>

namespace superwin {

struct NetAdapter {
    std::string name;          // friendly name (e.g. "Wi-Fi")
    std::string description;   // hardware description
    std::string mac;           // formatted MAC, may be empty
    std::vector<std::string> ipv4;
    std::vector<std::string> ipv6;
    bool up = false;           // operational status
};

struct PingResult {
    bool success = false;
    long roundTripMs = -1;
    std::string resolvedIp;    // the address actually pinged
    std::string error;         // populated on failure
};

// All non-loopback adapters, in the OS's order.
std::vector<NetAdapter> EnumAdapters();

// ICMP echo to `host` (hostname or IP). `timeoutMs` bounds the wait.
PingResult Ping(const std::string& host, int timeoutMs = 1000);

}  // namespace superwin
