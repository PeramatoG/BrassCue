// BrassCue - GPL-3.0. See LICENSE for terms and warranty disclaimer.
#include "../src/bridge_core.hpp"

#include "check.hpp"
#include <cstdint>

int main() {
    using namespace brasscue;
    CHECK(apply_master(255, 255) == 255);
    CHECK(apply_master(255, 0) == 0);
    CHECK(apply_master(128, 128) == 64);
    CHECK(apply_master(1, 128) == 1);

    State state;
    state.rgbw = {{255, 128, 1, 200}};
    state.master = 128;
    const auto packet = build_artdmx(state, 0x1234);
    static_assert(kPacketBytes == 530, "ArtDmx packet size changed");
    CHECK(packet[0] == 'A' && packet[7] == 0);
    CHECK(packet[8] == 0 && packet[9] == 0x50);
    CHECK(packet[10] == 0 && packet[11] == 14);
    CHECK(packet[14] == 0x34 && packet[15] == 0x12);
    CHECK(packet[16] == 2 && packet[17] == 0);
    CHECK(packet[18] == 128 && packet[19] == 64);
    CHECK(packet[20] == 1 && packet[21] == 100);
    CHECK(packet[22] == 0 && packet[529] == 0);

    state.master = 0;
    const auto blackout = build_artdmx(state, 0);
    for (std::size_t i = 18; i < blackout.size(); ++i)
        CHECK(blackout[i] == 0);

    bool rejected = false;
    try {
        (void)build_artdmx(state, 32768);
    } catch (const std::invalid_argument &) {
        rejected = true;
    }
    CHECK(rejected);
    return 0;
}
