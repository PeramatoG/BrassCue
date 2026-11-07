#include "../src/controller_profile.hpp"
#include <cassert>
#include <stdexcept>

int main() {
    using namespace brasscue;
    ControlProfile p;
    ControllerValues v;
    assert(valid_profile(p));
    v.encoders = {{255, 128, 64, 32}};
    v.fader = 128;
    auto dmx = build_profiled_artdmx(v, p, 0);
    assert(dmx[18] == 128 && dmx[19] == 64);
    assert(dmx[20] == 32 && dmx[21] == 16);
    assert(dmx[22] == 0);

    p.dmx_channels = {{100, 101, 200, 512}};
    dmx = build_profiled_artdmx(v, p, 42);
    assert(dmx[14] == 42 && dmx[18] == 0);
    assert(dmx[18 + 99] == 128 && dmx[18 + 100] == 64);
    assert(dmx[18 + 199] == 32 && dmx[18 + 511] == 16);

    p.fader_role = FaderRole::DmxChannel;
    p.fader_dmx_channel = 255;
    dmx = build_profiled_artdmx(v, p, 0);
    assert(dmx[18 + 99] == 255 && dmx[18 + 100] == 128);
    assert(dmx[18 + 254] == 128);
    v.blackout = true;
    dmx = build_profiled_artdmx(v, p, 0);
    assert(dmx[18 + 99] == 0 && dmx[18 + 254] == 0);
    v.blackout = false;

    p.dmx_channels = {{1, 1, 3, 4}};
    assert(!valid_profile(p));
    p.dmx_channels = {{1, 2, 3, 4}};
    p.fader_dmx_channel = 4;
    assert(!valid_profile(p));
    p.fader_dmx_channel = 513;
    assert(!valid_profile(p));
    p.fader_dmx_channel = 5;
    assert(valid_profile(p));

    ControllerValues preset;
    preset.encoders = {{1, 2, 3, 4}};
    preset.fader = 77;
    assert(apply_button(v, ButtonAction::Blackout) && v.blackout);
    assert(apply_button(v, ButtonAction::Full) && !v.blackout && v.fader == 255);
    assert(apply_button(v, ButtonAction::RecallPreset, &preset));
    assert(v.encoders == preset.encoders && v.fader == 77);
    assert(!apply_button(v, ButtonAction::Unassigned));
    assert(!apply_button(v, ButtonAction::RecallPreset, nullptr));
    return 0;
}
