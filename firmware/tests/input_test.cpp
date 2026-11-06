#include "../BrassCueFirmware/input_logic.hpp"

#include <iostream>

static int failures = 0;

void expect(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << "\n";
        ++failures;
    }
}

int main() {
    brasscue::QuadratureEncoder encoder;
    expect(encoder.sample(true, true) == 0, "encoder initializes without event");
    expect(encoder.sample(false, true) == 0, "first quarter");
    expect(encoder.sample(false, false) == 0, "second quarter");
    expect(encoder.sample(true, false) == 0, "third quarter");
    expect(encoder.sample(true, true) == 1, "four quarters form one detent");
    expect(encoder.sample(true, false) == 0, "reverse first quarter");
    expect(encoder.sample(false, false) == 0, "reverse second quarter");
    expect(encoder.sample(false, true) == 0, "reverse third quarter");
    expect(encoder.sample(true, true) == -1, "reverse four quarters form one detent");

    brasscue::QuadratureEncoder invalid;
    expect(invalid.sample(false, false) == 0, "start invalid-sequence test");
    expect(invalid.sample(true, true) == 0, "two-bit transition ignored");
    expect(invalid.sample(false, false) == 0, "two-bit reverse ignored");
    // Bounce between two neighboring states must not count a full detent.
    for (int i = 0; i < 20; ++i) {
        expect(invalid.sample(false, true) == 0, "bounce out");
        expect(invalid.sample(false, false) == 0, "bounce back");
    }

    brasscue::ButtonDebouncer button;
    expect(button.sample(false, 0) == 0, "button initializes released");
    expect(button.sample(true, 1) == 0, "press not immediate");
    expect(button.sample(false, 2) == 0, "bounce reverts");
    expect(button.sample(true, 4) == 0, "press candidate restarts");
    expect(button.sample(true, 18) == 0, "press below debounce threshold");
    expect(button.sample(true, 19) == 1, "confirmed press");
    expect(button.sample(true, 20) == 0, "no repeated press");
    expect(button.sample(false, 30) == 0, "release pending");
    expect(button.sample(false, 45) == -1, "confirmed release");

    brasscue::GrandmasterFilter fader;
    expect(fader.sample(0) == 0, "fader minimum");
    uint8_t output = 0;
    for (int i = 0; i < 100; ++i) output = fader.sample(1023);
    expect(output == 255, "fader maximum after settling");
    for (int i = 0; i < 100; ++i) output = fader.sample(0);
    expect(output == 0, "fader returns to minimum");
    brasscue::GrandmasterFilter midpoint;
    expect(midpoint.sample(512) == 128, "ADC midpoint maps to 8-bit midpoint");
    expect(midpoint.sample(4095) == 191, "ADC overrange clamped and filtered");

    if (failures != 0) return 1;
    std::cout << "BrassCue input logic tests passed\n";
    return 0;
}
