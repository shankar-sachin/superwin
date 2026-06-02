#include "modules/netinfo/NetInfoLogic.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>

#include <cstdio>
#include <cstring>
#include <memory>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace superwin {
namespace {

std::string SockAddrToString(const SOCKET_ADDRESS& addr) {
    char buf[NI_MAXHOST] = {};
    if (addr.lpSockaddr &&
        getnameinfo(addr.lpSockaddr, addr.iSockaddrLength, buf, sizeof(buf),
                    nullptr, 0, NI_NUMERICHOST) == 0) {
        // Strip the IPv6 scope id suffix ("%12") for a cleaner display.
        if (char* pct = std::strchr(buf, '%')) *pct = '\0';
        return buf;
    }
    return {};
}

std::string FormatMac(const BYTE* mac, ULONG len) {
    if (len == 0) return {};
    std::string out;
    char part[4];
    for (ULONG i = 0; i < len; ++i) {
        std::snprintf(part, sizeof(part), "%02X", mac[i]);
        if (i) out += '-';
        out += part;
    }
    return out;
}

// RAII guard for one-shot Winsock init.
struct WsaGuard {
    bool ok = false;
    WsaGuard() { WSADATA d; ok = (WSAStartup(MAKEWORD(2, 2), &d) == 0); }
    ~WsaGuard() { if (ok) WSACleanup(); }
};

}  // namespace

std::vector<NetAdapter> EnumAdapters() {
    std::vector<NetAdapter> result;
    WsaGuard wsa;  // for getnameinfo

    ULONG size = 15 * 1024;
    std::unique_ptr<char[]> buffer;
    ULONG ret = 0;
    const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    for (int attempt = 0; attempt < 3; ++attempt) {
        buffer = std::make_unique<char[]>(size);
        ret = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr,
                                   reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.get()), &size);
        if (ret != ERROR_BUFFER_OVERFLOW) break;
    }
    if (ret != NO_ERROR) return result;

    for (auto* a = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.get()); a; a = a->Next) {
        if (a->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;

        NetAdapter adapter;
        char name[256] = {};
        WideCharToMultiByte(CP_UTF8, 0, a->FriendlyName, -1, name, sizeof(name), nullptr, nullptr);
        adapter.name = name;
        char desc[256] = {};
        WideCharToMultiByte(CP_UTF8, 0, a->Description, -1, desc, sizeof(desc), nullptr, nullptr);
        adapter.description = desc;
        adapter.mac = FormatMac(a->PhysicalAddress, a->PhysicalAddressLength);
        adapter.up = (a->OperStatus == IfOperStatusUp);

        for (auto* u = a->FirstUnicastAddress; u; u = u->Next) {
            std::string ip = SockAddrToString(u->Address);
            if (ip.empty()) continue;
            if (u->Address.lpSockaddr->sa_family == AF_INET) adapter.ipv4.push_back(ip);
            else if (u->Address.lpSockaddr->sa_family == AF_INET6) adapter.ipv6.push_back(ip);
        }
        result.push_back(std::move(adapter));
    }
    return result;
}

PingResult Ping(const std::string& host, int timeoutMs) {
    PingResult r;
    WsaGuard wsa;
    if (!wsa.ok) { r.error = "Winsock init failed"; return r; }

    // Resolve to an IPv4 address.
    addrinfo hints{};
    hints.ai_family = AF_INET;
    addrinfo* info = nullptr;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &info) != 0 || !info) {
        r.error = "Could not resolve host";
        return r;
    }
    auto* sa = reinterpret_cast<sockaddr_in*>(info->ai_addr);
    IPAddr dest = sa->sin_addr.S_un.S_addr;
    char ipText[INET_ADDRSTRLEN] = {};
    inet_ntop(AF_INET, &sa->sin_addr, ipText, sizeof(ipText));
    r.resolvedIp = ipText;
    freeaddrinfo(info);

    HANDLE icmp = IcmpCreateFile();
    if (icmp == INVALID_HANDLE_VALUE) { r.error = "ICMP unavailable"; return r; }

    char sendData[32] = "SuperWin ping";
    char replyBuf[sizeof(ICMP_ECHO_REPLY) + sizeof(sendData) + 8] = {};
    DWORD got = IcmpSendEcho(icmp, dest, sendData, sizeof(sendData), nullptr,
                             replyBuf, sizeof(replyBuf), static_cast<DWORD>(timeoutMs));
    if (got > 0) {
        auto* reply = reinterpret_cast<ICMP_ECHO_REPLY*>(replyBuf);
        if (reply->Status == IP_SUCCESS) {
            r.success = true;
            r.roundTripMs = static_cast<long>(reply->RoundTripTime);
        } else {
            r.error = "Request timed out";
        }
    } else {
        r.error = "Request timed out";
    }
    IcmpCloseHandle(icmp);
    return r;
}

}  // namespace superwin
