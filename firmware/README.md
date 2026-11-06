# Arduino firmware (planned)

Target: classic Arduino Uno (ATmega328P) connected to the PC via ordinary USB serial.

Firmware implementation starts after confirming the TFT board's electrical characteristics and the final pin budget. Do **not** assume the Bridge's current terminal simulator is an Arduino serial protocol.

Initial milestones: input scan/debounce, encoder delta tests, fader calibration, independent display tests, then a documented versioned serial protocol and integration with the Bridge.
