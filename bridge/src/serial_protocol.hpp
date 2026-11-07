#pragma once

// Version 1 ASCII messages from the Arduino; strict parsing for noisy UART input.
#include "bridge_core.hpp"
#include <cctype>
#include <sstream>
#include <string>

namespace brasscue {

enum class EventType { Ready, Encoder, Button, Grandmaster, StateAck };

struct DeviceEvent {
    EventType type = EventType::Ready;
    int index = 0;
    int value = 0;
};

inline bool decimal(const std::string& token, int maximum, int& value) {
    if (token.empty()) return false;
    int n = 0;
    for (unsigned char c : token) {
        if (c < '0' || c > '9') return false;
        const int digit = int(c - '0');
        if (digit > maximum || n > (maximum - digit) / 10) return false;
        n = n * 10 + int(c - '0');
    }
    value = n;
    return true;
}

inline bool parse_device_message(const std::string& line, DeviceEvent& event) {
    if (line.size() > 80) return false;
    std::istringstream in(line);
    std::string prefix, kind, a, b, extra;
    if (!(in >> prefix >> kind) || prefix != "BC1") return false;

    if (kind == "READY" && !(in >> a)) {
        event = {EventType::Ready, 0, 0};
        return true;
    }
    if (kind == "ACK" && (in >> a) && a == "STATE" && !(in >> b)) {
        event = {EventType::StateAck, 0, 0};
        return true;
    }
    if (kind == "GM" && (in >> a) && !(in >> b)) {
        int value = 0;
        if (!decimal(a, 255, value)) return false;
        event = {EventType::Grandmaster, 0, value};
        return true;
    }
    if ((kind == "ENC" || kind == "BTN") && (in >> a >> b) && !(in >> extra)) {
        int index = 0, value = 0;
        if (!decimal(a, kind == "ENC" ? 3 : 2, index)) return false;
        if (kind == "ENC") {
            if (b == "1") value = 1;
            else if (b == "-1") value = -1;
            else return false;
            event = {EventType::Encoder, index, value};
        } else {
            if (!decimal(b, 1, value)) return false;
            event = {EventType::Button, index, value};
        }
        return true;
    }
    return false;
}

inline bool apply_event(State& state, const DeviceEvent& event, bool ignore_gm) {
    switch (event.type) {
    case EventType::Encoder: {
        if (event.index < 0 || event.index >= 4 || (event.value != -1 && event.value != 1))
            return false;
        const int current = state.rgbw[static_cast<std::size_t>(event.index)];
        const int updated = current + event.value;
        const auto value = static_cast<std::uint8_t>(updated < 0 ? 0 : updated > 255 ? 255 : updated);
        if (value == state.rgbw[static_cast<std::size_t>(event.index)]) return false;
        state.rgbw[static_cast<std::size_t>(event.index)] = value;
        return true;
    }
    case EventType::Grandmaster: {
        if (ignore_gm || event.value < 0 || event.value > 255) return false;
        const auto value = static_cast<std::uint8_t>(event.value);
        if (state.master == value) return false;
        state.master = value;
        return true;
    }
    default: return false;
    }
}

inline std::string serialize_state(const State& state) {
    std::ostringstream out;
    out << "BC1 STATE";
    for (auto component : state.rgbw) out << ' ' << static_cast<unsigned>(component);
    out << ' ' << static_cast<unsigned>(state.master) << '\n';
    return out.str();
}

} // namespace brasscue
