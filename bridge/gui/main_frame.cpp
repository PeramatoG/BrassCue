#include "../src/bridge_core.hpp"
#include "../src/controller_profile.hpp"
#include "../src/four_digit_display.hpp"
#include "../src/cue_engine.hpp"
#include <wx/numdlg.h>
#include <wx/tokenzr.h>
#include <cstdio>
#include <chrono>
#include <wx/textdlg.h>
#include <wx/choicdlg.h>
#include "../src/artnet_udp.hpp"

#include <wx/wx.h>
#include <wx/spinctrl.h>
#include <wx/dcbuffer.h>
#include <wx/statline.h>
#include <array>
#include <algorithm>
#include <memory>
#include <stdexcept>

namespace {
const wxColour kBackground(22, 24, 29);
const wxColour kSurface(33, 37, 44);
const wxColour kRaised(43, 48, 56);
const wxColour kBrass(205, 161, 84);
const wxColour kMuted(174, 180, 186);
const wxColour kText(238, 232, 219);

wxStaticText* MakeLabel(wxWindow* parent, const wxString& text, int pt = 10, bool strong = false) {
    auto* label = new wxStaticText(parent, wxID_ANY, text);
    wxFont font = label->GetFont();
    font.SetPointSize(pt);
    if (strong) font.SetWeight(wxFONTWEIGHT_BOLD);
    label->SetFont(font);
    label->SetForegroundColour(strong ? kText : kMuted);
    return label;
}

wxPanel* MakeSurface(wxWindow* parent) {
    auto* panel = new wxPanel(parent);
    panel->SetBackgroundColour(kSurface);
    return panel;
}

wxButton* MakeButton(wxWindow* parent, const wxString& text) {
    auto* button = new wxButton(parent, wxID_ANY, text, wxDefaultPosition, wxSize(-1, 38));
    button->SetBackgroundColour(kRaised);
    button->SetForegroundColour(kText);
    return button;
}

class ColorPreview final : public wxPanel {
public:
    explicit ColorPreview(wxWindow* parent) : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(175, 175)) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, &ColorPreview::OnPaint, this);
    }
    void SetState(const brasscue::State& state) { state_ = state; Refresh(); }
private:
    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(kSurface));
        dc.Clear();
        const wxSize size = GetClientSize();
        const int side = (std::max)(1, (std::min)(size.GetWidth(), size.GetHeight()) - 26);
        const int x = (size.GetWidth() - side) / 2, y = (size.GetHeight() - side) / 2;
        auto scaled = [&](std::size_t i) -> int {
            return brasscue::apply_master(state_.rgbw[i], state_.master);
        };
        // Additive RGBW visualization; white is added to each RGB component.
        const int w = scaled(3);
        const auto clamp = [](int v) -> unsigned char { return static_cast<unsigned char>((std::min)(255, v)); };
        dc.SetPen(wxPen(kBrass, 2));
        dc.SetBrush(wxBrush(wxColour(clamp(scaled(0) + w),
                                        clamp(scaled(1) + w),
                                        clamp(scaled(2) + w))));
        dc.DrawRoundedRectangle(x, y, side, side, 8);
    }
    brasscue::State state_;
};

