# BrassCue

**Open-source steampunk RGBW lighting controller** — classic Arduino Uno hardware, a desktop Bridge and future MIDI/OSC output options.

> Early prototype. The console Bridge supports a tested BC1 USB serial handshake with an Arduino Uno; the graphical simulator is software-only and the complete hardware controller is not ready yet.

## Hardware plan

- 4 endless rotary encoders for red, green, blue and white
- Bourns 100 mm fader for grandmaster
- 3 mechanical, typewriter-style buttons
- ST7735 1.44-inch color TFT and a yellow TM1637 numeric/hex display
- Classic Arduino Uno connected to the PC by USB serial (no native USB-MIDI modification)

See [hardware selection](docs/HARDWARE.md) and [architecture](docs/ARCHITECTURE.md). No electrical pin mapping has been finalized.

## First software milestone: PC simulator

The C++17 console simulator emulates RGBW controls and an 8-bit grandmaster. Its initial network adapter creates Art-Net ArtDmx output using DMX slots 1–4 (RGBW) of Port-Address 0. The headless Bridge also supports BC1 serial input from the Uno via `--port COM3` on Windows.

```sh
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Run on Linux with `./build/bridge/brasscue-bridge --simulate` (on Windows/MSVC, `build\\bridge\\Release\\brasscue-bridge.exe --simulate`).

At the `>` prompt, try:

```text
r 255
g 0x80
b 64
w 20
m 128
show
quit
```

**Network output is OFF by default.** To explicitly send Art-Net UDP to a test receiver at a chosen IPv4 address:

```text
brasscue-bridge --simulate --target 127.0.0.1 --universe 0
```

No Art-Net discovery or broadcast is performed. This is **not** a production lighting controller: continuous refresh, disconnect/failsafe behavior, channel configuration, Arduino firmware and the serial protocol are pending.

## Windows graphical simulator (wxWidgets)

An optional dark-themed desktop simulator is available as `brasscue-gui`, featuring RGBW controls, fine adjustments, grandmaster, numerical output, color preview, three virtual buttons and explicit opt-in Art-Net transmission.

Build it using the **existing classic vcpkg wxWidgets** installation with `-DBRASSCUE_BUILD_GUI=ON`. It is not part of the default headless build. See [GUI build guide](docs/GUI.md) and [BC1 serial protocol](docs/SERIAL_PROTOCOL.md).

## Layout

```text
AGENTS.md              Development instructions for Codex/agents
bridge/                Portable C++ core, console Bridge, wxWidgets GUI and tests
firmware/              Arduino implementation (planned)
docs/                  Hardware selection and software architecture
.github/workflows/     Windows/Linux Bridge checks
```

## Project status and license

Initial development; contributions and testing are welcome after the hardware is verified.

Software is licensed under the **GNU GPL v3.0** (see [LICENSE](LICENSE)). Future mechanical/electronic design files may use a separate open-hardware license.
