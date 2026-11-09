// BrassCue - GPL-3.0. Present canonical group-derived addresses without creating editable copies.
#include "encoder_group_routes.hpp"
#include <wx/grid.h>
wxWindow *CreateEncoderGroupRoutes(wxWindow *parent, const brasscue::ControlProfile &profile,
                                   unsigned color) {
    const auto layout = brasscue::fixture_layout(profile.fixture_groups);
    auto *panel = new wxPanel(parent);
    auto *root = new wxBoxSizer(wxVERTICAL);
    auto *heading =
        new wxStaticText(panel, wxID_ANY,
                         wxString::Format("Fixture-group assignments: %u addresses (read-only)",
                                          unsigned(layout.size())));
    root->Add(heading, 0, wxBOTTOM, panel->FromDIP(4));
    auto *help = new wxStaticText(
        panel, wxID_ANY,
        "These addresses already respond to this encoder, even if the manual table is empty.\n"
        "Edit or remove them in Profile > Quick RGBW Patch.");
    help->Wrap(panel->FromDIP(620));
    root->Add(help, 0, wxEXPAND | wxBOTTOM, panel->FromDIP(6));
    auto *grid = new wxGrid(panel, EncoderGroupRoutesId);
    grid->SetName("Fixture-group assignments for " +
                  wxString::FromUTF8(profile.encoder_labels.at(color)));
    grid->CreateGrid(int(layout.size()), 5);
    grid->SetMinSize(
        panel->FromDIP(wxSize(-1, 220))); // Scroll rather than grow with fixture count.
    grid->EnableEditing(false);
    grid->SetTabBehaviour(wxGrid::Tab_Leave);
    grid->SetRowLabelSize(panel->FromDIP(40));
    const char *columns[]{"Group", "Fixture", "Universe", "Channel", "Target (DMX)"};
    const int widths[]{210, 65, 85, 85, 110};
    for (int col = 0; col < 5; ++col) {
        grid->SetColLabelValue(col, columns[col]);
        grid->SetColSize(col, panel->FromDIP(widths[col]));
    }
    for (std::size_t row = 0; row < layout.size(); ++row) {
        const auto &fixture = layout[row];
        const auto address = fixture.channels.at(color);
        grid->SetCellValue(row, 0, wxString::FromUTF8(profile.fixture_groups[fixture.group].name));
        grid->SetCellValue(row, 1, wxString::Format("%u", fixture.fixture));
        grid->SetCellValue(row, 2, wxString::Format("%u", unsigned(address.universe)));
        grid->SetCellValue(row, 3, wxString::Format("%u", unsigned(address.channel)));
        grid->SetCellValue(row, 4, "255");
    }
    root->Add(grid, 1, wxEXPAND);
    panel->SetSizer(root);
    return panel;
}
