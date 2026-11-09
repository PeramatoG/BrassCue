// BrassCue - GPL-3.0. Group generation, migration and repatching through real sessions/output.
#include "../src/controller_session.hpp"
#include "../src/output_session.hpp"
#include "../src/profile_io.hpp"
#include "check.hpp"
#include <fstream>
namespace {
template <class F> void Reject(F action) {
    bool rejected = false;
    try {
        action();
    } catch (const std::exception &) {
        rejected = true;
    }
    CHECK(rejected);
}
brasscue::ControlProfile Empty() {
    brasscue::ControlProfile p;
    for (auto &patch : p.encoders)
        patch.clear();
    return p;
}
} // namespace
int main() {
    using namespace brasscue;
    auto p = Empty();
    p.fixture_groups = {{"HOUSE PANELS", "RGBW", 4, FixtureBoundary::Reject, {{1, 1, 3}}}};
    auto patch = effective_encoder_patch(p);
    auto layout = fixture_layout(p.fixture_groups);
    CHECK(layout.size() == 3 && layout[2].fixture == 3 && layout[2].start.channel == 9);
    for (unsigned color = 0; color < 4; ++color)
        for (unsigned n = 0; n < 3; ++n) {
            CHECK(patch[color][n].address.universe == 1);
            CHECK(patch[color][n].address.channel == n * 4 + color + 1);
            CHECK(patch[color][n].value == 255);
        }
    CHECK(p.encoders[0].empty()); // No generated copy in canonical manual assignments.
    p.fixture_groups[0].order = "WRGB";
    p.fixture_groups[0].stride = 8;
    p.fixture_groups[0].blocks = {{2, 12, 2}, {32768, 509, 1}};
    patch = effective_encoder_patch(p);
    CHECK(patch[0][0].address.channel == 13 && patch[3][0].address.channel == 12);
    CHECK(patch[0][1].address.channel == 21 && patch[3][2].address.channel == 509);
    p.fixture_groups.push_back({"LOBBY", "BGRW", 4, FixtureBoundary::Reject, {{3, 100, 2}}});
    CHECK(fixture_layout(p.fixture_groups).size() == 5);
    for (unsigned bad = 0; bad < 14; ++bad) {
        auto invalid = p;
        auto &g = invalid.fixture_groups[0];
        if (bad == 0)
            g.order = "RRGW";
        if (bad == 1)
            g.stride = 3;
        if (bad == 2)
            g.stride = 513;
        if (bad == 3)
            g.blocks[0].universe = 0;
        if (bad == 4)
            g.blocks[0].universe = 32769;
        if (bad == 5)
            g.blocks[0].address = 0;
        if (bad == 6)
            g.blocks[0].address = 510;
        if (bad == 7)
            g.blocks[0].count = 0;
        if (bad == 8)
            g.blocks[0] = {1, 505, 2}; // Stride eight crosses the boundary.
        if (bad == 9)
            g.blocks[0].count = 513;
        if (bad == 10)
            g.boundary = static_cast<FixtureBoundary>(1);
        if (bad == 11)
            g.name = "LOBBY";
        if (bad == 12)
            g.blocks.clear();
        if (bad == 13)
            g.name = std::string("\xc0\x80", 2);
        Reject([&] { validate(invalid); });
    }
    auto collisions = p;
    collisions.fixture_groups[1].blocks = {{2, 13, 1}};
    Reject([&] { validate(collisions); }); // Cross-color/group collision.
    collisions = p;
    collisions.fixture_groups[0].blocks.push_back(collisions.fixture_groups[0].blocks[0]);
    Reject([&] { validate(collisions); });
    for (unsigned source = 0; source < 3; ++source) {
        collisions = p;
        const Assignment overlap{{2, 12}, 53};
        if (source == 0)
            collisions.encoders[1].push_back(overlap);
        if (source == 1)
            collisions.fader.assignments.push_back(overlap);
        if (source == 2) {
            collisions.buttons[0].action = ButtonActionKind::DmxCue;
            collisions.buttons[0].controls.clear();
            collisions.buttons[0].assignments.push_back(overlap);
        }
        Reject([&] { validate(collisions); });
    }
    auto capacity = Empty();
    capacity.fixture_groups = {{"MAX",
                                "RGBW",
                                4,
                                FixtureBoundary::Reject,
                                {{1, 1, 128}, {2, 1, 128}, {3, 1, 128}, {4, 1, 128}}}};
    CHECK(effective_encoder_patch(capacity)[0].size() == 512);
    capacity.encoders[0].push_back({{5, 1}, 53});
    Reject([&] { validate(capacity); });
    capacity.fixture_groups[0].blocks[0].count = 127;
    CHECK(effective_encoder_patch(capacity)[0].size() == 512);
    capacity.fixture_groups.resize(65);
    Reject([&] { validate(capacity); });
    capacity = p;
    capacity.fixture_groups[0].blocks.resize(65);
    Reject([&] { validate(capacity); });

    ControllerSession session;
    auto live = Empty();
    live.encoders[0] = {{{9, 100}, 53}};
    live.encoders[1] = {{{9, 100}, 84}}; // Existing manual HTP stays legal.
    live.fixture_groups = {{"HOUSE", "RGBW", 4, FixtureBoundary::Reject, {{1, 5, 2}, {2, 1, 1}}}};
    session.configure(live);
    session.encoder(0, 255);
    session.encoder(1, 128);
    session.button(1, true, 0);
    session.button(1, false, 0);
    auto frames = session.engine().render(0);
    CHECK(frames[1][4] == 128 && frames[1][8] == 128 && frames[2][0] == 128);
    CHECK(frames[1][5] == 64 && frames[9][99] == 27); // Manual 53 and GM50 preserved.
    ControllerSession faded;
    auto timed = live;
    timed.buttons[2].controls = {{ControlTarget::Red, 54},
                                 {ControlTarget::Green, 100},
                                 {ControlTarget::Blue, 200},
                                 {ControlTarget::White, 20}};
    timed.buttons[2].fade_in_ms = 1000;
    faded.configure(timed);
    faded.button(2, true, 0);
    faded.button(2, false, 0);
    faded.advance(500);
    auto mid = faded.engine().render(500);
    CHECK(mid[1][4] == 27 && mid[1][8] == 27 && mid[2][0] == 27);
    CHECK(mid[1][5] == 50 && mid[1][6] == 100 && mid[1][7] == 10);
    faded.button(1, true, 500);
    faded.button(1, false, 500);
    faded.advance(2000);
    CHECK(faded.engine().render(2000)[1][4] == 14); // GM50 scales held RGBW after Kill.
    CHECK(faded.profile().encoders[0][0].value == 53);
    timed.fader.grandmaster = false;
    timed.fader.assignments = {{{6, 1}, 128}};
    faded.configure(timed);
    faded.fader(64);
    faded.button(1, true, 2000);
    faded.button(1, false, 2000);
    CHECK(faded.engine().render(2000)[6][0] == 16);
    CHECK(faded.engine().render(2000)[1][4] == 14);
    OutputSession output;
    std::vector<std::array<std::uint8_t, 530>> packets;
    auto send = [&](const auto &packet) { packets.push_back(packet); };
    output.update(frames, send);
    const auto before = serialize_profile(session.profile());
    auto invalid = session.profile();
    invalid.fixture_groups[0].blocks[0].address = 510;
    Reject([&] { session.configure(invalid); });
    CHECK(serialize_profile(session.profile()) == before && session.engine().render(0) == frames);
    auto repatch = session.profile();
    repatch.fixture_groups[0].blocks = {{1, 21, 1}};
    session.configure(repatch);
    packets.clear();
    output.update(session.engine().render(0), send);
    CHECK(packets.size() == 3);                        // Retired U2 plus full U1 and U9.
    CHECK(packets[0][14] == 1 && packets[0][15] == 0); // Visible U2 -> wire 1.
    for (unsigned i = 18; i < 530; ++i)
        CHECK(packets[0][i] == 0);
    auto next = session.engine().render(0);
    CHECK(next[1][4] == 0 && next[1][8] == 0 && next[1][20] == 128 && !next.count(2));
    CHECK(session.profile().encoders[0][0].value == 53 && session.state().master == 128);
    repatch.fixture_groups.clear();
    session.configure(repatch);
    next = session.engine().render(0);
    CHECK(!next.count(1) && next.count(9));

    auto legacy = load_profile(std::filesystem::path(TEST_FIXTURES) / "manual-v2.bcue");
    CHECK(legacy.fixture_groups.empty() && legacy.encoders[0][0].value == 53);
    CHECK(legacy.encoder_labels[0] == "Rojo ámbar" && legacy.fader.assignments[0].value == 99);
    CHECK(legacy.buttons[0].fade_in_ms == 1234 && legacy.buttons[0].assignments[0].value == 37);
    CHECK(legacy.buttons[1].action == ButtonActionKind::SetControlValues &&
          legacy.buttons[1].controls[0].value == 200);
    CHECK(legacy.network_interface_id == "legacy-interface" && legacy.target_ip == "127.0.0.1");
    legacy.fixture_groups = p.fixture_groups;
    legacy.fixture_groups[0].blocks[1].universe = 32767;
    auto text = serialize_profile(legacy);
    CHECK(text.rfind("BRASSCUE 3\n", 0) == 0 && serialize_profile(parse_profile(text)) == text);
    CHECK(parse_profile(text).encoders[0].size() == 1); // Generated routes not persisted twice.
    auto old = load_profile(std::filesystem::path(TEST_FIXTURES) / "room-v1.bcue");
    CHECK(old.fixture_groups.empty() && old.buttons[0].action == ButtonActionKind::DmxCue);
    CHECK(serialize_profile(parse_profile(serialize_profile(old))) == serialize_profile(old));
    for (auto bad : {text + "junk", text.substr(0, text.size() - 5), std::string("BRASSCUE 4\n")})
        Reject([&] { parse_profile(bad); });
    auto policy = text;
    auto position = policy.find("\"WRGB\" 8 0");
    CHECK(position != std::string::npos);
    policy.replace(position, 10, "\"WRGB\" 8 1");
    Reject([&] { parse_profile(policy); });
    auto path = std::filesystem::temp_directory_path() / "brasscue-groups-test.bcue";
    save_profile(path, legacy);
    save_profile(path, legacy);
    CHECK(serialize_profile(load_profile(path)) == text);
    invalid = legacy;
    invalid.fixture_groups.push_back(invalid.fixture_groups[0]);
    Reject([&] { save_profile(path, invalid); });
    CHECK(serialize_profile(load_profile(path)) == text);
    std::filesystem::remove(path);
}
