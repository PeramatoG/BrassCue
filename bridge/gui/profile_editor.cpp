// BrassCue - GPL-3.0. Contextual encoder/fader/screen editor; application state stays untouched.
#include "profile_editor.hpp"
#include "assignment_table.hpp"
#include "button_editor.hpp"
#include "dark_theme.hpp"
#include "editor_buttons.hpp"
#include "encoder_group_routes.hpp"
namespace {
class ElementDialog final : public wxDialog {
  public:
    ElementDialog(wxWindow *parent, const brasscue::ControlProfile &p, int element)
        : wxDialog(parent, wxID_ANY, "Edit control", wxDefaultPosition, wxSize(600, 620),
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
          profile(p), element_(element) {
        auto *root = new wxBoxSizer(wxVERTICAL);
        auto text = element < 4    ? p.encoder_labels[element]
                    : element == 4 ? p.fader_label
                                   : p.screen_title;
        root->Add(new wxStaticText(this, wxID_ANY, "Label / title"), 0, wxALL, 6);
        label_ = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(text), wxDefaultPosition,
                                wxDefaultSize, wxTE_PROCESS_ENTER);
        // Enter belongs to this field, never the dialog default button.
        label_->Bind(wxEVT_TEXT_ENTER, [](wxCommandEvent &) {});
        root->Add(label_, 0, wxEXPAND | wxALL, 6);
        mode_ = new wxChoice(this, wxID_ANY);
        if (element == 4) {
            mode_->Append("Grandmaster");
            mode_->Append("Patch assignments");
            mode_->SetSelection(p.fader.grandmaster ? 0 : 1);
        } else if (element == 8) {
            mode_->Append("Color");
            mode_->Append("Levels");
            mode_->SetSelection(int(p.screen_view));
        } else {
            mode_->Append("Target scaled by encoder position");
            mode_->SetSelection(0);
            mode_->Disable();
        }
        root->Add(mode_, 0, wxEXPAND | wxALL, 6);
        if (element < 4 && !p.fixture_groups.empty()) {
            root->Add(CreateEncoderGroupRoutes(this, p, unsigned(element)), 1, wxEXPAND | wxALL, 6);
            root->Add(new wxStaticText(this, wxID_ANY, "Additional manual assignments (editable)"),
                      0, wxLEFT | wxRIGHT | wxTOP, 6);
            SetMinClientSize(FromDIP(wxSize(680, 700)));
            SetClientSize(FromDIP(wxSize(720, 780)));
        }
        if (element != 8) {
            table_ = new AssignmentTable(
                this, element < 4 ? p.encoders[element] : p.fader.assignments, {}, p.units);
            root->Add(table_, 1, wxEXPAND | wxALL, 6);
        }
        root->Add(EditorButtons(*this), 0, wxEXPAND | wxALL, 6);
        SetSizer(root);
        ApplyDarkTheme(*this);
        Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent &) {
                try {
                    Commit();
                    EndModal(wxID_OK);
                } catch (const std::exception &e) {
                    wxMessageBox(wxString::FromUTF8(e.what()), "Invalid control",
                                 wxOK | wxICON_ERROR, this);
                }
            },
            wxID_OK);
    }
    brasscue::ControlProfile profile;

  private:
    void Commit() {
        auto candidate = profile;
        auto label = label_->GetValue().ToStdString(wxConvUTF8);
        if (table_) {
            table_->Read();
            candidate.units = table_->Units();
        }
        if (element_ < 4) {
            candidate.encoder_labels[element_] = label;
            candidate.encoders[element_] = table_->Dmx();
        } else if (element_ == 4) {
            candidate.fader_label = label;
            candidate.fader.grandmaster = mode_->GetSelection() == 0;
            candidate.fader.assignments = table_->Dmx();
        } else {
            candidate.screen_title = label;
            candidate.screen_view = static_cast<brasscue::ScreenView>(mode_->GetSelection());
        }
        brasscue::validate(candidate);
        profile = std::move(candidate);
    }
    int element_;
    wxTextCtrl *label_;
    wxChoice *mode_;
    AssignmentTable *table_ = nullptr;
};
} // namespace
bool EditProfileElement(wxWindow *parent, brasscue::ControlProfile &candidate, int element) {
    if (element < 0 || element > 8)
        throw std::invalid_argument("Invalid control editor index");
    if (element >= 5 && element < 8)
        return EditButtonProfile(parent, candidate, element - 5);
    ElementDialog dialog(parent, candidate, element);
    if (dialog.ShowModal() != wxID_OK)
        return false;
    candidate = std::move(dialog.profile);
    return true;
}
