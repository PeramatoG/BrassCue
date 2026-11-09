# BrassCue desktop simulator

The wxWidgets GUI runs without an Uno. The physical-style panel has fader/display
on the left, R/G/B/W rotary controls in the middle, and TFT plus HOUSE 100/50/0 on
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
Native GUI configurations run twenty CTest tests, including real modal editor/button presentation checks and OS keyboard/mouse interaction.
OS interaction uses wx UI simulation where enabled, or Win32 SendInput on Windows
vcpkg builds that disable it; other unsupported backends explicitly skip (77). Headless configurations run sixteen. Web builds add an actual embedded-renderer integration test. On Linux run GUI CTest under a display (for example `xvfb-run -a ctest --test-dir build-gui --output-on-failure`). Python 3 is required for test integration. No manifest migration is needed.

CI includes Windows GUI builds with classic vcpkg and tests in Debug/Release,
Linux GUI compilation, and four Windows/Linux headless configurations. An added
job is not evidence of a passed run: check the actual PR results before merge.

## Optional local web presentation

Use a separate build directory, retaining the existing native build:

```powershell
& "$env:VCPKG_ROOT/vcpkg.exe" install 'wxwidgets[webview]:x64-windows' --classic --recurse
cmake -S . -B out/web-x64 -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows -DVCPKG_MANIFEST_MODE=OFF `
  -DBRASSCUE_BUILD_GUI=ON -DBRASSCUE_ENABLE_WEB_PANEL=ON -DBUILD_TESTING=ON
