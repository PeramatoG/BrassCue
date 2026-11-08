// BrassCue - GPL-3.0. See LICENSE for terms and warranty disclaimer.
#include "../src/cue_engine.hpp"
#include "check.hpp"
int main() {
    using namespace brasscue;
    CHECK(from_percent(100) == 255 && to_percent(255) == 100);
    CHECK(from_percent(50) == 128);
    CueEngine engine;
    engine.set_encoder(0, {{{1, 23}, 255}, {{2, 8}, 128}}, 255);
    auto frames = engine.render(0);
    CHECK(frames[1][22] == 255 && frames[2][7] == 128);
    FaderConfig f;
    f.grandmaster = true;
    engine.configure_fader(f);
    engine.set_fader(128);
    frames = engine.render(0);
    CHECK(frames[1][22] == 128);
    f.grandmaster = false;
    f.assignments = {{{3, 7}, 200}};
    engine.configure_fader(f);
    frames = engine.render(0);
    CHECK(frames[1][22] == 255 && frames[3][6] == 100);
    ButtonConfig a;
    a.assignments = {{{4, 10}, 255}};
    a.fade_in_ms = 1000;
    a.fade_out_ms = 1000;
    engine.configure_button(0, a);
    engine.button(0, true, 100);
    CHECK(engine.render(600)[4][9] >= 126 && engine.render(600)[4][9] <= 129);
    engine.button(0, false, 600); // toggle release must not deactivate
    CHECK(engine.render(1100)[4][9] == 255);
    engine.button(0, true, 1200); // toggle off
    CHECK(engine.render(1700)[4][9] >= 126 && engine.render(1700)[4][9] <= 129);
    ButtonConfig b;
    b.assignments = {{{4, 11}, 255}};
    b.mode = ButtonMode::Flash;
    b.kill = true;
    engine.configure_button(1, b);
    engine.button(1, true, 1800);
    CHECK(engine.render(1800)[4][10] == 255);
    CHECK(engine.render(2300)[4][9] == 0);
    engine.button(1, false, 2300);
    CHECK(engine.render(2300)[4][10] == 0);
    auto packet = engine.packet(4, 2300);
    CHECK(packet[14] == 3 && packet[18 + 9] == 0);
    return 0;
}
