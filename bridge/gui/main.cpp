// BrassCue - GPL-3.0. See LICENSE for terms and warranty disclaimer.
#include "dark_theme.hpp"
#include "main_frame.hpp"
#include <wx/wx.h>

class BrassCueApp final : public wxApp {
  public:
    bool OnInit() override {
        InitializeDarkAppearance();
        auto *frame = CreateBrassCueFrame();
        frame->Show();
        return true;
    }
};

wxIMPLEMENT_APP(BrassCueApp);
