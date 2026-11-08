// BrassCue - GPL-3.0. See LICENSE for terms and warranty disclaimer.
#include "../src/four_digit_display.hpp"
#include "check.hpp"
int main() {
    brasscue::FourDigitDisplay d;
    CHECK(d.text(0) == " 100");
    d.set_master(128);
    CHECK(d.text(10) == "  50");
    d.encoder_changed(0, 53, 100);
    CHECK(d.text(100) == "R 21");
    CHECK(d.text(2099) == "R 21");
    CHECK(d.text(2100) == "  50");
    d.encoder_changed(2, 255, 2200);
    CHECK(d.text(2200) == "B100");
    d.set_master(0);
    CHECK(d.text(4200) == "   0");
    d.encoder_changed(3, 128, 4300);
    CHECK(d.text(4300) == "W 50");
    d.set_master(255);
    CHECK(d.text(6299) == "W 50");
    CHECK(d.text(6300) == " 100");
    d.set_master(0);
    d.encoder_changed(9, 200, 4300);
    CHECK(d.text(6300) == "   0");
    brasscue::FourDigitDisplay switched;
    switched.encoder_changed(0, 53, 100);
    switched.encoder_changed(3, 20, 1000);
    switched.set_master(128);
    CHECK(switched.text(2100) == "W  8");
    CHECK(switched.text(2999) == "W  8");
    CHECK(switched.text(3000) == "  50");
}
