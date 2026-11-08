// BrassCue - GPL-3.0. Native system surfaces may still follow OS appearance policy.
#include "dark_theme.hpp"
#include <wx/grid.h>
#ifdef _WIN32
#include <dwmapi.h>
#endif
void InitializeDarkAppearance() {
#if defined(__WXMSW__) && wxCHECK_VERSION(3, 3, 0)
    wxTheApp->MSWEnableDarkMode(wxApp::DarkMode_Always);
#endif
}
void ApplyDarkTitle(wxTopLevelWindow &window) {
#ifdef _WIN32
    const BOOL dark = TRUE;
    // Official attribute: failure on older Windows builds is nonfatal.
    DwmSetWindowAttribute(reinterpret_cast<HWND>(window.GetHandle()), DWMWA_USE_IMMERSIVE_DARK_MODE,
                          &dark, sizeof(dark));
#else
    (void)window;
#endif
}
void ApplyDarkTheme(wxWindow &window) {
    const wxColour background(22, 24, 29), foreground(238, 232, 219), surface(43, 48, 56);
    window.SetBackgroundColour(background);
    window.SetForegroundColour(foreground);
    if (auto *grid = wxDynamicCast(&window, wxGrid)) {
        grid->SetDefaultCellBackgroundColour(surface);
        grid->SetDefaultCellTextColour(foreground);
        grid->SetLabelBackgroundColour(background);
        grid->SetLabelTextColour(foreground);
        grid->SetGridLineColour(wxColour(75, 79, 87));
    }
    for (auto *child : window.GetChildren())
        ApplyDarkTheme(*child);
    if (auto *top = wxDynamicCast(&window, wxTopLevelWindow))
        ApplyDarkTitle(*top);
}
