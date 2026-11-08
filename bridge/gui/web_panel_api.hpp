// BrassCue - GPL-3.0. Optional renderer boundary: native-only builds never include wxWebView.
#pragma once
#include "presentation/controller_actions.hpp"
#include "presentation/view_snapshot.hpp"
#include <wx/wx.h>
class WebPanelInterface {
  public:
    virtual ~WebPanelInterface() = default;
    virtual wxWindow &Window() = 0;
    virtual void Apply(const brasscue::ViewSnapshot &) = 0;
    virtual void ReleaseInputs() = 0;
    virtual void CheckPointer() = 0;
};
WebPanelInterface *CreateWebPanel(wxWindow *, brasscue::ControllerActions,
                                  std::function<void()> ready,
                                  std::function<void(const std::string &)> failed,
                                  std::string &diagnostic, bool *backend_unavailable = nullptr);
