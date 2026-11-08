// BrassCue - GPL-3.0. Canonical, portable configuration; no live values.
#pragma once
#include "network_config.hpp"
#include "patch.hpp"
#include "utf8.hpp"
#include <array>
#include <string>
namespace brasscue {
enum class ScreenView { Color, Levels };
struct ControlProfile {
    std::string id = "rgbw-compact";
    std::string name = "RGBW Compact";
    std::string layout_id = "compact-three-column-v1";
    std::array<std::string, 4> encoder_labels{{"RED", "GREEN", "BLUE", "WHITE"}};
    std::array<std::vector<Assignment>, 4> encoders{
        {{{{1, 1}, 255}}, {{{1, 2}, 255}}, {{{1, 3}, 255}}, {{{1, 4}, 255}}}};
    std::string fader_label = "GRANDMASTER";
    FaderConfig fader;
    std::array<std::string, 3> button_labels{{"HOUSE 100%", "HOUSE 50%", "HOUSE 0%"}};
    std::array<ButtonConfig, 3> buttons;
    std::string screen_title = "RGBW LIVE";
    ScreenView screen_view = ScreenView::Color;
    ValueDisplay units = ValueDisplay::Percent;
    std::string target_ip = "127.0.0.1";
    bool allow_broadcast = false;
    std::string network_interface_id;
    ControlProfile() {
        const std::array<std::uint8_t, 3> levels{{255, 128, 0}};
        for (std::size_t i = 0; i < 3; ++i) {
            buttons[i].action = ButtonActionKind::SetControlValues;
            buttons[i].controls = {{ControlTarget::Grandmaster, levels[i]}};
        }
    }
};
inline void validate(const ControlProfile &p) {
    const auto text = [](const std::string &s) {
        if (!valid_utf8(s) || s.empty() || s.size() > 128 ||
            s.find_first_of("\r\n") != std::string::npos || s.find('\0') != std::string::npos)
            throw std::invalid_argument("Labels must contain 1..128 bytes without line breaks");
    };
    text(p.id);
    text(p.name);
    text(p.fader_label);
    text(p.screen_title);
    text(p.target_ip);
    if (!p.network_interface_id.empty())
        text(p.network_interface_id);
    validate_target(p.target_ip, p.allow_broadcast);
    if (p.layout_id != "compact-three-column-v1" ||
        (p.screen_view != ScreenView::Color && p.screen_view != ScreenView::Levels) ||
        (p.units != ValueDisplay::Dmx255 && p.units != ValueDisplay::Percent))
        throw std::invalid_argument("Unsupported profile layout or display mode");
    for (const auto &label : p.encoder_labels)
        text(label);
    for (const auto &label : p.button_labels)
        text(label);
    for (const auto &patch : p.encoders)
        validate(patch);
    validate(p.fader.assignments);
    for (const auto &b : p.buttons) {
        validate(b.assignments);
        validate_controls(b.controls);
        if (b.action != ButtonActionKind::DmxCue && b.action != ButtonActionKind::SetControlValues)
            throw std::invalid_argument("Invalid button action kind");
        if (b.action == ButtonActionKind::DmxCue && !b.controls.empty())
            throw std::invalid_argument("DMX action cannot contain internal targets");
        if (b.action == ButtonActionKind::SetControlValues &&
            (!b.assignments.empty() || b.controls.empty()))
            throw std::invalid_argument(
                "Internal action requires control targets and no DMX assignments");
        if ((b.mode != ButtonMode::Toggle && b.mode != ButtonMode::Flash) || b.fade_in_ms > 60000 ||
            b.fade_out_ms > 60000)
            throw std::invalid_argument("Invalid button mode or fade (0..60000 ms)");
    }
}
} // namespace brasscue
