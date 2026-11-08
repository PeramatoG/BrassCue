// BrassCue - GPL-3.0. Update only changed controls; no timer-driven button relabeling.
#include "native_panel.hpp"
#include "dark_theme.hpp"
namespace {
enum ControlId {
    EncoderBase = wxID_HIGHEST + 100,
    FaderId = EncoderBase + 4,
    CueBase = FaderId + 1,
    ScreenId = CueBase + 3
};
}
NativePanel::NativePanel(wxWindow *parent, brasscue::ControllerActions actions)
    : wxPanel(parent), actions_(std::move(actions)) {
    auto *root = new wxBoxSizer(wxHORIZONTAL);
    auto *left = new wxBoxSizer(wxVERTICAL);
    fader_label_ = new wxStaticText(this, wxID_ANY, "");
    left->Add(fader_label_, 0, wxALL, 8);
    percent_ = new wxStaticText(this, wxID_ANY, "");
    left->Add(percent_, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 4);
    fader_ = new wxSlider(this, FaderId, 255, 0, 255, wxDefaultPosition, FromDIP(wxSize(80, 360)),
                          wxSL_VERTICAL | wxSL_INVERSE);
    fader_->Bind(wxEVT_SLIDER,
                 [this](wxCommandEvent &e) { actions_.fader(std::uint8_t(e.GetInt())); });
    left->Add(fader_, 1, wxALIGN_CENTER_HORIZONTAL | wxALL, 8);
    digits_ = new wxStaticText(this, wxID_ANY, "");
    digits_->SetFont(wxFont(26, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
    left->Add(digits_, 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 12);
    BindEdit(fader_, 4);
    BindEdit(fader_label_, 4);
    BindEdit(percent_, 4);
    BindEdit(digits_, 4);
    root->Add(left, 1, wxEXPAND | wxALL, 8);
    auto *middle = new wxBoxSizer(wxVERTICAL);
    for (unsigned i = 0; i < 4; ++i) {
        labels_[i] = new wxStaticText(this, wxID_ANY, "");
        middle->Add(labels_[i], 0, wxALIGN_CENTER_HORIZONTAL | wxALL, 4);
        knobs_[i] =
            new EncoderKnob(this, [this, i](int delta) { actions_.encoder_delta(i, delta); });
        knobs_[i]->SetId(EncoderBase + i);
        middle->Add(knobs_[i], 1, wxEXPAND | wxALL, 4);
        BindEdit(knobs_[i], i);
        BindEdit(labels_[i], i);
    }
    root->Add(middle, 1, wxEXPAND | wxALL, 8);
    auto *right = new wxBoxSizer(wxVERTICAL);
    screen_ = new ColorPreview(this);
    screen_->SetId(ScreenId);
    BindEdit(screen_, 8);
    right->Add(screen_, 2, wxEXPAND | wxBOTTOM, 12);
    for (unsigned i = 0; i < 3; ++i) {
        buttons_[i] = new CueButton(this, [this, i](bool down) { actions_.button(i, down); });
        buttons_[i]->SetId(CueBase + i);
        BindEdit(buttons_[i], i + 5);
        right->Add(buttons_[i], 1, wxEXPAND | wxTOP | wxBOTTOM, 8);
    }
    root->Add(right, 1, wxEXPAND | wxALL, 16);
    SetSizer(root);
    ApplyDarkTheme(*this);
    digits_->SetForegroundColour(wxColour(255, 184, 36));
}
void NativePanel::BindEdit(wxWindow *w, int element) {
    w->Bind(wxEVT_RIGHT_UP, [this, element](wxMouseEvent &) {
        ReleaseInputs();
        actions_.edit(element);
    });
    w->Bind(wxEVT_CONTEXT_MENU, [this, element](wxContextMenuEvent &e) {
        if (e.GetPosition() == wxDefaultPosition) {
            ReleaseInputs();
            actions_.edit(element);
        }
    });
}
void NativePanel::Apply(const brasscue::ViewSnapshot &v) {
    auto changes = brasscue::view_changes(previous_ ? &*previous_ : nullptr, v);
    if (!changes.any())
        return;
    if (changes.fader) {
        if (fader_->GetValue() != v.fader)
            fader_->SetValue(v.fader);
        if (fader_label_->GetLabel() != wxString::FromUTF8(v.fader_label))
            fader_label_->SetLabel(wxString::FromUTF8(v.fader_label));
        percent_->SetLabel(wxString::Format("%u%%", v.fader_percent));
    }
    for (unsigned i = 0; i < 4; ++i)
        if (changes.encoders[i]) {
            knobs_[i]->SetValue(v.encoders[i].value);
            labels_[i]->SetLabel(wxString::FromUTF8(v.encoders[i].label) +
                                 wxString::Format("  %u%%", v.encoders[i].percent));
            knobs_[i]->SetName(labels_[i]->GetLabel() +
                               "; arrows adjust, Page Up/Down adjust ten steps");
        }
    for (unsigned i = 0; i < 3; ++i)
        if (changes.buttons[i])
            buttons_[i]->SetView(v.buttons[i]);
    if (changes.display)
        digits_->SetLabel(wxString::FromUTF8(v.display));
    if (changes.screen)
        screen_->SetView(v.screen);
    const bool labels_changed = !previous_ || previous_->fader_label != v.fader_label;
    previous_ = v;
    if (labels_changed)
        Layout();
}
void NativePanel::ReleaseInputs() {
    for (auto *k : knobs_)
        k->Release();
    for (auto *b : buttons_)
        b->Release();
    if (actions_.release)
        actions_.release();
}
void NativePanel::CheckPointer() {
    for (auto *k : knobs_)
        k->CheckPointer();
    for (auto *b : buttons_)
        b->CheckPointer();
}
