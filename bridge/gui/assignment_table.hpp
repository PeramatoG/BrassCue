// BrassCue - GPL-3.0. Transactional row editor shared by DMX and internal actions.
#pragma once
#include "../src/assignment_editor.hpp"
#include <wx/grid.h>
#include <wx/wx.h>
class AssignmentTable final : public wxPanel {
  public:
    enum { UnitsId = wxID_HIGHEST + 300, GridId, AddId, RemoveId, AddRgbwId };
    AssignmentTable(wxWindow *, std::vector<brasscue::Assignment>,
                    std::vector<brasscue::ControlValueAssignment>, brasscue::ValueDisplay,
                    bool internal = false);
    void SetInternal(bool);
    bool Internal() const {
        return internal_;
    }
    void Read();
    const auto &Dmx() const {
        return dmx_;
    }
    const auto &Controls() const {
        return controls_;
    }
    brasscue::ValueDisplay Units() const {
        return percent_ ? brasscue::ValueDisplay::Percent : brasscue::ValueDisplay::Dmx255;
    }

  private:
    void Fill();
    void Add(bool rgbw);
    void Remove();
    void Error(const std::exception &);
    wxGrid *grid_;
    wxChoice *units_;
    wxButton *rgbw_;
    std::vector<brasscue::Assignment> dmx_;
    std::vector<brasscue::ControlValueAssignment> controls_;
    std::vector<bool> edited_;
    bool percent_ = false, internal_ = false;
};
