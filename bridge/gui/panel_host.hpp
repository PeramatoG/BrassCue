// BrassCue - GPL-3.0. Renderer lifetime and safe handoff; both share the same actions and snapshot.
#pragma once
#include "native_panel.hpp"
#include "web_panel_api.hpp"
class PanelHost final : public wxPanel {
  public:
    PanelHost(wxWindow *, brasscue::ControllerActions, std::function<void()> mode_changed,
              bool allow_web = true);
    void Apply(const brasscue::ViewSnapshot &);
    void ReleaseInputs();
    void CheckPointer();
    void PreferNative(bool);
    std::string Mode() const;

  private:
    void TryWeb();
    void WebReady();
    void Fallback(const std::string &);
    NativePanel *native_;
    WebPanelInterface *web_ = nullptr;
    wxBoxSizer *sizer_;
    brasscue::ControllerActions actions_;
    std::function<void()> changed_;
    std::optional<brasscue::ViewSnapshot> latest_;
    bool native_preferred_ = false, web_ready_ = false, web_active_ = false;
    std::string diagnostic_;
};
