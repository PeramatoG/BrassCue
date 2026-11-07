# BrassCue desktop GUI (first milestone)

BrassCue includes two separate Windows executables:

- `brasscue-bridge`: headless/console application; supports the Windows Uno COM transport, Art-Net and terminal simulation.
- `brasscue-gui`: wxWidgets **simulation-only** desktop window. Hardware serial integration is deliberately deferred; the device is not opened by the GUI yet.

The GUI renders four RGBW controls (sliders with ±1 nudge buttons), a grandmaster, three unassigned test buttons, a software additive RGBW preview, numerical decimal/hex values, and an opt-in Art-Net UDP output. GUI state and ArtDmx serialization reuse `bridge_core.hpp` and `artnet_udp.hpp`; they do not duplicate network framing.

## Windows / Visual Studio 2026 using installed classic vcpkg

The GUI is **enabled by default on Windows** (`BRASSCUE_BUILD_GUI=ON`). Linux builds default to OFF, and the existing Windows/Linux core CI explicitly sets `BRASSCUE_BUILD_GUI=OFF` because its runners do not install wxWidgets. Set `-DBRASSCUE_BUILD_GUI=OFF` only when you intentionally need a headless build.

BrassCue expects a **pre-existing** classic vcpkg installation with `wxwidgets:x64-windows` (as already used for Perastage). It does not run `vcpkg install` or use manifest mode automatically.

In a Visual Studio x64 Developer PowerShell, confirm `$env:VCPKG_ROOT` points to your actual vcpkg checkout (the one containing `scripts/buildsystems/vcpkg.cmake` and `installed/x64-windows`). Then configure with a separate build directory to avoid overwriting the existing console build cache:

```powershell
cmake -S . -B out/gui-x64 -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DVCPKG_MANIFEST_MODE=OFF `
  -DBRASSCUE_BUILD_GUI=ON `
  -DBUILD_TESTING=ON
cmake --build out/gui-x64 --config Debug
ctest --test-dir out/gui-x64 -C Debug --output-on-failure
.\out\gui-x64\bridge\Debug\brasscue-gui.exe
```

If the existing checkout does not have a wxWidgets package configuration for x64-windows, use the same package setup you already use for Perastage; do not install another toolchain just for BrassCue. A different generator (Ninja) may place the executable directly in `out/gui-x64/bridge/`.

Visual Studio's CMake settings can pass the same cache variables. The `-A x64` argument applies to a Visual Studio multi-configuration generator and should be omitted if using Ninja. **CMake options are cached:** if your existing Visual Studio build folder was configured with `BRASSCUE_BUILD_GUI=OFF`, changing the default does not overwrite that cache. Set the option to ON in CMake settings or reconfigure that build directory with `-DBRASSCUE_BUILD_GUI=ON`.

## Test behavior
1. Drag sliders / use ±1 buttons and see decimal/hex plus output DMX 1–4 update.
2. Grandmaster scales RGBW; adding white contributes to all three preview RGB components.
3. Network output is OFF by default. Select a test IPv4 target, universe/port-address 0–32767, then check Enable output to send an ArtDmx packet.
4. The 3 virtual buttons are unassigned; clicking one displays a status message only.
5. The existing headless CLI and native tests must continue to work.

### Known limitations
- This is a first desktop GUI; the Windows build requires the user's existing wxWidgets/vcpkg toolchain and should be verified locally before merge.
- Native sliders + incremental buttons are placeholders for visually styled rotary controls; final hardware encoder behavior and presets are separate milestones.
- The GUI currently sends DMX on control changes and the Send button, **not continuously**. Changing the IP requires disable/re-enable. Changing universe takes effect on the next send. No automatic discovery, reconnection or fail-safe.
- No COM transport, live Arduino feedback, presets, transitions or DMX patch/profile editor in the GUI yet.
- Keep the current CLI for BC1 handshake and hardware diagnostics.
