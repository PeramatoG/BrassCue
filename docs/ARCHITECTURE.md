# Architecture and contracts

BrassCue is a bench-test prototype, not an unattended stage controller.
The Uno pinout and physical displays still require verification.

## Ownership

```
bridge/src/
  controller_profile.hpp   canonical configuration, defaults and validation
  fixture_groups.hpp       canonical RGBW groups, preview layout and derived encoder patch
  patch.hpp                visible addresses, assignments and cue configuration
  assignment_editor.hpp    pure table-row parsing and lossless unit edits
  utf8.hpp                 strict persisted-label validation
  controller_session.hpp   authoritative effective State and event coordination
  control_values.hpp       typed logical controls and preset targets
  internal_presets.hpp     LTP control ownership, interpolation and takeover
  cue_engine.hpp           derived cue state, fades, HTP and DMX snapshots
  value_conversion.hpp     neutral percent/DMX conversions
  four_digit_display.hpp   neutral four-character presentation
  profile_io.*             strict portable configuration interchange and atomic replacement
  runtime_state.hpp        stable recovery DTOs and ownership/contribution validation
  session_snapshot.*       separate checksummed local session format
  recovery_store.*         validated durable generations and crash-released writer lock
  session_autosave.*       debounce, transition checkpoints and bounded retry policy
  serial_protocol.hpp      BC1 record parsing and STATE serialization
  serial_session.hpp       handshake timing and bounded line receiver
  serial_port_win.hpp      Windows COM I/O
  network_config.hpp       mask-aware portable destination policy
  network_interfaces.*     active IPv4 adapter enumeration
  bridge_core.hpp          legacy RGBW scaling and ArtDmx header serialization
  output_session.hpp       snapshot framing, periodic output and cleanup
  artnet_udp.hpp           RAII UDP socket
  cli.cpp                  console grammar and Windows serial loop
  main.cpp                 console bootstrap
bridge/gui/
  main.cpp                 application bootstrap
  main_frame.cpp           service coordination
  native_panel.*           native three-column presentation
  panel_host.*             renderer selection and native fallback
  web_panel.cpp            optional local wxWebView lifetime/bridge
  presentation/*           toolkit-free actions, view diffs, gestures/protocol
  web_assets/*             local HTML/CSS/JS presentation
  network_panel.*          explicit NIC/destination draft
  assignment_table.*       transactional DMX/internal target rows
  button_editor.*          explicit action/mode/Kill/fade editor
  dark_theme.*             documented DWM and wx appearance APIs
  widgets.*                rotary input, cue edges and simulated TFT
  console_fader.*          DPI-scaled fader drawing/input; no live state ownership
  editor_buttons.hpp      standard button order without an implicit default OK
  quick_patch.*            transactional named groups, blocks and generated preview
  profile_commit.*         live-route confirmation and stop-before-commit boundary
  encoder_group_routes.*   read-only per-encoder projection of group routes
  profile_editor.*         transactional contextual editor
  profile_files.*          user-selected profile file workflow
bridge/tests/              pure native unit/integration tests
firmware/                  small input scan and BC1 sketch, no display driver
```

GUI and CLI own a `ControllerSession`. Its profile and live State are private;
all changes go through session methods. CueEngine configuration is derived from
that profile and exposed read-only to frontends. There are no parallel old
single-channel/FaderRole/ButtonAction profiles. BC1 STATE remains the historical
five raw bytes; room cues are host-owned and do not extend the wire protocol.
Core never includes wxWidgets or Arduino. CMake registers sources explicitly.

Internal preset selection follows control ownership. With Kill off, taking the
last control owned by an older preset clears its selection, including during a
fade. Presets that still own disjoint targets can remain selected together;
levels, fade timing and stale Flash release retain per-target ownership rules.

## Patch and merge

Visible universe 1..32768 converts to wire Port-Address 0..32767 only in packet
serialization. Channel 1..512 indexes a full 512-slot frame. CLI `--universe 0`
retains its historical wire meaning; it sets the default RGBW and room patch to
visible universe 1. Loading a profile replaces that default patch. In the console, `load <path>` consumes the remainder of the line as a UTF-8 path, including spaces (no quotes required). The historical `show` output monitors the first patched address of each encoder from the final merged engine frame; it does not assume targets are 255 or the fader is a GM.

Each control has up to 512 distinct addresses and independent 8-bit target
values. Duplicates within a control are rejected. Between controls, highest takes
precedence (HTP) is applied after scaling. Encoder/patch fader output is
`round(target * position / 255)`. Grandmaster scales all governed output with
`(value * master + 127) / 255`; in GM mode the fader patch is ignored entirely.
Switching into patch mode initializes GM to 255 for v1 compatibility; internal
GM presets can then scale encoders, DMX cues and the patch fader independently
of its position. DMX cues are independent unless their patch overlaps. Operational levels are percentages:
`(dmx * 100 + 127) / 255`; percent input maps with `(pct * 255 + 50) / 100`.
Switching editor units preserves untouched target bytes, even if a displayed
rounded percentage cannot represent the original byte exactly.

Fixture groups and manual patches are separate canonical profile fields.
`fixture_layout` validates the RGBW group definitions and produces preview rows;
`effective_encoder_patch` combines generated target-255 routes with manual rows,
checks collisions/capacity, and supplies the existing CueEngine. ControllerSession
caches this derived patch only on successful configuration; every input/preset
path uses it. Generated routes are never persisted or edited as manual rows.
Legacy profiles load with no groups. Universe overflow is rejected; multiple
universes require explicit blocks. See [Profiles](PROFILES.md) for v3 limits.