class MainFrame final : public wxFrame {
public:
    MainFrame() : wxFrame(nullptr, wxID_ANY, "BrassCue  |  Lighting Controller",
                          wxDefaultPosition, wxSize(1100, 720),
                          wxDEFAULT_FRAME_STYLE | wxCLIP_CHILDREN) {
        SetMinClientSize(wxSize(850, 570));
        SetBackgroundColour(kBackground);
        auto* outer = new wxBoxSizer(wxVERTICAL);
        auto* header = new wxBoxSizer(wxHORIZONTAL);
        header->Add(MakeLabel(this, "BRASSCUE", 23, true), 0, wxALIGN_CENTER_VERTICAL);
        header->AddSpacer(16);
        header->Add(MakeLabel(this, "STEAMPUNK LIGHTING CONTROL  /  SIMULATOR", 9),
                    0, wxALIGN_CENTER_VERTICAL);
        header->AddStretchSpacer();
        status_ = MakeLabel(this, "SIMULATION  |  ART-NET OFF", 9);
        header->Add(status_, 0, wxALIGN_CENTER_VERTICAL);
        outer->Add(header, 0, wxEXPAND | wxALL, 24);
        auto* line = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 2));
        line->SetBackgroundColour(kBrass);
        outer->Add(line, 0, wxEXPAND | wxLEFT | wxRIGHT, 24);

        auto* content = new wxBoxSizer(wxHORIZONTAL);
        auto* controls = MakeSurface(this);
        auto* controlBox = new wxBoxSizer(wxVERTICAL);
        controlBox->Add(MakeLabel(controls, "RGBW ENCODERS", 13, true), 0, wxALL, 16);
        auto* grid = new wxFlexGridSizer(2, 2, 12, 12);
        grid->AddGrowableCol(0, 1);
        grid->AddGrowableCol(1, 1);
        const wxString names[] = {"RED", "GREEN", "BLUE", "WHITE"};
        for (int i = 0; i < 4; ++i) {
            auto* card = MakeSurface(controls);
            card->SetBackgroundColour(kRaised);
            auto* column = new wxBoxSizer(wxVERTICAL);
            auto* row = new wxBoxSizer(wxHORIZONTAL);
            encoderLabels_[i] = MakeLabel(card, names[i], 11, true);
            row->Add(encoderLabels_[i], 0, wxALIGN_CENTER_VERTICAL);
            card->Bind(wxEVT_RIGHT_UP, [this, i](wxMouseEvent&) { EditEncoder(i); });
            encoderLabels_[i]->Bind(wxEVT_RIGHT_UP, [this, i](wxMouseEvent&) { EditEncoder(i); });
            row->AddStretchSpacer();
            values_[i] = MakeLabel(card, "000 / 00", 12, true);
            values_[i]->SetForegroundColour(kBrass);
            row->Add(values_[i], 0, wxALIGN_CENTER_VERTICAL);
            column->Add(row, 0, wxEXPAND | wxALL, 10);
            sliders_[i] = new wxSlider(card, wxID_ANY, 0, 0, 255,
                                      wxDefaultPosition, wxSize(190, -1));
            sliders_[i]->Bind(wxEVT_SLIDER, [this, i](wxCommandEvent& e) {
                state_.rgbw[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(e.GetInt());
                display_.encoder_changed(i, state_.rgbw[static_cast<std::size_t>(i)], Now());
                engine_.set_encoder(i, encoderAssignments_[i], state_.rgbw[i]);
                StateChanged();
            });
            column->Add(sliders_[i], 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
            auto* nudges = new wxBoxSizer(wxHORIZONTAL);
            auto* down = MakeButton(card, "-1");
            auto* up = MakeButton(card, "+1");
            down->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent&) { Nudge(i, -1); });
            up->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent&) { Nudge(i, 1); });
            nudges->Add(down, 1, wxRIGHT, 4);
            nudges->Add(up, 1, wxLEFT, 4);
            column->Add(nudges, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
            card->SetSizer(column);
            grid->Add(card, 1, wxEXPAND);
        }
        controlBox->Add(grid, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);

        auto* master = MakeSurface(controls);
        master->SetBackgroundColour(kRaised);
        auto* masterBox = new wxBoxSizer(wxVERTICAL);
        auto* masterHeading = new wxBoxSizer(wxHORIZONTAL);
        masterLabel_ = MakeLabel(master, "GRANDMASTER", 12, true);
        masterHeading->Add(masterLabel_, 0);
        master->Bind(wxEVT_RIGHT_UP, [this](wxMouseEvent&) { EditMaster(); });
        masterLabel_->Bind(wxEVT_RIGHT_UP, [this](wxMouseEvent&) { EditMaster(); });
        masterHeading->AddStretchSpacer();
        masterValue_ = MakeLabel(master, " 100", 12, true);
        masterValue_->SetForegroundColour(kBrass);
        masterHeading->Add(masterValue_);
        masterBox->Add(masterHeading, 0, wxEXPAND | wxALL, 10);
        grandmaster_ = new wxSlider(master, wxID_ANY, 255, 0, 255);
        grandmaster_->Bind(wxEVT_SLIDER, [this](wxCommandEvent& e) {
            state_.master = static_cast<std::uint8_t>(e.GetInt());
            engine_.set_fader(state_.master);
            StateChanged();
        });
        masterBox->Add(grandmaster_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
        master->SetSizer(masterBox);
        controlBox->Add(master, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);

        auto* buttons = new wxBoxSizer(wxHORIZONTAL);
        for (int i = 0; i < 3; ++i) {
            auto* button = MakeButton(controls, wxString::Format("BUTTON %d", i + 1));
            buttons_[i] = button;
            button->Bind(wxEVT_RIGHT_UP, [this, i](wxMouseEvent&) { EditButton(i); });
            button->Bind(wxEVT_LEFT_DOWN,[this,i](wxMouseEvent& e){ engine_.button(i,true,Now()); e.Skip(); });
            button->Bind(wxEVT_LEFT_UP,[this,i](wxMouseEvent& e){ engine_.button(i,false,Now()); e.Skip(); });
            button->Bind(wxEVT_BUTTON, [this, i](wxCommandEvent&) {
                SetStatus(wxString::Format("BUTTON %d TRIGGERED",i+1));
            });
            buttons->Add(button, 1, i == 2 ? 0 : wxRIGHT, 8);
        }
        controlBox->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);
        controls->SetSizer(controlBox);
        content->Add(controls, 3, wxEXPAND | wxRIGHT, 14);

        auto* output = MakeSurface(this);
        auto* outputBox = new wxBoxSizer(wxVERTICAL);
        outputBox->Add(MakeLabel(output, "OUTPUT MONITOR", 13, true), 0, wxALL, 16);
        preview_ = new ColorPreview(output);
        preview_->Bind(wxEVT_RIGHT_UP, [this](wxMouseEvent&) { EditScreen(); });
        outputBox->Add(preview_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);
        outputValues_ = MakeLabel(output, "", 10);
        outputBox->Add(outputValues_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);
        outputBox->Add(MakeLabel(output, "ART-NET UDP", 12, true), 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
        outputBox->Add(MakeLabel(output, "Target IPv4", 9), 0, wxLEFT | wxRIGHT, 16);
        targetIp_ = new wxTextCtrl(output, wxID_ANY, "127.0.0.1");
        outputBox->Add(targetIp_, 0, wxEXPAND | wxALL, 12);
        outputBox->Add(MakeLabel(output, "Port-Address (0-32767)", 9), 0, wxLEFT | wxRIGHT, 16);
        universe_ = new wxSpinCtrl(output, wxID_ANY, "0", wxDefaultPosition,
                                   wxDefaultSize, wxSP_ARROW_KEYS, 0, 32767, 0);
        outputBox->Add(universe_, 0, wxEXPAND | wxALL, 12);
        auto* networkRow = new wxBoxSizer(wxHORIZONTAL);
        enable_ = new wxCheckBox(output, wxID_ANY, "Enable output");
        enable_->SetForegroundColour(kText);
        enable_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { SetNetworkEnabled(); });
        networkRow->Add(enable_, 1, wxALIGN_CENTER_VERTICAL);
        auto* send = MakeButton(output, "SEND");
        send->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SendOutput(); });
        networkRow->Add(send, 0);
        outputBox->Add(networkRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);
        broadcast_ = new wxCheckBox(output, wxID_ANY, "Broadcast (target address)");
        broadcast_->SetForegroundColour(kText);
        outputBox->Add(broadcast_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 16);
        outputBox->Add(MakeLabel(output, "Simulation mode: hardware serial is not connected.", 9),
                       0, wxLEFT | wxRIGHT | wxBOTTOM, 16);
        output->SetSizer(outputBox);
        content->Add(output, 2, wxEXPAND);
        outer->Add(content, 1, wxEXPAND | wxALL, 24);
        displayTimer_.SetOwner(this);
        Bind(wxEVT_TIMER, [this](wxTimerEvent&) { UpdateFourDigit(); }, displayTimer_.GetId());
        displayTimer_.Start(100);
        engine_.configure_fader(faderConfig_);
        for (int i=0;i<3;++i) engine_.configure_button(i,buttonConfigs_[i]);
        for (int i=0; i<4; ++i) engine_.set_encoder(i, encoderAssignments_[i], 0);
        cueTimer_.SetOwner(this);
        Bind(wxEVT_TIMER, [this](wxTimerEvent&) { SendOutput(); }, cueTimer_.GetId());
        cueTimer_.Start(40);
        SetSizer(outer);
        StateChanged();
        Centre();
    }

