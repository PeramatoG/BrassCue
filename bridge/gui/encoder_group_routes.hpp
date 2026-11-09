// BrassCue - GPL-3.0. Read-only visibility of the group routes already assigned to an encoder.
#pragma once
#include "../src/controller_profile.hpp"
#include <wx/wx.h>
constexpr int EncoderGroupRoutesId = wxID_HIGHEST + 900;
wxWindow *CreateEncoderGroupRoutes(wxWindow *, const brasscue::ControlProfile &, unsigned color);
