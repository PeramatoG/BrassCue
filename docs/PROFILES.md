# Portable profiles v1

A `.bcue` file is UTF-8 text with an ordered, versioned grammar. It is configuration,
not a snapshot of live output. The canonical schema is `ControlProfile` and the
strict parser/serializer in `bridge/src/profile_io.cpp`. No extra third-party
JSON library is required.

Records in order:

1. `BRASSCUE 1`
2. Quoted ID, name, layout ID (`compact-three-column-v1`).
3. Four encoder records: quoted label followed by an assignment list.
4. Fader record: quoted label, GM boolean (0/1), assignment list.
5. Three button records: quoted label, mode (0 toggle / 1 flash), Kill boolean,
   fade-in ms, fade-out ms, assignment list.
6. Quoted TFT title, view (0 color / 1 levels), units (0 DMX / 1 percent),
   quoted target IPv4, broadcast permission boolean.

An assignment list is a count followed by that many `universe channel target`
triples. For example `3 1 23 255 2 8 128 3 128 0` represents
`1.23=255`, `2.8=128`, `3.128=0`. Targets are always stored in DMX bytes.
Strings use double quotes and backslash escaping for quotes/backslashes.
The parser rejects trailing tokens, missing fields, unsupported versions,
invalid integers, duplicate addresses, unsupported modes/layouts, labels above
128 bytes, line breaks/NULs, more than 512 assignments per control, fades above
60000 ms and files above 256 KiB. Universe/channel zero is invalid.

Load into a candidate; only successful validation replaces the current profile.
Save writes a sibling `.tmp`, closes it, then atomically replaces the destination
with platform rename/MoveFileEx. An existing `.tmp` causes a visible error rather
than overwriting it. A failed load leaves the active profile and output unchanged.
Canceling any editor leaves configuration unchanged. Configuration edits retain
raw encoder/fader values and reset button cues; opening/reset starts live values
from defaults and disables output.

Default SALA 100/50/0 are Toggle+Kill, zero fade times, on provisional patch 1.5.
That address is editable configuration, not a hardware property. Save / Save as
also serve export; Open serves import. The file chooser initially uses the
platform user-data directory under `profiles`. Startup does not auto-load a file.
An asterisk in the window title indicates unsaved configuration changes; closing,
opening or restoring defaults prompts before discarding them.
