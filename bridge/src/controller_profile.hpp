// BrassCue - GPL-3.0. Canonical, portable configuration; no live values.
#pragma once
#include "patch.hpp"
#include <array>
#include <string>
namespace brasscue {
enum class ScreenView { Color, Levels };
struct ControlProfile {
    std::string id = "rgbw-compact";
    std::string name = "RGBW Compact";
    std::string layout_id = "compact-three-column-v1";
    std::array<std::string,4> encoder_labels{{"R","G","B","W"}};
    std::array<std::vector<Assignment>,4> encoders{{
        {{{1,1},255}}, {{{1,2},255}}, {{{1,3},255}}, {{{1,4},255}}
    }};
    std::string fader_label = "GRANDMASTER";
    FaderConfig fader;
    std::array<std::string,3> button_labels{{"ROOM 100%","ROOM 50%","ROOM 0%"}};
    std::array<ButtonConfig,3> buttons;
    std::string screen_title = "RGBW LIVE";
    ScreenView screen_view = ScreenView::Color;
    ValueDisplay units = ValueDisplay::Percent;
    std::string target_ip = "127.0.0.1";
    bool allow_broadcast = false;
    ControlProfile() {
        buttons[0].assignments = {{{1,5},255}};
        buttons[1].assignments = {{{1,5},128}};
        buttons[2].assignments = {{{1,5},0}};
    }
};
inline void validate(const ControlProfile& p) {
    const auto text = [](const std::string& s) {
        if (s.empty() || s.size()>128 || s.find_first_of("\r\n")!=std::string::npos || s.find('\0')!=std::string::npos)
            throw std::invalid_argument("Labels must contain 1..128 bytes without line breaks");
    };
    text(p.id); text(p.name); text(p.fader_label); text(p.screen_title); text(p.target_ip);
    if (p.layout_id!="compact-three-column-v1" ||
        (p.screen_view!=ScreenView::Color && p.screen_view!=ScreenView::Levels) ||
        (p.units!=ValueDisplay::Dmx255 && p.units!=ValueDisplay::Percent))
        throw std::invalid_argument("Unsupported profile layout or display mode");
    for (const auto& label:p.encoder_labels) text(label);
    for (const auto& label:p.button_labels) text(label);
    for (const auto& patch:p.encoders) validate(patch);
    validate(p.fader.assignments);
    for (const auto& b:p.buttons) {
        validate(b.assignments);
        if ((b.mode!=ButtonMode::Toggle && b.mode!=ButtonMode::Flash) || b.fade_in_ms>60000 || b.fade_out_ms>60000)
            throw std::invalid_argument("Invalid button mode or fade (0..60000 ms)");
    }
}
}