cmake --build out/web-x64 --config Debug --parallel 4
ctest --test-dir out/web-x64 -C Debug --output-on-failure
cmake --build out/web-x64 --config Release --parallel 4
ctest --test-dir out/web-x64 -C Release --output-on-failure
.\out\web-x64\bridge\Release\brasscue-gui.exe
```

Install [Microsoft Edge WebView2 Evergreen Runtime](https://developer.microsoft.com/microsoft-edge/webview2/)
(x64) on the target machine. Keep the vcpkg-copied `WebView2Loader.dll` and wx
runtime DLLs with the executable, plus its `web_assets` directory. A build-time
feature/header failure is an explicit dependency error. At runtime an absent
Edge renderer or load/script failure falls back to native controls with a
status diagnostic; View → Prefer native controls also switches safely. The
browser test reports missing runtime as SKIP, separately from passing tests.

Linux optional builds also need `libwxgtk-webview3.2-dev`, WebKitGTK >=2.40 and
its runtime processes. Use `-DBRASSCUE_ENABLE_WEB_PANEL=ON`. Standard package
installations supply their process paths. See [web protocol](WEB_PANEL.md).

Windows title bars use documented `DwmSetWindowAttribute` with
DWMWA_USE_IMMERSIVE_DARK_MODE; unsupported systems fail nonfatally. wx >=3.3
also requests its supported dark mode. Native menus, file choosers and grid
editors can still follow OS/theme settings. No custom frame or undocumented
Windows theme functions are used; dark title appearance needs a visual check on
the user's Windows build.

## Controls and contextual editors

Default encoder labels are RED, GREEN, BLUE and WHITE; existing profiles keep their labels.
Fade editors use seconds (0..60 s), with 0.1 s steps and one decimal; storage
and the engine retain milliseconds for compatibility.

Use mouse wheel, circular relative drag, arrows (one DMX step), Page Up/Down (ten steps)
on each knob. Fader is vertical with maximum at the top. Its recessed dark rail, wide metallic cap
and central brass-edged groove use DPI-scaled drawing without image assets. Drag
the cap or click the rail; arrows adjust one byte, Page Up/Down ten, Home selects
255 and End selects 0. Tab enters/leaves the fader and a brass focus outline marks
keyboard focus. Native and WebView controls share the same session/snapshots. Right-click any knob,
label, fader, numeric display, TFT or cue button to edit. Keyboard context-menu
invocation is also accepted. One modal editor owns a candidate profile: edit
labels, table rows, units, fader role, button mode/Kill/fades, TFT title/view as
applicable, then OK once. Enter in a label or fade field confirms that field; Enter in a grid
commits only its cell. The editor stays open. Tab/Shift+Tab retain normal navigation;
Escape retains wxWidgets dialog cancellation. Only explicit OK (mouse, keyboard
activation while OK is focused, or its standard accelerator) commits the profile.
Cancel discards everything. Add/remove table rows and
edit cells in place. Errors identify invalid rows; duplicates cannot be committed.
Changing unit presentation does not rewrite untouched target bytes. Button action chooses explicit DMX assignments or internal GM/R/G/B/W targets; Add all RGBW creates four independent rows. Editing and Cancel do not alter live values.

Buttons display only their configured labels. Amber fill/border indicates
selection, including a selected zero preset; transition styling is subtle.
ARIA and the Windows native accessible button expose selection; validate Linux assistive technology manually. Dirty snapshots prevent repeated
button repaint while an unrelated encoder changes. Wheel remains exactly one
signed byte step per event. Drag begins without changing a value, supports
multiple clockwise/counterclockwise turns, wraps angles, ignores center noise/
teleports, and clamps in Core. Keyboard step sizes are unchanged.

Default HOUSE 100/50/0 are internal GM commands. Fader, display, TFT and DMX
follow their effective levels/fades. Manual movement cancels the owning preset;
matching 0/128/255 highlights without a synthetic press. Toggle-off targets zero;
Flash returns to previous still-owned values. Kill off uses the latest command
per logical target. A superseded preset loses its highlight when it owns no
remaining controls; presets still owning disjoint controls can remain highlighted. Explicit DMX actions retain HTP behavior. Focus/capture loss
and renderer handoff release Flash. See [Architecture](ARCHITECTURE.md).
TFT Color displays an approximate additive RGBW mix; Levels draws actual level
bars. The four-character display temporarily shows an encoder for two seconds,
then current GM. Custom labels keep stable R/G/B/W codes.

Profile menu: Open/import (Ctrl+O), Save (Ctrl+S), Save as/export (Ctrl+Shift+S), Restore defaults. Network starts
OFF on launch/open/reset. Network edits stop output intentionally; re-enable when
ready. Select a real active IPv4 interface by name/IP/mask, or Auto for unicast.
Broadcast requires an explicit capable NIC; its read-only destination is
IPv4 | ~netmask (not an assumed .255). An empty/invalid disabled unicast draft
does not block broadcast; the last valid unicast preference is retained. The
socket binds source and interface.
Missing/changed NICs stop output, with a warning, and require reactivation.
Loopback/unsupported masks cannot broadcast. VPN/virtual interfaces follow their
reported capability; no silent fallback to a different route.
Profile operations and limitations are documented in [Profiles](PROFILES.md).

## Manual Windows acceptance script

1. For a clean baseline, launch without Arduino and use Restore defaults:
   R/G/B/W zero, GM 100%, output OFF. Normal relaunch recovers the previous session. Resize and check
   at Windows 100%, 150%, 200% DPI, keyboard focus/tab and screen-reader labels.
2. Wheel/drag/arrow each encoder; check percent, mix and display takeover. Move
   another encoder before two seconds; GM must return two seconds after that move.
3. Right-click each control; Cancel must not dirty the profile. Change labels,
   add `1.23=255`, `2.8=128`, `3.128=0`, switch units twice, save/reopen and inspect
   targets. Duplicate/out-of-range entries must fail with the old profile intact.
   In every encoder, fader, button and TFT editor, press Enter in labels, both
   fade fields and the first/last grid cells: only the field/cell commits, the
   dialog stays open. Check Tab/Shift+Tab, focused OK/Cancel with Enter and Escape.
   Cancel after multiple field commits must preserve the original profile exactly.
4. Change fader GM to patch with target 128: midpoint gives about 64 with GM100;
   an internal GM50 preset scales that output to about 32. Restore GM: its patch is ignored and all output scales.
5. Toggle HOUSE 100->50->0 and press active again; RGBW remains unchanged. Set
   nonzero fades, reverse mid-fade, disable Kill and verify last-command internal
   ownership. Set Flash; release outside and via keyboard/lost focus; previous
   still-owned values must return. Check default GM50 synchronizes fader/display.
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

10. Repeat control/editor checks with web enabled and with Prefer native controls.
    Reload the local page during Flash/fade; ensure no stuck input, duplicate
    Toggle, page reload per tick or feedback loop. Try a non-ASCII/HTML-looking
    label; it must appear literally. Check idle updates and unchanged buttons.
11. Check circular motion across ±pi, center crossings, multiple turns, saturation,
    keyboard/wheel parity, DPI/resizing, fader cursor, TFT and zero selection.
    At 100/150/200% DPI and the minimum window size, inspect the fader cap/groove,
    rail and focus outline. Drag from an off-center point on the cap without a
    value jump; check full travel 0/128/255 and preset/manual synchronization.
    Check the native Windows slider role/value with Narrator; Linux assistive
    technology remains a manual check for the painted control.
12. Verify /24, /16, /23, /25 broadcast calculations without enabling output.
    Disconnect the selected NIC; output must stop within five seconds. Test actual
    broadcast only after explicitly arranging an isolated receiver/network.
13. On a machine without WebView2, verify understandable native fallback. With
    WebView2 installed, inspect the dark title/menu/dialog boundaries honestly.
    A green automated check does not replace these Windows visual checks.

## Quick RGBW Patch

Open **Profile → Quick RGBW Patch** (`Ctrl+Shift+P`, or the menu keyboard
accelerator) in either native or WebView mode. Add a named group such as HOUSE
PANELS. Set its four-channel order (`RGBW`, `GRBW`, etc.) and stride (default 4).
Each block row supplies visible universe, starting DMX address and fixture count.
Add more blocks for other universes. There is no automatic rollover: overflow
past channel 512 is an error, including a single four-slot fixture at start 510.

**Update group / Preview** validates the entire candidate profile and lists each
fixture's group, number, universe, start and individual R/G/B/W channel. OK is
available only after a valid preview; changing fields requires a new preview.
Selecting another group retains its draft. OK applies all groups together;
Cancel discards adds, removals and edits. Use Save / export to persist them in a
v3 portable profile. The application-local session also restores committed groups
on restart automatically; use Open / import to select a portable configuration.

Opening an encoder editor shows its fixture-group assignments in a read-only
list (group, fixture, universe, channel and target). These addresses are already
assigned to that encoder after Quick Patch OK. The separate **Additional manual
assignments** table can be empty while the encoder still controls every group
fixture. Deleting a manual row does not delete group routes; edit/remove those
in Profile → Quick RGBW Patch.

Manual assignments remain independent. The built-in manual patch occupies 1.1
through 1.4, so a new block initially starts at 1.5. Choose your actual addresses
and set the physical fixtures to match separately. Quick Patch never programs
fixture hardware. If you need group fixtures at 1.1, explicitly remove those
manual rows in the existing encoder editors first. Group-generated targets are
255; encoder positions and GM scale every fixture normally. Internal presets
continue to operate the shared RGBW/GM controls. Existing arbitrary manual
encoder, fader and DMX button patches are preserved and reserve their addresses.
Conflicts, duplicate names, invalid channel orders and combined capacity overflow
must be corrected before applying. See [Profiles](PROFILES.md) for limits.

With Art-Net ON, a routing change asks **Stop output and apply the new patch?**
(default No). No discards the proposed application. Yes attempts zero frames on
old routes and applies with output OFF. Re-enable only when ready. Label-only
edits do not stop output. Load/reset never enables output.

Manual acceptance: create two groups with different channel orders and explicit
blocks in two universes; inspect every preview column. Save/reopen and repatch
one group, then cancel another edit. Check the manual rows/target bytes are
unchanged and all corresponding fixtures follow RGBW, GM and HOUSE presets.
Try a block crossing 512 and a manual-address collision; OK must stay disabled.
On isolated loopback output, decline then accept a live repatch and verify the
old slots/universes are zeroed and output remains OFF after accepting. Inspect
keyboard Tab/Enter/Escape, resizing and screen-reader labels on the owner OS.

## Automatic session recovery

Startup automatically restores the last committed configuration and checkpointed
levels/cue state, including edits not exported with Save. Art-Net always starts
OFF, and an unavailable saved NIC is shown as unavailable without a substitute.
The status line identifies a restored session or warns about backup recovery or
save failure. There are no routine blocking startup dialogs.

**Save profile / Save profile as** still writes portable `.bcue` configuration.
The title's asterisk and associated Save path survive recovery; autosave does not
clear unsaved portable edits. Closing preserves the session without a discard
prompt. Open/import and Restore defaults still confirm replacement of unsaved
portable configuration. An interrupted fade holds its checkpointed level; Flash
is released safely. See [Session recovery](SESSION_RECOVERY.md) for exact policy,
backup files, autosave timing and failure handling.

Manual acceptance: change all five logical controls, configure a patch fader and
unsaved fixture group, select overlapping internal/DMX cues and close/relaunch.
Verify configuration, levels, held selections and output OFF before explicitly
enabling an isolated receiver. Repeat with a fade interrupted by termination,
Flash held, missing NIC, corrupt current snapshot with valid previous snapshot,
and unavailable recovery storage. Check the asterisk remains until explicit
portable Save, and that a cancelled modal draft is absent after restart.
