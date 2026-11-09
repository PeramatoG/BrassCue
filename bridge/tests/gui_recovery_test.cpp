// BrassCue - GPL-3.0. Actual frame startup, modal-draft isolation, autosave, portable Save and
// close/restart.
#include "../gui/console_fader.hpp"
#include "../gui/main_frame.hpp"
#include "../src/controller_session.hpp"
#include "../src/profile_io.hpp"
#include "../src/recovery_store.hpp"
#include "check.hpp"
#include "gui_input.hpp"
#include <chrono>
#include <fstream>
#include <wx/modalhook.h>
namespace {
void Pump(unsigned ms = 200) {
    for (unsigned i = 0; i < ms / 10; ++i) {
        wxMilliSleep(10);
        wxYield();
    }
}
std::string Read(const std::filesystem::path &path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
void Menu(wxFrame *frame, int id, int value = 0) {
    wxCommandEvent event(wxEVT_MENU, id);
    event.SetInt(value);
    frame->GetEventHandler()->ProcessEvent(event);
}
void Native(wxFrame *frame) {
#if BRASSCUE_ENABLE_WEB_PANEL
    Menu(frame, wxID_HIGHEST + 500, 1);
#endif
    frame->Show();
    frame->Raise();
    Pump();
}
ConsoleFader *Fader(wxFrame *frame) {
    auto *fader = dynamic_cast<ConsoleFader *>(wxWindow::FindWindowById(wxID_HIGHEST + 104, frame));
    CHECK(fader);
    return fader;
}
void OutputOff(wxFrame *frame) {
    auto *enable = wxDynamicCast(wxWindow::FindWindowByLabel("Enable Art-Net", frame), wxCheckBox);
    CHECK(enable && !enable->GetValue() && !enable->IsEnabled()); // Saved NIC is unavailable.
}
class NoUnexpectedDialog final : public wxModalDialogHook {
  public:
    int calls = 0;
    int Enter(wxDialog *) override {
        ++calls;
        return wxID_CANCEL;
    }
};
} // namespace
class RecoveryTestApp final : public wxApp {
  public:
    bool OnInit() override {
        SetExitOnFrameDelete(false);
        CallAfter([this] { Run(); });
        return true;
    }
    void Run() {
        const auto root =
            std::filesystem::temp_directory_path() /
            ("brasscue-gui-recovery-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root);
        const auto portable = root / "portable.bcue", directory = root / "recovery";
        brasscue::ControlProfile saved;
        saved.name = "Saved portable baseline";
        brasscue::save_profile(portable, saved);
        const auto before = Read(portable);
        brasscue::ControllerSession session;
        auto profile = session.profile();
        profile.name = "Recovered studio";
        profile.fader_label = "SESSION MASTER";
        profile.fixture_groups = {
            {"Unsaved patch", "RGBW", 4, brasscue::FixtureBoundary::Reject, {{2, 469, 11}}}};
        profile.network_interface_id = "saved-interface-not-present";
        profile.encoders[0][0].value = 53;
        session.configure(profile);
        for (unsigned i = 0; i < 4; ++i)
            session.encoder(i, std::uint8_t(23 + i * 58));
        session.fader(89);
        {
            brasscue::RecoveryStore seed(directory);
            seed.save({profile, session.checkpoint(0), {portable.u8string(), true}});
        }
        auto *frame = CreateBrassCueFrame(directory);
        Native(frame);
        CHECK(frame->GetTitle().Contains("Recovered studio *"));
        CHECK(Fader(frame)->GetValue() == 89);
        OutputOff(frame);
        auto snapshot = [&] {
            return brasscue::parse_session(Read(directory / "current.bcsession"));
        };
        CHECK(snapshot().profile.network_interface_id == "saved-interface-not-present");
        for (bool accept : {false, true}) {
            CallAfter([&, accept] {
                wxDialog *dialog = nullptr;
                for (auto *window : wxTopLevelWindows)
                    if (auto *d = wxDynamicCast(window, wxDialog))
                        if (d->IsModal())
                            dialog = d;
                CHECK(dialog);
                wxTextCtrl *label = nullptr;
                for (auto *child : dialog->GetChildren())
                    if (auto *text = wxDynamicCast(child, wxTextCtrl)) {
                        label = text;
                        break;
                    }
                CHECK(label);
                label->SetValue(accept ? "Committed master" : "DRAFT NOT ACCEPTED");
                Pump(2250); // Main-frame autosave continues while a modal draft is open.
                CHECK(snapshot().profile.fader_label == "SESSION MASTER");
                wxCommandEvent event(wxEVT_BUTTON, accept ? wxID_OK : wxID_CANCEL);
                dialog->GetEventHandler()->ProcessEvent(event);
            });
            auto *fader = Fader(frame);
            wxContextMenuEvent edit(wxEVT_CONTEXT_MENU, fader->GetId(), wxDefaultPosition);
            fader->GetEventHandler()->ProcessEvent(edit);
        }
        Pump(1000);
        CHECK(snapshot().profile.fader_label == "Committed master");
        CHECK(snapshot().document.dirty && snapshot().profile.fixture_groups.size() == 1);
        CHECK(Read(portable) == before);
#if BRASSCUE_GUI_INPUT_SUPPORTED
        Fader(frame)->SetFocus();
        Pump();
        GuiInput input;
        CHECK(input.Char(WXK_UP));
        Pump(1200);
        CHECK(Fader(frame)->GetValue() == 90 && snapshot().runtime.values.master == 90);
#endif
        const int level = Fader(frame)->GetValue();
        NoUnexpectedDialog hook;
        hook.Register();
        CHECK(frame->Close());
        Pump(500);
        CHECK(hook.calls ==
              0); // Closing preserves dirty configuration automatically, without a discard prompt.
        frame = CreateBrassCueFrame(directory);
        Native(frame);
        CHECK(frame->GetTitle().Contains("Recovered studio *") &&
              Fader(frame)->GetValue() == level);
        OutputOff(frame);
        CHECK(snapshot().profile.fader_label == "Committed master");
        CHECK(snapshot().runtime.values.rgbw == session.state().rgbw && Read(portable) == before);
        Menu(frame, wxID_SAVE);
        Pump(1000);
        CHECK(hook.calls == 0 && !frame->GetTitle().Contains(" *"));
        CHECK(brasscue::load_profile(portable).fader_label == "Committed master");
        CHECK(!snapshot().document.dirty && snapshot().runtime.values.master == level);
        CHECK(frame->Close());
        Pump(500);
        CHECK(hook.calls == 0);
        hook.Unregister();
        std::filesystem::remove_all(root);
        std::cout << "GUI recovery, unsaved patch, modal isolation, missing NIC, explicit Save and "
                     "close/restart passed\n";
        std::exit(0);
    }
};
wxIMPLEMENT_APP(RecoveryTestApp);
