// BrassCue - GPL-3.0. Named groups, explicit universe blocks and a core-generated preview.
#include "quick_patch.hpp"
#include "../src/assignment_editor.hpp"
#include "dark_theme.hpp"
#include "editor_buttons.hpp"
#include <wx/grid.h>
#include <wx/listbox.h>
namespace {
class QuickPatchDialog final : public wxDialog {
  public:
    QuickPatchDialog(wxWindow *parent, const brasscue::ControlProfile &profile)
        : wxDialog(parent, wxID_ANY, "Quick RGBW Patch", wxDefaultPosition, wxDefaultSize,
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
          candidate(profile) {
        auto *root = new wxBoxSizer(wxVERTICAL);
        root->Add(new wxStaticText(this, wxID_ANY,
                                   "Four-channel RGBW fixtures. Manual assignments remain separate "
                                   "and are preserved.\n"
                                   "Boundary policy: reject overflow at 512. Add explicit blocks "
                                   "for other universes."),
                  0, wxALL, 8);
        groups_ = new wxListBox(this, QuickPatchId::Groups);
        groups_->SetName("Fixture groups");
        root->Add(groups_, 0, wxEXPAND | wxALL, 8);
        auto *row = new wxBoxSizer(wxHORIZONTAL);
        auto *add = new wxButton(this, QuickPatchId::Add, "Add group");
        auto *remove = new wxButton(this, QuickPatchId::Remove, "Remove group");
        row->Add(add);
        row->Add(remove);
        root->Add(row, 0, wxLEFT | wxRIGHT, 8);
        auto *fields = new wxFlexGridSizer(2, 6, 8);
        fields->AddGrowableCol(1);
        name_ = Text(*fields, "Group name", QuickPatchId::Name);
        order_ = Text(*fields, "Channel order (RGBW permutation)", QuickPatchId::Order);
        stride_ = Text(*fields, "Address stride (4..512)", QuickPatchId::Stride);
        root->Add(fields, 0, wxEXPAND | wxALL, 8);
        blocks_ = new wxGrid(this, QuickPatchId::Blocks);
        blocks_->SetName("Address blocks");
        blocks_->CreateGrid(0, 3);
        blocks_->SetRowLabelSize(FromDIP(45));
        blocks_->SetTabBehaviour(wxGrid::Tab_Leave);
        blocks_->SetColLabelValue(0, "Universe (1..32768)");
        blocks_->SetColLabelValue(1, "Start address (1..509)");
        blocks_->SetColLabelValue(2, "Fixture count");
        for (int col = 0; col < 3; ++col)
            blocks_->SetColSize(col, FromDIP(180));
        root->Add(blocks_, 1, wxEXPAND | wxALL, 8);
        row = new wxBoxSizer(wxHORIZONTAL);
        auto *add_block = new wxButton(this, QuickPatchId::AddBlock, "Add block");
        auto *remove_block = new wxButton(this, QuickPatchId::RemoveBlock, "Remove block");
        auto *update = new wxButton(this, QuickPatchId::Update, "Update group / Preview");
        row->Add(add_block);
        row->Add(remove_block);
        row->Add(update);
        root->Add(row, 0, wxLEFT | wxRIGHT, 8);
        status_ = new wxStaticText(this, wxID_ANY, "");
        root->Add(status_, 0, wxEXPAND | wxALL, 8);
        preview_ = new wxGrid(this, QuickPatchId::Preview);
        preview_->SetName("Fixture patch preview");
        preview_->CreateGrid(0, 8);
        preview_->SetRowLabelSize(FromDIP(45));
        preview_->EnableEditing(false);
        preview_->SetTabBehaviour(wxGrid::Tab_Leave);
        const char *headers[]{"Group",     "Fixture",   "Universe",  "Start",
                              "R channel", "G channel", "B channel", "W channel"};
        for (int col = 0; col < 8; ++col) {
            preview_->SetColLabelValue(col, headers[col]);
            preview_->SetColSize(col, FromDIP(col == 0 ? 160 : 85));
        }
        root->Add(preview_, 2, wxEXPAND | wxALL, 8);
        root->Add(EditorButtons(*this), 0, wxEXPAND | wxALL, 8);
        SetSizer(root);
        SetMinClientSize(FromDIP(wxSize(760, 650)));
        SetClientSize(FromDIP(wxSize(850, 760)));
        RefreshList();
        Load();
        Preview();
        ApplyDarkTheme(*this);
        groups_->Bind(wxEVT_LISTBOX, [this](wxCommandEvent &) {
            const int next = groups_->GetSelection();
            // Save the previous draft before switching, without requiring it to be valid yet.
            if (Try([this] { Store(); })) {
                selected_ = next;
                RefreshList();
                Load();
                Preview();
            } else
                groups_->SetSelection(selected_);
        });
        add->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            if (!Try([this] { Store(); }))
                return;
            if (candidate.fixture_groups.size() >= 64) {
                SetStatus("At most 64 groups");
                return;
            }
            brasscue::FixtureGroup group;
            unsigned n = 1;
            do {
                group.name = "Group " + std::to_string(n++);
            } while (std::any_of(candidate.fixture_groups.begin(), candidate.fixture_groups.end(),
                                 [&](const auto &g) { return g.name == group.name; }));
            candidate.fixture_groups.push_back(group);
            selected_ = int(candidate.fixture_groups.size()) - 1;
            RefreshList();
            Load();
            Preview();
        });
        remove->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            if (selected_ < 0)
                return;
            candidate.fixture_groups.erase(candidate.fixture_groups.begin() + selected_);
            selected_ = candidate.fixture_groups.empty() ? -1 : 0;
            RefreshList();
            Load();
            Preview();
        });
        add_block->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            if (selected_ < 0)
                return;
            if (blocks_->GetNumberRows() >= 64) {
                SetStatus("At most 64 blocks per group");
                return;
            }
            blocks_->AppendRows();
            const int row = blocks_->GetNumberRows() - 1;
            for (int col = 0; col < 3; ++col)
                blocks_->SetCellValue(row, col, col == 1 ? "5" : "1");
            ClearPreview();
        });
        remove_block->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            if (blocks_->GetGridCursorRow() >= 0 && blocks_->GetNumberRows())
                blocks_->DeleteRows(blocks_->GetGridCursorRow());
            ClearPreview();
        });
        update->Bind(wxEVT_BUTTON, [this](wxCommandEvent &) {
            if (Try([this] { Store(); })) {
                RefreshList();
                Preview();
            }
        });
        for (auto *field : {name_, order_, stride_})
            field->Bind(wxEVT_TEXT, [this](wxCommandEvent &) { ClearPreview(); });
        blocks_->Bind(wxEVT_GRID_CELL_CHANGED, [this](wxGridEvent &e) {
            ClearPreview();
            e.Skip();
        });
        Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent &) {
                if (!Try([this] {
                        Store();
                        brasscue::validate(candidate);
                    }))
                    return;
                if (!preview_current_) {
                    Preview();
                    return;
                }
                EndModal(wxID_OK);
            },
            wxID_OK);
    }
    brasscue::ControlProfile candidate;

  private:
    wxTextCtrl *Text(wxSizer &sizer, const wxString &label, int id) {
        sizer.Add(new wxStaticText(this, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL);
        auto *text =
            new wxTextCtrl(this, id, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
        text->SetName(label);
        text->Bind(wxEVT_TEXT_ENTER, [](wxCommandEvent &) {});
        sizer.Add(text, 1, wxEXPAND);
        return text;
    }
    template <class F> bool Try(F action) {
        try {
            action();
            return true;
        } catch (const std::exception &e) {
            ClearPreview();
            SetStatus(wxString::FromUTF8(e.what()));
            return false;
        }
    }
    void SetStatus(const wxString &message) {
        status_->SetLabel(message);
        status_->Wrap(FromDIP(740));
        Layout();
    }
    void ClearPreview() {
        preview_current_ = false;
        FindWindow(wxID_OK)->Enable(false);
        if (preview_->GetNumberRows())
            preview_->DeleteRows(0, preview_->GetNumberRows());
        SetStatus(
            "Draft changed. Update group / Preview to validate; OK validates all pending edits.");
    }
    void RefreshList() {
        groups_->Clear();
        for (const auto &g : candidate.fixture_groups)
            groups_->Append(wxString::FromUTF8(g.name));
        if (selected_ < 0 && !candidate.fixture_groups.empty())
            selected_ = 0;
        groups_->SetSelection(selected_);
    }
    void Load() {
        const bool active = selected_ >= 0;
        for (auto *field : {name_, order_, stride_})
            field->Enable(active);
        blocks_->Enable(active);
        if (blocks_->GetNumberRows())
            blocks_->DeleteRows(0, blocks_->GetNumberRows());
        if (!active) {
            name_->ChangeValue("");
            order_->ChangeValue("");
            stride_->ChangeValue("");
            return;
        }
        const auto &g = candidate.fixture_groups.at(selected_);
        name_->ChangeValue(wxString::FromUTF8(g.name));
        order_->ChangeValue(wxString::FromUTF8(g.order));
        stride_->ChangeValue(wxString::Format("%u", g.stride));
        blocks_->AppendRows(int(g.blocks.size()));
        for (std::size_t row = 0; row < g.blocks.size(); ++row) {
            const auto &b = g.blocks[row];
            blocks_->SetCellValue(row, 0, wxString::Format("%u", b.universe));
            blocks_->SetCellValue(row, 1, wxString::Format("%u", b.address));
            blocks_->SetCellValue(row, 2, wxString::Format("%u", b.count));
        }
    }
    void Store() {
        if (selected_ < 0)
            return;
        blocks_->SaveEditControlValue();
        blocks_->HideCellEditControl();
        auto g = candidate.fixture_groups.at(selected_);
        g.name = name_->GetValue().ToStdString(wxConvUTF8);
        g.order = order_->GetValue().ToStdString(wxConvUTF8);
        g.stride = brasscue::assignment_number(stride_->GetValue().ToStdString(), 512);
        g.blocks.clear();
        for (int row = 0; row < blocks_->GetNumberRows(); ++row)
            g.blocks.push_back(
                {brasscue::assignment_number(blocks_->GetCellValue(row, 0).ToStdString(), 32768),
                 brasscue::assignment_number(blocks_->GetCellValue(row, 1).ToStdString(), 512),
                 brasscue::assignment_number(blocks_->GetCellValue(row, 2).ToStdString(), 512)});
        candidate.fixture_groups.at(selected_) = std::move(g);
    }
    void Preview() {
        ClearPreview();
        Try([this] {
            brasscue::validate(candidate);
            const auto layout = brasscue::fixture_layout(candidate.fixture_groups);
            if (!layout.empty())
                preview_->AppendRows(int(layout.size()));
            for (std::size_t row = 0; row < layout.size(); ++row) {
                const auto &p = layout[row];
                preview_->SetCellValue(row, 0,
                                       wxString::FromUTF8(candidate.fixture_groups[p.group].name));
                preview_->SetCellValue(row, 1, wxString::Format("%u", p.fixture));
                preview_->SetCellValue(row, 2, wxString::Format("%u", unsigned(p.start.universe)));
                preview_->SetCellValue(row, 3, wxString::Format("%u", unsigned(p.start.channel)));
                for (int col = 0; col < 4; ++col)
                    preview_->SetCellValue(
                        row, col + 4, wxString::Format("%u", unsigned(p.channels[col].channel)));
            }
            preview_current_ = true;
            FindWindow(wxID_OK)->Enable(true);
            SetStatus(wxString::Format("Valid preview: %u fixtures. Generated targets are "
                                       "255; manual patches are unchanged.",
                                       unsigned(layout.size())));
        });
    }
    bool preview_current_ = false;
    int selected_ = -1;
    wxListBox *groups_;
    wxTextCtrl *name_, *order_, *stride_;
    wxGrid *blocks_, *preview_;
    wxStaticText *status_;
};
} // namespace
bool EditQuickRgbwPatch(wxWindow *parent, brasscue::ControlProfile &profile) {
    QuickPatchDialog dialog(parent, profile);
    if (dialog.ShowModal() != wxID_OK)
        return false;
    profile = std::move(dialog.candidate);
    return true;
}
