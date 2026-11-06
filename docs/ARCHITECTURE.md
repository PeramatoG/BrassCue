# Architecture (initial)

```text
Four RGBW encoders       100 mm grandmaster fader       3 buttons
                 \              |                 /
                   Arduino Uno (ATmega328P)
                        | USB serial (future)
                 BrassCue Bridge (PC)
                        | state / output adapters
                   Art-Net UDP output (first)
                  MIDI / OSC / sACN (future)
```

## Roles
- **Arduino firmware:** scan controls, debounce/accumulate changes, update TFT and numeric display, exchange state with the Bridge.
- **Bridge:** own authoritative RGBW/grandmaster state; apply grandmaster; map channels and emit network output. A future serial transport will handle the hardware.
- **Simulator (initial implementation):** terminal-driven substitute for incoming hardware controls. Does not open serial ports.

## Initial state model
- Raw R, G, B and W each range from 0 to 255.
- Master ranges from 0 to 255, initially 255.
- Each emitted component is `(raw * master + 127) / 255` using integer arithmetic.
- The first prototype uses DMX channels 1–4 for R/G/B/W, in Art-Net Port-Address 0 by default. Patching and profiles are future features, not a fixed hardware decision.

## Art-Net simulator transport
- The simulator emits ArtDmx packets containing 512 DMX slots, UDP port 6454.
- Art-Net Protocol Version 14, little-endian opcode 0x5000, with the chosen 15-bit Port-Address in bytes 14–15.
- Network output is disabled unless the user passes `--target <IPv4>`. There is no implicit broadcast or discovery.
- The simulator sends on an interactive state change or explicit `send`; continuous output, keepalive, reconnect behavior and device-loss policy are still to be designed.

## Deferred design choices
1. Exact Uno pin map and TFT voltage-level compatibility; confirm from actual modules.
2. Serial framing, versioning, message acknowledgments and USB disconnect behavior.
3. Encoder step/acceleration, button bindings and visual UI layout.
4. Display behavior, preset/transition handling, output universes/channels, fixture modes.
5. Desktop UI toolkit and native packaging. A small CLI prototype comes first.
