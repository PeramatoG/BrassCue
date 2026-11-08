// BrassCue - GPL-3.0. Front-panel composition and application coordination.
#include "../src/controller_session.hpp"
#include "../src/four_digit_display.hpp"
#include "../src/output_session.hpp"
#include "profile_editor.hpp"
#include "profile_files.hpp"
#include "widgets.hpp"
#include <chrono>
#include <wx/slider.h>
namespace {
enum ControlId {
    EncoderBase = wxID_HIGHEST + 100,
    FaderId = EncoderBase + 4,
    CueBase = FaderId + 1,
    PreviewId = CueBase + 3
};
std::uint64_t Now() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}
wxStaticText *Label(wxWindow *p, const wxString &text) {
    auto *l = new wxStaticText(p, wxID_ANY, text);
    l->SetForegroundColour(wxColour(238, 232, 219));
    return l;
}
class MainFrame final : public wxFrame {
  public:
    MainFrame()
        : wxFrame(nullptr, wxID_ANY, "BrassCue", wxDefaultPosition, wxSize(850, 800)),
          timer_(this) {
        SetMinClientSize(FromDIP(wxSize(760, 680)));
        SetBackgroundColour(wxColour(22, 24, 29));
        auto *root = new wxBoxSizer(wxVERTICAL);
        status_ = Label(this, "SIMULATION | Art-Net OFF");
        root->Add(status_, 0, wxALL, 12);
        auto *front = new wxBoxSizer(wxHORIZONTAL);
        // The physical front has three columns: fader/display, encoders, TFT/buttons.
        auto *left = new wxBoxSizer(wxVERTICAL);
        masterLabel_ = Label(this, "");
        left->Add(masterLabel_, 0, wxALL, 8);
        fader_ = new wxSlider(this, wxID_ANY, 255, 0, 255, wxDefaultPosition,
                              FromDIP(wxSize(80, 360)), wxSL_VERTICAL | wxSL_INVERSE);
        fader_->SetId(FaderId);
        left->Add(fader_, 1, wxALIGN_CENTER_HORIZONTAL | wxALL, 8);
        digits_ = Label(this, " 100");
        digits_->SetForegroundColour(wxColour(255, 184, 36));
        digits_->SetFont(wxFont(26, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
        left->Add(digits_, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 12);
        BindEdit(fader_, 4);
        BindEdit(masterLabel_, 4);
        BindEdit(digits_, 4);
        fader_->Bind(wxEVT_SLIDER, [this](wxCommandEvent &e) {
            session_.fader(e.GetInt());
            RefreshState();
        });
        front->Add(left, 1, wxEXPAND | wxALL, 8);
        auto *middle = new wxBoxSizer(wxVERTICAL);
        for (int i = 0; i < 4; ++i) {
            auto *row = new wxBoxSizer(wxVERTICAL);
            encoderLabels_[i] = Label(this, "");
            row->Add(encoderLabels_[i], 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 4);
            knobs_[i] = new EncoderKnob(this, [this, i](int v) {
                session_.encoder(i, v);
                display_.encoder_changed(i, v, Now());
                RefreshState();
            });
            knobs_[i]->SetId(EncoderBase + i);
            row->Add(knobs_[i], 1, wxEXPAND);
            BindEdit(knobs_[i], i);
            BindEdit(encoderLabels_[i], i);
            middle->Add(row, 1, wxEXPAND | wxALL, 4);
        }
        front->Add(middle, 1, wxEXPAND | wxALL, 8);
        auto *right = new wxBoxSizer(wxVERTICAL);
        preview_ = new ColorPreview(this);
        preview_->SetId(PreviewId);
        BindEdit(preview_, 8);
        right->Add(preview_, 2, wxEXPAND | wxBOTTOM, 12);
        for (int i = 0; i < 3; ++i) {
            buttons_[i] = new CueButton(this, [this, i](bool down) {
                session_.button(i, down, Now());
                RefreshState();
            });
            buttons_[i]->SetId(CueBase + i);
            right->Add(buttons_[i], 1, wxEXPAND | wxTOP | wxBOTTOM, 8);
            BindEdit(buttons_[i], i + 5);
        }
        front->Add(right, 2, wxEXPAND | wxALL, 16);
        root->Add(front, 1, wxEXPAND);
        auto *network = new wxBoxSizer(wxHORIZONTAL);
        network->Add(Label(this, "Art-Net IPv4"), 0, wxALIGN_CENTER_VERTICAL | wxALL, 6);
        ip_ = new wxTextCtrl(this, wxID_ANY, "127.0.0.1");
        network->Add(ip_, 1, wxALL, 6);
        broadcast_ = new wxCheckBox(this, wxID_ANY, "Broadcast opt-in");
        broadcast_->SetForegroundColour(*wxWHITE);
        network->Add(broadcast_, 0, wxALIGN_CENTER_VERTICAL | wxALL, 6);
        enabled_ = new wxCheckBox(this, wxID_ANY, "Output ON");
        enabled_->SetForegroundColour(*wxWHITE);
        network->Add(enabled_, 0, wxALIGN_CENTER_VERTICAL | wxALL, 6);
        enabled_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent &) { Network(); });
        root->Add(network, 0, wxEXPAND | wxALL, 8);
        ip_->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { NetworkEdited(); });
        broadcast_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent &) { NetworkEdited(); });
        auto *menu = new wxMenu;
        menu->Append(wxID_OPEN, "Open / import...\tCtrl+O");
        menu->Append(wxID_SAVE, "Save\tCtrl+S");
        menu->Append(wxID_SAVEAS, "Save as / export...\tCtrl+Shift+S");
        menu->Append(wxID_NEW, "Restore defaults");
        auto *bar = new wxMenuBar;
        bar->Append(menu, "&Profile");
        SetMenuBar(bar);
        Bind(
            wxEVT_MENU,
            [this](wxCommandEvent &) {
                auto p = session_.profile();
                if (files_.Open(this, p)) {
                    Stop();
                    session_ = brasscue::ControllerSession{};
                    session_.configure(p);
                    ApplyProfile();
                }
            },
            wxID_OPEN);
        Bind(
            wxEVT_MENU,
            [this](wxCommandEvent &) {
                Save(false);
                RefreshState();
            },
            wxID_SAVE);
        Bind(
            wxEVT_MENU,
            [this](wxCommandEvent &) {
                Save(true);
                RefreshState();
            },
            wxID_SAVEAS);
        Bind(
            wxEVT_MENU,
            [this](wxCommandEvent &) {
                if (files_.Discard(this)) {
                    Stop();
                    session_ = brasscue::ControllerSession{};
                    files_.Changed();
                    ApplyProfile();
                }
            },
            wxID_NEW);
        Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent &e) {
            if (e.CanVeto() && !files_.Discard(this)) {
                e.Veto();
                return;
            }
            Stop();
            e.Skip();
        });
        Bind(wxEVT_ACTIVATE, [this](wxActivateEvent &e) {
            if (!e.GetActive())
                for (auto *b : buttons_)
                    b->Release();
            e.Skip();
        });
        Bind(wxEVT_TIMER, [this](wxTimerEvent &) {
            for (auto *button : buttons_)
                button->CheckPointer();
            RefreshState();
            Send();
        });
        timer_.Start(40);
        SetSizer(root);
        ApplyProfile();
        Centre();
    }

  private:
    void Edit(int element) {
        auto p = session_.profile();
        if (EditProfileElement(this, p, element)) {
            session_.configure(std::move(p));
            files_.Changed();
            ApplyProfile();
        }
    }
    void BindEdit(wxWindow *w, int element) {
        w->Bind(wxEVT_RIGHT_UP, [this, element](wxMouseEvent &) { Edit(element); });
        w->Bind(wxEVT_CONTEXT_MENU, [this, element](wxContextMenuEvent &e) {
            if (e.GetPosition() == wxDefaultPosition)
                Edit(element);
        });
    }
    void ApplyProfile() {
        const auto &p = session_.profile();
        ip_->ChangeValue(wxString::FromUTF8(p.target_ip));
        broadcast_->SetValue(p.allow_broadcast);
        masterLabel_->SetLabel(wxString::FromUTF8(p.fader_label));
        fader_->SetValue(session_.state().master);
        for (int i = 0; i < 4; ++i)
            knobs_[i]->SetValue(session_.state().rgbw[i]);
        RefreshState();
        Layout();
    }
    void RefreshState() {
        const auto &p = session_.profile();
        const auto &s = session_.state();
        for (int i = 0; i < 4; ++i)
            encoderLabels_[i]->SetLabel(
                wxString::FromUTF8(p.encoder_labels[i]) +
                wxString::Format("  %u%%", brasscue::to_percent(s.rgbw[i])));
        for (int i = 0; i < 3; ++i)
            buttons_[i]->SetLabel(wxString::FromUTF8(p.button_labels[i]) + "\n" +
                                  wxString::FromUTF8(session_.engine().button_status(i, Now())));
        display_.set_master(p.fader.grandmaster ? s.master : 255);
        digits_->SetLabel(wxString::FromUTF8(display_.text(Now())));
        preview_->SetState(s, p);
        SetTitle(wxString::FromUTF8(p.name) + (files_.dirty() ? " *" : "") + " | BrassCue");
    }
    void Save(bool save_as) {
        try {
            session_.network(ip_->GetValue().ToStdString(), broadcast_->GetValue());
            files_.Save(this, session_.profile(), save_as);
        } catch (const std::exception &e) {
            wxMessageBox(wxString::FromUTF8(e.what()), "Invalid profile", wxOK | wxICON_ERROR,
                         this);
        }
    }
    void NetworkEdited() {
        Stop();
        files_.Changed();
        auto p = session_.profile();
        p.target_ip = ip_->GetValue().ToStdString();
        p.allow_broadcast = broadcast_->GetValue();
        try {
            session_.network(p.target_ip, p.allow_broadcast);
            files_.Changed();
        } catch (const std::exception &) {
            status_->SetLabel("SIMULATION | Invalid network draft; correct before saving");
        }
    }
    void Network() {
        try {
            if (enabled_->GetValue())
                output_.enable(ip_->GetValue().ToStdString(), broadcast_->GetValue());
            else
                output_.stop();
            status_->SetLabel(enabled_->GetValue() ? "SIMULATION | Art-Net ON"
                                                   : "SIMULATION | Art-Net OFF");
            Send();
        } catch (const std::exception &e) {
            Error(e);
        }
    }
    void Stop() {
        try {
            output_.stop();
        } catch (const std::exception &e) {
            Error(e);
        }
        enabled_->SetValue(false);
        status_->SetLabel("SIMULATION | Art-Net OFF");
    }
    void Send() {
        try {
            output_.send(session_.engine(), Now());
        } catch (const std::exception &e) {
            Error(e);
        }
    }
    void Error(const std::exception &e) {
        enabled_->SetValue(false);
        try {
            output_.stop();
        } catch (...) {
        }
        status_->SetLabel("SIMULATION | Art-Net ERROR");
        wxMessageBox(wxString::FromUTF8(e.what()), "Art-Net", wxOK | wxICON_ERROR, this);
    }
    brasscue::ControllerSession session_;
    brasscue::FourDigitDisplay display_;
    brasscue::OutputSession output_;
    ProfileFiles files_;
    wxTimer timer_;
    wxStaticText *status_ = nullptr;
    wxStaticText *masterLabel_ = nullptr;
    wxStaticText *digits_ = nullptr;
    std::array<wxStaticText *, 4> encoderLabels_{};
    std::array<EncoderKnob *, 4> knobs_{};
    std::array<CueButton *, 3> buttons_{};
    wxSlider *fader_ = nullptr;
    ColorPreview *preview_ = nullptr;
    wxTextCtrl *ip_ = nullptr;
    wxCheckBox *broadcast_ = nullptr;
    wxCheckBox *enabled_ = nullptr;
};
} // namespace
wxFrame *CreateBrassCueFrame() {
    return new MainFrame;
}
