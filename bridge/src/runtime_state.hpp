// BrassCue - GPL-3.0. Stable recovery state, independent of wall-clock time and input holds.
#pragma once
#include "bridge_core.hpp"
#include "controller_profile.hpp"
#include <algorithm>
#include <cmath>
namespace brasscue {
struct InternalRuntime {
    std::array<bool, 3> selected{};
    std::array<int, 5> owners{{-1, -1, -1, -1, -1}};
    std::array<bool, 5> held{};
};
struct DmxRuntime {
    std::array<double, 3> levels{};
    std::array<bool, 3> selected{};
};
struct ControllerRuntime {
    State values;
    std::uint8_t patch_fader = 255;
    InternalRuntime internal;
    DmxRuntime dmx;
};
inline void validate_runtime(const ControlProfile &profile, const ControllerRuntime &r) {
    for (unsigned target = 0; target < 5; ++target) {
        const int owner = r.internal.owners[target];
        if (owner < -1 || owner > 2 || (owner < 0 && r.internal.held[target]))
            throw std::invalid_argument("Invalid recovered control owner");
        if (owner < 0)
            continue;
        const auto &b = profile.buttons[owner];
        if (b.action != ButtonActionKind::SetControlValues || b.mode == ButtonMode::Flash ||
            std::none_of(b.controls.begin(), b.controls.end(),
                         [target](const auto &a) { return unsigned(a.target) == target; }))
            throw std::invalid_argument("Recovered owner does not configure this control");
    }
    for (unsigned i = 0; i < 3; ++i) {
        const auto &b = profile.buttons[i];
        if (!std::isfinite(r.dmx.levels[i]) || r.dmx.levels[i] < 0 || r.dmx.levels[i] > 1)
            throw std::invalid_argument("Invalid recovered DMX cue level");
        if ((b.action != ButtonActionKind::DmxCue || b.mode == ButtonMode::Flash) &&
            (r.dmx.levels[i] != 0 || r.dmx.selected[i]))
            throw std::invalid_argument("Only Toggle DMX contributions may be recovered");
        if (r.internal.selected[i] &&
            (b.action != ButtonActionKind::SetControlValues || b.mode == ButtonMode::Flash ||
             std::find(r.internal.owners.begin(), r.internal.owners.end(), int(i)) ==
                 r.internal.owners.end()))
            throw std::invalid_argument("Invalid recovered internal selection");
    }
}
} // namespace brasscue
