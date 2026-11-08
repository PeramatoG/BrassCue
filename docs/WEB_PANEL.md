# Local web panel contract

Enable `BRASSCUE_ENABLE_WEB_PANEL` with GUI ON. Windows requires the classic
vcpkg `wxwidgets[webview]:x64-windows` feature, its WebView2 loader, and Microsoft
Edge WebView2 Evergreen Runtime. Only the Edge backend is requested. Linux uses
wxWebView WebKitGTK; version 2.40 or newer is needed for native asynchronous
script calls. Native builds have no webview dependency and remain fully usable.

`web_assets/compact.html`, `.css`, `.js` are copied beside each executable under
`web_assets/` and installed under `bin/web_assets`. Keep that directory when
packaging. There is no server, npm/build dependency, CDN or external resource.
The HTML CSP denies connections, objects, images, forms and arbitrary sources;
only local scripts/styles are allowed. User labels use textContent. The host
allows exactly the main file URL and vetoes other navigation/new windows.

## Input: BCW1

ASCII text, at most 256 bytes, fully parsed before dispatch:
`BCW1 <epoch> <sequence> <operation> [arguments]`. Epoch is a nonzero uint64;
sequence is 1..4294967295, strictly increasing. Invalid input does not consume a
sequence. The host increments epoch on each local navigation/reload and releases
held input. Only `ready` with sequence 1 unlocks subsequent input. Old epochs,
repeated/stale sequences, malformed values and unknown operations are rejected.

| Operation | Arguments |
| --- | --- |
| ready, release | none |
| fader | byte 0..255 |
| edit | element 0..8 (RGBW, fader, three buttons, TFT) |
| wheel | encoder 0..3, signed step -1 or 1 |
| key | encoder 0..3, code 0/+1, 1/-1, 2/+10, 3/-10 |
| drag-start, drag-move | encoder 0..3, normalized finite x/y bounded ±4 |
| drag-end | encoder 0..3 |
| button | button 0..2, down 0/1, source 0 keyboard/touch, 1 mouse, 2 accessible pulse |

C++ owns atan2 deltas, wrap, fractional accumulation, dead zone and teleport
rejection; Core clamps values. Per-source button holds combine before a Core
edge. Pointer polling releases mouse holds without canceling keyboard holds.
Focus/capture/visibility/close/reload releases input; a successful ready event
starts a full snapshot. Programmatic value updates never dispatch user input.

## Output and failures

`ViewSnapshot` is toolkit-free C++ data. Initial/full and subsequent JSON diffs
contain version 1, fader (byte/percent/effective GM/label), display, connection,
changed encoder/button entries and optional TFT data. All percentages, angles,
color/levels and selection/phase come from C++. JSON escaping includes control
characters, `<`, quotes, backslashes and U+2028/U+2029. No received text is code.
JS `apply` only updates DOM/ARIA/classes/styles; there is no DMX engine in JS.

`web_script.cpp` evaluates fixed host-authored begin/apply calls with encoded
C++ data using Edge ExecuteScript or WebKit's public asynchronous C API. It
avoids wxWidgets' eval-based wrapper so CSP needs no `unsafe-eval`. Linux resolves
the API from the already loaded backend, avoiding mixed libsoup 2/3 runtimes.
Weak references protect completion after window destruction. One snapshot script
is in flight; its epoch/request token guards stale completions. Further changes
coalesce into the latest snapshot. Identical snapshots produce no script traffic.

Backend absence, missing assets, script/load errors or a 10-second readiness/
script timeout release held input and expose native controls with a diagnostic.
“Prefer native controls” permits manual handoff. No renderer action enables a
socket. A missing runtime is a distinct CTest skip (77), not a passed browser
interaction. Actual renderer tests exercise DOM clicks against ControllerSession,
GM/display/fader/selection synchronization, escaped labels, reload and navigation
veto. Pure protocol tests exercise malformed/stale messages without a browser.
