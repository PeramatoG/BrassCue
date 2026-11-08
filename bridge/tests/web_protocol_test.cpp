// BrassCue - GPL-3.0. Actual frontend input contract with a real Core session, no browser mock
// engine.
#include "../gui/presentation/web_protocol.hpp"
#include "../src/controller_session.hpp"
#include "check.hpp"
int main() {
    using namespace brasscue;
    ControllerSession s;
    int presses = 0, edits = 0, releases = 0;
    ControllerActions actions;
    actions.encoder_delta = [&](std::size_t i, int delta) { s.encoder_delta(i, delta, 0); };
    actions.fader = [&](std::uint8_t value) { s.fader(value); };
    actions.button = [&](std::size_t i, bool down) {
        if (down)
            ++presses;
        s.button(i, down, 0);
    };
    actions.edit = [&](int i) {
        CHECK(i == 5);
        ++edits;
    };
    actions.release = [&] {
        ++releases;
        s.release_all(0);
    };
    WebInputBridge bridge(actions);
    bridge.reset(42);
    CHECK(!bridge.accept("BCW1 42 1 fader 128"));
    CHECK(bridge.accept("BCW1 42 1 ready"));
    CHECK(bridge.accept("BCW1 42 2 fader 66"));
    CHECK(bridge.accept("BCW1 42 3 button 1 1 0"));
    CHECK(s.state().master == 128);
    CHECK(!bridge.accept("BCW1 42 3 button 1 1 0"));
    CHECK(bridge.accept("BCW1 42 4 button 1 1 0"));
    bridge.reconcile_mouse(false);
    CHECK(presses == 1);
    CHECK(bridge.accept("BCW1 42 5 button 1 0 0"));
    CHECK(bridge.accept("BCW1 42 6 wheel 0 1"));
    CHECK(s.state().rgbw[0] == 1);
    for (auto bad : {"BCW1 41 7 fader 0", "BCW1 42 7 fader 256", "BCW1 42 7 button 3 1 0",
                     "BCW1 42 7 wheel 4 1", "BCW1 42 7 key 0 4", "BCW1 42 7 drag-start 0 nan 1",
                     "BCW1 42 7 drag-move 0 5 1", "BCW1 42 7 fader 2 garbage", "BCW1 42 7 ready",
                     "BCW1 42 7 eval alert(1)", "BCW1 42 7 fader -1"})
        CHECK(!bridge.accept(bad));
    CHECK(!bridge.accept(std::string(257, 'x')));
    CHECK(s.state().master == 128 && presses == 1);
    CHECK(bridge.accept("BCW1 42 7 button 0 1 0"));
    CHECK(bridge.accept("BCW1 42 8 button 0 1 1"));
    const auto held_presses = presses;
    bridge.reconcile_mouse(false);
    CHECK(presses == held_presses);
    CHECK(bridge.accept("BCW1 42 9 button 0 0 0"));
    CHECK(bridge.accept("BCW1 42 10 edit 5"));
    CHECK(edits == 1);
    bridge.reset(43);
    CHECK(!bridge.ready() && !bridge.accept("BCW1 42 8 fader 0"));
    CHECK(bridge.accept("BCW1 43 1 ready"));
    CHECK(bridge.accept("BCW1 43 2 release"));
    CHECK(releases >= 3);
    CHECK(allowed_panel_navigation("file:///app/compact.html", "file:///app/compact.html"));
    CHECK(!allowed_panel_navigation("https://example.com", "file:///app/compact.html"));
    CHECK(!allowed_panel_navigation("file:///etc/passwd", "file:///app/compact.html"));
}
