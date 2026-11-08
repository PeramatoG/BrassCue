# BrassCue USB serial protocol — BC1 (prototype)

The Uno uses **115200 8N1** USB serial; it is not a native USB-MIDI device. All lines are ASCII terminated by `\n`. Each message is capped at 80 characters on the Bridge and 63 on the Uno.

## Device to PC

| Message | Meaning |
| --- | --- |
| `BC1 READY` | Arduino booted, or answered HELLO |
| `BC1 ENC 0 1` / `BC1 ENC 0 -1` | Clockwise/counterclockwise change on RGBW encoder 0–3 |
| `BC1 BTN 0 1` / `BC1 BTN 0 0` | Button index 0–2, down/up |
| `BC1 GM 128` | Grandmaster 0–255 |
| `BC1 ACK STATE` | Valid RGBW/master state accepted |

The receiver rejects malformed records, unknown protocol versions and out-of-range values. The old `GM 57` diagnostic messages are not BC1 messages.

## PC to device

| Message | Meaning |
| --- | --- |
| `BC1 HELLO` | Ask the Arduino to announce READY |
| `BC1 STATE 255 128 64 0 255` | Current raw R/G/B/W and grandmaster (each 0–255) |

The Bridge sends its state when READY is received, and after accepted changes. The Uno currently stores it in RAM and acknowledges it; a display driver will consume the state in a future PR.

## Windows testing

1. Check out `feature/windows-graphical-simulator` for PR #6 validation and build the CMake project in Visual Studio.
2. Open `firmware/BrassCueFirmware/BrassCueFirmware.ino` with Arduino IDE; upload it to **Arduino Uno**.
3. **Close Arduino IDE Serial Monitor** (COM ports are exclusive).
4. Run the Windows Bridge executable from your build folder:

```powershell
brasscue-bridge.exe --port COM3 --ignore-gm
```

You should see `Arduino ready: sent initial RGBW/master state.` and `Arduino acknowledged state.`. The Arduino resets when the Bridge opens its COM port; the Bridge sends HELLO once after 2500 ms only if READY has not arrived. Duplicate READY within 1000 ms is ignored; later READY resynchronizes.

`--ignore-gm` is for the current unplugged slider on A0: the input floats, and ignoring its changing readings prevents unwanted DMX updates. Remove this option once the real fader is connected.

To output Art-Net to a test receiver, explicitly specify its IPv4 address (replace the example with your receiver's IP):

```powershell
brasscue-bridge.exe --port COM3 --ignore-gm --target 127.0.0.1 --universe 0
```

The output uses ArtDmx on UDP 6454, Port-Address 0, slots 1–4 = RGBW. BC1 BTN edges drive the shared room cue engine on provisional slot 5. Until encoders are attached, values remain at startup defaults: R/G/B/W=0 and master=255.

**Limitations**: The connection is not automatically reopened after unplugging; restart the Bridge. A lost serial connection terminates the Bridge with an error. Art-Net refreshes at about 25 Hz; normal exit attempts zero frames. Hard failure cannot guarantee cleanup. Physical display drivers are not implemented. This prototype is for isolated testing, not unattended stage operation.

## Manual tests before merge
- Native `ctest` (including `brasscue-serial`) passes on Windows and Linux.
- Arduino IDE verifies the Uno sketch and reports flash/RAM use.
- Upload and open COM3 from Bridge after closing Serial Monitor; handshake and ACK appear.
- With `--ignore-gm`, the unconnected fader produces no Art-Net changes.
- Later, connect one encoder (verified wiring) and observe 0–255 bounded changes.
- Serial Monitor at 115200 should display BC1 diagnostics if Bridge is closed.
