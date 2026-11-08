// BrassCue - GPL-3.0. Application coordination only: input, profile workflow, renderer and output.
#include "../src/controller_session.hpp"
#include "../src/four_digit_display.hpp"
#include "../src/output_session.hpp"
#include "dark_theme.hpp"
#include "network_panel.hpp"
#include "panel_host.hpp"
#include "profile_editor.hpp"
#include "profile_files.hpp"
#include <chrono>
namespace {
std::uint64_t Now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
class MainFrame final : public wxFrame {
  public:
    MainFrame()
        : wxFrame(nullptr, wxID_ANY, "BrassCue", wxDefaultPosition, wxSize(900, 900)),
          timer_(this) {
        SetMinClientSize(FromDIP(wxSize(780, 760)));
        SetBackgroundColour(wxColour(22, 24, 29));
        auto *root = new wxBoxSizer(wxVERTICAL);
        status_ = new wxStaticText(this, wxID_ANY, "");
        status_->SetForegroundColour(wxColour(238, 232, 219));
        root->Add(status_, 0, wxALL, 10);
        panel_ = new PanelHost(this, Actions(), [this] {
            if (!closing_)
                RefreshView();
        });
        root->Add(panel_, 1, wxEXPAND);
        network_ = new NetworkPanel(
            this, [this] { NetworkEdited(); }, [this](bool enabled) { EnableOutput(enabled); });
        root->Add(network_, 0, wxEXPAND | wxALL, 8);
        Menus();
        SetSizer(root);
        ApplyDarkTitle(*this);
        Bind(wxEVT_TIMER, [this](wxTimerEvent &) {
            if (closing_)
                return;
            panel_->CheckPointer();
            session_.advance(Now());
            Send();
            RefreshView();
        });
        timer_.Start(40);
        Bind(wxEVT_ACTIVATE, [this](wxActivateEvent &e) {
            if (!e.GetActive() && panel_)
                panel_->ReleaseInputs();
            e.Skip();
        });
        Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent &e) {
            if (e.CanVeto() && !files_.Discard(this)) {
                e.Veto();
                return;
            }
            closing_ = true;
            timer_.Stop();
            panel_->ReleaseInputs();
            Stop();
            e.Skip();
        });
        ApplyProfile(true);
        Centre();
    }

  private:
    brasscue::ControllerActions Actions() {
        brasscue::ControllerActions actions;
        actions.encoder_delta = [this](std::size_t i, int delta) {
            session_.encoder_delta(i, delta, Now());
            display_.encoder_changed(i, session_.state().rgbw.at(i), Now());
            RefreshView();
        };
        actions.fader = [this](std::uint8_t value) {
            session_.fader(value, Now());
            RefreshView();
        };
        actions.button = [this](std::size_t i, bool down) {
            session_.button(i, down, Now());
            RefreshView();
        };
        actions.edit = [this](int element) { Edit(element); };
        actions.release = [this] { session_.release_all(Now()); };
        return actions;
    }
    void Menus() {
        auto *profile = new wxMenu;
        profile->Append(wxID_OPEN, "Open / import...\tCtrl+O");
        profile->Append(wxID_SAVE, "Save\tCtrl+S");
        profile->Append(wxID_SAVEAS, "Save as / export...\tCtrl+Shift+S");
        profile->Append(wxID_NEW, "Restore defaults");
        auto *bar = new wxMenuBar;
        bar->Append(profile, "&Profile");
#if BRASSCUE_ENABLE_WEB_PANEL
        auto *view = new wxMenu;
        const int native = wxID_HIGHEST + 500;
        view->AppendCheckItem(native, "Prefer native controls");
        bar->Append(view, "&View");
        Bind(
            wxEVT_MENU, [this](wxCommandEvent &e) { panel_->PreferNative(e.IsChecked()); }, native);
#endif
        SetMenuBar(bar);
        Bind(
            wxEVT_MENU,
            [this](wxCommandEvent &) {
                auto p = session_.profile();
                if (files_.Open(this, p)) {
                    Stop();
                    session_ = brasscue::ControllerSession{};
                    session_.configure(std::move(p));
                    ApplyProfile(true);
                }
            },
            wxID_OPEN);
        Bind(wxEVT_MENU, [this](wxCommandEvent &) { Save(false); }, wxID_SAVE);
        Bind(wxEVT_MENU, [this](wxCommandEvent &) { Save(true); }, wxID_SAVEAS);
        Bind(
            wxEVT_MENU,
            [this](wxCommandEvent &) {
                if (files_.Discard(this)) {
                    Stop();
                    session_ = brasscue::ControllerSession{};
                    files_.Changed();
                    ApplyProfile(true);
                }
            },
            wxID_NEW);
    }
    void Edit(int element) {
        auto p = session_.profile();
        if (EditProfileElement(this, p, element)) {
            session_.configure(std::move(p));
            files_.Changed();
            ApplyProfile(false);
        }
    }
    void Save(bool save_as) {
        try {
            network_->WritePreferences(session_);
            files_.Save(this, session_.profile(), save_as);
        } catch (const std::exception &e) {
            wxMessageBox(wxString::FromUTF8(e.what()), "Invalid profile", wxOK | wxICON_ERROR,
                         this);
        }
        RefreshView();
    }
    void ApplyProfile(bool load_network) {
        if (load_network) {
            Stop();
            network_->Load(session_.profile());
        }
        RefreshView();
        Layout();
    }
    void RefreshView() {
        const auto now = Now();
        session_.advance(now);
        display_.set_master(session_.state().master);
        const auto connection = std::string("SIMULATION | Art-Net ") +
                                (output_.enabled()       ? "ON"
                                 : output_error_.empty() ? "OFF"
                                                         : "ERROR") +
                                " | " + panel_->Mode();
        if (status_->GetLabel() != wxString::FromUTF8(connection))
            status_->SetLabel(wxString::FromUTF8(connection));
        panel_->Apply(brasscue::make_snapshot(session_, display_, now, connection));
        const auto title = wxString::FromUTF8(session_.profile().name) +
                           (files_.dirty() ? " *" : "") + " | BrassCue";
        if (GetTitle() != title)
            SetTitle(title);
    }
    void NetworkEdited() {
        Stop();
        files_.Changed();
        try {
            network_->WritePreferences(session_);
        } catch (const std::exception &) { /* Keep incomplete network drafts local until valid. */
        }
        RefreshView();
    }
    void EnableOutput(bool enabled) {
        try {
            if (enabled) {
                network_->WritePreferences(session_);
                output_.enable(network_->Resolve());
                output_error_.clear();
            } else
                Stop();
            network_->SetOutput(output_.enabled());
        } catch (const std::exception &e) {
            OutputError(e);
        }
        RefreshView();
    }
    void Stop() {
        try {
            output_.stop();
        } catch (const std::exception &e) {
            output_error_ = e.what();
        }
        if (network_)
            network_->SetOutput(false);
    }
    void Send() {
        try {
            output_.send(session_.engine(), Now());
        } catch (const std::exception &e) {
            OutputError(e);
        }
    }
    void OutputError(const std::exception &e) {
        Stop();
        output_error_ = e.what();
        if (!closing_)
            wxMessageBox(wxString::FromUTF8(output_error_), "Art-Net stopped", wxOK | wxICON_ERROR,
                         this);
    }
    brasscue::ControllerSession session_;
    brasscue::FourDigitDisplay display_;
    brasscue::OutputSession output_;
    ProfileFiles files_;
    wxTimer timer_;
    PanelHost *panel_ = nullptr;
    NetworkPanel *network_ = nullptr;
    wxStaticText *status_ = nullptr;
    bool closing_ = false;
    std::string output_error_;
};
} // namespace
wxFrame *CreateBrassCueFrame() {
    return new MainFrame;
}
