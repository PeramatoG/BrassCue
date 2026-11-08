# BrassCue hardware — verified selection and provisional wiring

This sheet records the **final shopping selection**, not the earlier DigiKey alternatives. The firmware pin assignment is a **proposal**, not a finished wiring diagram. Verify the real modules and physical orientation before applying power.

| Function | Purchased/selected item | Qty | Verified properties |
| --- | --- | ---: | --- |
| Controller | Classic Arduino Uno R3 (already available) | 1 | ATmega328P, 5 V logic, 14 digital + 6 analog pins, 2 KB SRAM; USB-B UART |
| R/G/B/W controls | **Bourns PEC11R-4225F-N0024** (RS 789-9557) | 4 | Incremental quadrature, 24 detents/24 pulses per turn, continuous rotation, no push switch, 6 mm D-shaft |
| Grandmaster | **Bourns PTB0143-2010BPB103** (RS 737-7792) | 1 | Linear 10 kΩ, single-gang, 100 mm travel, approx. 128 × 16 mm body |
| Independent buttons | **Omron B3F-1070** (RS 686-6907; sold as pack of five) | 3 used | 6 × 6 mm tactile, SPST normally open, 9.5 mm tall, 0.25 mm travel |
| TFT | **GMT144 ST7735 1.44-inch 128×128** (Electronperdido SKU 0263L) | 1 | SPI, PCB approx. 45 × 30 mm; vendor states 3.3/5 V supply with **matching signal levels** |
| Yellow four-digit indicator | **TM1637 0.36-inch, 4-digit yellow** (Electronperdido SKU 0589L, yellow selection) | 1 | 2 data pins (CLK/DIO), separate power/GND, four physical digits; percent/temporary encoder presentation planned |

## Manufacturer/vendor sources
- Arduino Uno: https://docs.arduino.cc/hardware/uno-rev3
- Encoder Bourns datasheet: https://www.bourns.com/docs/product-datasheets/pec11r.pdf
- Fader Bourns datasheet: https://www.bourns.com/docs/product-datasheets/ptb.pdf
- Omron B3F datasheet: https://components.omron.com/us-en/datasheet_pdf/A070-E1.pdf
- TFT vendor listing: https://electronperdido.com/shop/pantallas-displays/tft/tft-144%E2%80%B3-128x128-st7735/
- TM1637 vendor listing: https://electronperdido.com/etiqueta-producto/7-segmentos/
- TM1637 controller datasheet: https://m5stack.oss-cn-shenzhen.aliyuncs.com/resource/docs/datasheet/unit/digi_clock/TM1637.pdf

## Proposed Uno pin budget (no I/O expander)

| Uno pin | Purpose |
| --- | --- |
| D0 / D1 | Reserved for USB serial RX/TX |
| D2 / D3 | Red encoder A/B |
| D4 / D5 | Green encoder A/B |
| D6 / D7 | Blue encoder A/B |
| D8 / D12 | White encoder A/B (D12 SPI MISO unused by write-only TFT) |
| D9 | TFT D/C |
| D10 | TFT CS |
| D11 | TFT MOSI |
| D13 | TFT SCK |
| A0 | Grandmaster slider wiper |
| A1 / A2 | TM1637 CLK/DIO |
| A3 / A4 / A5 | Buttons 1/2/3 |
| 5 V / GND | Common supply/ground, current budget still to be measured |
| RESET | Potential shared TFT RESET **only after verifying module reset circuit** |
| TFT backlight | Fixed on, supply/wiring according to actual module documentation |

The proposed assignment consumes every available general-purpose pin except D0/D1, which remain dedicated to the serial port. No encoder push buttons are included.

## Wiring and mechanical precautions
1. Vendor claims the **GMT144** TFT module supports 3.3 V and 5 V, provided supply and signal levels match. For a classic 5 V Uno, the plan is to run the **specified module** at 5 V with 5 V SPI logic. Do not extrapolate this to other bare ST7735 screens: check the markings/pin labels on the delivered board first.
2. TFT RESET and backlight are not assigned independent GPIO. Confirm the module's actual RESET and LED circuitry before tying anything to Uno RESET or 5 V. Avoid any direct connection unless electrical behavior is established.
3. Bourns encoder terminals are A, C (common), B. Use pullups and switch contacts to ground; don't drive encoder contacts actively. Mechanical bounce is specified at 2 ms max under test conditions, so decoding must reject bounce/invalid steps.
4. The slider's end terminals (1/3) are intended for 5 V and GND, wiper (2) for A0. Confirm the end orientation with a multimeter before fixing which direction raises the grandmaster.
5. B3F-1070 is **not** a Cherry MX keyboard switch. It has only 0.25 mm actuation travel. The manufacturer's B32 keycaps are specifically intended for projected-plunger variants, and cannot be assumed to fit this flat-plunger model. For the typewriter aesthetic, plan a custom mechanical actuator/cap after measuring the real parts.
6. TM1637's CLK/DIO interface uses two GPIO pins and is **not** ordinary addressable I²C. Test whether the purchased yellow module is happy on 5 V and confirm pin order.
7. Keep TFT graphics memory-light: a 128×128×16-bit framebuffer needs 32 KiB; Uno SRAM is 2 KiB. Render in small regions/stream to display.
8. Prototype the modules one at a time. Avoid relying on this table as an assembly schematic until the RESET circuit, power draw, switch cap mechanics and physical header labels have been checked.

## Firmware strategy
- Input scanning uses centralized provisional constants. The sketch emits BC1
  ENC/BTN/GM records, stores host STATE and acknowledges it at 115200 baud.
- Native tests exercise input decoding/filtering; they do not compile the sketch.
- Windows CLI is the COM adapter. GUI remains a simulator. TFT and TM1637 drivers,
  segment glyph approximations, reset circuit, signal levels and power budget
  require physical verification. Do not infer hardware readiness from GUI output.
- The software Compact preset mirrors a roughly 168 × 192 mm front: vertical
  100 mm fader and display left, four encoders center, TFT and three buttons right.
  SALA patch 1.5 is editable profile data, never a fixed electrical property.
