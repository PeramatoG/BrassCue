// BrassCue - GPL-3.0. Patch and cue configuration types.
#pragma once
#include "value_conversion.hpp"
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
namespace brasscue {
struct Address {
    std::uint16_t universe = 1; // User-facing, 1-based Art-Net port address
    std::uint16_t channel = 1;  // DMX 1..512
    bool operator<(const Address &rhs) const {
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
inline void validate(const std::vector<Assignment> &entries) {
    if (entries.size() > 512)
        throw std::invalid_argument("At most 512 assignments per control");
    std::map<Address, bool> used;
    std::size_t row = 0;
    for (const auto &e : entries) {
        ++row;
        if (!valid_address(e.address) || !used.emplace(e.address, true).second)
            throw std::invalid_argument("Row " + std::to_string(row) +
                                        ": invalid or duplicate universe.channel assignment");
    }
}
} // namespace brasscue
