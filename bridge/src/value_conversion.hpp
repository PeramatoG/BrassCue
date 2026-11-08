// BrassCue - GPL-3.0. Neutral DMX/percent conversions.
#pragma once
#include <cstdint>
#include <stdexcept>
namespace brasscue {
inline std::uint8_t from_percent(unsigned pct) {
    if (pct > 100) throw std::out_of_range("Percent must be 0..100");
    return static_cast<std::uint8_t>((pct * 255u + 50u) / 100u);
}
inline unsigned to_percent(std::uint8_t v) { return (unsigned(v) * 100u + 127u) / 255u; }
}
