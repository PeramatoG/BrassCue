// BrassCue - GPL-3.0. Last-command control presets; selection is not a numeric level.
#pragma once
#include "patch.hpp"
#include <algorithm>
#include <array>
#include <cmath>
namespace brasscue {
class InternalPresets {
  public:
    void advance(State &state, std::uint64_t now) {
        for (unsigned i = 0; i < 5; ++i) {
            auto &f = fades_[i];
            if (!f.running)
                continue;
            double fraction =
                f.duration
                    ? std::min(1.0, double(now >= f.start_ms ? now - f.start_ms : 0) / f.duration)
                    : 1;
            auto value =
                static_cast<std::uint8_t>(std::lround(f.start + (f.target - f.start) * fraction));
            set_control_value(state, static_cast<ControlTarget>(i), value);
            if (fraction == 1)
                f.running = false;
        }
    }
    bool selected(std::size_t i, const State &state, const std::array<ButtonConfig, 3> &configs,
                  bool allow_match = true) const {
        if (selected_.at(i))
            return true;
        const auto &b = configs[i];
        if (!allow_match || b.mode == ButtonMode::Flash || b.controls.empty())
            return false;
        for (const auto &a : b.controls) {
            if (control_value(state, a.target) != a.value || fades_[unsigned(a.target)].running)
                return false;
            for (std::size_t j = 0; j < 3; ++j)
                if (j != i && selected_[j])
                    for (const auto &other : configs[j].controls)
                        if (other.target == a.target)
                            return false;
        }
        return true; // Value match only: never dispatch an event or start a transition.
    }
    const char *status(std::size_t i, const State &state,
                       const std::array<ButtonConfig, 3> &configs, bool allow_match = true) const {
        for (const auto &f : fades_)
            if (f.owner == int(i) && f.running)
                return selected_[i] ? "FADE IN" : "FADE OUT";
        return selected(i, state, configs, allow_match) ? "ON" : "OFF";
    }
    void button(std::size_t i, bool down, State &state, const std::array<ButtonConfig, 3> &configs,
                std::uint64_t now, bool allow_match = true) {
        if (pressed_.at(i) == down)
            return;
        pressed_[i] = down;
        const auto &b = configs[i];
        if (b.mode == ButtonMode::Toggle && !down)
            return;
        const bool turn_on =
            b.mode == ButtonMode::Flash ? down : !selected(i, state, configs, allow_match);
        if (turn_on) {
            if (b.mode == ButtonMode::Flash)
                restore_[i] = state;
            if (b.kill)
                for (std::size_t j = 0; j < 3; ++j)
                    if (j != i)
                        cancel(j);
            selected_[i] = true;
            for (const auto &a : b.controls)
                transition(a.target, a.value, i, b.fade_in_ms, state, now);
        } else {
            selected_[i] = false;
            for (const auto &a : b.controls) {
                const auto owner = fades_[unsigned(a.target)].owner;
                if (owner != int(i) && b.mode == ButtonMode::Flash)
                    continue;
                if (owner >= 0 && owner != int(i))
                    continue; // Never undo a newer command.
                const auto value = b.mode == ButtonMode::Flash
                                       ? control_value(restore_[i], a.target)
                                       : std::uint8_t(0);
                transition(a.target, value, i, b.fade_out_ms, state, now);
            }
        }
        advance(state, now);
    }
    void manual(ControlTarget target, const std::array<ButtonConfig, 3> &configs) {
        // Taking any target cancels that preset's remaining movement without a jump.
        for (std::size_t i = 0; i < 3; ++i)
            for (const auto &a : configs[i].controls)
                if (a.target == target)
                    cancel(i);
        fades_[unsigned(target)].owner = -1;
    }
    void cancel_all() {
        for (std::size_t i = 0; i < 3; ++i)
            cancel(i);
    }
    void release_all(State &state, const std::array<ButtonConfig, 3> &configs, std::uint64_t now) {
        for (std::size_t i = 0; i < 3; ++i)
            if (configs[i].action == ButtonActionKind::SetControlValues)
                button(i, false, state, configs, now);
    }
    bool explicitly_selected(std::size_t i) const {
        return selected_.at(i);
    }

  private:
    struct Fade {
        double start = 0, target = 0;
        std::uint64_t start_ms = 0;
        unsigned duration = 0;
        int owner = -1;
        bool running = false;
    };
    void cancel(std::size_t i) {
        selected_[i] = false;
        for (auto &f : fades_)
            if (f.owner == int(i)) {
                f.running = false;
                f.owner = -1;
            }
    }
    void transition(ControlTarget target, std::uint8_t value, std::size_t owner, unsigned duration,
                    const State &state, std::uint64_t now) {
        fades_[unsigned(target)] = {
            double(control_value(state, target)), double(value), now, duration, int(owner), true};
    }
    std::array<Fade, 5> fades_{};
    std::array<bool, 3> pressed_{}, selected_{};
    std::array<State, 3> restore_{};
};
} // namespace brasscue
