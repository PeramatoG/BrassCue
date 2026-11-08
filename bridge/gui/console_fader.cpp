// BrassCue - GPL-3.0. No image assets; geometry and input use the same DPI-aware travel.
#include "console_fader.hpp"
#include <algorithm>
#include <cmath>
#include <wx/dcbuffer.h>
#if defined(__WXMSW__) && wxUSE_ACCESSIBILITY
#include <wx/access.h>
namespace {
class FaderAccessible final : public wxWindowAccessible {
  public:
    explicit FaderAccessible(ConsoleFader *f) : wxWindowAccessible(f), fader_(f) {}
    wxAccStatus GetRole(int child, wxAccRole *role) override {
        if (child)
            return wxWindowAccessible::GetRole(child, role);
        *role = wxROLE_SYSTEM_SLIDER;
        return wxACC_OK;
    }
    wxAccStatus GetValue(int child, wxString *value) override {
        if (child)
            return wxACC_NOT_SUPPORTED;
        *value = wxString::Format("%d / 255", fader_->GetValue());
        return wxACC_OK;
    }
    wxAccStatus GetState(int child, long *state) override {
        if (child)
            return wxWindowAccessible::GetState(child, state);
        *state = wxACC_STATE_SYSTEM_FOCUSABLE;
        if (fader_->HasFocus())
            *state |= wxACC_STATE_SYSTEM_FOCUSED;
        if (!fader_->IsEnabled())
            *state |= wxACC_STATE_SYSTEM_UNAVAILABLE;
        return wxACC_OK;
    }

