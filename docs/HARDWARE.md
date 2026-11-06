# Hardware — selection and open questions

These are the parts/functions selected for the first BrassCue prototype. This document is **not** a wiring diagram.

| Item | Selection / intent | Quantity |
| --- | --- | ---: |
| Microcontroller | Classic Arduino Uno, USB type-B serial connection | 1 |
| Rotary encoders | Bourns PEC11R-4225F-N0024, continuous rotation, no push switch | 4 |
| Master fader | Bourns PTB0143-2010BPB103, 10 kΩ, 100 mm travel | 1 |
| Buttons | Mechanical buttons with typewriter-style caps (final switch/mounting to verify) | 3 |
| Color TFT | ST7735 SPI, 1.44 inch, 128×128, selected from Electronperdido | 1 |
| Numeric display | Yellow/amber TM1637 module; intended to show hexadecimal values such as FF | 1 |

## Important electrical checks before connecting
- The classic Uno uses 5 V digital logic. Check whether the **actual TFT board** includes 5 V-safe level shifting and an onboard regulator. Do not wire 5 V SPI signals directly to a 3.3 V-only module.
- Confirm the TM1637 board's supply, pullups and pinout.
- Reserve D0/D1 for USB UART. Check all digital/SPI/analog pin requirements together, including the TFT reset arrangement; no pin map is approved yet.
- Verify encoder pinouts, fader terminals/endpoints, switch active level, button mounting and pullup strategy.
- Do not allocate a whole 128×128×16-bit framebuffer in Uno SRAM (requires 32 KiB; the Uno has 2 KiB).
- Test every module on the bench before assembling the enclosure.

## Mechanical
Steampunk enclosure, brass/copper aesthetic, encoder knobs, an accessible grandmaster and three typewriter-style buttons. Precise dimensions, placement and faceplate templates remain pending.
