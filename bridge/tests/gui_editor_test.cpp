// BrassCue - GPL-3.0. Exercise real modal edit/commit/cancel, not a model substitute.
#include "../gui/assignment_table.hpp"
#include "../gui/network_panel.hpp"
#include "../gui/profile_editor.hpp"
#include "../gui/widgets.hpp"
#include "../src/profile_io.hpp"
#include "check.hpp"
#include <vector>
#include <wx/choice.h>
#include <wx/grid.h>
#include <wx/textctrl.h>
class EditorTestApp final : public wxApp {
  public:
    bool OnInit() override {
        brasscue::ControlProfile profile;
        profile.encoders[0][0].value = 53;
        profile.encoders[0].push_back({{2, 2}, 100});
        const auto original = brasscue::serialize_profile(profile);
        auto act = [this](bool accept) {
            CallAfter([accept] {
                wxDialog *dialog = nullptr;
                for (auto *window : wxTopLevelWindows)
                    if (auto *candidate = wxDynamicCast(window, wxDialog))
                        dialog = candidate;
                CHECK(dialog);
                wxTextCtrl *label = nullptr;
                for (auto *child : dialog->GetChildren())
                    if (auto *text = wxDynamicCast(child, wxTextCtrl)) {
                        label = text;
                        break;
                    }
                CHECK(label);
                label->SetValue(wxString::FromUTF8("Rojo ámbar"));
                if (accept) {
                    auto *grid = wxDynamicCast(
                        wxWindow::FindWindowById(AssignmentTable::GridId, dialog), wxGrid);
                    CHECK(grid && grid->GetNumberRows() == 2);
                    grid->SetGridCursor(1, 0);
                    auto *remove = wxWindow::FindWindowByLabel("Remove selected", dialog);
                    CHECK(remove);
                    wxCommandEvent erase(wxEVT_BUTTON, remove->GetId());
                    remove->GetEventHandler()->ProcessEvent(erase);
                    CHECK(grid->GetNumberRows() == 1);
                    auto *units = wxDynamicCast(
                        wxWindow::FindWindowById(AssignmentTable::UnitsId, dialog), wxChoice);
                    CHECK(units && units->GetSelection() == 1);
                    for (int selection : {0, 1}) {
                        units->SetSelection(selection);
                        wxCommandEvent changed(wxEVT_CHOICE, units->GetId());
                        units->GetEventHandler()->ProcessEvent(changed);
                    }
                    wxCommandEvent ok(wxEVT_BUTTON, wxID_OK);
                    dialog->GetEventHandler()->ProcessEvent(ok);
                } else
                    dialog->EndModal(wxID_CANCEL);
            });
        };
        act(false);
        CHECK(!EditProfileElement(nullptr, profile, 0));
        CHECK(brasscue::serialize_profile(profile) == original);
        act(true);
        CHECK(EditProfileElement(nullptr, profile, 0));
        CHECK(profile.encoder_labels[0] == "Rojo ámbar");
        CHECK(profile.encoders[0][0].value == 53);
        const auto before_button = brasscue::serialize_profile(profile);
        CallAfter([] {
            wxDialog *dialog = nullptr;
            for (auto *window : wxTopLevelWindows)
                if (auto *d = wxDynamicCast(window, wxDialog))
                    dialog = d;
            CHECK(dialog);
            auto *grid =
                wxDynamicCast(wxWindow::FindWindowById(AssignmentTable::GridId, dialog), wxGrid);
            CHECK(grid && grid->GetNumberRows() == 1 && grid->GetCellValue(0, 0) == "Grandmaster");
            auto *add = wxWindow::FindWindowById(AssignmentTable::AddRgbwId, dialog);
            CHECK(add);
            wxCommandEvent click(wxEVT_BUTTON, add->GetId());
            add->GetEventHandler()->ProcessEvent(click);
            CHECK(grid->GetNumberRows() == 5);
            dialog->EndModal(wxID_CANCEL);
        });
        CHECK(!EditProfileElement(nullptr, profile, 6));
        CHECK(brasscue::serialize_profile(profile) == before_button);
        CallAfter([] {
            wxDialog *dialog = nullptr;
            for (auto *window : wxTopLevelWindows)
                if (auto *d = wxDynamicCast(window, wxDialog))
                    dialog = d;
            CHECK(dialog);
            auto *add = wxWindow::FindWindowById(AssignmentTable::AddRgbwId, dialog);
            CHECK(add);
            wxCommandEvent click(wxEVT_BUTTON, add->GetId());
            add->GetEventHandler()->ProcessEvent(click);
            wxCommandEvent ok(wxEVT_BUTTON, wxID_OK);
            dialog->GetEventHandler()->ProcessEvent(ok);
        });
        CHECK(EditProfileElement(nullptr, profile, 6));
        CHECK(profile.buttons[1].controls.size() == 5);
        CHECK(profile.buttons[1].controls[0].value == 128);
        auto *frame = new wxFrame(nullptr, wxID_ANY, "Cue input test");
        std::vector<bool> edges;
        auto *button = new CueButton(frame, [&](bool down) { edges.push_back(down); });
        button->SetView({"SALA 0", "ON", true});
        const auto revision = button->VisualRevision();
        for (int i = 0; i < 100; ++i)
            button->SetView({"SALA 0", "ON", true});
        CHECK(button->VisualRevision() == revision && button->GetLabel() == "SALA 0" &&
              button->Selected());
        for (int i = 0; i < 2; ++i) {
            wxKeyEvent down(wxEVT_KEY_DOWN);
            down.m_keyCode = WXK_SPACE;
            down.SetEventObject(button);
            button->GetEventHandler()->ProcessEvent(down);
        }
        button->CheckPointer(); // An idle pointer must not cancel a keyboard hold.
        CHECK(edges.size() == 1 && edges[0]);
        wxFocusEvent lost(wxEVT_KILL_FOCUS, button->GetId());
        lost.SetEventObject(button);
        button->GetEventHandler()->ProcessEvent(lost);
        CHECK(edges.size() == 2 && !edges[1]);
        wxKeyEvent up(wxEVT_KEY_UP);
        up.m_keyCode = WXK_SPACE;
        up.SetEventObject(button);
        button->GetEventHandler()->ProcessEvent(up);
        CHECK(edges.size() == 2);
        wxCommandEvent accessible(wxEVT_BUTTON, button->GetId());
        accessible.SetEventObject(button);
        button->GetEventHandler()->ProcessEvent(accessible);
        CHECK(edges.size() == 4 && edges[2] && !edges[3]);
        auto *network = new NetworkPanel(frame, [] {}, [](bool) {});
        brasscue::ControllerSession network_session;
        network->Load(network_session.profile());
        wxRadioBox *destination = nullptr;
        wxTextCtrl *unicast = nullptr;
        for (auto *child : network->GetChildren()) {
            if (auto *radio = wxDynamicCast(child, wxRadioBox))
                destination = radio;
            if (auto *text = wxDynamicCast(child, wxTextCtrl))
                if (text->IsEditable())
                    unicast = text;
        }
        CHECK(destination && unicast);
        destination->SetSelection(1);
        unicast->ChangeValue("");
        network->WritePreferences(network_session);
        CHECK(network_session.profile().allow_broadcast);
        CHECK(network_session.profile().target_ip == "127.0.0.1");
        frame->Destroy();
        std::cout << "Real modal editor commit/cancel and UTF-8 labels passed\n";
        std::exit(0);
    }
};
wxIMPLEMENT_APP(EditorTestApp);
