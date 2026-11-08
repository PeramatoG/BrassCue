# BrassCue

**Open-source steampunk RGBW lighting controller** — classic Arduino Uno hardware, a desktop Bridge and future MIDI/OSC output options.

> Bench-test prototype. Windows BC1 CLI and a wxWidgets desktop simulator share a multi-universe cue engine; physical display drivers and electrical validation remain pending.

## Hardware plan

- 4 endless rotary encoders for red, green, blue and white
- Bourns 100 mm fader for grandmaster
- 3 mechanical, typewriter-style buttons
- ST7735 1.44-inch color TFT and a yellow TM1637 four-character percent display
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

Output refreshes at approximately 25 Hz while enabled; normal stop attempts zero frames. This is **not** a production lighting controller: UDP delivery, process-loss behavior and electrical/hardware readiness are not guaranteed. No discovery or implicit broadcast.


## Desktop and profiles

The optional wxWidgets GUI has a three-column physical layout, rotary RGBW
controls, vertical fader, temporary encoder/GM readout and configurable TFT.
The default HOUSE 100/50/0 buttons are exclusive internal GM presets (255/128/0),
synchronized with the virtual fader, display and output. Explicit DMX cues remain
an alternative; labels, targets, Flash/Toggle, Kill and fades are editable.
An opt-in local wxWebView panel shares the C++ state and retains native fallback.
Multi-universe profiles can be imported/exported without saving live output.
GUI starts in simulation with Art-Net OFF.

See [GUI builds and manual checks](docs/GUI.md),
[architecture and output contracts](docs/ARCHITECTURE.md),
[portable profiles v2 / v1 migration](docs/PROFILES.md), and
[BC1 protocol](docs/SERIAL_PROTOCOL.md). Python 3 runs the CLI integration check;
all C++ test checks remain active in Debug and Release.
