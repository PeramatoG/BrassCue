#pragma once
// Four-character display policy, independent of the actual display driver.
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>
#include "cue_engine.hpp"
namespace brasscue {
class FourDigitDisplay {
public:
    void set_master(std::uint8_t value) { master_ = value; }
    void encoder_changed(std::size_t index, std::uint8_t value, std::uint64_t now_ms) {
        if (index >= 4) return;
        active_ = index;
        value_ = value;
        last_change_ms_ = now_ms;
        showing_encoder_ = true;
    }
    std::string text(std::uint64_t now_ms) const {
        char buffer[5]{};
        if (showing_encoder_ && now_ms >= last_change_ms_ &&
            now_ms - last_change_ms_ < 2000) {
            constexpr std::array<char,4> labels{{'R','G','B','W'}};
            std::snprintf(buffer,sizeof(buffer),"%c%3u",labels[active_],to_percent(value_));
        } else {
            std::snprintf(buffer,sizeof(buffer),"%4u",to_percent(master_));
        }
        return buffer;
    }
private:
    std::uint8_t master_ = 255, value_ = 0;
    std::size_t active_ = 0;
    std::uint64_t last_change_ms_ = 0;
    bool showing_encoder_ = false;
};
} // namespace brasscue
