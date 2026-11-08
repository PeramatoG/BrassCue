// BrassCue - GPL-3.0. C++ supplies effective levels, percentages and additive preview color.
#include "view_snapshot.hpp"
#include <algorithm>
#include <sstream>
namespace brasscue {
ViewSnapshot make_snapshot(const ControllerSession &s, const FourDigitDisplay &display,
                           std::uint64_t now, const std::string &connection) {
    ViewSnapshot v;
    const auto &p = s.profile();
    const auto &state = s.state();
    for (unsigned i = 0; i < 4; ++i)
        v.encoders[i] = {p.encoder_labels[i], state.rgbw[i], to_percent(state.rgbw[i]),
                         -135 + int((unsigned(state.rgbw[i]) * 270 + 127) / 255)};
    for (unsigned i = 0; i < 3; ++i)
        v.buttons[i] = {p.button_labels[i], s.button_status(i, now), s.button_selected(i, now)};
    v.fader_label = p.fader_label;
    v.fader = s.fader_value();
    v.fader_percent = to_percent(v.fader);
    v.master_percent = to_percent(state.master);
    v.display = display.text(now);
    v.connection = connection;
    v.screen.title = p.screen_title;
    v.screen.mode = p.screen_view;
    v.screen.labels = p.encoder_labels;
    const auto white = apply_master(state.rgbw[3], state.master);
    for (unsigned i = 0; i < 4; ++i) {
        const auto value = apply_master(state.rgbw[i], state.master);
        v.screen.levels[i] = to_percent(value);
        if (i < 3)
            v.screen.color[i] = std::uint8_t(std::min(255u, unsigned(value) + white));
    }
    return v;
}
ViewChanges view_changes(const ViewSnapshot *p, const ViewSnapshot &c) {
    ViewChanges changes;
    for (unsigned i = 0; i < 4; ++i)
        changes.encoders[i] = !p || !(p->encoders[i] == c.encoders[i]);
    for (unsigned i = 0; i < 3; ++i)
        changes.buttons[i] = !p || !(p->buttons[i] == c.buttons[i]);
    changes.fader = !p || p->fader != c.fader || p->fader_label != c.fader_label ||
                    p->master_percent != c.master_percent;
    changes.display = !p || p->display != c.display;
    changes.screen = !p || !(p->screen == c.screen);
    changes.connection = !p || p->connection != c.connection;
    return changes;
}
std::string json_string(const std::string &text) {
    std::string out = "\"";
    const char *hex = "0123456789abcdef";
    for (std::size_t i = 0; i < text.size(); ++i) {
        unsigned char c = text[i];
        if (c == '"' || c == '\\') {
            out += '\\';
            out += char(c);
        } else if (c < 32 || c == '<') {
            out += "\\u00";
            out += hex[c >> 4];
            out += hex[c & 15];
        } else if (c == 0xe2 && i + 2 < text.size() &&
                   static_cast<unsigned char>(text[i + 1]) == 0x80 &&
                   (static_cast<unsigned char>(text[i + 2]) == 0xa8 ||
                    static_cast<unsigned char>(text[i + 2]) == 0xa9)) {
            out += static_cast<unsigned char>(text[i + 2]) == 0xa8 ? "\\u2028" : "\\u2029";
            i += 2;
        } else
            out += char(c);
    }
    return out + '"';
}
std::string snapshot_json(const ViewSnapshot *previous, const ViewSnapshot &v) {
    const auto changed = view_changes(previous, v);
    if (!changed.any())
        return {};
    std::ostringstream out;
    out << "{\"version\":1";
    if (changed.fader)
        out << ",\"fader\":{\"label\":" << json_string(v.fader_label)
            << ",\"value\":" << unsigned(v.fader) << ",\"percent\":" << v.fader_percent
            << ",\"master\":" << v.master_percent << '}';
    if (changed.display)
        out << ",\"display\":" << json_string(v.display);
    if (changed.connection)
        out << ",\"connection\":" << json_string(v.connection);
    out << ",\"encoders\":[";
    bool first = true;
    for (unsigned i = 0; i < 4; ++i)
        if (changed.encoders[i]) {
            if (!first)
                out << ',';
            first = false;
            const auto &e = v.encoders[i];
            out << "{\"index\":" << i << ",\"label\":" << json_string(e.label)
                << ",\"value\":" << unsigned(e.value) << ",\"percent\":" << e.percent
                << ",\"angle\":" << e.angle << '}';
        }
    out << "],\"buttons\":[";
    first = true;
    for (unsigned i = 0; i < 3; ++i)
        if (changed.buttons[i]) {
            if (!first)
                out << ',';
            first = false;
            const auto &b = v.buttons[i];
            out << "{\"index\":" << i << ",\"label\":" << json_string(b.label)
                << ",\"selected\":" << (b.selected ? "true" : "false")
                << ",\"phase\":" << json_string(b.phase) << '}';
        }
    out << ']';
    if (changed.screen) {
        const auto &t = v.screen;
        out << ",\"screen\":{\"title\":" << json_string(t.title) << ",\"mode\":" << int(t.mode)
            << ",\"color\":[" << unsigned(t.color[0]) << ',' << unsigned(t.color[1]) << ','
            << unsigned(t.color[2]) << "],\"levels\":[";
        for (unsigned i = 0; i < 4; ++i) {
            if (i)
                out << ',';
            out << t.levels[i];
        }
        out << "],\"labels\":[";
        for (unsigned i = 0; i < 4; ++i) {
            if (i)
                out << ',';
            out << json_string(t.labels[i]);
        }
        out << "]}";
    }
    out << '}';
    return out.str();
}
} // namespace brasscue
