// BrassCue - GPL-3.0. Native fallback composition; dispatches inputs to shared controllers.
#pragma once
#include "console_fader.hpp"
#include "presentation/controller_actions.hpp"
#include "widgets.hpp"
#include <optional>
class NativePanel final : public wxPanel {
  public:
    NativePanel(wxWindow *, brasscue::ControllerActions);
    void Apply(const brasscue::ViewSnapshot &);
    void ReleaseInputs();
    void CheckPointer();

  private:
    void BindEdit(wxWindow *, int);
    brasscue::ControllerActions actions_;
    std::optional<brasscue::ViewSnapshot> previous_;
    ConsoleFader *fader_ = nullptr;
    wxStaticText *fader_label_ = nullptr;
    wxStaticText *digits_ = nullptr;
    wxStaticText *percent_ = nullptr;
    std::array<EncoderKnob *, 4> knobs_{};
    std::array<wxStaticText *, 4> labels_{};
    std::array<CueButton *, 3> buttons_{};
    ColorPreview *screen_ = nullptr;
};
