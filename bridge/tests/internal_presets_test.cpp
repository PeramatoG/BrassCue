// BrassCue - GPL-3.0. Deterministic preset selection, transitions and manual priority.
#include "../src/controller_session.hpp"
#include "../src/profile_io.hpp"
#include "check.hpp"
int main() {
    using namespace brasscue;
    ControllerSession s;
    s.encoder(0, 255);
    s.fader(66);
    s.button(0, true, 0);
    s.button(0, false, 0);
    CHECK(s.state().master == 255 && s.button_selected(0, 0));
    CHECK(s.engine().render(0)[1][0] == 255 && s.engine().render(0)[1][4] == 0);
    s.button(1, true, 0);
    s.button(1, true, 0);
    s.button(1, false, 0);
    CHECK(s.state().master == 128 && s.button_selected(1, 0) && !s.button_selected(0, 0));
    s.button(2, true, 0);
    s.button(2, false, 0);
    CHECK(s.state().master == 0 && s.button_selected(2, 0));
    s.button(2, true, 0);
    s.button(2, false, 0);
    CHECK(s.state().master == 0 && s.button_selected(2, 0));
    s.fader(128);
    CHECK(s.button_selected(1, 0));
    auto p = s.profile();
    p.buttons[0].fade_in_ms = 2000;
    p.buttons[0].fade_out_ms = 1000;
    s.configure(p);
    s.fader(0);
    s.button(0, true, 100);
    s.button(0, false, 100);
    s.advance(1100);
    CHECK(s.state().master == 128);
    s.button(1, true, 1100);
    s.button(1, false, 1100);
    CHECK(s.state().master == 128);
    s.fader(0, 1200);
    s.button(0, true, 1200);
    s.button(0, false, 1200);
    s.advance(2200);
    CHECK(s.state().master == 128);
    s.fader(77, 2200);
    s.advance(10000);
    CHECK(s.state().master == 77 && !s.button_selected(0, 10000));
    s.fader(255, 10000);
    s.button(0, true, 10000);
    s.button(0, false, 10000);
    s.advance(10500);
    CHECK(s.state().master == 128); // Toggle-off fade to safe zero.
    s.advance(11000);
    CHECK(s.state().master == 0);
    p.buttons[0].mode = ButtonMode::Flash;
    p.buttons[0].fade_in_ms = 0;
    p.buttons[0].fade_out_ms = 1000;
    s.configure(p);
    s.fader(80, 12000);
    s.button(0, true, 12000);
    CHECK(s.state().master == 255);
    s.release_all(12000);
    s.advance(12500);
    CHECK(s.state().master == 168);
    s.advance(13000);
    CHECK(s.state().master == 80);
    s.button(0, true, 14000);
    s.fader(42, 14000);
    s.button(0, false, 14000);
    s.advance(16000);
    CHECK(s.state().master == 42); // Stale Flash cannot undo manual takeover.
    p.buttons[0].mode = ButtonMode::Toggle;
    p.buttons[0].fade_in_ms = 0;
    p.buttons[0].controls = {
        {ControlTarget::Red, 77}, {ControlTarget::Blue, 204}, {ControlTarget::Grandmaster, 128}};
    s.configure(p);
    s.button(0, true, 0);
    s.button(0, false, 0);
    CHECK(s.state().rgbw[0] == 77 && s.state().rgbw[2] == 204 && s.state().master == 128);
    p.buttons[1].controls = {{ControlTarget::Red, 11},
                             {ControlTarget::Green, 22},
                             {ControlTarget::Blue, 33},
                             {ControlTarget::White, 44}};
    p.buttons[1].kill = false;
    s.configure(p);
    s.encoder(0, 0);
    s.encoder(2, 0);
    s.fader(0);
    s.button(0, true, 0);
    s.button(0, false, 0);
    s.button(1, true, 0);
    s.button(1, false, 0);
    CHECK(s.state().rgbw == std::array<std::uint8_t, 4>{{11, 22, 33, 44}});
    s.button(0, true, 0);
    s.button(0, false, 0);
    CHECK(s.state().rgbw[0] == 11); // Older command cannot overwrite latest owner on release.
    CHECK(!s.physical_fader());
    s.event({EventType::Grandmaster, 0, 90}, false, 0);
    CHECK(s.physical_fader() == 90);
    s.button(2, true, 0);
    CHECK(s.state().master == 0 && s.physical_fader() == 90);
    ControllerSession patch_session;
    auto patch_profile = patch_session.profile();
    patch_profile.fader.grandmaster = false;
    patch_profile.fader.assignments = {{{2, 1}, 200}};
    patch_session.configure(patch_profile);
    patch_session.encoder(0, 255);
    patch_session.fader(64);
    patch_session.button(1, true, 0);
    patch_session.button(1, false, 0);
    CHECK(patch_session.state().master == 128 && patch_session.fader_value() == 64);
    CHECK(patch_session.engine().render(0)[1][0] == 128 &&
          patch_session.engine().render(0)[2][0] == 25);
    ControllerSession mixed;
    auto mixed_profile = mixed.profile();
    auto &cue = mixed_profile.buttons[0];
    cue.action = ButtonActionKind::DmxCue;
    cue.controls.clear();
    cue.assignments = {{{1, 5}, 255}};
    mixed.configure(mixed_profile);
    mixed.button(0, true, 0);
    mixed.button(0, false, 0);
    CHECK(mixed.button_selected(0, 0));
    CHECK(!mixed.button_selected(1, 0));
    mixed.fader(128);
    CHECK(!mixed.button_selected(1, 0));
    mixed.button(1, true, 0);
    mixed.button(1, false, 0);
    CHECK(mixed.state().master == 128 && mixed.button_selected(1, 0) &&
          !mixed.button_selected(0, 0));
    auto old = load_profile(std::filesystem::path(TEST_FIXTURES) / "room-v1.bcue");
    CHECK(old.buttons[0].action == ButtonActionKind::DmxCue && old.buttons[0].controls.empty());
    s.configure(old);
    s.fader(255);
    s.button(0, true, 0);
    CHECK(s.engine().render(0)[1][4] == 255 && s.state().master == 255);
    CHECK(serialize_profile(old).rfind("BRASSCUE 2\n", 0) == 0);
    CHECK(serialize_profile(parse_profile(serialize_profile(old))) == serialize_profile(old));
}
