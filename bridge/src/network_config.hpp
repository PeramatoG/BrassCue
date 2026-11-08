// BrassCue - GPL-3.0. Portable IPv4/interface selection; never guesses a /24 network.
#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>
namespace brasscue {
inline std::uint32_t ipv4_number(const std::string &text) {
    std::uint32_t value = 0, octet = 0;
    unsigned part = 0, digits = 0;
    for (char c : text) {
        if (c == '.') {
            if (!digits || part >= 3)
                throw std::invalid_argument("Invalid IPv4 address");
            value = (value << 8) | octet;
            ++part;
            digits = 0;
            octet = 0;
        } else if (c >= '0' && c <= '9') {
            if (++digits > 3 || (digits > 1 && octet == 0))
                throw std::invalid_argument("Invalid IPv4 address");
            octet = octet * 10 + unsigned(c - '0');
            if (octet > 255)
                throw std::invalid_argument("Invalid IPv4 address");
        } else
            throw std::invalid_argument("Invalid IPv4 address");
    }
    if (part != 3 || !digits)
        throw std::invalid_argument("Invalid IPv4 address");
    return (value << 8) | octet;
}
inline std::string ipv4_text(std::uint32_t value) {
    return std::to_string(value >> 24) + "." + std::to_string((value >> 16) & 255) + "." +
           std::to_string((value >> 8) & 255) + "." + std::to_string(value & 255);
}
inline void validate_target(const std::string &ip, bool broadcast) {
    auto n = ipv4_number(ip), first = n >> 24;
    if (first == 0 || (first >= 224 && n != 0xffffffffu) || (n == 0xffffffffu && !broadcast))
        throw std::invalid_argument("Target must be unicast or explicitly enabled broadcast IPv4");
}
inline std::uint32_t prefix_mask(unsigned prefix) {
    if (prefix > 32)
        throw std::invalid_argument("Invalid IPv4 prefix");
    return prefix ? 0xffffffffu << (32 - prefix) : 0;
}
inline std::string directed_broadcast(const std::string &address, const std::string &mask) {
    if (mask.empty())
        throw std::invalid_argument("Selected interface IPv4 mask is unavailable");
    auto ip = ipv4_number(address), netmask = ipv4_number(mask), inverse = ~netmask;
    if (netmask == 0 || inverse < 3 || (inverse & (inverse + 1)) != 0)
        throw std::invalid_argument(
            "Interface needs a contiguous /1 through /30 mask for broadcast");
    if ((ip & inverse) == 0 || (ip & inverse) == inverse)
        throw std::invalid_argument("Interface IPv4 is not a host address");
    return ipv4_text(ip | inverse);
}
struct NetworkInterface {
    std::string id, name, address, netmask;
    unsigned index = 0;
    bool active = true, loopback = false, broadcast_capable = true;
};
struct ResolvedNetwork {
    std::string target, source;
    unsigned interface_index = 0;
    bool broadcast = false;
};
inline ResolvedNetwork resolve_network(const std::string &target, bool broadcast,
                                       const std::string &id,
                                       const std::vector<NetworkInterface> &interfaces) {
    if (id.empty()) {
        if (broadcast)
            throw std::invalid_argument(
                "Select an active IPv4 interface for broadcast; Auto is unicast only");
        validate_target(target, false);
        return {target, {}, 0, false};
    }
    for (const auto &nic : interfaces)
        if (nic.id == id) {
            if (!nic.active || nic.index == 0)
                throw std::invalid_argument("Selected network interface is unavailable");
            validate_target(nic.address, false);
            if (!broadcast) {
                validate_target(target, false);
                return {target, nic.address, nic.index, false};
            }
            if (nic.loopback || !nic.broadcast_capable)
                throw std::invalid_argument(
                    "Selected interface does not support directed broadcast");
            return {directed_broadcast(nic.address, nic.netmask), nic.address, nic.index, true};
        }
    throw std::invalid_argument("Saved/selected network interface is no longer available");
}
} // namespace brasscue
