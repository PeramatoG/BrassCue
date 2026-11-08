// BrassCue - GPL-3.0. OS keyboard/mouse events exercise real wx modal routing.
#include "../gui/assignment_table.hpp"
#include "../gui/console_fader.hpp"
#include "../gui/profile_editor.hpp"
#include "../src/controller_session.hpp"
#include "../src/profile_io.hpp"
#include "check.hpp"
#include "gui_input.hpp"
#include <cmath>
#include <wx/spinctrl.h>
#if BRASSCUE_GUI_INPUT_SUPPORTED
namespace {
void Pump() {
    for (int i = 0; i < 10; ++i) {
        wxMilliSleep(10);
        wxYield();
    }
}
void Key(int code, int modifiers = wxMOD_NONE) {
    GuiInput input;
    CHECK(input.Char(code, modifiers));
    Pump();
}
void Focus(wxWindow *window) {
    window->SetFocus();
    Pump();
    CHECK(window->HasFocus() || window->IsDescendant(wxWindow::FindFocus()));
}
wxDialog *Dialog() {
    for (auto *window : wxTopLevelWindows)
        if (auto *dialog = wxDynamicCast(window, wxDialog))
            if (dialog->IsModal())
                return dialog;
    CHECK(false);
    return nullptr;
}
void StillOpen(wxDialog *dialog) {
    CHECK(dialog->IsModal() && dialog->IsShown());
}
} // namespace
#endif
class KeyboardTestApp final : public wxApp {
  public:
    bool OnInit() override {
#if BRASSCUE_GUI_INPUT_SUPPORTED
        CallAfter([this] { Run(); });
        return true;
#else
        std::cerr << "SKIP: No supported OS input backend in this wxWidgets build\n";
        std::exit(77);
#endif
    }
#if BRASSCUE_GUI_INPUT_SUPPORTED
    void Run() {
        // Every contextual editor, including the shared three button editors.
        for (int element = 0; element <= 8; ++element)
            for (bool accept : {false, true}) {
                brasscue::ControlProfile profile;
                for (auto &encoder : profile.encoders)
                    encoder.push_back({{2, 2}, 53});
                profile.fader.assignments = {{{1, 5}, 53}, {{2, 2}, 53}};
                profile.buttons[0].action = brasscue::ButtonActionKind::DmxCue;
                profile.buttons[0].assignments = profile.fader.assignments;
                profile.buttons[0].controls.clear();
                for (int i : {1, 2})
                    profile.buttons[i].controls.push_back({brasscue::ControlTarget::Red, 53});
                profile.buttons[0].fade_out_ms = 1234;
                const auto original = brasscue::serialize_profile(profile);
                CallAfter([element, accept] {
                    std::cerr << "Editor " << element << " accept " << accept << "\n";
                    auto *dialog = Dialog();
                    Pump();
                    wxTextCtrl *label = nullptr;
                    for (auto *child : dialog->GetChildren())
                        if (auto *text = wxDynamicCast(child, wxTextCtrl)) {
                            label = text;
                            break;
                        }
                    CHECK(label);
                    Focus(label);
                    Key('A', wxMOD_CONTROL);
                    GuiInput input;
                    CHECK(input.Text("Key"));
                    Key(WXK_RETURN);
                    StillOpen(dialog);
                    CHECK(label->GetValue() == "Key");
                    CHECK(label->HasFocus());
                    Key(WXK_TAB);
                    StillOpen(dialog);
                    CHECK(!label->HasFocus());
                    Key(WXK_TAB, wxMOD_SHIFT);
                    CHECK(label->HasFocus());
                    Key(WXK_TAB);
                    if (element >= 5 && element <= 7) {
                        wxSpinCtrlDouble *fade = nullptr, *out = nullptr;
                        for (auto *child : dialog->GetChildren())
                            if (auto *spin = wxDynamicCast(child, wxSpinCtrlDouble)) {
                                if (!fade)
                                    fade = spin;
                                else
                                    out = spin;
                            }
                        CHECK(fade);
                        Focus(fade);
                        Key('A', wxMOD_CONTROL);
                        CHECK(input.Text("2.3"));
                        Key(WXK_RETURN);
                        StillOpen(dialog);
                        CHECK(std::abs(fade->GetValue() - 2.3) < 0.001);
                        Key(WXK_TAB);
                        StillOpen(dialog);
                        CHECK(out);
                        Focus(out);
                        Key(WXK_RETURN); // Confirming the rounded field keeps legacy milliseconds.
                        StillOpen(dialog);
                    }
                    if (element != 8) {
                        auto *grid = wxDynamicCast(
                            wxWindow::FindWindowById(AssignmentTable::GridId, dialog), wxGrid);
                        CHECK(grid && grid->GetNumberRows() > 0);
                        const int col = element >= 6 && element <= 7 ? 1 : 2;
                        grid->SetGridCursor(1, col);
                        Focus(grid);
                        grid->EnableCellEditControl();
                        Pump();
                        Key(WXK_RETURN); // Unchanged rounded 21% must still store byte 53.
                        StillOpen(dialog);
                        grid->SetGridCursor(0, col);
                        Focus(grid);
                        grid->EnableCellEditControl();
                        Pump();
                        Key('A', wxMOD_CONTROL);
                        CHECK(input.Text("42"));
                        Key(WXK_RETURN);
                        StillOpen(dialog);
                        CHECK(grid->GetCellValue(0, col) == "42");
                        CHECK(!grid->IsCellEditControlShown());
                        // Tab retains the grid's normal commit/navigation behavior.
                        grid->SetGridCursor(0, col);
                        Focus(grid);
                        grid->EnableCellEditControl();
                        Pump();
                        Key('A', wxMOD_CONTROL);
                        CHECK(input.Text("43"));
                        Key(WXK_TAB);
                        StillOpen(dialog);
                        CHECK(grid->GetCellValue(0, col) == "43");
                    }
                    auto *button = wxWindow::FindWindowById(accept ? wxID_OK : wxID_CANCEL, dialog);
                    CHECK(button);
                    Focus(button);
                    Key(WXK_RETURN);
                });
                CHECK(EditProfileElement(nullptr, profile, element) == accept);
                if (!accept)
                    CHECK(brasscue::serialize_profile(profile) == original);
                else if (element < 4) {
                    CHECK(profile.encoder_labels[element] == "Key");
                    CHECK(profile.encoders[element][0].value == brasscue::from_percent(43));
                    CHECK(profile.encoders[element][1].value == 53);
                } else if (element == 4) {
                    CHECK(profile.fader_label == "Key");
                    CHECK(profile.fader.assignments[0].value == brasscue::from_percent(43));
                    CHECK(profile.fader.assignments[1].value == 53);
                } else if (element == 8)
                    CHECK(profile.screen_title == "Key");
                else {
                    CHECK(profile.button_labels[element - 5] == "Key");
                    CHECK(profile.buttons[element - 5].fade_in_ms == 2300);
                    if (element == 5) {
                        CHECK(profile.buttons[0].assignments[0].value ==
                              brasscue::from_percent(43));
                        CHECK(profile.buttons[0].assignments[1].value == 53);
                    } else {
                        CHECK(profile.buttons[element - 5].controls[0].value ==
                              brasscue::from_percent(43));
                        CHECK(profile.buttons[element - 5].controls[1].value == 53);
                    }
                    if (element == 5)
                        CHECK(profile.buttons[0].fade_out_ms == 1234);
                }
            }
        brasscue::ControlProfile profile;
        const auto original = brasscue::serialize_profile(profile);
        CallAfter([] {
            auto *dialog = Dialog();
            wxTextCtrl *label = nullptr;
            for (auto *child : dialog->GetChildren())
                if (auto *text = wxDynamicCast(child, wxTextCtrl))
                    label = text;
            CHECK(label);
            Focus(label);
            Key(WXK_ESCAPE);
        });
        CHECK(!EditProfileElement(nullptr, profile, 8));
        CHECK(brasscue::serialize_profile(profile) == original);
        Fader();
        std::cout << "OS keyboard editor commit/cancel, Tab/Escape and fader input passed\n";
        std::exit(0);
    }
    void Fader() {
        auto *frame = new wxFrame(nullptr, wxID_ANY, "Fader keyboard and drag", wxDefaultPosition,
                                  wxSize(240, 480));
        // Match the real NativePanel's standard traversal container on Windows.
        auto *panel = new wxPanel(frame);
        brasscue::ControllerSession session;
        int notifications = 0;
        auto *fader = new ConsoleFader(panel, wxID_ANY, [&](int value) {
            ++notifications;
            session.fader(value);
        });
        auto *layout = new wxBoxSizer(wxVERTICAL);
        layout->Add(fader, 1, wxEXPAND);
        layout->Add(new wxButton(panel, wxID_ANY, "Tab destination"));
        panel->SetSizer(layout);
        auto *root = new wxBoxSizer(wxVERTICAL);
        root->Add(panel, 1, wxEXPAND);
        frame->SetSizer(root);
        frame->Show();
        frame->Raise();
        Pump();
        Focus(fader);
        Key(WXK_END);
        CHECK(session.state().master == 0);
        Key(WXK_UP);
        CHECK(session.state().master == 1);
        Key(WXK_PAGEUP);
        CHECK(session.state().master == 11);
        Key(WXK_DOWN);
        CHECK(session.state().master == 10);
        Key(WXK_PAGEDOWN);
        CHECK(session.state().master == 0);
        Key(WXK_HOME);
        CHECK(session.state().master == 255);
        const int before = notifications;
        session.button(1, true, 0);
        session.button(1, false, 0);
        fader->SetValue(session.fader_value());
        CHECK(fader->GetValue() == 128 && notifications == before);
        // Clicking and dragging the rail reaches both limits and midpoint.
        GuiInput mouse;
        const auto origin = fader->ClientToScreen(wxPoint(0, 0));
        const auto size = fader->GetClientSize();
        CHECK(mouse.MouseMove(origin.x + size.x / 2, origin.y + size.y - 1));
        CHECK(mouse.MouseDown());
        Pump();
        CHECK(session.state().master == 0);
        CHECK(mouse.MouseMove(origin.x + size.x / 2, origin.y + size.y / 2));
        Pump();
        CHECK(std::abs(int(session.state().master) - 128) <= 1);
        CHECK(mouse.MouseMove(origin.x + size.x / 2, origin.y));
        Pump();
        CHECK(mouse.MouseUp());
        Pump();
        CHECK(session.state().master == 255 && !fader->HasCapture());
        // At compact heights the travel has fewer pixels than DMX values.
        // An off-center cap click must not quantize an untouched byte on grab/release.
        frame->SetSize(240, 240);
        Pump();
        session.fader(53);
        fader->SetValue(53);
        const auto compact = fader->GetClientSize();
        const auto position = fader->ClientToScreen(wxPoint(0, 0));
        const int margin = fader->FromDIP(22);
        const int center = margin + int(std::lround((compact.y - 2 * margin) * 202 / 255.0));
        const int unchanged = notifications;
        CHECK(mouse.MouseMove(position.x + compact.x / 2, position.y + center + fader->FromDIP(5)));
        CHECK(mouse.MouseDown());
        Pump();
        CHECK(mouse.MouseUp());
        Pump();
        CHECK(session.state().master == 53 && fader->GetValue() == 53 &&
              notifications == unchanged);
        Focus(fader);
        Key(WXK_TAB);
        CHECK(!fader->HasFocus());
        frame->Destroy();
    }
#endif
};
wxIMPLEMENT_APP(KeyboardTestApp);
