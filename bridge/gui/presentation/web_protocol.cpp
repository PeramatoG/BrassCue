// BrassCue - GPL-3.0. Parse fully before dispatch; reject stale, repeated or malformed input.
#include "web_protocol.hpp"
#include <charconv>
#include <cmath>
#include <limits>
#include <sstream>
#include <vector>
namespace brasscue {
namespace {
bool integer(const std::string &text, std::uint64_t max, std::uint64_t &value) {
    auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return !text.empty() && result.ec == std::errc{} && result.ptr == text.data() + text.size() &&
           value <= max;
}
bool coordinate(const std::string &text, double &value) {
    auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    return !text.empty() && result.ec == std::errc{} && result.ptr == text.data() + text.size() &&
           std::isfinite(value) && std::abs(value) <= 4;
}
} // namespace
void WebInputBridge::release_inputs() {
    for (auto &g : gestures_)
        g.end();
    buttons_.fill(0);
    if (actions_.release)
        actions_.release();
}
void WebInputBridge::reconcile_mouse(bool down) {
    if (down)
        return;
    for (std::size_t i = 0; i < buttons_.size(); ++i) {
        const auto previous = buttons_[i];
        buttons_[i] &= ~2u;
        if (previous && !buttons_[i] && actions_.button)
            actions_.button(i, false);
    }
    for (auto &g : gestures_)
        g.end();
}
void WebInputBridge::reset(std::uint64_t epoch) {
    release_inputs();
    epoch_ = epoch;
    last_sequence_ = 0;
    ready_ = false;
}
bool WebInputBridge::accept(const std::string &record) {
    if (record.empty() || record.size() > 256)
        return false;
    for (unsigned char c : record)
        if (c < 32 || c > 126)
            return false;
    std::istringstream in(record);
    std::vector<std::string> t;
    std::string word;
    while (in >> word)
        t.push_back(word);
    if (t.size() < 4 || t[0] != "BCW1")
        return false;
    std::uint64_t epoch = 0, sequence = 0, index = 0, value = 0, source = 0;
    if (!integer(t[1], std::numeric_limits<std::uint64_t>::max(), epoch) || !epoch ||
        epoch != epoch_ || !integer(t[2], 0xffffffffu, sequence) || sequence <= last_sequence_)
        return false;
    const auto &kind = t[3];
    if (kind == "ready") {
        if (t.size() != 4 || ready_ || sequence != 1)
            return false;
        ready_ = true;
        last_sequence_ = sequence;
        return true;
    }
    if (!ready_)
        return false;
    double x = 0, y = 0;
    int delta = 0;
    if (kind == "release") {
        if (t.size() != 4)
            return false;
    } else if (kind == "fader" || kind == "edit") {
        if (t.size() != 5 || !integer(t[4], kind == "fader" ? 255 : 8, value))
            return false;
    } else if (kind == "drag-start" || kind == "drag-move") {
        if (t.size() != 7 || !integer(t[4], 3, index) || !coordinate(t[5], x) ||
            !coordinate(t[6], y))
            return false;
    } else if (kind == "drag-end") {
        if (t.size() != 5 || !integer(t[4], 3, index))
            return false;
    } else if (kind == "button" || kind == "key" || kind == "wheel") {
        if (t.size() != (kind == "button" ? 7u : 6u) ||
            !integer(t[4], kind == "button" ? 2 : 3, index) ||
            (kind == "button" && !integer(t[6], 2, source)))
            return false;
        if (kind == "wheel") {
            if (t[5] == "1")
                delta = 1;
            else if (t[5] == "-1")
                delta = -1;
            else
                return false;
        } else if (!integer(t[5], kind == "button" ? 1 : 3, value))
            return false;
    } else
        return false;
    last_sequence_ = sequence;
    if (kind == "release")
        release_inputs();
    else if (kind == "edit") {
        release_inputs();
        if (actions_.edit)
            actions_.edit(int(value));
    } else if (kind == "fader") {
        if (actions_.fader)
            actions_.fader(std::uint8_t(value));
    } else if (kind == "button") {
        const bool previous = buttons_[index] != 0;
        if (value)
            buttons_[index] |= 1u << source;
        else
            buttons_[index] &= ~(1u << source);
        const bool down = buttons_[index] != 0;
        if (previous != down && actions_.button)
            actions_.button(index, down);
    } else if (kind == "drag-start")
        gestures_[index].begin(x, y);
    else if (kind == "drag-end")
        gestures_[index].end();
    else {
        if (kind == "drag-move")
            delta = gestures_[index].move(x, y);
        else if (kind == "key") {
            constexpr int steps[]{1, -1, 10, -10};
            delta = steps[value];
        }
        if (delta && actions_.encoder_delta)
            actions_.encoder_delta(index, delta);
    }
    return true;
}
bool allowed_panel_navigation(const std::string &url, const std::string &main_url) {
    return !main_url.empty() && url == main_url;
}
} // namespace brasscue
