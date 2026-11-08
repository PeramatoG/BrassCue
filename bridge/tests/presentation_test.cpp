// BrassCue - GPL-3.0. Geometry, saturation through Core and view diffs without wxWidgets.
#include "../gui/presentation/encoder_gesture.hpp"
#include "../gui/presentation/view_snapshot.hpp"
#include "check.hpp"
#include <cmath>
int main() {
    using namespace brasscue;
    constexpr double pi = 3.14159265358979323846;
    EncoderGesture gesture;
    gesture.begin(1, 0);
    CHECK(gesture.move(1, 0) == 0);
    int sum = 0;
    for (unsigned i = 1; i <= 144; ++i) {
        double a = i * 4 * pi / 144;
        sum += gesture.move(std::cos(a), std::sin(a));
    }
    CHECK(sum >= 509 && sum <= 510);
    gesture.begin(1, 0);
    sum = 0;
    for (unsigned i = 1; i <= 144; ++i) {
        double a = -int(i) * 4 * pi / 144;
        sum += gesture.move(std::cos(a), std::sin(a));
    }
    CHECK(sum <= -509 && sum >= -510);
    gesture.begin(std::cos(pi - 0.1), std::sin(pi - 0.1));
    CHECK(gesture.move(std::cos(-pi + 0.1), std::sin(-pi + 0.1)) > 0);
    gesture.begin(1, 0);
    CHECK(gesture.move(0, 0) == 0 && gesture.move(-1, 0) == 0 && gesture.move(1, 0) == 0);
    gesture.end();
    CHECK(gesture.move(0, 1) == 0);
    ControllerSession s;
    FourDigitDisplay display;
    auto first = make_snapshot(s, display, 0, "SIMULATION | OFF");
    CHECK(first.buttons[0].selected && !first.buttons[1].selected);
    CHECK(!snapshot_json(nullptr, first).empty());
    CHECK(!view_changes(&first, first).any() && snapshot_json(&first, first).empty());
    s.encoder(0, 255);
    auto changed = make_snapshot(s, display, 0, "SIMULATION | OFF");
    auto diff = view_changes(&first, changed);
    CHECK(diff.encoders[0] && !diff.buttons[0] && !diff.buttons[1] && !diff.buttons[2]);
    auto json = snapshot_json(&first, changed);
    CHECK(json.find("\"buttons\":[]") != std::string::npos);
    s.fader(42);
    s.button(1, true, 0);
    s.button(1, false, 0);
    display.set_master(s.state().master);
    auto half = make_snapshot(s, display, 0, "SIMULATION | OFF");
    CHECK(half.fader == 128 && half.master_percent == 50 && half.display == "  50" &&
          half.buttons[1].selected);
    CHECK(half.screen.color[0] == 128);
    for (int i = 0; i < 300; ++i)
        s.event({EventType::Encoder, 0, 1}, false, 0);
    CHECK(s.state().rgbw[0] == 255);
    for (int i = 0; i < 300; ++i)
        s.event({EventType::Encoder, 0, -1}, false, 0);
    CHECK(s.state().rgbw[0] == 0);
    CHECK(json_string("\"\\\n</script>") == "\"\\\"\\\\\\u000a\\u003c/script>\"");
}
