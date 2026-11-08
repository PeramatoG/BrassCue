// BrassCue - GPL-3.0. Renderer failure never takes down the controller or enables a socket.
#include "panel_host.hpp"
PanelHost::PanelHost(wxWindow *parent, brasscue::ControllerActions actions,
                     std::function<void()> changed, bool allow_web)
    : wxPanel(parent), actions_(std::move(actions)), changed_(std::move(changed)),
      native_preferred_(!allow_web) {
    sizer_ = new wxBoxSizer(wxVERTICAL);
    native_ = new NativePanel(this, actions_);
    sizer_->Add(native_, 1, wxEXPAND);
    SetSizer(sizer_);
    SetBackgroundColour(wxColour(22, 24, 29));
    if (allow_web)
        CallAfter([this] { TryWeb(); });
}
void PanelHost::TryWeb() {
    if (native_preferred_)
        return;
    if (web_) {
        if (web_ready_)
            WebReady();
        return;
    }
    web_ = CreateWebPanel(
        this, actions_, [this] { WebReady(); },
        [this](const std::string &reason) { Fallback(reason); }, diagnostic_);
    if (web_) {
        sizer_->Add(&web_->Window(), 1, wxEXPAND);
        web_->Window().Hide();
    } else if (!diagnostic_.empty())
        Fallback(diagnostic_);
}
void PanelHost::Apply(const brasscue::ViewSnapshot &snapshot) {
    latest_ = snapshot;
    if (web_)
        web_->Apply(snapshot);
    if (!web_active_)
        native_->Apply(snapshot);
}
void PanelHost::WebReady() {
    web_ready_ = true;
    if (!web_ || native_preferred_)
        return;
    if (!web_active_) {
        native_->ReleaseInputs();
        native_->Hide();
        web_->Window().Show();
        web_active_ = true;
        diagnostic_.clear();
        Layout();
        if (changed_)
            changed_();
    }
    if (latest_)
        web_->Apply(*latest_);
}
void PanelHost::Fallback(const std::string &reason) {
    diagnostic_ = reason;
    web_ready_ = false;
    if (web_) {
        web_->ReleaseInputs();
        web_->Window().Hide();
    }
    web_active_ = false;
    native_->Show();
    if (latest_)
        native_->Apply(*latest_);
    Layout();
    if (changed_)
        changed_();
}
void PanelHost::PreferNative(bool preferred) {
    native_preferred_ = preferred;
    if (preferred) {
        ReleaseInputs();
        if (web_)
            web_->Window().Hide();
        web_active_ = false;
        native_->Show();
        if (latest_)
            native_->Apply(*latest_);
        Layout();
        if (changed_)
            changed_();
    } else
        TryWeb();
}
void PanelHost::ReleaseInputs() {
    native_->ReleaseInputs();
    if (web_)
        web_->ReleaseInputs();
}
void PanelHost::CheckPointer() {
    if (web_active_ && web_)
        web_->CheckPointer();
    else
        native_->CheckPointer();
}
std::string PanelHost::Mode() const {
    return web_active_           ? "Web panel"
           : diagnostic_.empty() ? "Native controls"
                                 : "Native controls — " + diagnostic_;
}
