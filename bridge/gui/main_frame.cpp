// BrassCue - GPL-3.0. Application coordination only: input, profile workflow, renderer and output.
#include "main_frame.hpp"
#include "../src/controller_session.hpp"
#include "../src/four_digit_display.hpp"
#include "../src/output_session.hpp"
#include "../src/session_autosave.hpp"
#include "dark_theme.hpp"
#include "network_panel.hpp"
#include "panel_host.hpp"
#include "profile_commit.hpp"
#include "profile_editor.hpp"
#include "profile_files.hpp"
#include "quick_patch.hpp"
#include <chrono>
namespace {
std::uint64_t Now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
class MainFrame final : public wxFrame {
  public:
    explicit MainFrame(const std::filesystem::path &recovery_directory)
        : wxFrame(nullptr, wxID_ANY, "BrassCue", wxDefaultPosition, wxSize(900, 900)),
          recovery_(recovery_directory.empty()
                        ? ProfileFiles::Directory().parent_path() / "recovery"
                        : recovery_directory),
          timer_(this) {
        const auto restored = recovery_.load();
        if (restored.snapshot) {
            session_.restore(restored.snapshot->profile, restored.snapshot->runtime);
            files_.RestoreDocument(restored.snapshot->document);
        }
        recovery_note_ = restored.warning;
        recovery_label_ = !restored.warning.empty() ? "Recovery warning"
                          : restored.snapshot       ? "Session restored"
                                                    : "Auto recovery";
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
            RecoveryTick();
        });
        timer_.Start(40);
        Bind(wxEVT_ACTIVATE, [this](wxActivateEvent &e) {
            if (!e.GetActive() && panel_)
                panel_->ReleaseInputs();
            e.Skip();
        });
        Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent &e) {
            panel_->ReleaseInputs();
            if (!recovery_.flush(Snapshot()) && e.CanVeto() &&
                wxMessageBox("Automatic session recovery could not be saved. Close anyway?\n" +
                                 wxString::FromUTF8(recovery_.error()),
                             "Session recovery", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING,
                             this) != wxYES) {
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
        profile->Append(wxID_SAVE, "Save profile\tCtrl+S",
                        "Explicitly save configuration to .bcue; automatic recovery also preserves "
                        "live levels.");
        profile->Append(wxID_SAVEAS, "Save profile as / export...\tCtrl+Shift+S");
        profile->Append(wxID_NEW, "Restore defaults");
        const int quick_patch = wxID_HIGHEST + 700;
        profile->AppendSeparator();
        profile->Append(quick_patch, "&Quick RGBW Patch...\tCtrl+Shift+P");
        Bind(
            wxEVT_MENU,
            [this](wxCommandEvent &) {
                auto p = session_.profile();
                if (EditQuickRgbwPatch(this, p))
                    CommitEdit(std::move(p));
            },
            quick_patch);
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
                    files_.Defaults();
                    ApplyProfile(true);
                }
            },
            wxID_NEW);
    }
    void Edit(int element) {
        auto p = session_.profile();
        if (EditProfileElement(this, p, element))
            CommitEdit(std::move(p));
    }
    void CommitEdit(brasscue::ControlProfile p) {
        try {
            if (CommitProfileEdit(this, session_, output_, std::move(p))) {
                files_.Changed();
                ApplyProfile(false);
            }
        } catch (const std::exception &e) {
            wxMessageBox(wxString::FromUTF8(e.what()), "Profile not changed", wxOK | wxICON_ERROR,
                         this);
        }
        network_->SetOutput(output_.enabled());
        RefreshView();
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
                                " | " + panel_->Mode() + " | " + recovery_label_;
        if (status_->GetLabel() != wxString::FromUTF8(connection))
            status_->SetLabel(wxString::FromUTF8(connection));
        const auto help = wxString::FromUTF8(
            recovery_note_.empty() ? "Session recovery is automatic. Save profile writes portable "
                                     "configuration separately. Art-Net must be enabled explicitly."
                                   : recovery_note_);
        if (status_->GetToolTipText() != help)
            status_->SetToolTip(help);
        panel_->Apply(brasscue::make_snapshot(session_, display_, now, connection));
        const auto title = wxString::FromUTF8(session_.profile().name) +
                           (files_.dirty() ? " *" : "") + " | BrassCue";
        if (GetTitle() != title)
            SetTitle(title);
    }
    brasscue::SessionSnapshot Snapshot() const {
        return {session_.profile(), session_.checkpoint(Now()), files_.Document()};
    }
    void RecoveryTick() {
        const auto now = Now();
        if (!recovery_.due(now))
            return;
        recovery_.poll(Snapshot(), now);
        if (!recovery_.error().empty()) {
            recovery_label_ = "Recovery save failed";
            recovery_note_ = recovery_.error();
        } else if (recovery_label_ == "Recovery save failed") {
            recovery_label_ = "Auto recovery";
            recovery_note_.clear();
        }
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
    brasscue::SessionAutosave recovery_;
    std::string recovery_label_, recovery_note_;
    wxTimer timer_;
    PanelHost *panel_ = nullptr;
    NetworkPanel *network_ = nullptr;
    wxStaticText *status_ = nullptr;
    bool closing_ = false;
    std::string output_error_;
};
} // namespace
wxFrame *CreateBrassCueFrame(const std::filesystem::path &recovery_directory) {
    return new MainFrame(recovery_directory);
}
