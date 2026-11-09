// BrassCue - GPL-3.0. Reconstruct controller/ownership/envelopes, never replay input or merged
// frames.
#include "../src/controller_session.hpp"
#include "../src/session_snapshot.hpp"
#include "check.hpp"
#include <limits>
namespace {
using namespace brasscue;
template <class F> void Reject(F action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::exception &) {
        rejected = true;
    }
    CHECK(rejected);
}
void Press(ControllerSession &s, unsigned i, std::uint64_t now) {
    s.button(i, true, now);
    s.button(i, false, now);
}
ControllerSession RoundTrip(const ControllerSession &s, std::uint64_t now) {
    SessionSnapshot data{s.profile(), s.checkpoint(now), {"unsaved profile.bcue", true}};
    auto text = serialize_session(data);
    const auto parsed = parse_session(text);
    CHECK(serialize_session(parsed) == text && parsed.document.dirty);
    ControllerSession recovered;
    recovered.restore(parsed.profile, parsed.runtime);
    recovered.advance(
        999999999999ULL); // Neither wall-clock downtime nor a new monotonic origin advances a fade.
    return recovered;
}
ControlProfile Mixed() {
    ControlProfile p;
    p.fixture_groups = {{"Recovered house", "GRBW", 4, FixtureBoundary::Reject, {{2, 469, 11}}}};
    p.encoders[0][0].value = 53;
    p.buttons[0].controls = {{ControlTarget::Grandmaster, 180}, {ControlTarget::Red, 200}};
    p.buttons[1].controls = {{ControlTarget::Red, 40}};
    auto &cue = p.buttons[2];
    cue.action = ButtonActionKind::DmxCue;
    cue.controls.clear();
    cue.assignments = {{{1, 1}, 128}, {{3, 7}, 234}};
    for (auto &b : p.buttons) {
        b.kill = false;
        b.fade_in_ms = 1000;
        b.fade_out_ms = 1000;
    }
    return p;
}
} // namespace
int main() {
    using namespace brasscue;
    ControllerSession s;
    for (unsigned i = 0; i < 4; ++i)
        s.encoder(i, std::uint8_t(23 + 57 * i));
    s.fader(137);
    auto stable = RoundTrip(s, 20);
    CHECK(stable.state().rgbw == s.state().rgbw && stable.state().master == 137);
    CHECK(stable.engine().render(0) == s.engine().render(20));
    CHECK(!stable.physical_fader());
    for (unsigned i = 0; i < 3; ++i) {
        ControllerSession preset;
        preset.fader(73);
        Press(preset, i, 0);
        auto restored = RoundTrip(preset, 0);
        CHECK(restored.button_selected(i, 0));
        CHECK(restored.state().master == preset.state().master);
        CHECK(restored.engine().render(0) == preset.engine().render(0));
        // One real click after recovery toggles off; no synthetic down edge or Kill on restore.
        Press(restored, i, 1);
        CHECK(restored.state().master == 0);
    }
    ControllerSession held_key;
    held_key.fader(73);
    held_key.button(0, true, 0);
    auto no_pressed_key = RoundTrip(held_key, 0);
    no_pressed_key.button(0, true, 100);
    CHECK(no_pressed_key.state().master ==
          0); // A checkpointed Toggle down is not a stuck input hold.
    auto mixed = Mixed();
    s.configure(mixed);
    s.fader(100);
    s.encoder(0, 0);
    Press(s, 0, 0);
    s.advance(500);
    Press(s, 1, 500);
    Press(s, 2, 500);
    s.advance(750);
    const auto before = s.engine().render(750);
    const auto checkpoint = s.checkpoint(750);
    CHECK(checkpoint.internal.owners[0] == 0 && checkpoint.internal.owners[1] == 1);
    CHECK(checkpoint.internal.selected[0] && checkpoint.internal.selected[1]);
    CHECK(checkpoint.dmx.selected[2] && checkpoint.dmx.levels[2] == 0.25);
    auto frozen = RoundTrip(s, 750);
    CHECK(frozen.engine().render(0) == before && frozen.state().master == s.state().master);
    CHECK(frozen.state().rgbw == s.state().rgbw && frozen.profile().encoders[0][0].value == 53);
    CHECK(std::string(frozen.button_status(2, 0)) == "HELD");
    CHECK(frozen.button_selected(0, 0) && frozen.button_selected(1, 0) &&
          frozen.button_selected(2, 0));
    frozen.encoder(0, 10, 1000000000000ULL);
    CHECK(!frozen.button_selected(1, 0) && !frozen.button_selected(0, 0));
    Press(frozen, 2, 1000000000000ULL);
    frozen.advance(1000000001000ULL);
    CHECK(!frozen.button_selected(2, 0) && frozen.engine().render(1000000001000ULL)[3][6] == 0);
    // Patch-mode position and logical GM are separate and must both survive.
    mixed.fader.grandmaster = false;
    mixed.fader.assignments = {{{4, 1}, 200}};
    s.configure(mixed);
    s.fader(77);
    Press(s, 0, 1000);
    s.advance(1500);
    auto patch = RoundTrip(s, 1500);
    CHECK(patch.fader_value() == 77 && patch.state().master == s.state().master);
    CHECK(patch.engine().render(0) == s.engine().render(1500));
    // Internal fade passing an idle preset's exact byte must not manufacture selection on recovery.
    ControllerSession passing;
    auto p = passing.profile();
    p.buttons[0].fade_in_ms = 2000;
    passing.configure(p);
    passing.fader(0);
    Press(passing, 0, 0);
    passing.advance(1000);
    CHECK(passing.state().master == 128 && !passing.button_selected(1, 1000));
    auto held = RoundTrip(passing, 1000);
    CHECK(held.state().master == 128 && held.button_selected(0, 0) && !held.button_selected(1, 0));
    // Both outgoing and incoming DMX contributions survive independently, including HTP overlaps.
    ControllerSession cues;
    auto cp = cues.profile();
    for (unsigned i = 0; i < 3; ++i) {
        auto &b = cp.buttons[i];
        b.action = ButtonActionKind::DmxCue;
        b.controls.clear();
        b.assignments = {{{5, 1}, std::uint8_t(255 - i * 80)}};
        b.fade_in_ms = b.fade_out_ms = 1000;
    }
    cues.configure(cp);
    Press(cues, 0, 0);
    Press(cues, 1, 500);
    cues.advance(750);
    auto outgoing = RoundTrip(cues, 750);
    CHECK(outgoing.engine().render(0) == cues.engine().render(750));
    CHECK(!outgoing.button_selected(0, 0) && outgoing.button_selected(1, 0));
    CHECK(outgoing.checkpoint(0).dmx.levels[0] == 0.375);
    Press(outgoing, 2, 1000000000000ULL);
    outgoing.advance(1000000001000ULL);
    CHECK(outgoing.checkpoint(1000000001000ULL).dmx.levels[0] == 0);
    CHECK(outgoing.engine().render(1000000001000ULL)[5][0] == 95);
    ControllerSession precision;
    auto precise = cp;
    precise.buttons[0].assignments = {{{6, 1}, 255}, {{6, 2}, 128}};
    precision.configure(precise);
    Press(precision, 0, 0);
    auto fractional = RoundTrip(precision, 3);
    CHECK(fractional.engine().render(0)[6][0] == 1 && fractional.engine().render(0)[6][1] == 0);
    CHECK(fractional.checkpoint(0).dmx.levels[0] ==
          0.003); // Byte-quantizing the envelope would incorrectly light channel 2.
    // Flash recovery releases only still-owned controls; newer ownership is never undone.
    ControllerSession flash;
    auto fp = flash.profile();
    fp.buttons[0].mode = ButtonMode::Flash;
    fp.buttons[0].kill = false;
    fp.buttons[0].controls = {{ControlTarget::Grandmaster, 255}, {ControlTarget::Red, 255}};
    fp.buttons[1].kill = false;
    fp.buttons[1].controls = {{ControlTarget::Red, 40}};
    fp.buttons[0].fade_out_ms = 1000;
    flash.configure(fp);
    flash.fader(80);
    flash.encoder(0, 20);
    flash.button(0, true, 0);
    Press(flash, 1, 10);
    auto released = RoundTrip(flash, 10);
    CHECK(released.state().master == 80 && released.state().rgbw[0] == 40);
    CHECK(!released.button_selected(0, 0) && released.button_selected(1, 0));
    // Capturing a checkpoint must never release live input in the original session.
    CHECK(flash.state().master == 255);
    released.button(0, true, 100);
    CHECK(released.state().master == 255);
    flash.button(0, false, 20);
    flash.advance(520);
    auto released_fade = RoundTrip(flash, 520);
    CHECK(released_fade.state().master == 80);
    cp.buttons[0].mode = ButtonMode::Flash;
    cues.configure(cp);
    cues.button(0, true, 0);
    cues.advance(500);
    auto no_flash = RoundTrip(cues, 500);
    CHECK(no_flash.engine().render(0)[5][0] == 0 && !no_flash.button_selected(0, 0));
    // Runtime validation rejects corruption and incompatible ownership transactionally.
    auto good = s.checkpoint(1500);
    const auto saved_frames = patch.engine().render(0);
    auto bad = good;
    bad.internal.owners[1] = 2; // DMX cue cannot own Red.
    Reject([&] { patch.restore(mixed, bad); });
    CHECK(patch.engine().render(0) == saved_frames);
    bad = good;
    bad.dmx.levels[2] = std::numeric_limits<double>::quiet_NaN();
    Reject([&] { serialize_session({mixed, bad, {}}); });
    bad = good;
    bad.dmx.levels[2] = 1.1;
    Reject([&] { patch.restore(mixed, bad); });
    const auto text = serialize_session({s.profile(), good, {"C:/profiles/ámb ar.bcue", true}});
    for (auto invalid :
         {text + "junk", text.substr(0, text.size() - 1), std::string("BRASSCUE_SESSION 2\n")})
        Reject([&] { parse_session(invalid); });
    auto corrupt = text;
    corrupt.back() = 'x';
    Reject([&] { parse_session(corrupt); });
    CHECK(s.engine().packet(2, 1500)[14] ==
          1); // Restored logical universes still use normal Art-Net numbering.
}
