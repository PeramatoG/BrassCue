#pragma once

// Reusable controller profile and patch model.
// No wxWidgets dependencies: hardware, headless Bridge and UI can share it.
#include "bridge_core.hpp"
#include <array>
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace brasscue {

enum class FaderRole : unsigned char { Grandmaster = 0, DmxChannel = 1 };
enum class ScreenView : unsigned char { Color = 0, Levels = 1 };
enum class ButtonAction : unsigned char { Blackout = 0, Full = 1, RecallPreset = 2,
                                           StorePreset = 3, Unassigned = 4 };

struct ControlProfile {
    std::string name = "RGBW Compact";
    std::string layout_id = "compact-three-column-v1";
    std::array<std::string, 4> encoder_labels{{"RED", "GREEN", "BLUE", "WHITE"}};
    std::array<std::uint16_t, 4> dmx_channels{{1, 2, 3, 4}};
    std::string fader_label = "GRANDMASTER";
    FaderRole fader_role = FaderRole::Grandmaster;
    std::uint16_t fader_dmx_channel = 5;
    std::string screen_title = "RGBW LIVE";
    ScreenView screen_view = ScreenView::Color;
    bool hex_display = true;
    std::array<std::string, 3> button_labels{{"BLACKOUT", "FULL", "PRESET"}};
    std::array<ButtonAction, 3> button_actions{{
        ButtonAction::Blackout, ButtonAction::Full, ButtonAction::RecallPreset
    }};
};

struct ControllerValues {
    std::array<std::uint8_t, 4> encoders{{0, 0, 0, 0}};
    std::uint8_t fader = 255;
    bool blackout = false;
};

inline bool valid_profile(const ControlProfile& p) {
    if (p.layout_id != "compact-three-column-v1") return false;
    if (p.fader_role != FaderRole::Grandmaster && p.fader_role != FaderRole::DmxChannel)
        return false;
    for (std::size_t i = 0; i < p.dmx_channels.size(); ++i) {
        if (p.dmx_channels[i] < 1 || p.dmx_channels[i] > 512) return false;
        for (std::size_t j = 0; j < i; ++j)
            if (p.dmx_channels[i] == p.dmx_channels[j]) return false;
    }
    if (p.fader_role == FaderRole::DmxChannel) {
        if (p.fader_dmx_channel < 1 || p.fader_dmx_channel > 512) return false;
        for (auto ch : p.dmx_channels)
            if (ch == p.fader_dmx_channel) return false;
    }
    return true;
}

inline State compute_output_state(const ControllerValues& v, const ControlProfile& p) {
    State result;
    result.rgbw = v.encoders;
    result.master = v.blackout ? 0 : (p.fader_role == FaderRole::Grandmaster ? v.fader : 255);
    return result;
}

// The ArtDmx header is owned by bridge_core.hpp. This adapter maps the
// controller's logical controls to profile-selected 1-based DMX slots.
inline std::array<std::uint8_t, kPacketBytes> build_profiled_artdmx(
    const ControllerValues& v, const ControlProfile& p, std::uint16_t port_address) {
    if (!valid_profile(p)) throw std::invalid_argument("Invalid BrassCue control profile");
    const State output = compute_output_state(v, p);
    auto packet = build_artdmx(output, port_address);
    std::fill(packet.begin() + kArtDmxHeader, packet.end(), 0);
    for (std::size_t i = 0; i < v.encoders.size(); ++i)
        packet[kArtDmxHeader + p.dmx_channels[i] - 1] =
            apply_master(v.encoders[i], output.master);
    if (p.fader_role == FaderRole::DmxChannel)
        packet[kArtDmxHeader + p.fader_dmx_channel - 1] = v.blackout ? 0 : v.fader;
    return packet;
}

inline bool apply_button(ControllerValues& values, ButtonAction action,
                         const ControllerValues* preset = nullptr) {
    switch (action) {
    case ButtonAction::Blackout:
        values.blackout = true; return true;
    case ButtonAction::Full:
        values.fader = 255; values.blackout = false; return true;
    case ButtonAction::RecallPreset:
        if (!preset) return false;
        values = *preset; return true;
    case ButtonAction::StorePreset:
    case ButtonAction::Unassigned:
        return false;
    }
    return false;
}

} // namespace brasscue
