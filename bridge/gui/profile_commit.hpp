// BrassCue - GPL-3.0. One confirmation/cleanup boundary for all GUI patch edits.
#pragma once
#include "../src/controller_session.hpp"
#include "../src/output_session.hpp"
#include <wx/wx.h>
bool CommitProfileEdit(wxWindow *, brasscue::ControllerSession &, brasscue::OutputSession &,
                       brasscue::ControlProfile);
