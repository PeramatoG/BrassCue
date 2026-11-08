// BrassCue - GPL-3.0. Internal commands and DMX assignments remain explicitly different actions.
#include "button_editor.hpp"
#include "assignment_table.hpp"
#include "dark_theme.hpp"
#include "editor_buttons.hpp"
#include "fade_field.hpp"
#include <cmath>
#include <wx/spinctrl.h>
namespace {
class ButtonDialog final : public wxDialog {
  public:
    enum { ActionId = wxID_HIGHEST + 400, ModeId, KillId, FadeInId, FadeOutId, LabelId };
    ButtonDialog(wxWindow *parent, const brasscue::ControlProfile &p, std::size_t index)
        : wxDialog(parent, wxID_ANY, "Edit button", wxDefaultPosition, wxSize(640, 740),
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
          profile(p), index_(index) {
        const auto &b = p.buttons.at(index);
        auto *root = new wxBoxSizer(wxVERTICAL);
        root->Add(new wxStaticText(this, wxID_ANY, "Button label"), 0, wxALL, 6);
        label_ = new wxTextCtrl(this, LabelId, wxString::FromUTF8(p.button_labels[index]),
                                wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
        // Enter belongs to this field, never the dialog default button.
        label_->Bind(wxEVT_TEXT_ENTER, [](wxCommandEvent &) {});
        root->Add(label_, 0, wxEXPAND | wxALL, 6);
        action_ = new wxChoice(this, ActionId);
        action_->Append("DMX assignments");
        action_->Append("Internal controls / preset");
        action_->SetSelection(int(b.action));
        root->Add(action_, 0, wxEXPAND | wxALL, 6);
        root->Add(new wxStaticText(
                      this, wxID_ANY,
                      "Internal presets change control levels; DMX cues write patched slots."),
                  0, wxALL, 6);
        auto *options = new wxBoxSizer(wxHORIZONTAL);
        mode_ = new wxChoice(this, ModeId);
        mode_->Append("Toggle");
        mode_->Append("Flash");
        mode_->SetSelection(int(b.mode));
        options->Add(mode_, 1, wxRIGHT, 6);
        kill_ = new wxCheckBox(this, KillId, "Kill other buttons");
        kill_->SetValue(b.kill);
        options->Add(kill_, 1, wxALIGN_CENTER_VERTICAL);
        root->Add(options, 0, wxEXPAND | wxALL, 6);
        auto *timings = new wxFlexGridSizer(2, 2, 6, 6);
        timings->Add(new wxStaticText(this, wxID_ANY, "Fade in (s)"));
        in_ = new wxSpinCtrlDouble(this, FadeInId, "", wxDefaultPosition, wxDefaultSize,
                                   wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER, 0, 60,
                                   b.fade_in_ms / 1000.0, 0.1);
        in_->SetDigits(1);
        initial_in_ = in_->GetValue();
        timings->Add(in_);
        timings->Add(new wxStaticText(this, wxID_ANY, "Fade out (s)"));
        out_ = new wxSpinCtrlDouble(this, FadeOutId, "", wxDefaultPosition, wxDefaultSize,
                                    wxSP_ARROW_KEYS | wxTE_PROCESS_ENTER, 0, 60,
                                    b.fade_out_ms / 1000.0, 0.1);
        out_->SetDigits(1);
        initial_out_ = out_->GetValue();
        for (auto *field : {in_, out_})
            field->Bind(wxEVT_TEXT_ENTER, [this, field](wxCommandEvent &) {
                try {
                    // On Windows GetValue() can still hold the pre-edit number until focus loss.
                    field->SetValue(ReadFadeSeconds(*field));
                } catch (const std::exception &e) {
                    Error(e);
                }
            });
        timings->Add(out_);
        root->Add(timings, 0, wxALL, 6);
        table_ = new AssignmentTable(this, b.assignments, b.controls, p.units,
                                     b.action == brasscue::ButtonActionKind::SetControlValues);
        root->Add(table_, 1, wxEXPAND | wxALL, 6);
        root->Add(new wxStaticText(this, wxID_ANY,
                                   "Manual control takes over. Internal Flash restores its "
                                   "previous values; Toggle-off targets zero."),
                  0, wxALL, 6);
        root->Add(EditorButtons(*this), 0, wxEXPAND | wxALL, 6);
        SetSizer(root);
        ApplyDarkTheme(*this);
        action_->Bind(wxEVT_CHOICE, [this](wxCommandEvent &) {
            try {
                table_->SetInternal(action_->GetSelection() == 1);
            } catch (const std::exception &e) {
                action_->SetSelection(table_->Internal() ? 1 : 0);
                Error(e);
            }
        });
        Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent &) {
                try {
                    Commit();
                    EndModal(wxID_OK);
                } catch (const std::exception &e) {
                    Error(e);
                }
            },
            wxID_OK);
    }
    brasscue::ControlProfile profile;

  private:
    void Error(const std::exception &e) {
        wxMessageBox(wxString::FromUTF8(e.what()), "Invalid button", wxOK | wxICON_ERROR, this);
    }
    void Commit() {
        table_->Read();
        auto candidate = profile;
        auto &b = candidate.buttons[index_];
        candidate.button_labels[index_] = label_->GetValue().ToStdString(wxConvUTF8);
        candidate.units = table_->Units();
        b.action = static_cast<brasscue::ButtonActionKind>(action_->GetSelection());
        b.mode = static_cast<brasscue::ButtonMode>(mode_->GetSelection());
        b.kill = kill_->GetValue();
        // Keep legacy millisecond precision unless the displayed duration changes.
        const auto milliseconds = [](wxSpinCtrlDouble *control, unsigned original, double initial) {
            const auto value = ReadFadeSeconds(*control);
            const auto tenths = std::lround(value * 10.0);
            return value == initial ? original : unsigned(tenths * 100);
        };
        b.fade_in_ms = milliseconds(in_, b.fade_in_ms, initial_in_);
        b.fade_out_ms = milliseconds(out_, b.fade_out_ms, initial_out_);
        b.assignments = table_->Internal() ? std::vector<brasscue::Assignment>{} : table_->Dmx();
        b.controls = table_->Internal() ? table_->Controls()
                                        : std::vector<brasscue::ControlValueAssignment>{};
        brasscue::validate(candidate);
        profile = std::move(candidate);
    }
    std::size_t index_;
    wxTextCtrl *label_;
    wxChoice *action_;
    wxChoice *mode_;
    wxCheckBox *kill_;
    wxSpinCtrlDouble *in_;
    wxSpinCtrlDouble *out_;
    AssignmentTable *table_;
    double initial_in_ = 0, initial_out_ = 0;
};
} // namespace
bool EditButtonProfile(wxWindow *parent, brasscue::ControlProfile &profile, std::size_t i) {
    ButtonDialog dialog(parent, profile, i);
    if (dialog.ShowModal() != wxID_OK)
        return false;
    profile = std::move(dialog.profile);
    return true;
}
