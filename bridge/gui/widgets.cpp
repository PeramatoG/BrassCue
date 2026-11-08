// BrassCue - GPL-3.0. Relative circular input and dirty-only native painting.
#include "widgets.hpp"
#include <algorithm>
#include <cmath>
#include <wx/dcbuffer.h>
#if defined(__WXMSW__) && wxUSE_ACCESSIBILITY
#include <wx/access.h>
namespace {
class CueAccessible final : public wxWindowAccessible {
  public:
    explicit CueAccessible(CueButton *b) : wxWindowAccessible(b), button_(b) {}
    wxAccStatus GetRole(int child, wxAccRole *role) override {
        if (child)
            return wxWindowAccessible::GetRole(child, role);
        *role = wxROLE_SYSTEM_PUSHBUTTON;
        return wxACC_OK;
    }
    wxAccStatus GetState(int child, long *state) override {
        if (child)
            return wxWindowAccessible::GetState(child, state);
        *state = wxACC_STATE_SYSTEM_FOCUSABLE;
        if (button_->HasFocus())
            *state |= wxACC_STATE_SYSTEM_FOCUSED;
        if (button_->Selected())
            *state |= wxACC_STATE_SYSTEM_PRESSED | wxACC_STATE_SYSTEM_SELECTED;
        if (!button_->IsEnabled())
            *state |= wxACC_STATE_SYSTEM_UNAVAILABLE;
        return wxACC_OK;
    }
    wxAccStatus DoDefaultAction(int child) override {
        if (child)
            return wxACC_NOT_SUPPORTED;
        wxCommandEvent pulse(wxEVT_BUTTON, button_->GetId());
        pulse.SetEventObject(button_);
        button_->GetEventHandler()->ProcessEvent(pulse);
        return wxACC_OK;
    }

  private:
    CueButton *button_;
};
} // namespace
#endif
EncoderKnob::EncoderKnob(wxWindow *parent, std::function<void(int)> delta)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(100, 90)), changed_(std::move(delta)) {
    SetMinSize(FromDIP(wxSize(100, 90)));
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetCanFocus(true);
    Bind(wxEVT_PAINT, [this](wxPaintEvent &) {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(wxColour(33, 37, 44)));
        dc.Clear();
        const auto sz = GetClientSize();
        const int x = sz.x / 2, y = sz.y / 2, r = std::max(1, std::min(sz.x, sz.y) / 2 - 10);
        dc.SetPen(wxPen(wxColour(205, 161, 84), 2));
        dc.SetBrush(wxBrush(wxColour(43, 48, 56)));
        dc.DrawCircle(x, y, r);
        const double angle = (-225.0 + 270.0 * value_ / 255.0) * 3.141592653589793 / 180.0;
        dc.DrawLine(x, y, x + int(std::cos(angle) * r), y + int(std::sin(angle) * r));
    });
    Bind(wxEVT_MOUSEWHEEL,
         [this](wxMouseEvent &e) { changed_(e.GetWheelRotation() > 0 ? 1 : -1); });
    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
        SetFocus();
        if (!HasCapture())
            CaptureMouse();
        auto p = Point(e);
        gesture_.begin(p.first, p.second);
    });
    Bind(wxEVT_LEFT_UP, [this](wxMouseEvent &) { Release(); });
    Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent &) { gesture_.end(); });
    Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &e) {
        Release();
        e.Skip();
    });
    Bind(wxEVT_MOTION, [this](wxMouseEvent &e) {
        if (HasCapture() && e.Dragging()) {
            auto p = Point(e);
            auto delta = gesture_.move(p.first, p.second);
            if (delta)
                changed_(delta);
        }
    });
    Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent &e) {
        switch (e.GetKeyCode()) {
        case WXK_UP:
        case WXK_RIGHT:
            changed_(1);
            break;
        case WXK_DOWN:
        case WXK_LEFT:
            changed_(-1);
            break;
        case WXK_PAGEUP:
            changed_(10);
            break;
        case WXK_PAGEDOWN:
            changed_(-10);
            break;
        default:
            e.Skip();
        }
    });
}
std::pair<double, double> EncoderKnob::Point(const wxMouseEvent &e) const {
    auto s = GetClientSize();
    double radius = std::max(1, std::min(s.x, s.y) / 2 - 10);
    return {(e.GetX() - s.x / 2) / radius, (e.GetY() - s.y / 2) / radius};
}
void EncoderKnob::SetValue(int value) {
    if (value_ == value)
        return;
    value_ = value;
    Refresh();
}
void EncoderKnob::Release() {
    gesture_.end();
    if (HasCapture())
        ReleaseMouse();
}
void EncoderKnob::CheckPointer() {
    if (HasCapture() && !wxGetMouseState().LeftIsDown())
        Release();
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
        dc.DrawText(wxString::FromUTF8(view_.title), 8, 6);
        auto size = GetClientSize();
        const int side = std::max(1, std::min(size.x - 16, size.y - 40)), x = (size.x - side) / 2,
                  y = 32;
        dc.SetPen(wxPen(wxColour(205, 161, 84), 1));
        dc.SetBrush(wxBrush(view_.mode == brasscue::ScreenView::Color
                                ? wxColour(view_.color[0], view_.color[1], view_.color[2])
                                : wxColour(22, 24, 29)));
        dc.DrawRectangle(x, y, side, side);
        if (view_.mode == brasscue::ScreenView::Levels)
            for (unsigned i = 0; i < 4; ++i) {
                dc.DrawText(wxString::FromUTF8(view_.labels[i]), x + 6, y + 8 + i * 30);
                dc.SetBrush(wxBrush(wxColour(205, 161, 84)));
                dc.DrawRectangle(x + 44, y + 10 + i * 30,
                                 int(view_.levels[i]) * std::max(1, side - 52) / 100, 16);
            }
    });
}
void ColorPreview::SetView(const brasscue::ScreenViewData &view) {
    if (view_ == view)
        return;
    view_ = view;
    Refresh();
}
CueButton::CueButton(wxWindow *parent, std::function<void(bool)> event)
    : wxControl(parent, wxID_ANY, wxDefaultPosition, wxSize(100, 70),
                wxBORDER_NONE | wxWANTS_CHARS),
      event_(std::move(event)) {
    SetCanFocus(true);
    SetMinSize(FromDIP(wxSize(80, 50)));
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetBackgroundColour(wxColour(43, 48, 56));
    SetForegroundColour(wxColour(238, 232, 219));
    Bind(wxEVT_PAINT, [this](wxPaintEvent &) {
        wxAutoBufferedPaintDC dc(this);
        const bool fade = view_.phase.rfind("FADE", 0) == 0;
        const wxColour fill = view_.selected ? wxColour(78, 60, 32) : wxColour(43, 48, 56);
        dc.SetBackground(wxBrush(fill));
        dc.Clear();
        dc.SetBrush(wxBrush(fill));
        dc.SetPen(wxPen(view_.selected ? wxColour(219, 169, 79)
                        : fade         ? wxColour(149, 116, 63)
                                       : wxColour(85, 83, 76),
                        2));
        const auto size = GetClientSize();
        dc.DrawRoundedRectangle(1, 1, std::max(1, size.x - 2), std::max(1, size.y - 2), 4);
        dc.SetTextForeground(view_.selected ? wxColour(255, 196, 93) : wxColour(238, 232, 219));
        dc.SetFont(GetFont());
        dc.DrawLabel(GetLabel(), GetClientRect(), wxALIGN_CENTER);
        if (HasFocus()) {
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.SetPen(wxPen(wxColour(255, 196, 93), 1, wxPENSTYLE_DOT));
            dc.DrawRectangle(5, 5, std::max(1, size.x - 10), std::max(1, size.y - 10));
        }
    });
    Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent &e) {
        Refresh(false);
        e.Skip();
    });
