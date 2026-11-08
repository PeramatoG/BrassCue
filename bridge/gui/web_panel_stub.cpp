// BrassCue - GPL-3.0. No browser component or runtime is required by a native build.
#include "web_panel_api.hpp"
WebPanelInterface *CreateWebPanel(wxWindow *, brasscue::ControllerActions, std::function<void()>,
                                  std::function<void(const std::string &)>,
                                  std::string &diagnostic) {
    diagnostic.clear();
    return nullptr;
}
