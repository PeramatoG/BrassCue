#include "input_logic.hpp"

// BrassCue BC1 serial protocol, 115200 baud. Displays are not connected yet.

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
char rx_line[64]{};
uint8_t rx_length = 0;
bool rx_overflow = false;
uint8_t host_rgbw[4] = {0, 0, 0, 0};
uint8_t host_master = 255;

void report_encoder(uint8_t index, int8_t delta) {
    Serial.print(F("BC1 ENC "));
    Serial.print(index);
    Serial.print(' ');
    Serial.println(delta);
}

void report_button(uint8_t index, int8_t delta) {
    Serial.print(F("BC1 BTN "));
    Serial.print(index);
    Serial.print(delta > 0 ? F(" 1") : F(" 0"));
    Serial.println();
}
}

bool parse_decimal(const char*& ptr, uint8_t& out) {
    if (*ptr < '0' || *ptr > '9') return false;
    uint16_t value = 0;
    while (*ptr >= '0' && *ptr <= '9') {
        value = uint16_t(value * 10u + uint8_t(*ptr - '0'));
        if (value > 255u) return false;
        ++ptr;
    }
    out = uint8_t(value);
    return true;
}

void handle_command(const char* line) {
    if (strcmp(line, "BC1 HELLO") == 0) {
        Serial.println(F("BC1 READY"));
        return;
    }
    if (strncmp(line, "BC1 STATE ", 10) != 0) return;
    const char* p = line + 10;
    uint8_t fields[5];
    for (uint8_t i = 0; i < 5; ++i) {
        if (!parse_decimal(p, fields[i])) return;
        if (i != 4) {
            if (*p != ' ') return;
            ++p;
        }
    }
    if (*p != '\0') return;
    for (uint8_t i = 0; i < 4; ++i) host_rgbw[i] = fields[i];
    host_master = fields[4];
    // No displays yet. Acknowledge only after a complete, valid state.
    Serial.println(F("BC1 ACK STATE"));
}

void poll_serial() {
    // Bound per-loop work so encoder scan is not starved by UART traffic.
    for (uint8_t i = 0; i < 24 && Serial.available() > 0; ++i) {
        const char ch = char(Serial.read());
        if (ch == '\r') continue;
        if (ch == '\n') {
            if (!rx_overflow && rx_length > 0) {
                rx_line[rx_length] = '\0';
                handle_command(rx_line);
            }
            rx_length = 0;
            rx_overflow = false;
        } else if (!rx_overflow) {
            if (ch < 32 || ch > 126 || rx_length >= sizeof(rx_line) - 1) {
                rx_length = 0;
                rx_overflow = true;
            } else {
                rx_line[rx_length++] = ch;
            }
        }
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
    Serial.println(F("BC1 READY"));
}

void loop() {
    poll_serial();
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
            Serial.print(F("BC1 GM "));
            Serial.println(master);
            last_master = master;
            first_master = false;
        }
    }
}