  private:
    ConsoleFader *fader_;
};
} // namespace
#endif
ConsoleFader::ConsoleFader(wxWindow *parent, wxWindowID id, std::function<void(int)> changed)
    : wxControl(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxWANTS_CHARS),
      changed_(std::move(changed)) {
    SetInitialSize(FromDIP(wxSize(80, 360)));
    SetMinSize(FromDIP(wxSize(80, 160)));
    SetCanFocus(true);
    SetName("Grandmaster");
    SetHelpText("Vertical fader, 0 to 255. Arrows: one step; Page Up/Down: ten; Home/End: limits.");
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetCursor(wxCursor(wxCURSOR_SIZENS));
#if defined(__WXMSW__) && wxUSE_ACCESSIBILITY
    SetAccessible(new FaderAccessible(this));
#endif
    Bind(wxEVT_PAINT, &ConsoleFader::Paint, this);
    Bind(wxEVT_SIZE, [this](wxSizeEvent &e) {
        Refresh();
        e.Skip();
    });
    Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent &e) {
        Refresh();
        e.Skip();
    });
    Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &e) {
        Release();
        Refresh();
        e.Skip();
    });
    Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent &e) {
        SetFocus();
        // Preserve byte precision when the rail has fewer than 255 pixels of travel.
        const double center =
            Margin() + (GetClientSize().y - 2 * Margin()) * (255 - value_) / 255.0;
        grab_offset_ = Cap().Contains(e.GetPosition()) ? e.GetY() - center : 0;
        if (!HasCapture())
            CaptureMouse();
        MoveTo(e.GetY());
    });
    Bind(wxEVT_MOTION, [this](wxMouseEvent &e) {
        if (HasCapture() && e.LeftIsDown())
            MoveTo(e.GetY());
    });
    Bind(wxEVT_LEFT_UP, [this](wxMouseEvent &e) {
        if (HasCapture())
            MoveTo(e.GetY());
        Release();
    });
    Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent &) { grab_offset_ = 0; });
    Bind(wxEVT_MOUSEWHEEL, [this](wxMouseEvent &e) {
        if (e.GetWheelRotation())
            Change(value_ + (e.GetWheelRotation() > 0 ? 1 : -1));
    });
    Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent &e) {
        switch (e.GetKeyCode()) {
        case WXK_TAB:
            Navigate(e.ShiftDown() ? wxNavigationKeyEvent::IsBackward
                                   : wxNavigationKeyEvent::IsForward);
            break;
        case WXK_UP:
        case WXK_RIGHT:
            Change(value_ + 1);
            break;
        case WXK_DOWN:
        case WXK_LEFT:
            Change(value_ - 1);
            break;
        case WXK_PAGEUP:
            Change(value_ + 10);
            break;
        case WXK_PAGEDOWN:
            Change(value_ - 10);
            break;
        case WXK_HOME:
            Change(255);
            break;
        case WXK_END:
            Change(0);
            break;
        default:
            e.Skip();
        }
    });
}
int ConsoleFader::Margin() const {
    return std::min(GetClientSize().y / 2, FromDIP(22));
}
wxRect ConsoleFader::Cap() const {
    const auto s = GetClientSize();
    const int width = std::max(1, std::min(FromDIP(48), s.x - FromDIP(12)));
    const int height = std::max(1, std::min(FromDIP(28), s.y));
    const int y = Margin() + int(std::lround((s.y - 2 * Margin()) * (255 - value_) / 255.0));
    return wxRect((s.x - width) / 2, y - height / 2, width, height);
}
void ConsoleFader::SetValue(int value) {
    value = std::clamp(value, 0, 255);
    if (value_ == value)
        return;
    value_ = value;
    Refresh();
#if defined(__WXMSW__) && wxUSE_ACCESSIBILITY
    wxAccessible::NotifyEvent(wxACC_EVENT_OBJECT_VALUECHANGE, this, wxOBJID_CLIENT, wxACC_SELF);
#endif
}
void ConsoleFader::Change(int value) {
    const int previous = value_;
    SetValue(value);
    if (value_ != previous)
        changed_(value_);
}
void ConsoleFader::MoveTo(int y) {
    const int travel = GetClientSize().y - 2 * Margin();
    if (travel > 0)
        Change(int(std::lround(255.0 * (1.0 - double(y - grab_offset_ - Margin()) / travel))));
}
void ConsoleFader::Release() {
    if (HasCapture())
        ReleaseMouse();
    grab_offset_ = 0;
}
void ConsoleFader::CheckPointer() {
    if (HasCapture() && !wxGetMouseState().LeftIsDown())
        Release();
}
void ConsoleFader::Paint(wxPaintEvent &) {
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();
    const auto s = GetClientSize();
    const int x = s.x / 2, top = Margin(), bottom = s.y - top;
    const int slot = FromDIP(8);
    dc.SetPen(wxPen(wxColour(74, 66, 49), FromDIP(1)));
    dc.SetBrush(wxBrush(wxColour(10, 11, 14)));
    dc.DrawRoundedRectangle(x - slot / 2, top, slot, std::max(1, bottom - top), FromDIP(3));
    dc.SetPen(wxPen(wxColour(105, 94, 71), FromDIP(1)));
    for (int i = 0; i <= 10; ++i) {
        const int y = top + (bottom - top) * i / 10;
        for (int side : {-1, 1})
            dc.DrawLine(x + side * FromDIP(15), y, x + side * FromDIP(i % 5 ? 25 : 31), y);
    }
    auto cap = Cap();
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(wxColour(8, 9, 11)));
    dc.DrawRectangle(cap.x + FromDIP(2), cap.y + FromDIP(3), cap.width, cap.height);
    dc.GradientFillLinear(cap, wxColour(153, 157, 159), wxColour(62, 66, 70), wxSOUTH);
    dc.SetPen(wxPen(wxColour(181, 177, 162), FromDIP(1)));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(cap);
    const int y = cap.y + cap.height / 2;
    dc.SetPen(wxPen(wxColour(28, 30, 33), FromDIP(3)));
    dc.DrawLine(cap.x + FromDIP(5), y, cap.GetRight() - FromDIP(4), y);
    dc.SetPen(wxPen(wxColour(205, 161, 84), FromDIP(1)));
    dc.DrawLine(cap.x + FromDIP(5), y + FromDIP(1), cap.GetRight() - FromDIP(4), y + FromDIP(1));
    if (HasFocus()) {
        dc.SetPen(wxPen(wxColour(255, 196, 93), FromDIP(1), wxPENSTYLE_DOT));
        dc.DrawRectangle(GetClientRect().Deflate(FromDIP(2)));
    }
}
