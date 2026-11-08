// BrassCue - GPL-3.0. Local browser presentation; all controller rules remain in C++.
#include "presentation/web_protocol.hpp"
#include "web_panel_api.hpp"
#include <wx/filename.h>
#include <wx/filesys.h>
#include <wx/stdpaths.h>
#include <wx/webview.h>
namespace {
class WebPanel final : public wxPanel, public WebPanelInterface {
  public:
    WebPanel(wxWindow *parent, brasscue::ControllerActions actions, std::function<void()> ready,
             std::function<void(const std::string &)> failed, const wxString &backend)
        : wxPanel(parent), bridge_(std::move(actions)), ready_(std::move(ready)),
          failed_(std::move(failed)), deadline_(this) {
        const auto executable = wxFileName(wxStandardPaths::Get().GetExecutablePath());
        const auto file = wxFileName(executable.GetPath() + "/web_assets", "compact.html");
        if (!file.FileExists())
            throw std::runtime_error("Packaged web_assets/compact.html is missing");
        url_ = wxFileSystem::FileNameToURL(file);
        browser_ = wxWebView::New(backend);
        if (!browser_)
            throw std::runtime_error("wxWebView could not create its backend");
        if (!browser_->Create(this, wxID_ANY, "about:blank"))
            throw std::runtime_error("wxWebView initialization failed");
        auto *layout = new wxBoxSizer(wxVERTICAL);
        layout->Add(browser_, 1, wxEXPAND);
        SetSizer(layout);
        browser_->EnableContextMenu(false);
        if (!browser_->AddScriptMessageHandler("brasscueHost"))
            throw std::runtime_error("wxWebView local message handler is unavailable");
        browser_->Bind(wxEVT_WEBVIEW_NAVIGATING, [this](wxWebViewEvent &event) {
            if (!brasscue::allowed_panel_navigation(event.GetURL().ToStdString(wxConvUTF8),
                                                    url_.ToStdString(wxConvUTF8))) {
                event.Veto();
                return;
            }
            bridge_.reset(++epoch_);
            sent_.reset();
            inflight_.reset();
            busy_ = false;
            begin_id_ = snapshot_id_ = 0;
            deadline_.StartOnce(10000);
        });
        browser_->Bind(wxEVT_WEBVIEW_NEWWINDOW, [](wxWebViewEvent &event) { event.Veto(); });
        browser_->Bind(wxEVT_WEBVIEW_ERROR, [this](wxWebViewEvent &) {
            Fail("Local panel failed to load; native controls remain available");
        });
        browser_->Bind(wxEVT_WEBVIEW_LOADED, [this](wxWebViewEvent &event) {
            if (event.GetURL() != url_ || failed_state_)
                return;
            begin_id_ = ++script_serial_;
            browser_->RunScriptAsync(
                "window.brasscuePanel.begin(" +
                    wxString::FromUTF8(brasscue::json_string(std::to_string(epoch_))) + ");",
                reinterpret_cast<void *>(begin_id_));
        });
        browser_->Bind(wxEVT_WEBVIEW_SCRIPT_MESSAGE_RECEIVED, [this](wxWebViewEvent &event) {
            if (failed_state_ || event.GetURL() != url_)
                return;
            const bool was_ready = bridge_.ready();
            if (!bridge_.accept(event.GetString().ToStdString(wxConvUTF8)))
                return;
            if (!was_ready && bridge_.ready()) {
                deadline_.Stop();
                if (ready_)
                    ready_();
                Flush();
            }
        });
        browser_->Bind(wxEVT_WEBVIEW_SCRIPT_RESULT, [this](wxWebViewEvent &event) {
            if (failed_state_)
                return;
            const auto id = reinterpret_cast<std::uintptr_t>(event.GetClientData());
            if (!id || (id != begin_id_ && id != snapshot_id_))
                return;
            if (event.IsError()) {
                Fail("Local panel script failed; using native controls");
                return;
            }
            if (id != snapshot_id_ || !busy_)
                return;
            deadline_.Stop();
            sent_ = inflight_;
            inflight_.reset();
            busy_ = false;
            Flush();
        });
        browser_->Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent &event) {
            ReleaseInputs();
            event.Skip();
        });
        Bind(wxEVT_TIMER, [this](wxTimerEvent &) {
            Fail("Web renderer readiness timed out; using native controls");
        });
        browser_->LoadURL(url_);
        deadline_.StartOnce(10000);
    }
    wxWindow &Window() override {
        return *this;
    }
    void Apply(const brasscue::ViewSnapshot &snapshot) override {
        latest_ = snapshot;
        Flush();
    }
    void ReleaseInputs() override {
        bridge_.release_inputs();
    }
    void CheckPointer() override {
        bridge_.reconcile_mouse(wxGetMouseState().LeftIsDown());
    }

  private:
    void Flush() {
        if (failed_state_ || !bridge_.ready() || busy_ || !latest_)
            return;
        const auto json = brasscue::snapshot_json(sent_ ? &*sent_ : nullptr, *latest_);
        if (json.empty())
            return;
        inflight_ = latest_;
        busy_ = true;
        snapshot_id_ = ++script_serial_;
        deadline_.StartOnce(10000);
        browser_->RunScriptAsync("window.brasscuePanel.apply(" + wxString::FromUTF8(json) + ");",
                                 reinterpret_cast<void *>(snapshot_id_));
    }
    void Fail(const std::string &reason) {
        if (failed_state_)
            return;
        failed_state_ = true;
        deadline_.Stop();
        ReleaseInputs();
        if (failed_)
            failed_(reason);
    }
    wxWebView *browser_ = nullptr;
    brasscue::WebInputBridge bridge_;
    std::function<void()> ready_;
    std::function<void(const std::string &)> failed_;
    wxTimer deadline_;
    wxString url_;
    std::uint64_t epoch_ = 0;
    std::uintptr_t script_serial_ = 0, begin_id_ = 0, snapshot_id_ = 0;
    bool busy_ = false, failed_state_ = false;
    std::optional<brasscue::ViewSnapshot> latest_, sent_, inflight_;
};
} // namespace
WebPanelInterface *CreateWebPanel(wxWindow *parent, brasscue::ControllerActions actions,
                                  std::function<void()> ready,
                                  std::function<void(const std::string &)> failed,
                                  std::string &diagnostic) {
#ifdef __WXMSW__
    const wxString backend = wxASCII_STR(wxWebViewBackendEdge);
#else
    const wxString backend = wxASCII_STR(wxWebViewBackendWebKit);
#endif
    if (!wxWebView::IsBackendAvailable(backend)) {
        diagnostic = "Web renderer unavailable. On Windows install the Microsoft Edge WebView2 "
                     "Evergreen Runtime.";
        return nullptr;
    }
    try {
        return new WebPanel(parent, std::move(actions), std::move(ready), std::move(failed),
                            backend);
    } catch (const std::exception &e) {
        diagnostic = e.what();
        return nullptr;
    }
}