`CommitProfileEdit` guards both GUI patch workflows. Changed live routing needs
explicit confirmation, then OutputSession stop/cleanup before configuration.
Output remains OFF. Candidate validation and declined confirmation have no
session effect. Packet numbering, output cleanup and engine merge rules remain
shared with the CLI.

## Buttons and time

Two action kinds share input edges but have different value semantics. Default
HOUSE 100/50/0 target the effective GM at 255/128/0; they are not channel 1.5 cues.
`InternalPresets` interpolates each logical control from its current effective
value using injected monotonic milliseconds. The latest command owns each target
(LTP), including with Kill off. Kill cancels other internal ownership, holding
current values; the new preset starts from those values without a zero jump.
Toggle-off targets safe zero. HOUSE 0 stays at zero and can remain highlighted.
Flash stores previous values and restores only targets it still owns on release,
with fade-out. A later preset or manual command cannot be undone by a stale
Flash release. Manual movement of any target cancels that preset's remaining
movement and selection. Matching an idle Toggle preset's exact values highlights
it without an input event/fade; active DMX selections suppress implicit matches.
Explicit selection survives zero output and a transition.

DMX actions retain CueEngine's HTP merge, Toggle/Flash/Kill and independent
fade-in/out envelopes. Their crossfade can dip below the lower target. Mixed
kinds with Kill cancel/deactivate the other kind's selection; internal effective
levels stay held when a DMX cue takes ownership of the button group. Internal
presets never become HTP contributions. Duplicate down edges do nothing.
Mouse up outside, lost capture/focus, keyboard release and renderer handoff
release Flash. Pointer reconciliation affects mouse holds, preserving keyboard
holds. Configuration commits reset ownership, preserving control values; GUI
open/reset starts RGBW zero, GM 255, output OFF. Changing GM to patch initializes
logical GM to 255; patch to GM adopts the patch position.

BC1 physical fader observations are separate from the effective GM; buttons do
not move a nonmotorized physical fader. Current device input takes over on its
next accepted event. A future measured pickup policy should ignore physical
movement until it crosses the effective GM, with tolerance/hysteresis reviewed
on actual hardware. No motor, pinout or physical display driver is implied.

The console `load` command is a live configuration edit: it preserves raw control
positions and the explicit `--target` destination; profile IP preferences never
redirect CLI output. Retired patch addresses still follow the cleanup policy.

## Output lifetime

Art-Net is opt-in; no discovery or implicit broadcast. Output uses one coherent
snapshot per tick (approximately 25 Hz), including while console input is idle.
The serial loop preserves spontaneous READY, a single HELLO after 2500 ms if
needed, and suppression of READY duplicates less than 1000 ms apart. A later
READY resynchronizes STATE and releases pressed Flash cues. `--ignore-gm` still
ignores device GM events. Serial ports are not automatically reopened.

Full snapshots clear removed channels. A removed universe gets one all-zero
packet. Stop, target change and normal process exit attempt zero frames for all
previous universes before releasing the socket. Delivery is best effort UDP;
crash, power loss, receiver failure or forced termination cannot guarantee
blackout. Configure receiver-side loss behavior before any real equipment use.
Network edits in the GUI turn output OFF; re-enable explicitly after editing.
Auto uses the default unicast route. Explicit NIC selection binds both the source
IPv4 and IP_UNICAST_IF; broadcast additionally uses SO_BROADCAST and computes
IPv4 | ~netmask for contiguous /1..30 masks. Loopback, unavailable NICs and
nonbroadcast links cannot broadcast. A changed/disappearing NIC stops output
within the GUI enumeration interval (5 seconds); re-enable explicitly. No
multicast or automatic alternative route. Tests send only to loopback, never a
real broadcast.

## Presentation and persistence

The neutral four-character display temporarily shows R/G/B/W plus percent until
exactly 2000 ms after the last encoder event, then returns to current GM. Custom
encoder labels keep the stable physical R/G/B/W codes. Seven-segment hardware
cannot reproduce those letters faithfully (notably W); firmware must map them
to reviewed segment approximations or numeric codes before a real driver is
claimed. Leading spaces are the current fixed alignment policy. No physical
TM1637/ST7735 driver or Uno framebuffer is included.

See [Profiles](PROFILES.md) for the ordered portable v3 schema and explicit v1/v2 migration, and
[GUI](GUI.md) for builds and manual checks. Portable profiles persist no live values or network ON flag. The GUI restores
its independent local session snapshot automatically, falling back to an older
valid snapshot or RGBW Compact defaults. Output always starts OFF. Session
reconstruction uses static envelope levels plus logical selections and ownership,
never button replay or final merged frames; interrupted transitions hold at their
checkpointed level and Flash input is released safely. MainFrame coordinates the
pure recovery modules with the portable-file document metadata. See
[Session recovery](SESSION_RECOVERY.md) for policy, storage and dirty-state rules.


## Shared presentation

C++ `ViewSnapshot` supplies effective values, percentages, TFT color, display,
labels and selection/phase. Native controls apply only changed fields; the web
host coalesces diffs with one script in flight and suppresses identical snapshots.
JS supplies input/DOM updates only. Circular relative gesture math lives in C++:
atan2, wrap at ±pi, fractional steps, dead zone, teleport rejection, Core clamp.
Wheel remains one signed step per event, independent of drag sensitivity.

The optional browser uses local packaged assets, restrictive CSP, exact main-file
navigation allowlist and vetoed new windows. No HTTP server, CDN, Node or UI
framework. Windows explicitly requests Edge, never IE. Compile-time missing
webview is an actionable CMake error; runtime absence/error/readiness timeout
keeps native controls and a diagnostic. See [web protocol](WEB_PANEL.md).