private:
    void EditEncoder(int i) {
        auto& label = profile_.encoder_labels[static_cast<std::size_t>(i)];
        wxTextEntryDialog name(this, "Encoder label", "Edit encoder", wxString::FromUTF8(label));
        if (name.ShowModal() != wxID_OK) return;
        const auto newLabel = name.GetValue().ToStdString();
        if (newLabel.empty()) return;
        const long channel = wxGetNumberFromUser("DMX channel (1-512)", "Channel", "Encoder assignment", profile_.dmx_channels[i], 1, 512, this);
        if (channel < 1) return;
        auto proposed = profile_;
        proposed.encoder_labels[i] = newLabel;
        proposed.dmx_channels[i] = static_cast<std::uint16_t>(channel);
        if (!brasscue::valid_profile(proposed)) {
            wxMessageBox("DMX channels must be unique and valid.", "Invalid assignment",
                         wxOK | wxICON_WARNING, this);
            return;
        }
        profile_ = proposed;
        EditAssignments(encoderAssignments_[i], "Encoder assignments");
        engine_.set_encoder(i, encoderAssignments_[i], state_.rgbw[i]);
        encoderLabels_[i]->SetLabel(wxString::FromUTF8(newLabel));
        StateChanged();
    }

    void EditMaster() {
        wxTextEntryDialog name(this, "Fader label", "Edit fader",
                               wxString::FromUTF8(profile_.fader_label));
        if (name.ShowModal() != wxID_OK || name.GetValue().empty()) return;
        profile_.fader_label = name.GetValue().ToStdString();
        masterLabel_->SetLabel(name.GetValue());
        wxArrayString choices;
        choices.Add("Grandmaster");
        choices.Add("DMX channel");
        wxSingleChoiceDialog mode(this, "Fader role", "Edit fader", choices);
        mode.SetSelection(profile_.fader_role == brasscue::FaderRole::Grandmaster ? 0 : 1);
        if (mode.ShowModal() != wxID_OK) return;
        if (mode.GetSelection() == 0) profile_.fader_role = brasscue::FaderRole::Grandmaster;
        else {
            const long channel = wxGetNumberFromUser("DMX channel (1-512)", "Channel", "Fader assignment", profile_.fader_dmx_channel, 1, 512, this);
            if (channel < 1) return;
            auto proposed = profile_;
            proposed.fader_role = brasscue::FaderRole::DmxChannel;
            proposed.fader_dmx_channel = static_cast<std::uint16_t>(channel);
            if (!brasscue::valid_profile(proposed)) {
                wxMessageBox("Channel conflicts with an encoder.", "Invalid assignment",
                             wxOK | wxICON_WARNING, this);
                return;
            }
            profile_ = proposed;
        }
        faderConfig_.grandmaster = profile_.fader_role == brasscue::FaderRole::Grandmaster;
        if (!faderConfig_.grandmaster) EditAssignments(faderConfig_.assignments, "Fader assignments");
        engine_.configure_fader(faderConfig_);
        StateChanged();
    }

    void EditButton(int i) {
        wxTextEntryDialog name(this, "Button label", "Edit button",
                               wxString::FromUTF8(profile_.button_labels[i]));
        if (name.ShowModal() != wxID_OK || name.GetValue().empty()) return;
        profile_.button_labels[i] = name.GetValue().ToStdString();
        buttons_[i]->SetLabel(name.GetValue());
        wxArrayString actions;
        actions.Add("Blackout"); actions.Add("Full"); actions.Add("Recall preset");
        actions.Add("Store preset"); actions.Add("Unassigned");
        wxSingleChoiceDialog action(this, "Button action", "Edit button", actions);
        action.SetSelection(static_cast<int>(profile_.button_actions[i]));
        if (action.ShowModal() == wxID_OK)
            profile_.button_actions[i] = static_cast<brasscue::ButtonAction>(action.GetSelection());
        EditAssignments(buttonConfigs_[i].assignments, "Button assignments");
        wxArrayString modes; modes.Add("Toggle"); modes.Add("Flash");
        wxSingleChoiceDialog mode(this, "Button mode", "Button editor", modes);
        mode.SetSelection(buttonConfigs_[i].mode == brasscue::ButtonMode::Toggle ? 0 : 1);
        if (mode.ShowModal() == wxID_OK) buttonConfigs_[i].mode = mode.GetSelection() == 0 ? brasscue::ButtonMode::Toggle : brasscue::ButtonMode::Flash;
        long fadeIn = wxGetNumberFromUser("Fade in (ms)", "ms", "Button editor", buttonConfigs_[i].fade_in_ms, 0, 60000, this);
        if (fadeIn >= 0) buttonConfigs_[i].fade_in_ms = static_cast<unsigned>(fadeIn);
        long fadeOut = wxGetNumberFromUser("Fade out (ms)", "ms", "Button editor", buttonConfigs_[i].fade_out_ms, 0, 60000, this);
        if (fadeOut >= 0) buttonConfigs_[i].fade_out_ms = static_cast<unsigned>(fadeOut);
        buttonConfigs_[i].kill = wxMessageBox("Enable Kill mode?", "Button editor", wxYES_NO | wxICON_QUESTION, this) == wxYES;
        engine_.configure_button(i, buttonConfigs_[i]);
    }

    void EditScreen() {
        wxTextEntryDialog title(this, "Screen title", "Edit screen",
                                wxString::FromUTF8(profile_.screen_title));
        if (title.ShowModal() != wxID_OK || title.GetValue().empty()) return;
        profile_.screen_title = title.GetValue().ToStdString();
        wxArrayString views;
        views.Add("Color mix"); views.Add("Levels");
        wxSingleChoiceDialog view(this, "Display mode", "Edit screen", views);
        view.SetSelection(static_cast<int>(profile_.screen_view));
        if (view.ShowModal() == wxID_OK)
            profile_.screen_view = static_cast<brasscue::ScreenView>(view.GetSelection());
        SetStatus("SCREEN PROFILE UPDATED");
    }

    bool EditAssignments(std::vector<brasscue::Assignment>& entries, const wxString& title) {
        wxString initial;
        for (const auto& a : entries)
            initial += wxString::Format("%u.%u = %u\n", unsigned(a.address.universe), unsigned(a.address.channel), unsigned(a.value));
        wxTextEntryDialog dlg(this, "One assignment per line: universe.channel = value (0-255)", title, initial, wxOK | wxCANCEL | wxTE_MULTILINE);
        if (dlg.ShowModal() != wxID_OK) return false;
        std::vector<brasscue::Assignment> proposed;
        wxStringTokenizer lines(dlg.GetValue(), "\n");
        while (lines.HasMoreTokens()) {
            wxString line = lines.GetNextToken().Trim(true).Trim(false);
            if (line.empty()) continue;
            unsigned uni=0, channel=0, value=0;
            char trailing=0;
            if (std::sscanf(line.ToStdString().c_str(), "%u.%u = %u %c", &uni, &channel, &value, &trailing) != 3 || uni<1 || uni>32768 || channel<1 || channel>512 || value>255) {
                wxMessageBox("Invalid assignment: " + line, title, wxOK | wxICON_ERROR, this);
                return false;
            }
            proposed.push_back({{static_cast<std::uint16_t>(uni),static_cast<std::uint16_t>(channel)},static_cast<std::uint8_t>(value)});
        }
        try { brasscue::validate(proposed); }
        catch (const std::exception& e) { wxMessageBox(wxString::FromUTF8(e.what()), title, wxOK | wxICON_ERROR, this); return false; }
        entries = std::move(proposed);
        return true;
    }
    void Nudge(int channel, int change) {
        const int current = state_.rgbw[static_cast<std::size_t>(channel)];
        const auto value = static_cast<std::uint8_t>((std::max)(0, (std::min)(255, current + change)));
        state_.rgbw[static_cast<std::size_t>(channel)] = value;
        display_.encoder_changed(channel, value, Now());
        sliders_[channel]->SetValue(value);
        engine_.set_encoder(channel, encoderAssignments_[channel], value);
        StateChanged();
    }

    static std::uint64_t Now() {
        using namespace std::chrono;
        return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }
    void UpdateFourDigit() {
        masterValue_->SetLabel(wxString::FromUTF8(display_.text(Now())));
    }
    void SetStatus(const wxString& message) { status_->SetLabel(message); }

    void StateChanged() {
        for (std::size_t i = 0; i < 4; ++i) {
            const auto value = state_.rgbw[i];
            values_[i]->SetLabel(wxString::Format("%u%%", brasscue::to_percent(value)));
        }
        display_.set_master(state_.master);
        UpdateFourDigit();
        outputValues_->SetLabel(wxString::Format("DMX 1-4: %u  %u  %u  %u",
            unsigned(brasscue::apply_master(state_.rgbw[0], state_.master)),
            unsigned(brasscue::apply_master(state_.rgbw[1], state_.master)),
            unsigned(brasscue::apply_master(state_.rgbw[2], state_.master)),
            unsigned(brasscue::apply_master(state_.rgbw[3], state_.master))));
        preview_->SetState(state_);
        if (udp_) SendOutput();
    }

    void SetNetworkEnabled() {
        if (!enable_->IsChecked()) {
            udp_.reset();
            SetStatus("SIMULATION  |  ART-NET OFF");
            return;
        }
        try {
            udp_ = std::make_unique<brasscue::UdpTarget>(targetIp_->GetValue().ToStdString(), broadcast_->GetValue());
            SetStatus("SIMULATION  |  ART-NET ENABLED");
            SendOutput();
        } catch (const std::exception& e) {
            enable_->SetValue(false);
            udp_.reset();
            wxMessageBox(wxString::FromUTF8(e.what()), "Art-Net target error",
                         wxOK | wxICON_ERROR, this);
        }
    }

    void SendOutput() {
        if (!udp_) return;
        try {
            const auto frames = engine_.render(Now());
            for (const auto& frame : frames) udp_->send(engine_.packet(frame.first, Now()));
        } catch (const std::exception& e) {
            udp_.reset();
            enable_->SetValue(false);
            SetStatus("SIMULATION  |  ART-NET ERROR");
            wxMessageBox(wxString::FromUTF8(e.what()), "Art-Net send error",
                         wxOK | wxICON_ERROR, this);
        }
    }

    brasscue::ControlProfile profile_;
    std::array<wxStaticText*, 4> encoderLabels_{};
    std::array<wxButton*, 3> buttons_{};
    wxStaticText* masterLabel_ = nullptr;
    brasscue::State state_;
    brasscue::FourDigitDisplay display_;
    brasscue::CueEngine engine_;
    std::array<brasscue::ButtonConfig,3> buttonConfigs_{};
    brasscue::FaderConfig faderConfig_;
    std::array<std::vector<brasscue::Assignment>,4> encoderAssignments_{{
        {{{1,1},255}}, {{{1,2},255}}, {{{1,3},255}}, {{{1,4},255}}
    }};
    wxTimer cueTimer_;
    wxCheckBox* broadcast_ = nullptr;
    wxTimer displayTimer_;
    std::array<wxSlider*, 4> sliders_{};
    std::array<wxStaticText*, 4> values_{};
    wxSlider* grandmaster_ = nullptr;
    wxStaticText* masterValue_ = nullptr;
    wxStaticText* outputValues_ = nullptr;
    wxStaticText* status_ = nullptr;
    ColorPreview* preview_ = nullptr;
    wxTextCtrl* targetIp_ = nullptr;
    wxSpinCtrl* universe_ = nullptr;
    wxCheckBox* enable_ = nullptr;
    std::unique_ptr<brasscue::UdpTarget> udp_;
};

} // namespace

wxFrame* CreateBrassCueFrame() { return new MainFrame(); }
