// BrassCue - GPL-3.0. Front-panel controls; no output transport policy.
#pragma once
#include "../src/controller_profile.hpp"
#include "../src/bridge_core.hpp"
#include <wx/wx.h>
#include <functional>
class EncoderKnob final : public wxPanel {
public:
    EncoderKnob(wxWindow* parent,std::function<void(int)> changed);
    void SetValue(int value) { value_=value; Refresh(); }
private:
    void Change(int delta);
    int value_=0;
    std::function<void(int)> changed_;
};
class ColorPreview final : public wxPanel {
public:
    explicit ColorPreview(wxWindow* parent);
    void SetState(const brasscue::State& state,const brasscue::ControlProfile& profile);
private:
    brasscue::State state_;
    brasscue::ControlProfile profile_;
};
class CueButton final : public wxButton {
public:
    CueButton(wxWindow* parent,std::function<void(bool)> event);
    void Release();
private:
    void Press();
    bool down_=false;
    std::function<void(bool)> event_;
};
