// BrassCue - GPL-3.0. Native presentation widgets; Core owns all live values.
#pragma once
#include "presentation/encoder_gesture.hpp"
#include "presentation/view_snapshot.hpp"
#include <functional>
#include <wx/wx.h>
class EncoderKnob final : public wxPanel {
  public:
    EncoderKnob(wxWindow *, std::function<void(int)> delta);
    void SetValue(int value);
    void Release();
    void CheckPointer();

  private:
    std::pair<double, double> Point(const wxMouseEvent &) const;
    int value_ = 0;
    brasscue::EncoderGesture gesture_;
    std::function<void(int)> changed_;
};
class ColorPreview final : public wxPanel {
  public:
    explicit ColorPreview(wxWindow *);
    void SetView(const brasscue::ScreenViewData &);

  private:
    brasscue::ScreenViewData view_;
};
class CueButton final : public wxButton {
  public:
    CueButton(wxWindow *, std::function<void(bool)>);
    void Release(bool release_capture = true);
    void CheckPointer();
    void SetView(const brasscue::ButtonView &);
    bool Selected() const {
        return view_.selected;
    }
    unsigned VisualRevision() const {
        return revision_;
    }

  private:
    void Press();
    bool down_ = false, pointer_down_ = false;
    brasscue::ButtonView view_;
    unsigned revision_ = 0;
    std::function<void(bool)> event_;
};
