// BrassCue - GPL-3.0. Frame factory with an injectable local recovery directory for GUI tests.
#pragma once
#include <filesystem>
#include <wx/wx.h>
wxFrame *CreateBrassCueFrame(const std::filesystem::path &recovery_directory = {});
