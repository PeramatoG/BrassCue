# Portable profiles v2

A `.bcue` file is ordered UTF-8 configuration, never live output. `ControlProfile`
and `profile_io.cpp` define the schema. The writer emits **BRASSCUE 2**; the reader
accepts versions 1 and 2. No third-party JSON library is needed.

## Ordered records

1. `BRASSCUE 2`
2. Quoted ID, name, layout ID (`compact-three-column-v1`).
3. Four encoder records: quoted label, DMX assignment list.
4. Fader: quoted label, GM boolean, DMX assignment list.
5. Three buttons, each: quoted label, mode (0 Toggle / 1 Flash), Kill boolean,
   fade-in ms, fade-out ms, action (0 DmxCue / 1 SetControlValues), DMX assignment
   list, then internal control assignment list.
6. Quoted TFT title, view (0 Color / 1 Levels), units (0 DMX / 1 Percent), quoted
   unicast IPv4 preference, broadcast boolean, quoted interface ID (empty = Auto).

A DMX list is a count followed by `universe channel byte` triples. An internal
list is a count followed by `target byte` pairs; targets are 0 Grandmaster,
1 Red, 2 Green, 3 Blue, 4 White. Each target can occur once, at most five entries.
An internal action requires at least one internal target and no DMX assignments;
a DMX action has no internal targets. “Add all RGBW” expands four independent
rows. Displayed units never change stored bytes unless that value cell is edited.
Strings use double quotes with escaped quotes/backslashes.

The parser rejects trailing tokens, missing fields, unsupported versions, invalid
UTF-8, integers, duplicate addresses/targets, actions, modes or layouts. Limits:
128 bytes per label/ID, no NUL/line breaks, 512 DMX assignments per control,
60000 ms per fade, 256 KiB per file, universe 1..32768, channel 1..512, byte 0..255.
The interface ID is a preference; loading never silently opens a socket or picks a
replacement NIC. Resolve it against current interfaces before enabling output.

## Version 1 migration

Version 1 lacks action/control-list fields and the interface ID. Every v1 button
is explicitly imported as **DmxCue** with its original DMX patch, mode, Kill and
fade times. No label-based inference occurs. In particular old SALA labels on
patch 1.5 remain DMX cues. Saving that imported profile writes v2 with those same
semantics. `bridge/tests/fixtures/room-v1.bcue` is a literal old-format fixture.
New defaults use internal GM presets 255/128/0, Toggle+Kill, zero fades. They do
not write channel 1.5.

## Transactions and live state

Load validates a candidate before replacement. Save writes a sibling `.tmp`,
closes it, then atomically replaces the destination with rename/MoveFileEx. An
existing `.tmp` is an error; a failed load leaves the profile intact. Editors own
local drafts, including inactive action tables; Cancel has no session effect.
Accepting a configuration edit preserves controls and resets button
ownership/transitions. Changing fader role is explicit: GM to patch initializes
logical GM to 255; patch to GM adopts the patch fader position. GUI open/reset starts RGBW zero, GM 255, output OFF.
Console `load` preserves current controls and its explicit `--target`; profile
network preferences cannot redirect it.

No active selection, fade progress, effective/physical control values or network
ON state is saved. Startup uses built-in RGBW Compact; files load explicitly.
Profile menu supports Open/import, Save, Save as/export and Restore defaults.
An asterisk marks unsaved configuration; discard prompts protect it.
