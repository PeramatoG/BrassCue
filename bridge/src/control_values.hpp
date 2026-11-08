// BrassCue - GPL-3.0. Logical control targets, independent of physical input positions.
#pragma once
#include "bridge_core.hpp"
#include <stdexcept>
#include <vector>
namespace brasscue {
enum class ButtonActionKind { DmxCue, SetControlValues };
enum class ControlTarget { Grandmaster, Red, Green, Blue, White };
struct ControlValueAssignment {
    ControlTarget target = ControlTarget::Grandmaster;
    std::uint8_t value = 255;
};
inline std::uint8_t control_value(const State &s, ControlTarget target) {
    const auto i = static_cast<unsigned>(target);
    if (i > 4)
        throw std::invalid_argument("Invalid logical control target");
    return i == 0 ? s.master : s.rgbw[i - 1];
}
inline void set_control_value(State &s, ControlTarget target, std::uint8_t value) {
    const auto i = static_cast<unsigned>(target);
    if (i > 4)
        throw std::invalid_argument("Invalid logical control target");
    if (i == 0)
        s.master = value;
    else
        s.rgbw[i - 1] = value;
}
inline void validate_controls(const std::vector<ControlValueAssignment> &values) {
    if (values.size() > 5)
        throw std::invalid_argument("At most five internal control targets");
    unsigned used = 0;
    for (const auto &a : values) {
        auto i = static_cast<unsigned>(a.target);
        if (i > 4 || (used & (1u << i)))
            throw std::invalid_argument("Invalid or duplicate internal control target");
        used |= 1u << i;
    }
}
} // namespace brasscue
