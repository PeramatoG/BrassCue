// BrassCue - GPL-3.0. Versioned profile interchange and atomic file writes.
#pragma once
#include "controller_profile.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#endif
namespace brasscue {
std::string serialize_profile(const ControlProfile &profile);
ControlProfile parse_profile(const std::string &text);
void save_profile(const std::filesystem::path &path, const ControlProfile &profile);
ControlProfile load_profile(const std::filesystem::path &path);
} // namespace brasscue
