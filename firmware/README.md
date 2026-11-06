# BrassCue Uno diagnostic firmware

The first sketch is an input-scanning **diagnostic prototype**, not the final firmware or the Bridge serial transport.

- Open `BrassCueFirmware/BrassCueFirmware.ino` in Arduino IDE.
- Select **Arduino Uno** (classic ATmega328P), and upload.
- Open Serial Monitor at **115200 baud**.
- Input lines after attaching **verified test wiring** are `ENC 0 +1` or `ENC 0 -1`, `BTN 0 DOWN` / `UP`, and `GM 0..255`. Encoder numbering 0–3 corresponds to R/G/B/W. Actual UART output uses `1` rather than `+1` for positive steps.

**Do not connect the TFT or TM1637 to this pin budget until real board labels, supply and reset requirements have been verified.** The sketch does not use them yet.

The native CMake tests cover hardware-independent decoder, button debounce and fader filter logic, but do **not** compile the Arduino sketch. Perform an Arduino IDE build to verify the AVR integration before bench testing.

See [the verified hardware selection and provisional pin table](../docs/HARDWARE.md).
