// BrassCue - GPL-3.0. Transactional profile editor entry point.
#pragma once
#include "../src/controller_profile.hpp"
#include <wx/wx.h>
// 0..3 encoders, 4 fader, 5..7 buttons, 8 screen.
bool EditProfileElement(wxWindow* parent,brasscue::ControlProfile& candidate,int element);
