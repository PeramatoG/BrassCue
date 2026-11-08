#include "../src/four_digit_display.hpp"
#include <cassert>
int main() {
 brasscue::FourDigitDisplay d;
 assert(d.text(0)==" 100");
 d.set_master(128);
 assert(d.text(10)=="  50");
 d.encoder_changed(0,53,100);
 assert(d.text(100)=="R 21");
 assert(d.text(2099)=="R 35");
 assert(d.text(2100)=="0080");
 d.encoder_changed(2,255,2200);
 assert(d.text(2200)=="B100");
 d.set_master(0);
 assert(d.text(4200)=="   0");
 d.encoder_changed(9,200,4300);
 assert(d.text(4300)=="0000");
}
