// BrassCue - GPL-3.0. Evaluate host-authored calls without wx's eval-based script wrapper.
#pragma once
#include <wx/webview.h>
void RunPanelScript(wxWebView &, const wxString &host_script, void *client_data = nullptr);
