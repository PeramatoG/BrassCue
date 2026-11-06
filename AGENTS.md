# Agent instructions — BrassCue

## Scope and approach
- BrassCue is an open-source steampunk RGBW lighting controller, split into Arduino Uno firmware and a desktop Bridge.
- Work in small, reviewable changes. Prefer correct, testable behavior over speculative abstractions.
- Preserve existing behavior and document any changed protocol or user-visible behavior.
- All code comments and identifiers should be in English. Documentation may be in English.
- Never assume unverified electrical pinouts, supply voltages or module level shifting are confirmed.

## Firmware constraints
- Target is a classic Arduino Uno (ATmega328P) over USB serial, not native USB-MIDI.
- Four detented rotary encoders, a 100 mm analog grandmaster fader, three buttons, a 1.44-inch ST7735 SPI TFT, and a yellow TM1637 numeric display are planned.
- Pin map, electrical interface, debouncing, display boards, and USB protocol are NOT final. Document decisions before freezing them.
- Respect Uno SRAM constraints (2 KB): do not add a full-frame 128×128 RGB565 buffer.
- Use nonblocking input scanning and avoid dynamic allocation in timing-sensitive firmware code.

## Desktop Bridge
- Use C++17 and CMake for now; keep dependencies small and justified.
- Art-Net is the first output transport. MIDI, OSC and sACN are potential future outputs, not current features.
- Separate controller state, transport serialization, and user interaction.
- Do not send Art-Net by default. Require an explicit target IP when running the simulator.
- State is 8-bit RGBW plus an 8-bit grandmaster. Test scaling and ArtDmx packet serialization.
- Do not advertise USB serial support, fixture patching, DMX timing, or stage-readiness until implemented and verified.

## Validation
- Build and run `ctest` for each relevant Bridge change.
- Include unit tests for value scaling and Art-Net framing when editing those areas.
- Make no wiring or electrical-safety claims without checking the actual modules and documentation.
- Keep GitHub Actions green before merging. Use feature branches/PRs rather than direct work on main.
