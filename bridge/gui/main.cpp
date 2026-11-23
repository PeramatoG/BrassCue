// BrassCue - GPL-3.0. See LICENSE for terms and warranty disclaimer.
#include "dark_theme.hpp"
#include "main_frame.hpp"
#include <wx/wx.h>

class BrassCueApp final : public wxApp {
  public:
    bool OnInit() override {
        std::filesystem::path smoke_directory;
        if (argc == 3 && wxString(argv[1]) == "--runtime-smoke")
            smoke_directory = std::filesystem::u8path(wxString(argv[2]).utf8_string());
        InitializeDarkAppearance();
        auto *frame = CreateBrassCueFrame(smoke_directory);
        frame->Show();
        if (!smoke_directory.empty())
            CallAfter([frame] { frame->Close(true); });
        return true;
    }
};

wxIMPLEMENT_APP(BrassCueApp);
