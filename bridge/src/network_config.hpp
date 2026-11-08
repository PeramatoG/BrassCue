// BrassCue - GPL-3.0. Conservative IPv4 output validation without socket side effects.
#pragma once
#include <stdexcept>
#include <string>
namespace brasscue {
inline void validate_target(const std::string &ip, bool broadcast) {
    unsigned octets[4]{}, part = 0, digits = 0;
    for (char c : ip) {
        if (c == '.') {
            if (!digits || part >= 3)
                throw std::invalid_argument("Invalid IPv4 target");
            ++part;
            digits = 0;
        } else if (c >= '0' && c <= '9') {
            if (++digits > 3)
                throw std::invalid_argument("Invalid IPv4 target");
            octets[part] = octets[part] * 10 + unsigned(c - '0');
            if (octets[part] > 255)
                throw std::invalid_argument("Invalid IPv4 target");
        } else
            throw std::invalid_argument("Invalid IPv4 target");
    }
    if (part != 3 || !digits || octets[0] == 0 || (octets[0] >= 224 && ip != "255.255.255.255"))
        throw std::invalid_argument("Target must be unicast or explicit broadcast IPv4");
    // Without interface masks, support only conventional x.x.x.255 broadcasts.
    if (broadcast && octets[3] != 255)
        throw std::invalid_argument("Broadcast requires an explicit x.x.x.255 target");
    if (!broadcast && octets[3] == 255)
        throw std::invalid_argument("Broadcast target requires opt-in");
}
} // namespace brasscue
