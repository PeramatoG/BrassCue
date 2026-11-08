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
- ControllerSession owns one canonical profile and live 8-bit RGBW/fader State; CueEngine is derived. Test scaling, patch, cues and ArtDmx serialization.
- Windows CLI BC1 and profile patching are implemented; GUI COM, physical display drivers and stage-readiness are not verified.

## GUI direction
- Native GUI uses wx::core / wx::base; BRASSCUE_ENABLE_WEB_PANEL optionally requires wx::webview. Use the user's existing classic vcpkg x64-windows installation.
- Preserve headless C++ core/CLI targets and CI without wxWidgets by keeping GUI optional in CMake.
- Design Windows UI with a restrained dark palette and brass accents; favor legibility, resizing and accessible controls over decoration.
- Keep calculations, Art-Net framing and protocol semantics in shared testable modules, not wx event callbacks.
- C++ owns effective GM/RGBW, preset ownership, circular gesture math and snapshot diffs. JS is local input/DOM presentation only; retain native fallback and block external navigation.
- Write profiles v2; import v1 explicitly as DMX actions without reinterpreting SALA labels. New HOUSE defaults are internal GM presets, not channel 1.5.
- Broadcast requires an active explicit IPv4 NIC and its real mask; bind source/interface, keep Auto unicast and stop output when the route changes. Never test broadcast on a real network implicitly.
- GUI simulation must not silently claim to be connected to the Uno: USB COM integration is future work.
- Do not introduce wxWidgets into the portable serial/core unit tests.

## Validation
- Build and run `ctest` for each relevant Bridge change.
- Include unit tests for value scaling and Art-Net framing when editing those areas.
- Make no wiring or electrical-safety claims without checking the actual modules and documentation.
- Keep GitHub Actions green before merging. Use feature branches/PRs rather than direct work on main.

## Sustainable module policy
- Follow docs/ARCHITECTURE.md for ownership and dependency direction; keep one source of truth there.
- MainFrame composes widgets and coordinates services; editors, file workflow, protocol, engine and transport remain separate.
- Use explicit CMake source lists. Core must not include wxWidgets or Arduino.
- Every test must execute checks with and without NDEBUG; never put verification or essential side effects in standard assert.
- Run Debug/Release CTest plus python3 tests/check_source_file_size.py. CI also builds Windows GUI with classic vcpkg.
- tests/source_file_size_policy.json sets a 600 physical-line maximum; review responsibility around 300–400 lines. No automatic baseline increases.
- Use concise GPL-3.0 headers without invented attribution; .clang-format defines readable formatting.
- Keep profiles transactional, preserve untouched target precision on unit changes, keep network OFF on load and explicitly clean abandoned output addresses.
- Update user documentation when behavior changes. Record unexecuted checks and hardware limitations accurately.
- Work only on the requested feature branch; agents must never merge, publish a release or write directly to main.
