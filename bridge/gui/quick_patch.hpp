// BrassCue - GPL-3.0. Transactional group editor; no output or session ownership.
#pragma once
#include "../src/controller_profile.hpp"
#include <wx/wx.h>
namespace QuickPatchId {
enum {
    Groups = wxID_HIGHEST + 800,
    Name,
    Order,
    Stride,
    Blocks,
    Preview,
    Add,
    Remove,
    AddBlock,
    RemoveBlock,
    Update
};
}
bool EditQuickRgbwPatch(wxWindow *, brasscue::ControlProfile &);
