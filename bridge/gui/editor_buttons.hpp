// BrassCue - GPL-3.0. Standard dialog order without an implicit accept-on-Enter action.
#pragma once
#include <wx/wx.h>
inline wxSizer *EditorButtons(wxDialog &dialog) {
    // CreateSeparatedButtonSizer(wxOK | wxCANCEL) calls OK->SetDefault().
    // SetDefaultItem(nullptr) does not undo that native GTK default widget.
    // Keep platform button ordering and focused-button keyboard activation instead.
    auto *buttons = new wxStdDialogButtonSizer;
    buttons->AddButton(new wxButton(&dialog, wxID_OK));
    buttons->AddButton(new wxButton(&dialog, wxID_CANCEL));
    buttons->Realize();
    return dialog.CreateSeparatedSizer(buttons);
}
