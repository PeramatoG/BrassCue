// BrassCue - GPL-3.0. One modal transaction with a multi-universe assignment table.
#include "profile_editor.hpp"
#include "../src/assignment_editor.hpp"
#include <vector>
#include <wx/choice.h>
#include <wx/grid.h>
#include <wx/spinctrl.h>
namespace {
class ElementDialog final : public wxDialog {
  public:
    ElementDialog(wxWindow *parent, const brasscue::ControlProfile &original, int element)
        : wxDialog(parent, wxID_ANY, "Edit control", wxDefaultPosition, wxSize(580, 620),
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
          profile_(original), element_(element) {
        using namespace brasscue;
        auto *box = new wxBoxSizer(wxVERTICAL);
        const auto &label = element < 4    ? profile_.encoder_labels[element]
                            : element == 4 ? profile_.fader_label
                            : element < 8  ? profile_.button_labels[element - 5]
                                           : profile_.screen_title;
        box->Add(new wxStaticText(this, wxID_ANY, "Label / title"), 0, wxALL, 8);
        label_ = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(label));
        box->Add(label_, 0, wxEXPAND | wxALL, 8);
        mode_ = new wxChoice(this, wxID_ANY);
        if (element == 4) {
            mode_->Append("Grandmaster");
            mode_->Append("Patch assignments");
            mode_->SetSelection(profile_.fader.grandmaster ? 0 : 1);
        } else if (element >= 5 && element < 8) {
            mode_->Append("Toggle");
            mode_->Append("Flash");
            mode_->SetSelection(int(profile_.buttons[element - 5].mode));
        } else if (element == 8) {
            mode_->Append("Color");
            mode_->Append("Levels");
            mode_->SetSelection(int(profile_.screen_view));
        } else {
            mode_->Append("Target scaled by encoder position");
            mode_->SetSelection(0);
            mode_->Disable();
        }
        box->Add(mode_, 0, wxEXPAND | wxALL, 8);
        if (element >= 5 && element < 8) {
            const auto &b = profile_.buttons[element - 5];
            kill_ = new wxCheckBox(this, wxID_ANY, "Kill other room buttons");
            kill_->SetValue(b.kill);
            box->Add(kill_, 0, wxALL, 8);
            for (auto pair : {std::make_pair("Fade in (ms)", b.fade_in_ms),
                              std::make_pair("Fade out (ms)", b.fade_out_ms)}) {
                box->Add(new wxStaticText(this, wxID_ANY, pair.first), 0, wxLEFT, 8);
                auto *spin = new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
                                            wxSP_ARROW_KEYS, 0, 60000, pair.second);
                box->Add(spin, 0, wxEXPAND | wxALL, 8);
                fades_.push_back(spin);
            }
        }
        if (element != 8) {
            entries_ = element < 4    ? profile_.encoders[element]
                       : element == 4 ? profile_.fader.assignments
                                      : profile_.buttons[element - 5].assignments;
            units_ = new wxChoice(this, wxID_ANY);
            units_->Append("DMX 0..255");
            units_->Append("Percent 0..100");
            units_->SetSelection(int(profile_.units));
            box->Add(units_, 0, wxEXPAND | wxALL, 8);
            grid_ = new wxGrid(this, wxID_ANY);
            grid_->CreateGrid(0, 3);
            grid_->SetColLabelValue(0, "Universe (1..32768)");
            grid_->SetColLabelValue(1, "Channel (1..512)");
            grid_->SetColLabelValue(2, "Target");
            grid_->SetColSize(0, 170);
            grid_->SetColSize(1, 160);
            box->Add(grid_, 1, wxEXPAND | wxALL, 8);
            Fill();
            grid_->Bind(wxEVT_GRID_CELL_CHANGED, [this](wxGridEvent &e) {
                if (e.GetCol() == 2)
                    dirty_rows_.push_back(e.GetRow());
                e.Skip();
            });
            units_->Bind(wxEVT_CHOICE, [this](wxCommandEvent &) {
                try {
                    Read();
                    percent_ = units_->GetSelection() == 1;
                    Fill();
                } catch (const std::exception &e) {
                    units_->SetSelection(percent_ ? 1 : 0);
                    Error(e.what());
                }
            });
            percent_ = int(profile_.units) == 1;
            Fill();
            auto *row = new wxBoxSizer(wxHORIZONTAL);
            auto *add = new wxButton(this, wxID_ADD, "Add");
            auto *remove = new wxButton(this, wxID_REMOVE, "Remove selected");
            row->Add(add);
            row->Add(remove);
            add->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
                try {
                    Read();
                    entries_.push_back({{1, 1}, 255});
                    Fill();
                } catch (const std::exception &e) {
                    Error(e.what());
                }
            });
            remove->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
                int row = grid_->GetGridCursorRow();
                if (row >= 0 && row < grid_->GetNumberRows()) {
                    grid_->DeleteRows(row);
                    entries_.erase(entries_.begin() + row);
                    dirty_rows_.clear();
                    for (int i = 0; i < grid_->GetNumberRows(); ++i)
                        dirty_rows_.push_back(i);
                }
            });
            box->Add(row, 0, wxALL, 8);
        }
        box->Add(CreateSeparatedButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 8);
        SetSizer(box);
        Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent &) {
                try {
                    Commit();
                    EndModal(wxID_OK);
                } catch (const std::exception &e) {
                    Error(e.what());
                }
            },
            wxID_OK);
    }
    brasscue::ControlProfile profile_;

  private:
    void Error(const char *text) {
        wxMessageBox(wxString::FromUTF8(text), "Invalid control", wxOK | wxICON_ERROR, this);
    }
    void Fill() {
        if (grid_->GetNumberRows())
            grid_->DeleteRows(0, grid_->GetNumberRows());
        if (!entries_.empty())
            grid_->AppendRows(entries_.size());
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            const auto &a = entries_[i];
            grid_->SetCellValue(i, 0, wxString::Format("%u", unsigned(a.address.universe)));
            grid_->SetCellValue(i, 1, wxString::Format("%u", unsigned(a.address.channel)));
            grid_->SetCellValue(i, 2,
                                wxString::Format("%u", percent_ ? brasscue::to_percent(a.value)
                                                                : unsigned(a.value)));
        }
        dirty_rows_.clear();
    }
    void Read() {
        grid_->SaveEditControlValue();
        grid_->HideCellEditControl();
        auto candidate = entries_;
        for (int row = 0; row < grid_->GetNumberRows(); ++row) {
            try {
                candidate[row] = brasscue::edit_assignment_row(
                    entries_[row], grid_->GetCellValue(row, 0).ToStdString(),
                    grid_->GetCellValue(row, 1).ToStdString(),
                    grid_->GetCellValue(row, 2).ToStdString(),
                    percent_ ? brasscue::ValueDisplay::Percent : brasscue::ValueDisplay::Dmx255,
                    std::find(dirty_rows_.begin(), dirty_rows_.end(), row) != dirty_rows_.end());
            } catch (const std::exception &e) {
                throw std::invalid_argument("Row " + std::to_string(row + 1) + ": " + e.what());
            }
        }
        brasscue::validate(candidate);
        entries_ = std::move(candidate);
    }
    void Commit() {
        using namespace brasscue;
        const std::string label = label_->GetValue().ToStdString(wxConvUTF8);
        if (element_ < 4) {
            Read();
            profile_.encoder_labels[element_] = label;
            profile_.encoders[element_] = entries_;
        } else if (element_ == 4) {
            Read();
            profile_.fader_label = label;
            profile_.fader.grandmaster = mode_->GetSelection() == 0;
            profile_.fader.assignments = entries_;
        } else if (element_ < 8) {
            Read();
            int i = element_ - 5;
            profile_.button_labels[i] = label;
            auto &b = profile_.buttons[i];
            b.assignments = entries_;
            b.mode = ButtonMode(mode_->GetSelection());
            b.kill = kill_->GetValue();
            b.fade_in_ms = fades_[0]->GetValue();
            b.fade_out_ms = fades_[1]->GetValue();
        } else {
            profile_.screen_title = label;
            profile_.screen_view = ScreenView(mode_->GetSelection());
        }
        if (units_)
            profile_.units = ValueDisplay(units_->GetSelection());
        validate(profile_);
    }
    int element_;
    wxTextCtrl *label_ = nullptr;
    wxChoice *mode_ = nullptr;
    wxChoice *units_ = nullptr;
    wxCheckBox *kill_ = nullptr;
    wxGrid *grid_ = nullptr;
    std::vector<wxSpinCtrl *> fades_;
    std::vector<brasscue::Assignment> entries_;
    std::vector<int> dirty_rows_;
    bool percent_ = false;
};
} // namespace
bool EditProfileElement(wxWindow *parent, brasscue::ControlProfile &candidate, int element) {
    ElementDialog dialog(parent, candidate, element);
    if (dialog.ShowModal() != wxID_OK)
        return false;
    candidate = std::move(dialog.profile_);
    return true;
}
