// BrassCue - GPL-3.0. Pure assignment-row validation and lossless unit presentation.
#pragma once
#include "patch.hpp"
#include <charconv>
#include <string>
namespace brasscue {
inline unsigned assignment_number(const std::string &text, unsigned maximum) {
    unsigned value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || value > maximum)
        throw std::invalid_argument("Invalid assignment number: " + text);
    return value;
}
inline Assignment edit_assignment_row(Assignment original, const std::string &universe,
                                      const std::string &channel, const std::string &target,
                                      ValueDisplay units, bool target_edited) {
    const unsigned u = assignment_number(universe, 32768), c = assignment_number(channel, 512);
    const unsigned v = assignment_number(target, units == ValueDisplay::Percent ? 100 : 255);
    if (u < 1 || c < 1)
        throw std::invalid_argument("Universe and channel are 1-based");
    original.address = {static_cast<std::uint16_t>(u), static_cast<std::uint16_t>(c)};
    if (target_edited)
        original.value =
            units == ValueDisplay::Percent ? from_percent(v) : static_cast<std::uint8_t>(v);
    return original;
}
} // namespace brasscue
