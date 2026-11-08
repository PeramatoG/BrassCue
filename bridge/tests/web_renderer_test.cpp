// BrassCue - GPL-3.0. Exercise the actual embedded renderer and real Core, not a mock engine.
#include "../gui/web_panel_api.hpp"
#include "../gui/web_script.hpp"
#include "../src/controller_session.hpp"
#include "../src/four_digit_display.hpp"
#include "check.hpp"
#include <wx/webview.h>
class RendererTestApp final : public wxApp {
  public:
    bool OnInit() override {
        session_.fader(66);
        frame_ = new wxFrame(nullptr, wxID_ANY, "BrassCue renderer integration", wxDefaultPosition,
                             wxSize(900, 760));
        brasscue::ControllerActions actions;
        actions.encoder_delta = [this](std::size_t i, int delta) {
            session_.encoder_delta(i, delta, 0);
        };
        actions.fader = [this](std::uint8_t value) { session_.fader(value); };
        actions.button = [this](std::size_t i, bool down) { session_.button(i, down, 0); };
        actions.release = [this] {
            ++releases_;
            session_.release_all(0);
        };
        std::string diagnostic;
        bool backend_unavailable = false;
        panel_ = CreateWebPanel(
            frame_, actions, [this] { ++ready_count_; },
            [this](const std::string &why) {
                if (phase_ == 6) {
                    CHECK(releases_ > releases_before_failure_ && session_.state().master == 7);
                    std::cout << "Actual web renderer: preset click, DOM sync, UTF-8 labels, "
                                 "reload, navigation veto and script failure release passed\n";
                    std::exit(0);
                }
                std::cerr << why << '\n';
                std::exit(1);
            },
            diagnostic, &backend_unavailable);
        if (!panel_) {
            std::cerr << (backend_unavailable ? "SKIP: " : "FAIL: ") << diagnostic << '\n';
            std::exit(backend_unavailable ? 77 : 1);
        }
        auto *layout = new wxBoxSizer(wxVERTICAL);
        layout->Add(&panel_->Window(), 1, wxEXPAND);
        frame_->SetSizer(layout);
        frame_->Show();
        browser_ = FindBrowser(&panel_->Window());
        CHECK(browser_);
        browser_->Bind(wxEVT_WEBVIEW_SCRIPT_RESULT, [this](wxWebViewEvent &event) {
            if (event.GetClientData() != this) {
                event.Skip();
                return;
            }
            if (event.IsError())
                std::cerr << "DOM query failed at phase " << phase_ << ": " << event.GetString()
                          << "\n";
            CHECK(!event.IsError());
            const auto result = event.GetString();
            if (result == "ok" || result == "\"ok\"") {
                if (phase_ == 1) {
                    CHECK(session_.state().master == 128);
                    phase_ = 2;
                    browser_->Reload();
                } else if (phase_ == 3) {
                    CHECK(session_.state().master == 128);
                    browser_->LoadURL("https://example.invalid/brasscue-blocked");
                    phase_ = 4;
                } else if (phase_ == 5) {
                    CHECK(session_.state().master == 0);
                    CHECK(session_.button_selected(2, 0));
                    phase_ = 6;
                    releases_before_failure_ = releases_;
                    RunPanelScript(*browser_, "window.brasscuePanel={apply:function(){throw new "
                                              "Error('test failure');}};");
                    session_.fader(7);
                }
            } else if (result != "wait" && result != "\"wait\"") {
                std::cerr << "Unexpected DOM result: " << result << '\n';
                std::exit(1);
            }
            pending_ = false;
        });
        timer_ = new wxTimer(this);
        Bind(wxEVT_TIMER, [this](wxTimerEvent &) { Tick(); });
        timer_->Start(100);
        return true;
    }

  private:
    static wxWebView *FindBrowser(wxWindow *root) {
        if (auto *browser = wxDynamicCast(root, wxWebView))
            return browser;
        for (auto *child : root->GetChildren())
            if (auto *found = FindBrowser(child))
                return found;
        return nullptr;
    }
    void Query(const std::string &expression) {
        pending_ = true;
        RunPanelScript(*browser_, wxString::FromUTF8(expression), this);
    }
    void Tick() {
        CHECK(++ticks_ < 200);
        brasscue::FourDigitDisplay display;
        display.set_master(session_.state().master);
        panel_->Apply(brasscue::make_snapshot(session_, display, 0, "Renderer test"));
        if (pending_)
            return;
        if (phase_ == 0 && ready_count_ == 1) {
            auto profile = session_.profile();
            profile.button_labels[1] = "SALA <50> ámbar";
            session_.configure(profile);
            RunPanelScript(*browser_, "document.getElementById('cue-1').click();");
            phase_ = 1;
        } else if (phase_ == 1) {
            Query("document.getElementById('fader').value==='128' && "
                  "document.getElementById('display').textContent.trim()==='50' && "
                  "document.getElementById('cue-1').textContent==='SALA <50> ámbar' && "
                  "document.getElementById('cue-1').getAttribute('aria-pressed')==='true' ? 'ok' : "
                  "'wait'");
        } else if (phase_ == 2 && ready_count_ == 2) {
            phase_ = 3;
        } else if (phase_ == 3) {
            Query("document.getElementById('fader').value==='128' ? 'ok' : 'wait'");
        } else if (phase_ == 4) {
            CHECK(browser_->GetCurrentURL().StartsWith("file:"));
            RunPanelScript(*browser_, "document.getElementById('cue-2').click();");
            phase_ = 5;
        } else if (phase_ == 5) {
            Query("document.getElementById('fader').value==='0' && "
                  "document.getElementById('cue-2').getAttribute('aria-pressed')==='true' ? 'ok' : "
                  "'wait'");
        }
    }
    brasscue::ControllerSession session_;
    wxFrame *frame_ = nullptr;
    WebPanelInterface *panel_ = nullptr;
    wxWebView *browser_ = nullptr;
    wxTimer *timer_ = nullptr;
    int phase_ = 0, ready_count_ = 0, ticks_ = 0, releases_ = 0, releases_before_failure_ = 0;
    bool pending_ = false;
};
wxIMPLEMENT_APP(RendererTestApp);
