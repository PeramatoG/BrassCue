// BrassCue - GPL-3.0. See LICENSE for terms and warranty disclaimer.
#pragma once
// Hardware-independent, multi-universe cue and control engine.
#include "bridge_core.hpp"
#include "patch.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <vector>

namespace brasscue {
class CueEngine {
  public:
    explicit CueEngine(std::vector<ButtonConfig> buttons = std::vector<ButtonConfig>(3))
        : buttons_(std::move(buttons)), tracks_(buttons_.size()) {
        for (const auto &b : buttons_)
            validate(b.assignments);
    }
    void configure_fader(FaderConfig config) {
        validate(config.assignments);
        fader_ = std::move(config);
    }
    void set_fader(std::uint8_t value) {
        fader_value_ = value;
    }
    void set_encoder(std::size_t index, std::vector<Assignment> assignments, std::uint8_t value) {
        if (index >= encoders_.size())
            throw std::out_of_range("Encoder index");
        validate(assignments);
        encoders_[index] = {std::move(assignments), value};
    }
    void configure_button(std::size_t index, ButtonConfig config) {
        if (index >= buttons_.size())
            throw std::out_of_range("Button index");
        validate(config.assignments);
        buttons_[index] = std::move(config);
        tracks_[index] = {};
    }
    void button(std::size_t index, bool down, std::uint64_t now_ms) {
        if (index >= buttons_.size())
            throw std::out_of_range("Button index");
        auto &track = tracks_[index];
        const auto &config = buttons_[index];
        if (track.pressed == down)
            return; // Ignore repeated MIDI press/release
        track.pressed = down;
        if (config.mode == ButtonMode::Toggle) {
            if (!down)
                return;
            const bool turn_on = track.target < 0.5;
            set_target(index, turn_on ? 1.0 : 0.0, now_ms);
            if (turn_on && config.kill)
                kill_others(index, now_ms);
        } else {
            set_target(index, down ? 1.0 : 0.0, now_ms);
            if (down && config.kill)
                kill_others(index, now_ms);
        }
    }
    bool button_selected(std::size_t i) const {
        return tracks_.at(i).target > 0;
    }
    void deactivate(std::size_t i, std::uint64_t now) {
        set_target(i, 0, now);
    }
    void deactivate_all(std::uint64_t now) {
        for (std::size_t i = 0; i < tracks_.size(); ++i)
            deactivate(i, now);
    }
    void release_all(std::uint64_t now) {
        for (std::size_t i = 0; i < buttons_.size(); ++i)
            button(i, false, now);
    }
    const char *button_status(std::size_t i, std::uint64_t now) const {
        const auto &t = tracks_.at(i);
        if (level_at(i, now) != t.target)
            return t.target > t.start ? "FADE IN" : "FADE OUT";
        return t.target > 0 ? "ON" : "OFF";
    }
    std::map<std::uint16_t, std::array<std::uint8_t, 512>> render(std::uint64_t now_ms) const {
        std::map<std::uint16_t, std::array<std::uint8_t, 512>> frames;
        const auto write = [&](Address address, std::uint8_t value) {
            // Highest-takes-precedence merge for overlapping addresses.
            auto &slot = frames[address.universe][address.channel - 1];
            slot = (std::max)(slot, value);
        };
        const unsigned master = fader_.grandmaster ? fader_value_ : 255u;
        for (const auto &encoder : encoders_)
            for (const auto &a : encoder.assignments)
                write(a.address,
                      apply_master(static_cast<std::uint8_t>(
                                       (unsigned(a.value) * encoder.value + 127u) / 255u),
                                   static_cast<std::uint8_t>(master)));
        if (!fader_.grandmaster)
            for (const auto &a : fader_.assignments)
                write(a.address,
                      static_cast<std::uint8_t>((unsigned(a.value) * fader_value_ + 127u) / 255u));
        for (std::size_t i = 0; i < buttons_.size(); ++i) {
            const double level = level_at(i, now_ms);
            for (const auto &a : buttons_[i].assignments)
                write(a.address,
                      apply_master(static_cast<std::uint8_t>(std::lround(double(a.value) * level)),
                                   static_cast<std::uint8_t>(master)));
        }
        return frames;
    }
    std::array<std::uint8_t, kPacketBytes> packet(std::uint16_t universe,
                                                  std::uint64_t now_ms) const {
        if (universe < 1 || universe > 32768)
            throw std::out_of_range("Universe");
        State empty;
        auto bytes = build_artdmx(empty, static_cast<std::uint16_t>(universe - 1));
        const auto frames = render(now_ms);
        auto it = frames.find(universe);
        if (it != frames.end())
            std::copy(it->second.begin(), it->second.end(), bytes.begin() + kArtDmxHeader);
        return bytes;
    }

  private:
    struct Encoder {
        std::vector<Assignment> assignments;
        std::uint8_t value = 0;
    };
    struct Track {
        bool pressed = false;
        double start = 0, target = 0;
        std::uint64_t start_ms = 0;
        unsigned duration_ms = 0;
    };
    double level_at(std::size_t i, std::uint64_t now) const {
        const auto &t = tracks_[i];
        if (!t.duration_ms)
            return t.target;
        if (now <= t.start_ms)
            return t.start;
        const double f = (std::min)(1.0, double(now - t.start_ms) / t.duration_ms);
        return t.start + (t.target - t.start) * f;
    }
    void set_target(std::size_t i, double target, std::uint64_t now) {
        auto &t = tracks_[i];
        t.start = level_at(i, now);
        t.target = target;
        t.start_ms = now;
        t.duration_ms = target > t.start ? buttons_[i].fade_in_ms : buttons_[i].fade_out_ms;
    }
    void kill_others(std::size_t selected, std::uint64_t now) {
        for (std::size_t i = 0; i < tracks_.size(); ++i)
            if (i != selected && tracks_[i].target > 0.0)
                set_target(i, 0.0, now);
    }
    FaderConfig fader_;
    std::uint8_t fader_value_ = 255;
    std::array<Encoder, 4> encoders_{};
    std::vector<ButtonConfig> buttons_;
    std::vector<Track> tracks_;
};
} // namespace brasscue
