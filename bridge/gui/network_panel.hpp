// BrassCue - GPL-3.0. Network preferences/draft UI; adapters and output stay outside widgets.
#pragma once
#include "../src/controller_session.hpp"
#include "../src/network_interfaces.hpp"
#include <functional>
#include <wx/radiobox.h>
#include <wx/wx.h>
class NetworkPanel final : public wxPanel {
  public:
    NetworkPanel(wxWindow *, std::function<void()> changed, std::function<void(bool)> requested);
    void Load(const brasscue::ControlProfile &);
    void WritePreferences(brasscue::ControllerSession &) const;
    brasscue::ResolvedNetwork Resolve() const;
    void SetOutput(bool enabled);

  private:
    void RefreshInterfaces(bool notify);
    void UpdateDestination();
    void Changed();
    std::string SelectedId() const;
    wxChoice *interfaces_ = nullptr;
    wxRadioBox *mode_ = nullptr;
    wxTextCtrl *ip_ = nullptr;
    wxTextCtrl *target_ = nullptr;
    wxCheckBox *enable_ = nullptr;
    wxStaticText *hint_ = nullptr;
    wxTimer timer_;
    std::vector<brasscue::NetworkInterface> adapters_;
    std::vector<std::string> ids_;
    std::string wanted_, fingerprint_ = "\x01";
    std::function<void()> changed_;
    std::function<void(bool)> requested_;
};
