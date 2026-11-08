// BrassCue - GPL-3.0. Painted vertical fader; ControllerSession owns the live value.
#pragma once
#include <functional>
#include <wx/wx.h>
class ConsoleFader final : public wxControl {
  public:
    ConsoleFader(wxWindow *, wxWindowID, std::function<void(int)>);
    int GetValue() const {
        return value_;
    }
    void SetValue(int);
    void Release();
    void CheckPointer();

  private:
    wxRect Cap() const;
    int Margin() const;
    void MoveTo(int y);
    void Change(int);
    void Paint(wxPaintEvent &);
    int value_ = 255;
    int grab_offset_ = 0;
    std::function<void(int)> changed_;
};
