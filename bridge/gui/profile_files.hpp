// BrassCue - GPL-3.0. GUI profile file workflow, separate from model persistence.
#pragma once
#include "../src/controller_profile.hpp"
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
    static std::filesystem::path Directory();

  private:
    std::filesystem::path path_;
    bool dirty_ = false;
};
