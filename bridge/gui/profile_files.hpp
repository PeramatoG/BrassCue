// BrassCue - GPL-3.0. GUI profile file workflow, separate from model persistence.
#pragma once
#include "../src/controller_profile.hpp"
#include "../src/session_snapshot.hpp"
#include <filesystem>
#include <wx/wx.h>
class ProfileFiles {
  public:
    bool Open(wxWindow *parent, brasscue::ControlProfile &result);
    bool Save(wxWindow *parent, const brasscue::ControlProfile &profile, bool save_as = false);
    bool Discard(wxWindow *parent) const;
    void Changed() {
        dirty_ = true;
    }
    bool dirty() const {
        return dirty_;
    }
    brasscue::ProfileDocument Document() const {
        return {path_.u8string(), dirty_};
    }
    void RestoreDocument(const brasscue::ProfileDocument &document) {
        path_ = std::filesystem::u8path(document.path);
        dirty_ = document.dirty;
    }
    void Defaults() {
        path_.clear();
        dirty_ = true;
    }
    static std::filesystem::path Directory();

  private:
    std::filesystem::path path_;
    bool dirty_ = false;
};
