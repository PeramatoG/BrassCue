#include "input_logic.hpp"

// Diagnostic Arduino Uno input reader. No display or Bridge transport yet.
// Output lines are intentionally not the finalized wire protocol.

namespace {
struct EncoderPins { uint8_t a; uint8_t b; };

constexpr EncoderPins kEncoderPins[4] = {
    {2, 3}, {4, 5}, {6, 7}, {8, 12}
};
constexpr uint8_t kButtonPins[3] = {A3, A4, A5};
constexpr uint8_t kGrandmasterPin = A0;

brasscue::QuadratureEncoder encoders[4];
brasscue::ButtonDebouncer buttons[3];
brasscue::GrandmasterFilter grandmaster;
uint8_t last_master = 255;
bool first_master = true;
uint32_t last_fader_poll = 0;

void report_encoder(uint8_t index, int8_t delta) {
    Serial.print(F("ENC "));
    Serial.print(index);
    Serial.print(' ');
    Serial.println(delta);
}

void report_button(uint8_t index, int8_t delta) {
    Serial.print(F("BTN "));
    Serial.print(index);
    Serial.print(delta > 0 ? F(" DOWN") : F(" UP"));
    Serial.println();
}
}

void setup() {
    Serial.begin(115200);
    for (uint8_t i = 0; i < 4; ++i) {
        pinMode(kEncoderPins[i].a, INPUT_PULLUP);
        pinMode(kEncoderPins[i].b, INPUT_PULLUP);
        (void)encoders[i].sample(digitalRead(kEncoderPins[i].a) == HIGH,
                                 digitalRead(kEncoderPins[i].b) == HIGH);
    }
    for (uint8_t i = 0; i < 3; ++i) {
        pinMode(kButtonPins[i], INPUT_PULLUP);
        (void)buttons[i].sample(digitalRead(kButtonPins[i]) == LOW, millis());
    }
    pinMode(kGrandmasterPin, INPUT);
    Serial.println(F("BrassCue Uno input diagnostic v0.1"));
}

void loop() {
    for (uint8_t i = 0; i < 4; ++i) {
        const int8_t delta = encoders[i].sample(digitalRead(kEncoderPins[i].a) == HIGH,
                                                  digitalRead(kEncoderPins[i].b) == HIGH);
        if (delta != 0) report_encoder(i, delta);
    }

    const uint32_t now = millis();
    for (uint8_t i = 0; i < 3; ++i) {
        const int8_t delta = buttons[i].sample(digitalRead(kButtonPins[i]) == LOW, now);
        if (delta != 0) report_button(i, delta);
    }

    if (uint32_t(now - last_fader_poll) >= 5u) {
        last_fader_poll = now;
        const uint8_t master = grandmaster.sample(analogRead(kGrandmasterPin));
        if (first_master || master != last_master) {
            Serial.print(F("GM "));
            Serial.println(master);
            last_master = master;
            first_master = false;
        }
    }
}
