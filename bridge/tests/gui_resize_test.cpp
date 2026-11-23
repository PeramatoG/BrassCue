// BrassCue - GPL-3.0. Resize the native frame without changing controller values.
#include "../gui/main_frame.hpp"
#include "../gui/widgets.hpp"
#include "check.hpp"
#include <array>
#include <chrono>
#include <wx/scrolwin.h>

namespace {
void Pump() {
    for (int i = 0; i < 10; ++i) {
        wxYield();
        wxMilliSleep(10);
    }
}

wxRect ScreenRect(wxWindow &window) {
    return {window.ClientToScreen(wxPoint{}), window.GetClientSize()};
}

template <std::size_t N> void CheckLayout(const std::array<wxWindow *, N> &controls) {
    for (std::size_t i = 0; i < N; ++i) {
        const auto bounds = controls[i]->GetRect();
        CHECK(controls[i]->GetParent()->GetClientRect().Contains(bounds));
        const auto minimum = controls[i]->GetEffectiveMinSize();
        CHECK(bounds.width >= minimum.x && bounds.height >= minimum.y);
        for (std::size_t j = 0; j < i; ++j)
            CHECK(!bounds.Intersects(controls[j]->GetRect()));
    }
}

class ResizeTestApp final : public wxApp {
  public:
    bool OnInit() override {
        SetExitOnFrameDelete(false);
        CallAfter([this] { Run(); });
        return true;
    }

  private:
    void Run() {
        const auto recovery =
            std::filesystem::temp_directory_path() /
            ("brasscue-resize-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        auto *frame = CreateBrassCueFrame(recovery, false);
        frame->Move(0, 0);
        frame->SetClientSize(frame->FromDIP(wxSize(780, 760)));
        frame->Show();
        Pump();

        auto *body = dynamic_cast<wxScrolledWindow *>(frame->GetChildren().GetFirst()->GetData());
        auto *output =
            wxDynamicCast(wxWindow::FindWindowByLabel("Enable Art-Net", frame), wxCheckBox);
        CHECK(body && output && !output->GetValue());
        CHECK(ScreenRect(*body).Contains(ScreenRect(*output)));
        std::array<wxWindow *, 9> controls{};
        for (unsigned i = 0; i < controls.size(); ++i) {
            controls[i] = frame->FindWindow(wxID_HIGHEST + 100 + int(i));
            CHECK(controls[i]);
        }
        CheckLayout(controls);
        auto *preview = dynamic_cast<ColorPreview *>(controls.back());
        CHECK(preview);
        brasscue::ScreenViewData levels;
        levels.mode = brasscue::ScreenView::Levels;
        levels.labels = {"Red", "Green", "Blue", "White"};
        levels.levels = {100, 80, 60, 40};
        preview->SetView(levels);
        Pump();

#ifdef __WXMSW__
        // The exposed strip alone used to be painted on MSW. Demand damage
        // over the whole new client rectangle without changing any values.
        std::array<bool, 9> redrawn{};
        for (std::size_t i = 0; i < controls.size(); ++i)
            controls[i]->Bind(wxEVT_PAINT, [&, i](wxPaintEvent &event) {
                const auto &control = *controls[i];
                redrawn[i] |=
                    control.GetUpdateRegion().Contains(control.GetClientRect()) == wxInRegion;
                event.Skip();
            });
#endif
        const auto old_knob = controls.front()->GetSize();
        frame->SetClientSize(frame->FromDIP(wxSize(960, 830)));
        Pump();
        CHECK(controls.front()->GetSize().x > old_knob.x);
        CheckLayout(controls);
#ifdef __WXMSW__
        for (bool full_paint : redrawn)
            CHECK(full_paint);
        frame->Maximize(true);
        Pump();
        CHECK(frame->IsMaximized());
        CheckLayout(controls);
        frame->Maximize(false);
        Pump();
#endif

        frame->SetClientSize(frame->FromDIP(wxSize(520, 380)));
        Pump();
        CHECK(body->GetScrollRange(wxVERTICAL) > body->GetScrollThumb(wxVERTICAL));
        CheckLayout(controls);
        body->Scroll(body->GetScrollRange(wxHORIZONTAL), body->GetScrollRange(wxVERTICAL));
        Pump();
        CHECK(ScreenRect(*body).Contains(ScreenRect(*output)));
        CHECK(ScreenRect(*body).Contains(ScreenRect(*controls[7])));

        frame->SetClientSize(frame->FromDIP(wxSize(780, 760)));
        Pump();
        CHECK(body->GetScrollRange(wxVERTICAL) <= body->GetScrollThumb(wxVERTICAL));
        CHECK(ScreenRect(*body).Contains(ScreenRect(*output)));
        CHECK(!output->GetValue());
        CheckLayout(controls);
        frame->Close(true);
        Pump();
        std::filesystem::remove_all(recovery);
        ExitMainLoop();
    }
};
} // namespace

wxIMPLEMENT_APP(ResizeTestApp);
