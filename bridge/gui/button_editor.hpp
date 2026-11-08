// BrassCue - GPL-3.0. A button editor commits one validated candidate profile.
#pragma once
#include "../src/controller_profile.hpp"
#include <wx/wx.h>
bool EditButtonProfile(wxWindow *, brasscue::ControlProfile &, std::size_t index);
