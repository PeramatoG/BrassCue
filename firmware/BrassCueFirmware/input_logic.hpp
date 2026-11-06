#pragma once

// Hardware-independent input processing; compiled by Arduino and native tests.
// Only small integer types and fixed storage are used.

#include <stdint.h>

namespace brasscue {

class QuadratureEncoder {
public:
    // Pass the *logic level* of both pull-up inputs, true for HIGH.
    // Returns -1, 0, or +1 for a completed four-edge detent.
    int8_t sample(bool a_high, bool b_high) {
        const uint8_t state = (uint8_t(a_high) << 1) | uint8_t(b_high);
        if (!initialized_) {
            previous_ = state;
            initialized_ = true;
            return 0;
        }
        // Index is previous AB then current AB (two bits each).
        static const int8_t transition[16] = {
             0, -1, +1,  0,
            +1,  0,  0, -1,
            -1,  0,  0, +1,
             0, +1, -1,  0
        };
        const uint8_t changed = uint8_t(previous_ ^ state);
        const int8_t delta = transition[(previous_ << 2) | state];
        previous_ = state;
        if (changed == 3) {
            quarter_steps_ = 0; // Invalid two-bit transition.
            return 0;
        }
        quarter_steps_ = int8_t(quarter_steps_ + delta);
        if (quarter_steps_ >= 4) { quarter_steps_ = 0; return +1; }
        if (quarter_steps_ <= -4) { quarter_steps_ = 0; return -1; }
        return 0;
    }

private:
    uint8_t previous_ = 0;
    int8_t quarter_steps_ = 0;
    bool initialized_ = false;
};

class ButtonDebouncer {
public:
    // pressed=true when the pull-up pin reads LOW.
    // Returns +1 on press, -1 on release, 0 when unchanged.
    int8_t sample(bool pressed, uint32_t milliseconds, uint16_t debounce_ms = 15) {
        if (!initialized_) {
            initialized_ = true;
            stable_ = pressed;
            candidate_ = pressed;
            changed_at_ = milliseconds;
            return 0;
        }
        if (pressed != candidate_) {
            candidate_ = pressed;
            changed_at_ = milliseconds;
        }
        if (candidate_ != stable_ &&
            uint32_t(milliseconds - changed_at_) >= debounce_ms) {
            stable_ = candidate_;
            return stable_ ? +1 : -1;
        }
        return 0;
    }

private:
    uint32_t changed_at_ = 0;
    bool initialized_ = false;
    bool stable_ = false;
    bool candidate_ = false;
};

class GrandmasterFilter {
public:
    // ADC range 0..1023, result is 0..255.
    uint8_t sample(uint16_t adc) {
        if (adc > 1023) adc = 1023;
        if (!initialized_) {
            filtered_ = adc;
            initialized_ = true;
        } else {
            filtered_ = uint16_t((uint32_t(filtered_) * 3u + adc + 2u) / 4u);
        }
        return uint8_t((uint32_t(filtered_) * 255u + 511u) / 1023u);
    }

private:
    uint16_t filtered_ = 0;
    bool initialized_ = false;
};

} // namespace brasscue
