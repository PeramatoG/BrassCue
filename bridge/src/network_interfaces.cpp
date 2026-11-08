// BrassCue - GPL-3.0. Enumerate active IPv4 addresses without creating output sockets.
#include "network_interfaces.hpp"
#ifdef _WIN32
#include <winsock2.h>

#include <iphlpapi.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <memory>
#include <net/if.h>
#endif
namespace brasscue {
std::vector<NetworkInterface> network_interfaces() {
    std::vector<NetworkInterface> result;
#ifdef _WIN32
    ULONG size = 16384;
    std::vector<unsigned char> buffer;
    ULONG status = ERROR_BUFFER_OVERFLOW;
    for (unsigned attempt = 0; attempt < 4 && status == ERROR_BUFFER_OVERFLOW; ++attempt) {
        if (size > 1024 * 1024)
            throw std::runtime_error("Network adapter metadata is too large");
        buffer.resize(size);
        status = GetAdaptersAddresses(
            AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
            nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data()), &size);
    }
    if (status == ERROR_NO_DATA)
        return result;
    if (status != NO_ERROR)
        throw std::runtime_error("GetAdaptersAddresses failed: " + std::to_string(status));
    for (auto *adapter = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data()); adapter;
         adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp)
            continue;
        std::string name = adapter->AdapterName;
        if (adapter->FriendlyName) {
            int bytes = WideCharToMultiByte(CP_UTF8, 0, adapter->FriendlyName, -1, nullptr, 0,
                                            nullptr, nullptr);
            if (bytes > 1) {
                std::string utf8(bytes, '\0');
                WideCharToMultiByte(CP_UTF8, 0, adapter->FriendlyName, -1, utf8.data(), bytes,
                                    nullptr, nullptr);
                utf8.pop_back();
                name = std::move(utf8);
            }
        }
        for (auto *address = adapter->FirstUnicastAddress; address; address = address->Next) {
            if (!address->Address.lpSockaddr || address->Address.lpSockaddr->sa_family != AF_INET)
                continue;
            const auto ip = ntohl(
                reinterpret_cast<sockaddr_in *>(address->Address.lpSockaddr)->sin_addr.s_addr);
            if (!ip)
                continue;
            const auto text = ipv4_text(ip);
            const bool loop = adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK;
            const bool broadcast =
                !loop && adapter->IfType != IF_TYPE_PPP && adapter->IfType != IF_TYPE_TUNNEL;
            result.push_back({std::string(adapter->AdapterName) + "/" + text, name, text,
                              ipv4_text(prefix_mask(address->OnLinkPrefixLength)), adapter->IfIndex,
                              true, loop, broadcast});
        }
    }
#else
    ifaddrs *raw = nullptr;
    if (getifaddrs(&raw) != 0)
        throw std::runtime_error("getifaddrs failed");
    std::unique_ptr<ifaddrs, decltype(&freeifaddrs)> guard(raw, freeifaddrs);
    for (auto *a = raw; a; a = a->ifa_next) {
        if (!a->ifa_addr || !a->ifa_netmask || a->ifa_addr->sa_family != AF_INET ||
            !(a->ifa_flags & IFF_UP))
            continue;
        const auto ip = ntohl(reinterpret_cast<sockaddr_in *>(a->ifa_addr)->sin_addr.s_addr);
        if (!ip)
            continue;
        const auto text = ipv4_text(ip);
        const auto mask = ntohl(reinterpret_cast<sockaddr_in *>(a->ifa_netmask)->sin_addr.s_addr);
        result.push_back({std::string(a->ifa_name) + "/" + text, a->ifa_name, text, ipv4_text(mask),
                          if_nametoindex(a->ifa_name), true, bool(a->ifa_flags & IFF_LOOPBACK),
                          bool(a->ifa_flags & IFF_BROADCAST)});
    }
#endif
    return result;
}
} // namespace brasscue
