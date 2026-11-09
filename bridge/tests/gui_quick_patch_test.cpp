// BrassCue - GPL-3.0. Real modal group drafts and the explicit live-output commit boundary.
#include "../gui/assignment_table.hpp"
#include "../gui/encoder_group_routes.hpp"
#include "../gui/profile_commit.hpp"
#include "../gui/profile_editor.hpp"
#include "../gui/quick_patch.hpp"
#include "../src/profile_io.hpp"
#include "check.hpp"
#include <wx/grid.h>
#include <wx/listbox.h>
#include <wx/modalhook.h>
namespace {
wxDialog *Dialog() {
    for (auto *window : wxTopLevelWindows)
        if (auto *dialog = wxDynamicCast(window, wxDialog))
            if (dialog->IsModal())
                return dialog;
    CHECK(false);
    return nullptr;
}
void Click(wxDialog *dialog, int id) {
    auto *window = wxWindow::FindWindowById(id, dialog);
    CHECK(window);
    wxCommandEvent click(wxEVT_BUTTON, id);
    click.SetEventObject(window);
    window->GetEventHandler()->ProcessEvent(click);
}
wxTextCtrl *Text(wxDialog *dialog, int id) {
    auto *text = wxDynamicCast(wxWindow::FindWindowById(id, dialog), wxTextCtrl);
    CHECK(text);
    return text;
}
class ConfirmHook final : public wxModalDialogHook {
  public:
    int result = wxID_NO, calls = 0;
    int Enter(wxDialog *dialog) override {
        CHECK(dialog->GetTitle() == "Confirm live repatch");
        ++calls;
        return result;
    }
};
} // namespace
class QuickPatchTestApp final : public wxApp {
  public:
    bool OnInit() override {
        CallAfter([this] { Run(); });
        return true;
    }
    void Run() {
        brasscue::ControlProfile profile;
        profile.encoders[0][0].value = 53;
        const auto original = brasscue::serialize_profile(profile);
        for (bool accept : {false, true}) {
            CallAfter([&, accept] {
                auto *dialog = Dialog();
                Click(dialog, QuickPatchId::Add);
                Text(dialog, QuickPatchId::Name)->SetValue("HOUSE PANELS");
                Text(dialog, QuickPatchId::Order)->SetValue("GRBW");
                Text(dialog, QuickPatchId::Stride)->SetValue("8");
                auto *blocks =
                    wxDynamicCast(wxWindow::FindWindowById(QuickPatchId::Blocks, dialog), wxGrid);
                auto *preview =
                    wxDynamicCast(wxWindow::FindWindowById(QuickPatchId::Preview, dialog), wxGrid);
                CHECK(blocks && preview && blocks->GetNumberRows() == 1);
                blocks->SetCellValue(0, 0, "2");
                blocks->SetCellValue(0, 1, "17");
                blocks->SetCellValue(0, 2, "3");
                Click(dialog, QuickPatchId::AddBlock);
                blocks->SetCellValue(1, 0, "3");
                blocks->SetCellValue(1, 1, "1");
                blocks->SetCellValue(1, 2, "2");
                CHECK(!dialog->FindWindow(wxID_OK)->IsEnabled());
                Click(dialog, QuickPatchId::Update);
                CHECK(preview->GetNumberRows() == 5 && dialog->FindWindow(wxID_OK)->IsEnabled());
                CHECK(preview->GetCellValue(0, 0) == "HOUSE PANELS");
                CHECK(preview->GetCellValue(0, 4) == "18" && preview->GetCellValue(0, 5) == "17");
                CHECK(preview->GetCellValue(3, 2) == "3");
                wxCommandEvent enter(wxEVT_TEXT_ENTER, QuickPatchId::Name);
                Text(dialog, QuickPatchId::Name)->GetEventHandler()->ProcessEvent(enter);
                CHECK(dialog->IsModal() && brasscue::serialize_profile(profile) == original);
                Click(dialog, accept ? wxID_OK : wxID_CANCEL);
            });
            CHECK(EditQuickRgbwPatch(nullptr, profile) == accept);
            if (!accept)
                CHECK(brasscue::serialize_profile(profile) == original);
        }
        CHECK(profile.fixture_groups.size() == 1 && profile.encoders[0][0].value == 53);
        CHECK(profile.fixture_groups[0].blocks[0].count == 3 &&
              profile.fixture_groups[0].order == "GRBW");
        auto before = brasscue::serialize_profile(profile);
        CallAfter([&] {
            auto *dialog = Dialog();
            auto *blocks =
                wxDynamicCast(wxWindow::FindWindowById(QuickPatchId::Blocks, dialog), wxGrid);
            auto *preview =
                wxDynamicCast(wxWindow::FindWindowById(QuickPatchId::Preview, dialog), wxGrid);
            CHECK(blocks && preview);
            blocks->SetCellValue(0, 1, "510");
            Click(dialog, QuickPatchId::Update);
            CHECK(preview->GetNumberRows() == 0 && !dialog->FindWindow(wxID_OK)->IsEnabled());
            CHECK(dialog->IsModal());
            blocks->SetCellValue(0, 1, "49");
            Click(dialog, QuickPatchId::Update);
            CHECK(preview->GetNumberRows() == 5 && dialog->FindWindow(wxID_OK)->IsEnabled());
            CHECK(preview->GetCellValue(0, 3) == "49");
            CHECK(brasscue::serialize_profile(profile) == before);
            Click(dialog, wxID_OK);
        });
        CHECK(EditQuickRgbwPatch(nullptr, profile));
        CHECK(profile.fixture_groups[0].blocks[0].address == 49);
        before = brasscue::serialize_profile(profile);
        CallAfter([&] {
            auto *dialog = Dialog();
            Click(dialog, QuickPatchId::Remove);
            Click(dialog, wxID_CANCEL);
        });
        CHECK(!EditQuickRgbwPatch(nullptr, profile));
        CHECK(brasscue::serialize_profile(profile) == before);
        CallAfter([&] {
            auto *dialog = Dialog();
            Click(dialog, QuickPatchId::Add);
            Text(dialog, QuickPatchId::Name)->SetValue("LOBBY");
            auto *blocks =
                wxDynamicCast(wxWindow::FindWindowById(QuickPatchId::Blocks, dialog), wxGrid);
            blocks->SetCellValue(0, 0, "4");
            Click(dialog, QuickPatchId::Update);
            auto *list =
                wxDynamicCast(wxWindow::FindWindowById(QuickPatchId::Groups, dialog), wxListBox);
            CHECK(list && list->GetCount() == 2);
            list->SetSelection(0);
            wxCommandEvent select(wxEVT_LISTBOX, QuickPatchId::Groups);
            list->GetEventHandler()->ProcessEvent(select);
            Text(dialog, QuickPatchId::Name)->SetValue("RENAMED HOUSE");
            list->SetSelection(1);
            list->GetEventHandler()->ProcessEvent(select);
            CHECK(list->GetString(0) == "RENAMED HOUSE");
            CHECK(Text(dialog, QuickPatchId::Name)->GetValue() == "LOBBY");
            Click(dialog, wxID_CANCEL);
        });
        CHECK(!EditQuickRgbwPatch(nullptr, profile));
        CHECK(brasscue::serialize_profile(profile) == before);
        EncoderVisibility();
        // Confirmation is shared by Quick Patch and manual editors. Only loopback output.
        brasscue::ControllerSession session;
        session.configure(profile);
        session.encoder(0, 255);
        brasscue::OutputSession output;
        output.enable("127.0.0.1");
        output.send(session.engine(), 0);
        auto changed = profile;
        changed.fixture_groups[0].blocks[0].address = 81;
        ConfirmHook hook;
        hook.Register();
        CHECK(!CommitProfileEdit(nullptr, session, output, changed));
        CHECK(hook.calls == 1 && output.enabled() &&
              brasscue::serialize_profile(session.profile()) == before);
        hook.result = wxID_YES;
        CHECK(CommitProfileEdit(nullptr, session, output, changed));
        CHECK(hook.calls == 2 && !output.enabled() &&
              session.profile().fixture_groups[0].blocks[0].address == 81);
        CHECK(session.state().rgbw[0] == 255 && session.profile().encoders[0][0].value == 53);
        // A label edit leaves enabled routing alone and requires no repatch confirmation.
        output.enable("127.0.0.1");
        auto label = session.profile();
        label.name = "Edited label";
        CHECK(CommitProfileEdit(nullptr, session, output, label));
        CHECK(hook.calls == 2 && output.enabled());
        auto manual = session.profile();
        manual.encoders[0][0].address.channel = 25;
        hook.result = wxID_NO;
        CHECK(!CommitProfileEdit(nullptr, session, output, manual));
        CHECK(hook.calls == 3 && output.enabled());
        output.stop();
        hook.Unregister();
        std::cout << "Group modal preview/commit/cancel and live routing confirmation passed\n";
        std::exit(0);
    }
    void EncoderVisibility() {
        // Exact reported case: no manual rows, U2/469, eleven RGBW fixtures through channel 512.
        brasscue::ControlProfile profile;
        for (auto &patch : profile.encoders)
            patch.clear();
        CallAfter([&] {
            auto *dialog = Dialog();
            Click(dialog, QuickPatchId::Add);
            Text(dialog, QuickPatchId::Name)->SetValue("PANEL SALA 1");
            auto *blocks =
                wxDynamicCast(wxWindow::FindWindowById(QuickPatchId::Blocks, dialog), wxGrid);
            CHECK(blocks);
            blocks->SetCellValue(0, 0, "2");
            blocks->SetCellValue(0, 1, "469");
            blocks->SetCellValue(0, 2, "11");
            Click(dialog, QuickPatchId::Update);
            Click(dialog, wxID_OK);
        });
        CHECK(EditQuickRgbwPatch(nullptr, profile));
        brasscue::ControllerSession session;
        brasscue::OutputSession output;
        CHECK(CommitProfileEdit(nullptr, session, output, profile));
        CHECK(!output.enabled());
        for (unsigned color = 0; color < 4; ++color)
            session.encoder(color, std::uint8_t(40 + color * 50));
        session.fader(128);
        const auto frames = session.engine().render(0);
        CHECK(frames.size() == 1 && frames.count(2));
        const auto packet = brasscue::frame_packet(2, frames.at(2));
        CHECK(packet[14] == 1 && packet[15] == 0); // Visible U2 -> Art-Net wire address 1.
        for (unsigned slot = 0; slot < 468; ++slot)
            CHECK(packet[18 + slot] == 0);
        for (unsigned fixture = 0; fixture < 11; ++fixture)
            for (unsigned color = 0; color < 4; ++color) {
                CHECK(session.encoder_patch()[color].size() == 11);
                CHECK(packet[18 + 468 + fixture * 4 + color] == 20 + color * 25);
            }
        const auto original = brasscue::serialize_profile(profile);
        for (unsigned color = 0; color < 4; ++color) {
            for (bool accept : {false, true}) {
                CallAfter([&, color, accept] {
                    auto *dialog = Dialog();
                    auto *groups = wxDynamicCast(
                        wxWindow::FindWindowById(EncoderGroupRoutesId, dialog), wxGrid);
                    auto *manual = wxDynamicCast(
                        wxWindow::FindWindowById(AssignmentTable::GridId, dialog), wxGrid);
                    CHECK(groups && manual && !groups->IsEditable());
                    CHECK(groups->GetNumberRows() == 11 && manual->GetNumberRows() == 0);
                    for (unsigned fixture = 0; fixture < 11; ++fixture) {
                        CHECK(groups->GetCellValue(fixture, 0) == "PANEL SALA 1");
                        CHECK(groups->GetCellValue(fixture, 2) == "2");
                        CHECK(groups->GetCellValue(fixture, 3) ==
                              wxString::Format("%u", 469 + fixture * 4 + color));
                        CHECK(groups->GetCellValue(fixture, 4) == "255");
                    }
                    Click(dialog, accept ? wxID_OK : wxID_CANCEL);
                });
                CHECK(EditProfileElement(nullptr, profile, int(color)) == accept);
                CHECK(brasscue::serialize_profile(profile) == original);
            }
        }
        // Manual and generated assignments coexist in the editor without flattening either source.
        profile.encoders[0].push_back({{1, 1}, 53});
        CallAfter([&] {
            auto *dialog = Dialog();
            auto *groups =
                wxDynamicCast(wxWindow::FindWindowById(EncoderGroupRoutesId, dialog), wxGrid);
            auto *manual =
                wxDynamicCast(wxWindow::FindWindowById(AssignmentTable::GridId, dialog), wxGrid);
            CHECK(groups && groups->GetNumberRows() == 11 && manual &&
                  manual->GetNumberRows() == 1);
            Click(dialog, wxID_OK);
        });
        CHECK(EditProfileElement(nullptr, profile, 0));
        CHECK(profile.encoders[0].size() == 1 && profile.encoders[0][0].value == 53);
        CHECK(profile.fixture_groups.size() == 1);
    }
};
wxIMPLEMENT_APP(QuickPatchTestApp);
