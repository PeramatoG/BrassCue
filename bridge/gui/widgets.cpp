// BrassCue - GPL-3.0. Accessible rotary input, cue edges and simulated TFT.
#include "widgets.hpp"
#include <algorithm>
#include <cmath>
#include <wx/dcbuffer.h>
EncoderKnob::EncoderKnob(wxWindow *parent, std::function<void(int)> changed)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(100, 90)), changed_(std::move(changed)) {
    SetMinSize(FromDIP(wxSize(100, 90)));
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetCanFocus(true);
    SetName("Encoder: arrows adjust one DMX step; Page Up/Down adjust ten");
    Bind(wxEVT_PAINT, [this](wxPaintEvent &) {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(wxColour(33, 37, 44)));
        dc.Clear();
        const auto sz = GetClientSize();
        const int x = sz.x / 2, y = sz.y / 2, r = std::min(sz.x, sz.y) / 2 - 10;
        dc.SetPen(wxPen(wxColour(205, 161, 84), 2));
        dc.SetBrush(wxBrush(wxColour(43, 48, 56)));
        dc.DrawCircle(x, y, r);
        const double angle = (-225.0 + 270.0 * value_ / 255.0) * 3.141592653589793 / 180.0;
        dc.DrawLine(x, y, x + int(std::cos(angle) * r), y + int(std::sin(angle) * r));
    });
    Bind(wxEVT_MOUSEWHEEL, [this](wxMouseEvent &e) { Change(e.GetWheelRotation() > 0 ? 1 : -1); });
    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &) {
        SetFocus();
        CaptureMouse();
    });
    Bind(wxEVT_LEFT_UP, [this](wxMouseEvent &) {
        if (HasCapture())
            ReleaseMouse();
    });
    Bind(wxEVT_MOTION, [this](wxMouseEvent &e) {
        if (HasCapture() && e.Dragging()) {
            const int v = std::clamp(255 - e.GetY() * 255 / std::max(1, GetClientSize().y), 0, 255);
            Change(v - value_);
        }
    });
    Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent &e) {
        if (e.GetKeyCode() == WXK_UP || e.GetKeyCode() == WXK_RIGHT)
            Change(1);
        else if (e.GetKeyCode() == WXK_DOWN || e.GetKeyCode() == WXK_LEFT)
            Change(-1);
        else if (e.GetKeyCode() == WXK_PAGEUP)
            Change(10);
        else if (e.GetKeyCode() == WXK_PAGEDOWN)
            Change(-10);
        else
            e.Skip();
    });
}
void EncoderKnob::Change(int delta) {
    value_ = std::clamp(value_ + delta, 0, 255);
    Refresh();
    changed_(value_);
}
ColorPreview::ColorPreview(wxWindow *parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(180, 180)) {
    SetMinSize(FromDIP(wxSize(180, 180)));
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT, [this](wxPaintEvent &) {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(wxColour(22, 24, 29)));
        dc.Clear();
        dc.SetTextForeground(wxColour(238, 232, 219));
        dc.DrawText(wxString::FromUTF8(profile_.screen_title), 8, 6);
        const auto size = GetClientSize();
        const int side = std::max(1, std::min(size.x - 16, size.y - 40));
        const int x = (size.x - side) / 2, y = 32;
        dc.SetPen(wxPen(wxColour(205, 161, 84), 1));
        dc.SetBrush(wxBrush(wxColour(22, 24, 29)));
        dc.DrawRectangle(x, y, side, side);
        const int master = profile_.fader.grandmaster ? state_.master : 255;
        auto scaled = [&](int i) { return int(brasscue::apply_master(state_.rgbw[i], master)); };
        if (profile_.screen_view == brasscue::ScreenView::Color) {
            dc.SetBrush(wxBrush(wxColour(std::min(255, scaled(0) + scaled(3)),
                                         std::min(255, scaled(1) + scaled(3)),
                                         std::min(255, scaled(2) + scaled(3)))));
            dc.DrawRectangle(x, y, side, side);
        } else {
            for (int i = 0; i < 4; ++i) {
                dc.DrawText(wxString::FromUTF8(profile_.encoder_labels[i]), x + 6, y + 8 + i * 30);
                dc.SetBrush(wxBrush(wxColour(205, 161, 84)));
                dc.DrawRectangle(x + 44, y + 10 + i * 30, scaled(i) * std::max(1, side - 52) / 255,
                                 16);
            }
        }
    });
}
void ColorPreview::SetState(const brasscue::State &s, const brasscue::ControlProfile &p) {
    state_ = s;
    profile_ = p;
    Refresh();
}
CueButton::CueButton(wxWindow *parent, std::function<void(bool)> event)
    : wxButton(parent, wxID_ANY, ""), event_(std::move(event)) {
    SetBackgroundColour(wxColour(43, 48, 56));
    SetForegroundColour(wxColour(238, 232, 219));
    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &) {
        SetFocus();
        if (!HasCapture())
            CaptureMouse();
        pointer_down_ = true;
        Press();
    });
    Bind(wxEVT_LEFT_UP, [this](wxMouseEvent &) {
        Release();
        if (HasCapture())
            ReleaseMouse();
    });
    Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent &) { Release(false); });
    Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &e) {
        Release();
        e.Skip();
    });
    Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent &e) {
        if (e.GetKeyCode() == WXK_SPACE || e.GetKeyCode() == WXK_RETURN)
            Press();
        else
            e.Skip();
    });
    Bind(wxEVT_KEY_UP, [this](wxKeyEvent &e) {
        if (e.GetKeyCode() == WXK_SPACE || e.GetKeyCode() == WXK_RETURN)
            Release();
        else
            e.Skip();
    });
    // Accessibility-generated command events are complete pulses. Mouse/key paths
    // consume native events above, so they cannot produce a second command.
    Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        Press();
        Release();
    });
}
void CueButton::Press() {
    if (!down_) {
        down_ = true;
        event_(true);
    }
}
void CueButton::CheckPointer() {
    // Some native backends consume mouse-up while a pointer grab is active.
    // Poll only mouse-origin holds; keyboard Flash must survive an idle mouse.
    if (pointer_down_ && !wxGetMouseState().LeftIsDown())
        Release();
}
void CueButton::Release(bool release_capture) {
    pointer_down_ = false;
    if (down_) {
        down_ = false;
        event_(false);
    }
    if (release_capture && HasCapture())
        ReleaseMouse();
}
