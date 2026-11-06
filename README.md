# BrassCue

**Open-source steampunk RGBW lighting controller** — classic Arduino Uno hardware, a desktop Bridge and future MIDI/OSC output options.

> Early prototype. The desktop simulator builds without an Arduino; serial firmware and a hardware-ready product do not exist yet.

## Hardware plan

- 4 endless rotary encoders for red, green, blue and white
- Bourns 100 mm fader for grandmaster
- 3 mechanical, typewriter-style buttons
- ST7735 1.44-inch color TFT and a yellow TM1637 numeric/hex display
- Classic Arduino Uno connected to the PC by USB serial (no native USB-MIDI modification)

See [hardware selection](docs/HARDWARE.md) and [architecture](docs/ARCHITECTURE.md). No electrical pin mapping has been finalized.

## First software milestone: PC simulator

The C++17 console simulator emulates RGBW controls and an 8-bit grandmaster. Its initial network adapter creates Art-Net ArtDmx output using DMX slots 1–4 (RGBW) of Port-Address 0. Only local simulated input is supported so far.

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

## Layout

```text
AGENTS.md              Development instructions for Codex/agents
bridge/                Portable C++ console simulator and tests
firmware/              Arduino implementation (planned)
docs/                  Hardware selection and software architecture
.github/workflows/     Windows/Linux Bridge checks
```

## Project status and license

Initial development; contributions and testing are welcome after the hardware is verified.

Software is licensed under the **GNU GPL v3.0** (see [LICENSE](LICENSE)). Future mechanical/electronic design files may use a separate open-hardware license.
