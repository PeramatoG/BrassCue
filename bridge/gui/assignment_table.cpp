// BrassCue - GPL-3.0. Display units never quantize unchanged stored bytes.
#include "assignment_table.hpp"
#include <algorithm>
#include <array>
namespace {
const wxString targets[]{"Grandmaster", "Red", "Green", "Blue", "White"};
}
AssignmentTable::AssignmentTable(wxWindow *parent, std::vector<brasscue::Assignment> dmx,
                                 std::vector<brasscue::ControlValueAssignment> controls,
                                 brasscue::ValueDisplay units, bool internal)
    : wxPanel(parent), dmx_(std::move(dmx)), controls_(std::move(controls)),
      percent_(units == brasscue::ValueDisplay::Percent), internal_(internal) {
    auto *root = new wxBoxSizer(wxVERTICAL);
    units_ = new wxChoice(this, UnitsId);
    units_->Append("DMX 0..255");
    units_->Append("Percent 0..100");
    units_->SetSelection(percent_ ? 1 : 0);
    root->Add(units_, 0, wxEXPAND | wxALL, 6);
    grid_ = new wxGrid(this, GridId);
    grid_->CreateGrid(0, 3);
    root->Add(grid_, 1, wxEXPAND | wxALL, 6);
    auto *row = new wxBoxSizer(wxHORIZONTAL);
    auto *add = new wxButton(this, AddId, "Add");
    auto *remove = new wxButton(this, RemoveId, "Remove selected");
    rgbw_ = new wxButton(this, AddRgbwId, "Add all RGBW");
    row->Add(add);
    row->Add(remove);
    row->Add(rgbw_);
    root->Add(row, 0, wxALL, 6);
    SetSizer(root);
    Fill();
    grid_->Bind(wxEVT_GRID_CELL_CHANGED, [this](wxGridEvent &e) {
        if (e.GetCol() == (internal_ ? 1 : 2) && e.GetRow() >= 0 &&
            std::size_t(e.GetRow()) < edited_.size())
            edited_[e.GetRow()] = true;
        e.Skip();
    });
    units_->Bind(wxEVT_CHOICE, [this](wxCommandEvent &) {
        try {
            Read();
            percent_ = units_->GetSelection() == 1;
            Fill();
        } catch (const std::exception &e) {
            units_->SetSelection(percent_ ? 1 : 0);
            Error(e);
        }
    });
    add->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        try {
            Read();
            Add(false);
            Fill();
        } catch (const std::exception &e) {
            Error(e);
        }
    });
    rgbw_->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
        try {
            Read();
            Add(true);
            Fill();
        } catch (const std::exception &e) {
            Error(e);
        }
    });
    remove->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) { Remove(); });
}
void AssignmentTable::Error(const std::exception &e) {
    wxMessageBox(wxString::FromUTF8(e.what()), "Invalid assignment", wxOK | wxICON_ERROR, this);
}
void AssignmentTable::Fill() {
    if (grid_->GetNumberRows())
        grid_->DeleteRows(0, grid_->GetNumberRows());
    const auto count = internal_ ? controls_.size() : dmx_.size();
    if (count)
        grid_->AppendRows(count);
    grid_->SetColLabelValue(0, internal_ ? "Logical control" : "Universe (1..32768)");
    grid_->SetColLabelValue(1, internal_ ? "Target" : "Channel (1..512)");
    grid_->SetColLabelValue(2, "Target");
    grid_->SetColSize(0, 185);
    grid_->SetColSize(1, 155);
    if (internal_)
        grid_->HideCol(2);
    else
        grid_->ShowCol(2);
    for (std::size_t i = 0; i < count; ++i) {
        const auto value = internal_ ? controls_[i].value : dmx_[i].value;
        if (internal_) {
            grid_->SetCellValue(i, 0, targets[unsigned(controls_[i].target)]);
            grid_->SetCellEditor(i, 0, new wxGridCellChoiceEditor(5, targets, false));
        } else {
            grid_->SetCellValue(i, 0, wxString::Format("%u", unsigned(dmx_[i].address.universe)));
            grid_->SetCellValue(i, 1, wxString::Format("%u", unsigned(dmx_[i].address.channel)));
        }
        grid_->SetCellValue(
            i, internal_ ? 1 : 2,
            wxString::Format("%u", percent_ ? brasscue::to_percent(value) : unsigned(value)));
    }
    edited_.assign(count, false);
    rgbw_->Show(internal_);
    Layout();
}
void AssignmentTable::Read() {
    grid_->SaveEditControlValue();
    grid_->HideCellEditControl();
    auto dmx = dmx_;
    auto controls = controls_;
    for (int row = 0; row < grid_->GetNumberRows(); ++row)
        try {
            const auto unit = Units();
            if (internal_) {
                unsigned target = 0;
                while (target < 5 && grid_->GetCellValue(row, 0) != targets[target])
                    ++target;
                if (target >= 5)
                    throw std::invalid_argument("Invalid logical control target");
                auto value = brasscue::assignment_number(grid_->GetCellValue(row, 1).ToStdString(),
                                                         percent_ ? 100 : 255);
                controls[row].target = static_cast<brasscue::ControlTarget>(target);
                if (edited_[row])
                    controls[row].value =
                        percent_ ? brasscue::from_percent(value) : std::uint8_t(value);
            } else
                dmx[row] = brasscue::edit_assignment_row(
                    dmx_[row], grid_->GetCellValue(row, 0).ToStdString(),
                    grid_->GetCellValue(row, 1).ToStdString(),
                    grid_->GetCellValue(row, 2).ToStdString(), unit, edited_[row]);
        } catch (const std::exception &e) {
            throw std::invalid_argument("Row " + std::to_string(row + 1) + ": " + e.what());
        }
    if (internal_)
        brasscue::validate_controls(controls);
    else
        brasscue::validate(dmx);
    dmx_ = std::move(dmx);
    controls_ = std::move(controls);
}
void AssignmentTable::SetInternal(bool internal) {
    if (internal_ == internal)
        return;
    Read();
    internal_ = internal;
    if (internal_ && controls_.empty())
        controls_.push_back({brasscue::ControlTarget::Grandmaster, 255});
    Fill();
}
void AssignmentTable::Add(bool rgbw) {
    if (internal_) {
        for (unsigned target = rgbw ? 1 : 0; target < 5; ++target) {
            auto found = std::find_if(controls_.begin(), controls_.end(), [target](const auto &a) {
                return unsigned(a.target) == target;
            });
            if (found == controls_.end()) {
                controls_.push_back({static_cast<brasscue::ControlTarget>(target), 255});
                if (!rgbw)
                    return;
            }
        }
        if (!rgbw)
            throw std::invalid_argument("All five logical controls are already assigned");
    } else {
        if (dmx_.size() >= 512)
            throw std::invalid_argument("At most 512 assignments per control");
        for (unsigned channel = 1; channel <= 512; ++channel) {
            auto used = std::find_if(dmx_.begin(), dmx_.end(), [channel](const auto &a) {
                return a.address.universe == 1 && a.address.channel == channel;
            });
            if (used == dmx_.end()) {
                dmx_.push_back({{1, std::uint16_t(channel)}, 255});
                return;
            }
        }
    }
}
void AssignmentTable::Remove() {
    grid_->SaveEditControlValue();
    grid_->HideCellEditControl();
    int row = grid_->GetGridCursorRow();
    if (row < 0 || row >= grid_->GetNumberRows())
        return;
    grid_->DeleteRows(row);
    if (internal_)
        controls_.erase(controls_.begin() + row);
    else
        dmx_.erase(dmx_.begin() + row);
    edited_.erase(edited_.begin() + row);
}