#if defined(__WXMSW__) && wxUSE_ACCESSIBILITY
    SetAccessible(new CueAccessible(this));
#endif
    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &) {
        SetFocus();
        if (!HasCapture())
            CaptureMouse();
        Hold(1, true);
    });
    Bind(wxEVT_LEFT_UP, [this](wxMouseEvent &) {
        Hold(1, false);
        if (HasCapture())
            ReleaseMouse();
    });
    Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent &) { Release(false); });
    Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &e) {
        Release();
        Refresh(false);
        e.Skip();
    });
    Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent &e) {
        if (e.GetKeyCode() == WXK_SPACE || e.GetKeyCode() == WXK_RETURN)
            Hold(e.GetKeyCode() == WXK_SPACE ? 2 : 4, true);
        else
            e.Skip();
    });
    Bind(wxEVT_KEY_UP, [this](wxKeyEvent &e) {
        if (e.GetKeyCode() == WXK_SPACE || e.GetKeyCode() == WXK_RETURN)
            Hold(e.GetKeyCode() == WXK_SPACE ? 2 : 4, false);
        else
            e.Skip();
    });
    Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        Hold(8, true);
        Hold(8, false);
    });
}
void CueButton::Hold(unsigned source, bool down) {
    const bool previous = holds_ != 0;
    if (down)
        holds_ |= source;
    else
        holds_ &= ~source;
    if (previous != (holds_ != 0))
        event_(holds_ != 0);
}
void CueButton::CheckPointer() {
    if ((holds_ & 1) && !wxGetMouseState().LeftIsDown()) {
        Hold(1, false);
        if (HasCapture())
            ReleaseMouse();
    }
}
void CueButton::Release(bool release_capture) {
    if (holds_) {
        holds_ = 0;
        event_(false);
    }
    if (release_capture && HasCapture())
        ReleaseMouse();
}
void CueButton::SetView(const brasscue::ButtonView &view) {
    if (view_ == view)
        return;
    if (view_.label != view.label) {
        SetLabel(wxString::FromUTF8(view.label));
        SetName(wxString::FromUTF8(view.label));
    }
    if (view_.selected != view.selected || view_.phase != view.phase) {
        SetHelpText(view.phase == "FADE IN"    ? "Transitioning in"
                    : view.phase == "FADE OUT" ? "Transitioning out"
                    : view.selected            ? "Selected"
                                               : "Not selected");
    }
    view_ = view;
    ++revision_;
    Refresh(false);
#if defined(__WXMSW__) && wxUSE_ACCESSIBILITY
    wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_STATECHANGE, this, wxOBJID_CLIENT, 0);
#endif
}
