// BrassCue - GPL-3.0. Patch and cue configuration types.
#pragma once
#include "value_conversion.hpp"
#include <cstdint>
#include <map>
#include <stdexcept>
#include <vector>
namespace brasscue {
struct Address {
    std::uint16_t universe = 1; // User-facing, 1-based Art-Net port address
    std::uint16_t channel = 1;  // DMX 1..512
    bool operator<(const Address& rhs) const {
        return universe < rhs.universe || (universe == rhs.universe && channel < rhs.channel);
    }
};
struct Assignment {
    Address address;
    std::uint8_t value = 255; // Always normalized to 0..255 internally
};
enum class ValueDisplay { Dmx255, Percent };
enum class ButtonMode { Toggle, Flash };
struct ButtonConfig {
    std::vector<Assignment> assignments;
    ButtonMode mode = ButtonMode::Toggle;
    bool kill = true; // Mutually exclusive within the button group
    unsigned fade_in_ms = 0;
    unsigned fade_out_ms = 0;
};
struct FaderConfig {
    bool grandmaster = true;
    std::vector<Assignment> assignments;
};
inline bool valid_address(Address a) {
    return a.universe >= 1 && a.universe <= 32768 && a.channel >= 1 && a.channel <= 512;
}
inline void validate(const std::vector<Assignment>& entries) {
    std::map<Address, bool> used;
    for (const auto& e : entries) {
        if (!valid_address(e.address) || !used.emplace(e.address, true).second)
            throw std::invalid_argument("Invalid or duplicate universe.channel assignment");
    }
}
}
