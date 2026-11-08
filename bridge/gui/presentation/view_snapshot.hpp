// BrassCue - GPL-3.0. Toolkit-free presentation data and control-level dirty detection.
#pragma once
#include "../../src/controller_session.hpp"
#include "../../src/four_digit_display.hpp"
#include <array>
#include <string>
namespace brasscue {
struct EncoderView {
    std::string label;
    std::uint8_t value = 0;
    unsigned percent = 0;
    int angle = 0;
    bool operator==(const EncoderView &b) const {
        return label == b.label && value == b.value && percent == b.percent && angle == b.angle;
    }
};
struct ButtonView {
    std::string label, phase;
    bool selected = false;
    bool operator==(const ButtonView &b) const {
        return label == b.label && phase == b.phase && selected == b.selected;
    }
};
struct ScreenViewData {
    std::string title;
    ScreenView mode = ScreenView::Color;
    std::array<std::uint8_t, 3> color{};
    std::array<unsigned, 4> levels{};
    std::array<std::string, 4> labels{};
    bool operator==(const ScreenViewData &b) const {
        return title == b.title && mode == b.mode && color == b.color && levels == b.levels &&
               labels == b.labels;
    }
};
struct ViewSnapshot {
    std::array<EncoderView, 4> encoders{};
    std::array<ButtonView, 3> buttons{};
    std::string fader_label, display, connection;
    std::uint8_t fader = 255;
    unsigned fader_percent = 100, master_percent = 100;
    ScreenViewData screen;
};
struct ViewChanges {
    std::array<bool, 4> encoders{};
    std::array<bool, 3> buttons{};
    bool fader = false, display = false, screen = false, connection = false;
    bool any() const {
        for (bool v : encoders)
            if (v)
                return true;
        for (bool v : buttons)
            if (v)
                return true;
        return fader || display || screen || connection;
    }
};
ViewSnapshot make_snapshot(const ControllerSession &, const FourDigitDisplay &, std::uint64_t,
                           const std::string &connection);
ViewChanges view_changes(const ViewSnapshot *previous, const ViewSnapshot &current);
std::string snapshot_json(const ViewSnapshot *previous, const ViewSnapshot &current);
std::string json_string(const std::string &text);
} // namespace brasscue
