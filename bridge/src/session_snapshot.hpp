// BrassCue - GPL-3.0. Application-local session format, separate from portable .bcue profiles.
#pragma once
#include "runtime_state.hpp"
#include <string>
namespace brasscue {
struct ProfileDocument {
    std::string path; // UTF-8 association for explicit Save only. Never an autosave destination.
    bool dirty = false;
};
struct SessionSnapshot {
    ControlProfile profile;
    ControllerRuntime runtime;
    ProfileDocument document;
};
std::string serialize_session(const SessionSnapshot &);
SessionSnapshot parse_session(const std::string &);
} // namespace brasscue
