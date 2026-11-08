// BrassCue - GPL-3.0. See LICENSE for terms and warranty disclaimer.
#include "../src/assignment_editor.hpp"
#include "../src/controller_session.hpp"
#include "../src/profile_io.hpp"
#include "check.hpp"
int main() {
    using namespace brasscue;
    Assignment row{{1, 23}, 53};
    CHECK(edit_assignment_row(row, "2", "8", "21", ValueDisplay::Percent, false).value == 53);
    CHECK(edit_assignment_row(row, "3", "128", "50", ValueDisplay::Percent, true).value == 128);
    CHECK(edit_assignment_row(row, "32768", "512", "255", ValueDisplay::Dmx255, true).value == 255);
    for (const auto &bad : {"-1", "256", "12junk", "1.5", "", "+1", "4294967296"}) {
        bool failed = false;
        try {
            edit_assignment_row(row, "1", "23", bad, ValueDisplay::Dmx255, true);
        } catch (const std::exception &) {
            failed = true;
        }
        CHECK(failed);
    }
    for (unsigned v = 0; v < 256; ++v) {
        row.value = static_cast<std::uint8_t>(v);
        CHECK(edit_assignment_row(row, "1", "23", std::to_string(to_percent(row.value)),
                                  ValueDisplay::Percent, false)
                  .value == v);
    }
    ControlProfile p;
    p.encoder_labels[0] = "Rojo ámbar";
    p.encoders[0] = {{{1, 23}, 255}, {{2, 8}, 128}, {{3, 128}, 0}, {{32768, 512}, 42}};
    CHECK(valid_utf8(p.encoder_labels[0]));
    CHECK(!valid_utf8(std::string("\xc0\x80", 2)));
    CHECK(!valid_utf8(std::string("\xed\xa0\x80", 3)));
    CHECK(!valid_utf8(std::string("\xf4\x90\x80\x80", 4)));
    auto text = serialize_profile(p);
    CHECK(serialize_profile(parse_profile(text)) == text);
    for (auto bad : {text + "garbage", std::string("BRASSCUE 2"), text.substr(0, 30)}) {
        bool rejected = false;
        try {
            parse_profile(bad);
        } catch (const std::exception &) {
            rejected = true;
        }
        CHECK(rejected);
    }
    ControllerSession session;
    const auto before = serialize_profile(session.profile());
    p.encoders[0].push_back(p.encoders[0][0]);
    bool rejected = false;
    try {
        session.configure(p);
    } catch (const std::exception &) {
        rejected = true;
    }
    CHECK(rejected && serialize_profile(session.profile()) == before);
    auto path = std::filesystem::temp_directory_path() / "brasscue-profile-test.bcue";
    save_profile(path, session.profile());
    save_profile(path, session.profile());
    CHECK(serialize_profile(load_profile(path)) == before);
    std::filesystem::remove(path);
}
