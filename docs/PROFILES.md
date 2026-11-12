# Portable profiles v3

A `.bcue` file is ordered UTF-8 configuration, never live output. `ControlProfile`
and `profile_io.cpp` define the schema. The writer emits **BRASSCUE 3**; the reader
accepts versions 1, 2 and 3. No third-party JSON library is needed.

## Ordered records

1. `BRASSCUE 3`
2. Quoted ID, name, layout ID (`compact-three-column-v1`).
3. Four encoder records: quoted label, **manual** DMX assignment list.
4. Fader: quoted label, GM boolean, DMX assignment list.
5. Three buttons, each: quoted label, mode (0 Toggle / 1 Flash), Kill boolean,
   fade-in ms, fade-out ms, action (0 DmxCue / 1 SetControlValues), DMX assignment
   list, then internal control assignment list.
6. Quoted TFT title, view (0 Color / 1 Levels), units (0 DMX / 1 Percent), quoted
   unicast IPv4 preference, broadcast boolean, quoted interface ID (empty = Auto).
7. Fixture-group count (0..64). For each group: quoted name, quoted channel order,
   stride, boundary policy (`0` = Reject), block count. Each block is
   `visible-universe start-address fixture-count`. No generated assignment list is saved.

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
patch 1.5 remain DMX cues. Saving that imported profile writes v3 with those same
semantics. `bridge/tests/fixtures/room-v1.bcue` is a literal old-format fixture.
New defaults use internal GM presets 255/128/0, Toggle+Kill, zero fades. They do
not write channel 1.5.

## Version 2 migration and fixture groups

Version 2 has the same records 2..6 as v3 and no group count or blocks. Both
legacy versions load with **zero groups**. Manual patches, target bytes, labels,
internal/DMX preset actions, fades and network preferences retain their original
meaning. No address/label inference turns old assignments into groups. Version 3
is necessary because flattened assignments cannot recover group names, fixture
counts, strides or block boundaries. Older applications reject v3 explicitly.

A group is exclusively a four-slot RGBW personality. Order is any uppercase
permutation of `RGBW`, stride 4..512 (default 4). Each group has 1..64 explicit
blocks; total fixtures across all groups are at most 512. Names are unique,
nonblank UTF-8 strings under the existing 128-byte limit. A block has a positive
count, universe 1..32768 and start address 1..509. Every fixture occupies four
consecutive channels, with starts separated by stride. The last fixture must
end at or before channel 512. **No implicit wrap, split or rollover** is allowed;
use another explicit block for the next universe. Stride gaps are not allocated.

Generated R/G/B/W assignments use target byte 255 and are derived from the group
records, combined with the manual encoder lists only in the session/engine.
Combined capacity is still 512 assignments per encoder. Groups cannot collide
with each other or with any manual encoder, fader (including inactive GM-mode
patches) or DMX cue assignment, even a zero target. Existing overlaps exclusively
between manual controls retain their HTP behavior. Manual editors show only
manual rows; edit generated routes in Quick RGBW Patch. Removing/repatching a
group never removes manual assignments.

Example group suffix (following the network record):

```text
1
"HOUSE PANELS" "GRBW" 4 0 2
1 17 12
2 1 8
```

This creates 20 fixtures. In the first fixture G is channel 17, R 18, B 19 and W 20
on visible universe 1. The second block independently starts at universe 2/channel 1.

## Transactions and live state

Load validates a candidate before replacement. Save writes a sibling `.tmp`,
closes it, then atomically replaces the destination with rename/MoveFileEx. An
existing `.tmp` is an error; a failed load leaves the profile intact. Editors own
local drafts, including inactive action tables; Cancel has no session effect.
Accepting a configuration edit preserves controls and resets button
ownership/transitions. Changing fader role is explicit: GM to patch initializes
logical GM to 255; patch to GM adopts the patch fader position. GUI explicit open/reset starts RGBW zero, GM 255, output OFF.
Console `load` preserves current controls and its explicit `--target`; profile
network preferences cannot redirect it.

No active selection, fade progress, effective/physical control values or network
ON state is saved in a portable profile. Explicit portable files load through
Open/import. The GUI independently restores its application-local session on
startup, including committed unsaved configuration and stable live state, with
output always OFF; see [Session recovery](SESSION_RECOVERY.md).
Profile menu supports Open/import, Save, Save as/export and Restore defaults.
An asterisk marks unsaved portable configuration; replacement prompts protect it,
while normal close preserves the independent recovery session.

When a GUI edit changes patch routing while Art-Net is ON, an explicit default-No
confirmation precedes any session change. Yes stops output and attempts zero
frames on all previous universes, then commits; output remains OFF. No leaves
profile and routing unchanged. This applies to Quick RGBW Patch and the manual
editors. Normal OutputSession snapshots also zero removed slots and send a zero
frame for retired universes; Art-Net numbering/serialization is unchanged.
