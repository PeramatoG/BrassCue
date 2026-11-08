// BrassCue - GPL-3.0. Front-panel controls; no output transport policy.
#pragma once
#include "../src/bridge_core.hpp"
#include "../src/controller_profile.hpp"
#include <functional>
#include <wx/wx.h>
class EncoderKnob final : public wxPanel {
  public:
    EncoderKnob(wxWindow *parent, std::function<void(int)> changed);
    void SetValue(int value) {
        if(value_==value)return;
        value_ = value;
        Refresh();
    }

  private:
    void Change(int delta);
    int value_ = 0;
    std::function<void(int)> changed_;
};
class ColorPreview final : public wxPanel {
  public:
    explicit ColorPreview(wxWindow *parent);
    void SetState(const brasscue::State &state, const brasscue::ControlProfile &profile);

  private:
    brasscue::State state_;
    brasscue::ControlProfile profile_;
};
class CueButton final : public wxButton {
  public:
    CueButton(wxWindow *parent, std::function<void(bool)> event);
    void Release(bool release_capture = true);
    void CheckPointer();

  private:
    void Press();
    bool down_ = false;
    bool pointer_down_ = false;
    std::function<void(bool)> event_;
};
