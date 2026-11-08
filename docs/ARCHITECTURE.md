# Architecture and contracts

BrassCue is a bench-test prototype, not an unattended stage controller.
The Uno pinout and physical displays still require verification.

## Ownership

```
bridge/src/
  controller_profile.hpp   canonical configuration, defaults and validation
  patch.hpp                visible addresses, assignments and cue configuration
  assignment_editor.hpp    pure table-row parsing and lossless unit edits
  utf8.hpp                 strict persisted-label validation
  controller_session.hpp   authoritative live State and event coordination
  cue_engine.hpp           derived cue state, fades, HTP and DMX snapshots
  value_conversion.hpp     neutral percent/DMX conversions
  four_digit_display.hpp   neutral four-character presentation
  profile_io.*             strict versioned interchange and atomic replacement
  serial_protocol.hpp      BC1 record parsing and STATE serialization
  serial_session.hpp       handshake timing and bounded line receiver
  serial_port_win.hpp      Windows COM I/O
  network_config.hpp       conservative explicit IPv4 validation
  bridge_core.hpp          legacy RGBW scaling and ArtDmx header serialization
  output_session.hpp       snapshot framing, periodic output and cleanup
  artnet_udp.hpp           RAII UDP socket
  cli.cpp                  console grammar and Windows serial loop
  main.cpp                 console bootstrap
bridge/gui/
  main.cpp                 application bootstrap
  main_frame.cpp           three-column composition and service coordination
  widgets.*                rotary input, cue edges and simulated TFT
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

## Patch and merge

Visible universe 1..32768 converts to wire Port-Address 0..32767 only in packet
serialization. Channel 1..512 indexes a full 512-slot frame. CLI `--universe 0`
retains its historical wire meaning; it sets the default RGBW and room patch to
visible universe 1. Loading a profile replaces that default patch.

Each control has up to 512 distinct addresses and independent 8-bit target
values. Duplicates within a control are rejected. Between controls, highest takes
precedence (HTP) is applied after scaling. Encoder/patch fader output is
`round(target * position / 255)`. Grandmaster scales all governed output with
`(value * master + 127) / 255`; in GM mode the fader patch is ignored entirely.
In patch mode global master is 255. Encoders and room cues are independent
unless their patch explicitly overlaps. Operational levels are percentages:
`(dmx * 100 + 127) / 255`; percent input maps with `(pct * 255 + 50) / 100`.
Switching editor units preserves untouched target bytes, even if a displayed
rounded percentage cannot represent the original byte exactly.

## Buttons and time

Toggle alternates on each distinct down edge; up only rearms it. Pressing the
active room button again turns it off. Flash follows down/up, including release
outside the widget, lost capture, keyboard release and lost focus. Repeated down
edges have no effect. Activating a Kill cue fades out the other two buttons;
encoders and fader are unaffected. Kill off allows HTP coexistence. The three
buttons constitute one explicit fixed room group in this hardware layout.

Each track interpolates from its current level to its new target with the
selected fade-in/out duration. Zero duration is immediate. A zero-target cue is
still ON and kills other cues, then produces zero; outgoing cues complete their
fade-out. For 100->50, old and new envelopes merge HTP, so the level can briefly
dip below 50 during a crossfade before settling. This is the same HTP rule as
all other overlaps, not a special LTP room override. Time comes from an injected
millisecond argument; frontends use a monotonic clock. Reconfiguring a profile
preserves raw encoder/fader positions but resets button envelopes. Opening or
resetting a profile starts RGBW at zero, master at 255 and network OFF.

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
Conventional x.x.x.255 broadcast destinations require opt-in; other directed
broadcast masks are not inferred. No multicast support.

## Presentation and persistence

The neutral four-character display temporarily shows R/G/B/W plus percent until
exactly 2000 ms after the last encoder event, then returns to current GM. Custom
encoder labels keep the stable physical R/G/B/W codes. Seven-segment hardware
cannot reproduce those letters faithfully (notably W); firmware must map them
to reviewed segment approximations or numeric codes before a real driver is
claimed. Leading spaces are the current fixed alignment policy. No physical
TM1637/ST7735 driver or Uno framebuffer is included.

See [Profiles](PROFILES.md) for the ordered portable v1 file schema, and
[GUI](GUI.md) for builds and manual checks. No live output values or network ON
flag are persisted. User profiles are loaded explicitly; startup always uses
the built-in RGBW Compact preset.
