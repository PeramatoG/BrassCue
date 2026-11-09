// BrassCue - GPL-3.0. Explicit portable file saving stays independent from session recovery.
#include "profile_files.hpp"
#include "../src/profile_io.hpp"
#include <wx/filedlg.h>
#include <wx/stdpaths.h>
std::filesystem::path ProfileFiles::Directory() {
#ifdef _WIN32
    return std::filesystem::path(wxStandardPaths::Get().GetUserDataDir().ToStdWstring()) /
           L"profiles";
#else
    return std::filesystem::path(wxStandardPaths::Get().GetUserDataDir().ToStdString()) /
           "profiles";
#endif
}
namespace {
std::filesystem::path Path(const wxString &text) {
#ifdef _WIN32
    return text.ToStdWstring();
#else
    return text.ToStdString();
#endif
}
} // namespace
bool ProfileFiles::Discard(wxWindow *parent) const {
    return !dirty_ ||
           wxMessageBox(
               "Replace unsaved portable-profile changes?\n"
               "Automatic session recovery is separate from Save and will follow the replacement.",
               "Profile", wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, parent) == wxYES;
}
bool ProfileFiles::Open(wxWindow *parent, brasscue::ControlProfile &result) {
    if (!Discard(parent))
        return false;
    wxFileDialog dlg(parent, "Open / import profile", wxString(Directory().wstring()), "",
                     "BrassCue (*.bcue)|*.bcue", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK)
        return false;
    try {
        auto path = Path(dlg.GetPath());
        auto profile = brasscue::load_profile(path);
        result = std::move(profile);
        path_ = std::move(path);
        dirty_ = false;
        return true;
    } catch (const std::exception &e) {
        wxMessageBox(wxString::FromUTF8(e.what()), "Profile error", wxOK | wxICON_ERROR, parent);
        return false;
    }
}
bool ProfileFiles::Save(wxWindow *parent, const brasscue::ControlProfile &profile, bool save_as) {
    auto path = path_;
    if (save_as || path.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(Directory(), ec);
        wxFileDialog dlg(parent, "Save / export profile", wxString(Directory().wstring()),
                         "profile.bcue", "BrassCue (*.bcue)|*.bcue",
                         wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (dlg.ShowModal() != wxID_OK)
            return false;
        path = Path(dlg.GetPath());
    }
    try {
        brasscue::save_profile(path, profile);
        path_ = std::move(path);
        dirty_ = false;
        return true;
    } catch (const std::exception &e) {
        wxMessageBox(wxString::FromUTF8(e.what()), "Profile error", wxOK | wxICON_ERROR, parent);
        return false;
    }
}
