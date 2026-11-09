# Automatic session recovery

The desktop GUI automatically restores the last valid application-local session.
This is separate from portable **Save profile / Save profile as / Open** `.bcue`
files. Portable v1/v2/v3 reading and v3 writing are unchanged; those files still
contain configuration only. Automatic recovery never writes the associated
portable file, even if its configuration has unsaved edits.

Recovery restores the committed profile (including fixture groups and network
preferences), effective GM and RGBW bytes, the independent patch-fader byte,
internal Toggle selection and per-control ownership, and each Toggle DMX cue's
own envelope contribution and logical selection. The derived encoder patch and
CueEngine are rebuilt from the profile; merged DMX frames are never saved.
Physical fader observations and mouse/key holds are not recovered.

**Art-Net always starts OFF.** No socket is opened by restoration, no unavailable
interface is replaced, and no broadcast occurs. Explicitly enable output after
checking the recovered state and route. The first enabled frame uses the
recovered levels and the existing HTP/LTP, GM and universe conversion rules.
Normal stop/exit still attempts zero frames for previous output universes.

## Interrupted cues

Interrupted Toggle fades are frozen at their last checkpointed effective level,
not resumed against elapsed wall time. Logical selection and ownership remain
separate from level: an outgoing DMX cue can retain a fractional contribution
while unselected, and an incoming selected cue can be below full level. Both
contributions are restored individually before HTP. Double envelope precision is
round-tripped, avoiding a second byte-quantization step. `HELD` status/tooltips
identify recovered partial levels; they do not animate as new fades.

A new operator command starts the configured fade from the recovered level.
Toggle operates the saved logical selection, manual movement uses the existing
ownership takeover rules, and Kill also clears frozen outgoing contributions.
Internal held controls suppress accidental numeric preset matching while held.

Flash recovery is the deliberate exception to holding the checkpointed output:
all Flash contributions and held input edges are released immediately, with no
release fade replay. DMX Flash becomes zero. Internal Flash restores its previous
values only for controls it still owns; a Flash already fading out completes to
its release target. Newer preset/manual ownership is never undone. Checkpoint
capture normalizes a copy, so autosaving does not release live Flash input.

## Writes, backup and errors

The recovery directory is `recovery` alongside the existing application-local
`profiles` directory from wxStandardPaths. Its fixed files are:

- `current.bcsession`: newest checkpoint.
- `previous.bcsession`: a previously validated checkpoint (initially a second
  copy of the first valid checkpoint).
- `current.tmp` / `previous.tmp`: staging files; never used directly on startup.
- `writer.lock`: an OS-held writer lock, automatically released on process death.

The independent `BRASSCUE_SESSION 1` format embeds a length-delimited validated
portable profile, stable runtime fields and portable-file association/dirty flag.
It has a bounded payload length and CRC-32 integrity check. Parsers reject
truncation, trailing data, unsupported versions, invalid owners, nonfinite or
out-of-range cue levels and recoverable Flash selections/contributions.

Each replacement writes a new temporary file exclusively, flushes it to storage,
reads and validates it, preserves a valid previous generation, and atomically
replaces the current name. POSIX additionally synchronizes the directory where
supported; Windows uses FlushFileBuffers and MoveFileEx with write-through.
An invalid current file never replaces a good backup. Recovery files are never
interpreted as portable profile destinations.

The GUI samples committed state at most every 250 ms, debounces writes for 500 ms
of quiet, and checkpoints sustained changes/active fades about every two seconds
(subject to timer/disk scheduling). It does not write at the 25 Hz display rate.
Unchanged state does not produce repeated writes. Failed writes retain the last
valid recovery and retry no faster than every five seconds; an existing status
line reports the failure without repeatedly opening a dialog.

Startup tries current, then previous, then built-in defaults. Corrupt/incompatible
files or abandoned partial writes cannot partially configure a session. Backup
fallback or inability to recover produces a concise status warning; first launch
without any recovery file is normal. A valid backup is repaired into the current
file on the next checkpoint. A second application instance can read the snapshot
but cannot overwrite the first instance's recovery; its status reports read-only
recovery storage. Restart that instance after closing the writer to take over.

## Portable files and closing

An asterisk still means the **portable profile** has unsaved configuration.
Automatic recovery does not clear it or pretend that Save was used. The explicit
Save destination and dirty flag survive restart. Save continues to write only
portable configuration, never live state.

Closing normally checkpoints the latest committed state without asking to
"discard" edits that will be recovered. If the final checkpoint fails, a normal
close offers default-No confirmation to exit without that latest recovery.
Forced OS termination cannot show this prompt and can lose changes since the
last successful checkpoint. Opening/importing or restoring defaults still asks
before replacing unsaved portable configuration; recovery follows the accepted
replacement. Neither operation enables output.

An autosave is crash recovery, not a long-term profile backup or a guarantee
against storage/controller failure. Real power-loss durability depends on the
filesystem/device honoring flush and atomic-replacement semantics. UDP blackout
is best effort and still requires receiver-side loss behavior for real equipment.
