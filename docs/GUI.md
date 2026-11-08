# BrassCue desktop simulator

The wxWidgets GUI runs without an Uno. The physical-style panel has fader/display
on the left, R/G/B/W rotary controls in the middle, and TFT plus SALA 100/50/0 on
the right. Network controls sit below. GUI does not open COM ports; use the CLI
for BC1. Operational levels use percentages, not hexadecimal.

## Windows / VS2026 / classic vcpkg

In an x64 Developer PowerShell with the existing vcpkg checkout:

```powershell
& "$env:VCPKG_ROOT/vcpkg.exe" install wxwidgets:x64-windows --classic
cmake -S . -B out/gui-x64 -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows -DVCPKG_MANIFEST_MODE=OFF `
  -DBRASSCUE_BUILD_GUI=ON -DBUILD_TESTING=ON
cmake --build out/gui-x64 --config Debug --parallel 4
ctest --test-dir out/gui-x64 -C Debug --output-on-failure
cmake --build out/gui-x64 --config Release --parallel 4
ctest --test-dir out/gui-x64 -C Release --output-on-failure
.\out\gui-x64\bridge\Debug\brasscue-gui.exe
```

For Ninja omit `-A x64` and specify `-DCMAKE_BUILD_TYPE=Debug` (or Release)
in separate directories. Windows defaults GUI ON; Linux defaults OFF.
Linux GUI uses system wxWidgets 3.2 (`libwxgtk3.2-dev`) with
`cmake -S . -B build-gui -DBRASSCUE_BUILD_GUI=ON -DCMAKE_BUILD_TYPE=Debug`.
GUI configurations run nine CTest tests, including a real modal-editor commit/cancel test. Headless configurations run eight. On Linux run GUI CTest under a display (for example `xvfb-run -a ctest --test-dir build-gui --output-on-failure`). Python 3 is required for test integration. No manifest migration is needed.

CI includes Windows GUI builds with classic vcpkg and tests in Debug/Release,
Linux GUI compilation, and four Windows/Linux headless configurations. An added
job is not evidence of a passed run: check the actual PR results before merge.

## Controls and contextual editors

Use mouse wheel, drag vertically, arrows (one DMX step), Page Up/Down (ten steps)
on each knob. Fader is vertical with maximum at the top. Right-click any knob,
label, fader, numeric display, TFT or cue button to edit. Keyboard context-menu
invocation is also accepted. One modal editor owns a candidate profile: edit
labels, table rows, units, fader role, button mode/Kill/fades, TFT title/view as
applicable, then OK once. Cancel discards everything. Add/remove table rows and
edit cells in place. Errors identify invalid rows; duplicates cannot be committed.
Changing unit presentation does not rewrite untouched DMX targets.

Toggle buttons persist across release; pressing the active one again turns it
off. Flash buttons release on mouse up outside, lost capture, key up or lost
focus. A 40 ms pointer-state check releases a mouse hold even if a native backend consumes mouse-up; idle mouse state does not cancel keyboard holds. Labels show OFF, ON, FADE IN or FADE OUT. Kill affects only the three room
buttons. See [Architecture](ARCHITECTURE.md) for HTP crossfade behavior.
TFT Color displays an approximate additive RGBW mix; Levels draws actual level
bars. The four-character display temporarily shows an encoder for two seconds,
then current GM. Custom labels keep stable R/G/B/W codes.

Profile menu: Open/import (Ctrl+O), Save (Ctrl+S), Save as/export (Ctrl+Shift+S), Restore defaults. Network starts
OFF on launch/open/reset. Network edits stop output intentionally; re-enable when
ready. Broadcast additionally requires a conventional x.x.x.255 target and opt-in.
Profile operations and limitations are documented in [Profiles](PROFILES.md).

## Manual Windows acceptance script

1. Launch without Arduino: R/G/B/W zero, GM 100%, output OFF. Resize and check
   at Windows 100%, 150%, 200% DPI, keyboard focus/tab and screen-reader labels.
2. Wheel/drag/arrow each encoder; check percent, mix and display takeover. Move
   another encoder before two seconds; GM must return two seconds after that move.
3. Right-click each control; Cancel must not dirty the profile. Change labels,
   add `1.23=255`, `2.8=128`, `3.128=0`, switch units twice, save/reopen and inspect
   targets. Duplicate/out-of-range entries must fail with the old profile intact.
4. Change fader GM to patch with target 128: midpoint gives about 64 and does not
   dim encoders. Restore GM: its patch is ignored and all output scales.
5. Toggle SALA 100->50->0 and press active again; RGBW remains unchanged. Set
   nonzero fades, reverse mid-fade, disable Kill and verify HTP coexistence.
   Set Flash; release outside and via keyboard/lost focus; it must fade out.
6. Change TFT to Levels and edit its title; verify drawing changes immediately.
7. Bind an isolated UDP receiver to 127.0.0.1:6454 only. Enable output, verify
   25 Hz full frames for each patched universe. Repatch to a new universe and
   inspect one zero frame for the old one; removed channels are zero. Disable
   output/close and check zero frames. Editing IP stops output, never silently
   redirects it. No real fixtures are needed for these checks.
8. Corrupt a saved file / change its version, then import: error and active
   profile unchanged. Test unwritable save paths and unsaved-change prompts.
9. Keep CLI `--port COM3 --ignore-gm` for separate handshake tests. Real Arduino,
   TFT/TM1637, electrical pinout, power budget and receiver-loss behavior remain
   physical bench checks. Do not merge until Windows build and manual checks pass.
