// BrassCue - GPL-3.0. Presentation inputs shared by native and web adapters.
#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
namespace brasscue {
struct ControllerActions {
    std::function<void(std::size_t, int)> encoder_delta;
    std::function<void(std::uint8_t)> fader;
    std::function<void(std::size_t, bool)> button;
    std::function<void(int)> edit;
    std::function<void()> release;
};
} // namespace brasscue
