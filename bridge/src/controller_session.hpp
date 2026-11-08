// BrassCue - GPL-3.0. Authoritative effective values, physical input and shared event boundary.
#pragma once
#include "controller_profile.hpp"
#include "cue_engine.hpp"
#include "internal_presets.hpp"
#include "serial_protocol.hpp"
#include <optional>
namespace brasscue {
class ControllerSession {
  public:
    ControllerSession() {
        configure(ControlProfile{});
    }
    const ControlProfile &profile() const {
        return profile_;
    }
    const State &state() const {
        return state_;
    }
    const CueEngine &engine() const {
        return engine_;
    }
    std::uint8_t fader_value() const {
        return profile_.fader.grandmaster ? state_.master : patch_fader_;
    }
    std::optional<std::uint8_t> physical_fader() const {
        return physical_fader_;
    }
    void network(const std::string &ip, bool broadcast, const std::string &interface_id = {}) {
        auto p = profile_;
        p.target_ip = ip;
        p.allow_broadcast = broadcast;
        p.network_interface_id = interface_id;
        validate(p);
        profile_ = std::move(p);
    }
    void configure(ControlProfile p) {
        validate(p);
        CueEngine next({p.buttons.begin(), p.buttons.end()});
        next.configure_fader(p.fader);
        auto next_state = state_;
        if (p.fader.grandmaster != profile_.fader.grandmaster)
            next_state.master = p.fader.grandmaster ? patch_fader_ : std::uint8_t(255);
        next.set_fader(p.fader.grandmaster ? next_state.master : patch_fader_);
        next.set_master_override(next_state.master);
        for (std::size_t i = 0; i < 4; ++i)
            next.set_encoder(i, p.encoders[i], state_.rgbw[i]);
        profile_ = std::move(p);
        state_ = next_state;
        engine_ = std::move(next);
        presets_ = InternalPresets{};
    }
    void advance(std::uint64_t now) {
        const auto previous = state_;
        presets_.advance(state_, now);
        if (previous.master != state_.master) {
            engine_.set_master_override(state_.master);
            if (profile_.fader.grandmaster)
                engine_.set_fader(state_.master);
        }
        for (std::size_t i = 0; i < 4; ++i)
            if (previous.rgbw[i] != state_.rgbw[i])
                engine_.set_encoder(i, profile_.encoders[i], state_.rgbw[i]);
    }
    void encoder_delta(std::size_t i, int delta, std::uint64_t now) {
        if (i >= 4 || delta < -255 || delta > 255)
            throw std::invalid_argument("Invalid encoder delta/index");
        advance(now);
        encoder(i, std::uint8_t(std::clamp(int(state_.rgbw[i]) + delta, 0, 255)), now);
    }
    void encoder(std::size_t i, std::uint8_t value, std::uint64_t now = 0) {
        advance(now);
        if (i >= 4)
            throw std::out_of_range("Encoder index");
        presets_.manual(static_cast<ControlTarget>(i + 1), profile_.buttons);
        engine_.set_encoder(i, profile_.encoders[i], value);
        state_.rgbw[i] = value;
    }
    void fader(std::uint8_t value, std::uint64_t now = 0) {
        advance(now);
        patch_fader_ = value;
        if (profile_.fader.grandmaster) {
            presets_.manual(ControlTarget::Grandmaster, profile_.buttons);
            state_.master = value;
            engine_.set_master_override(value);
        }
        engine_.set_fader(value);
    }
    bool button_selected(std::size_t i, std::uint64_t now) const {
        (void)now;
        return profile_.buttons.at(i).action == ButtonActionKind::DmxCue
                   ? engine_.button_selected(i)
                   : presets_.selected(i, state_, profile_.buttons, !dmx_selected());
    }
    const char *button_status(std::size_t i, std::uint64_t now) const {
        return profile_.buttons.at(i).action == ButtonActionKind::DmxCue
                   ? engine_.button_status(i, now)
                   : presets_.status(i, state_, profile_.buttons, !dmx_selected());
    }
    void button(std::size_t i, bool down, std::uint64_t now) {
        advance(now);
        const auto &b = profile_.buttons.at(i);
        if (b.action == ButtonActionKind::DmxCue) {
            const bool previous = engine_.button_selected(i);
            engine_.button(i, down, now);
            if (!previous && engine_.button_selected(i) && b.kill)
                presets_.cancel_all();
        } else {
            const bool previous = presets_.explicitly_selected(i);
            presets_.button(i, down, state_, profile_.buttons, now, !dmx_selected());
            if (!previous && presets_.explicitly_selected(i) && b.kill)
                engine_.deactivate_all(now);
            sync_values();
        }
    }
    void release_all(std::uint64_t now) {
        advance(now);
        presets_.release_all(state_, profile_.buttons, now);
        engine_.release_all(now);
        sync_values();
    }
    bool event(const DeviceEvent &e, bool ignore_gm, std::uint64_t now) {
        advance(now);
        if (e.type == EventType::Button) {
            if (e.index < 0 || e.index >= 3 || e.value < 0 || e.value > 1)
                return false;
            button(e.index, e.value != 0, now);
            return true;
        }
        if (e.type == EventType::Grandmaster) {
            if (e.value < 0 || e.value > 255)
                return false;
            physical_fader_ = std::uint8_t(e.value);
            if (ignore_gm)
                return false;
            fader(std::uint8_t(e.value), now);
            return true;
        }
        if (e.type == EventType::Encoder) {
            if (e.index < 0 || e.index >= 4 || (e.value != 1 && e.value != -1))
                return false;
            const auto value = std::clamp(int(state_.rgbw[e.index]) + e.value, 0, 255);
            encoder(e.index, std::uint8_t(value), now);
            return true;
        }
        return false;
    }

  private:
    bool dmx_selected() const {
        for (std::size_t i = 0; i < 3; ++i)
            if (profile_.buttons[i].action == ButtonActionKind::DmxCue &&
                engine_.button_selected(i))
                return true;
        return false;
    }
    void sync_values() {
        engine_.set_fader(fader_value());
        engine_.set_master_override(state_.master);
        for (std::size_t j = 0; j < 4; ++j)
            engine_.set_encoder(j, profile_.encoders[j], state_.rgbw[j]);
    }
    std::uint8_t patch_fader_ = 255;
    ControlProfile profile_;
    State state_;
    CueEngine engine_;
    InternalPresets presets_;
    std::optional<std::uint8_t> physical_fader_;
};
} // namespace brasscue
