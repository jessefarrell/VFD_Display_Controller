# Prototype firmware

## Persistent settings

Settings are saved automatically when applied over USB serial. On reboot the
controller resumes the last selected application. Animation resumes its saved
effect immediately, and Static Text resumes its saved line, without asking for
configuration again. A new board, or a
board with no valid settings records, starts at the launcher with defaults.

The following values survive resets and power loss:

| Setting | Default |
| --- | --- |
| Last application | Launcher on first boot |
| Animation selection | Random |
| Random effect shuffle interval | 8 seconds |
| Marquee message | `  Schneider Electric  ` |
| Marquee character interval | 150 ms |
| Static text | `VFD READY` |
| Display test cursor blink | Off |
| Clock date/time (manual save or minute checkpoint) | January 1, 2026, 00:00:00 |

Esc or Ctrl+C returns to the launcher for the current session. The last selected
app remains saved for the next reboot. A queued Esc/Ctrl+C during the two-second
USB startup delay bypasses automatic resume for that boot. At the animation
selection menu, Enter starts the saved effect without configuration prompts;
select an effect number explicitly to edit its configuration. Effect-selection
keys and Ctrl+A interrupt animation waits so they can be handled promptly. The
clock likewise accepts `s` during its refresh wait. CR-only, LF-only, and CRLF
terminal line endings are supported by the shared console reader.

The marquee accepts up to **2,048 printable characters**, including a multiline
paste. Pasted line breaks and tabs become spaces. After the first newline it
collects input until there is a 250 ms pause, so the rest of a paste stays in the
message instead of entering the speed prompt or triggering animation commands.
The last pasted line need not end with a newline. For a single line typed by
hand, press Enter as usual. **Wait for the speed prompt before entering its
value**, including when scripting the serial interface.

An oversized paste is consumed and rejected with a message; the previous text
and speed remain applied. The speed prompt accepts whole numbers from 1 through
10,000 milliseconds per character. Mixed text, negative values, overflowing
numbers, and values outside that range keep the previous speed. A previously
saved delay above 10,000 ms is restored as the default 150 ms so the marquee
does not appear frozen after a bad setting.

Date/time is saved immediately when the date/time wizard completes or is
cancelled after confirming at least one field. Cancelling the first field saves
nothing. Once the clock has been set, it also saves a checkpoint about every
**60 seconds while the Clock app is running**. Ordinary display refreshes do not
write flash. A failed checkpoint is reported on USB and retried after another
minute, rather than on every refresh. Re-entering Clock or explicitly setting
the time starts a fresh one-minute interval.

After power loss, the clock resumes from the latest valid checkpoint or manual
save. During normal continuous clock display this loses roughly up to a minute
of powered runtime, **plus all time spent powered off**. Checkpoints pause while
another app or a blocking configuration prompt is active. The hardware RTC
continues running while powered, including when leaving and re-entering Clock.
**The Pico has no battery-backed RTC and cannot measure time while powered
off.** Accurate time across outages requires an external battery-backed RTC or
time synchronization. Display test text and cursor position are transient.

## Static text

Select **[4] Static Text** at the launcher to display a fixed, left-aligned line
of up to **20 printable characters**. Type the line and press Enter to save and
display it; Enter with no text keeps the current line. Text remains stationary
and the cursor is hidden. A message longer than twenty characters is rejected
without replacing the previous line.

While viewing the line, **E** edits it, **C** clears it, and **Esc / Ctrl+C**
returns to the launcher. During editing, Esc/Ctrl+C cancels the edit and keeps
the saved line. Pasted line breaks and tabs become spaces, using the same
250 ms paste-completion rule as the marquee. Wait until editing finishes before
sending view commands.

Static text has its own saved message and does not change the marquee message.
If Static Text was the last selected app, reboot displays the saved line
immediately without a prompt. Factory reset restores `VFD READY`.

## Factory reset

Return to the main launcher with Esc or Ctrl+C, then press **R** (lowercase `r`
also works). Type **RESET** and press Enter to confirm. Esc, Ctrl+C, or any
other nonempty response followed by Enter cancels without changing settings.

Factory reset restores every value in the defaults table above, clears the
display and cursor blink, and resets the live clock to January 1, 2026,
00:00:00. It stays at the launcher without rebooting. The next boot also starts
at the launcher until another application is selected.

The reset commits defaults before erasing the previous settings copy and clock
checkpoint journal. A power interruption during backup cleanup leaves defaults as the newest
valid settings. If writing defaults or erasing/verifying the previous copy
fails, the console reports an incomplete reset. Defaults remain active in RAM;
retry **R** before rebooting to finish the persistent reset. Reset touches only
the reserved settings and clock journal sectors; the firmware remains installed.

## Storage and recovery

`settings.cpp` encodes a versioned, little-endian record with CRC-32 and validates
all restored fields. The two final 4 KiB flash sectors hold alternating copies.
The current version 3 record adds static text. Existing version 1 and 2 records
are read and upgraded on the next save without losing their settings; the new
static-text setting starts at `VFD READY`.
On a standard 2 MiB Pico these begin at flash offsets `0x1FE000` and `0x1FF000`.
The linker rejects firmware that overlaps them, and the UF2 contains no settings
data. Ordinary programming of this firmware's UF2 preserves settings; a full chip
erase removes them. Older firmware without this reservation can overwrite them
if its image grows into that area.

The clock journal reserves a separate **32 KiB** immediately before those
settings sectors, at flash offsets `0x1F6000` through `0x1FDFFF` on a standard
2 MiB Pico. Its eight sectors each contain sixteen 256-byte pages. Each minute
appends one checksummed page without erasing or rewriting previously used
pages. After 128 checkpoints the ring begins recycling sectors; during steady
operation each sector is erased once per 128 checkpoints, rather than erasing
a settings sector every minute. Flash still has finite endurance.

Checkpoint records are tied to their configuration revision. A manual time
adjustment starts a new revision, even if it matches the original saved time,
so an older checkpoint cannot override it. Other configuration changes retain
the latest checkpointed time in their settings record. Factory reset disables
checkpoint restoration before cleanup, preventing an interrupted reset from
restoring the old clock. Corrupt or incomplete checkpoint pages are ignored and
skipped on retry; the previous valid time remains available.

A save erases and programs the inactive sector, then verifies its contents.
If power fails during a save, startup chooses the newest valid copy or falls
back to defaults when neither copy is valid. Unchanged preference saves skip
flash writes; explicit clock adjustments force a new revision. Explicit settings
changes still consume erase cycles, so avoid
using this API for continuously changing telemetry.

`settings_flash.cpp` uses the [Pico SDK flash safety API](https://www.raspberrypi.com/documentation/pico-sdk/high_level.html#pico_flash)
to disable interrupts during erase/program operations. This firmware uses only
core 0; adding a second core requires setting up the SDK multicore flash lockout.
Programming errors are reported on USB; RAM settings remain applied, and the
last successful saved copy remains available at reboot.

Large input and flash-record work buffers live in static RAM rather than on the
Pico's small stack. Marquee playback reads the saved text directly and builds
only a twenty-character display window. GCC builds emit stack-usage reports
for animation and settings code and warn about frames exceeding 512 bytes.

## Build and validation

Build using the existing Pico SDK configuration:

```powershell
cmake --build build -j 4
```

The firmware output is `build/test.uf2`. Host tests use a native C++17 compiler
and simulated flash/RTC/GPIO; they do not require a connected board. In a shell
with a native compiler and Ninja available:

```powershell
cmake -S tests -B build/settings-tests -G Ninja
cmake --build build/settings-tests
ctest --test-dir build/settings-tests --output-on-failure
```

The tests cover settings round trips, maximum marquee length, redundant-write
suppression, invalid data, checksummed unsupported versions, generation rollover,
readback failures, and interrupted writes at every byte boundary into either
sector. Clock tests cover restoring saved/default dates, preserving a running
RTC, waiting for RTC register synchronization, leap-day/year clamping, and no
per-refresh flash writes between checkpoints. Display tests cover right-edge cursor clamping and
limiting writes to twenty characters. Console tests cover non-consuming waits,
command preservation, CR/LF normalization, and CRLF split across USB packets.
Interactive app tests switch out of all six animations during playback, submit
marquee and shuffle settings using CRLF, and open the clock wizard during its
refresh wait. Marquee regressions include the reported dialogue in plain and
formatted form, multiline packets without a final newline, the full 2,048
character limit, oversized-paste rejection, strict numeric input, old settings
migration, and recovery from excessive saved delays. Clock journal tests cover
minute cadence and rate-limited retries, restoration across midnight/leap day,
multiple ring rotations, distributed sector erases, torn writes at every byte
boundary, checksum and calendar validation, erase/readback failures, revision
changes, manual rewinds, sequence rollover, and factory-reset cleanup.
Factory-reset tests exercise the real launcher confirmation flow, cancellation,
live clock reset with and without an already-running RTC, persistence across a
reload, removal of the old backup, repeated resets, and write/erase failures.
Static-text tests cover editing, clearing, cancellation, multiline and oversized
input, the twenty-character limit, saved startup selection, settings migration,
and the absence of scrolling or repeated display writes while idle.

Board validation is still required:

1. Flash `build/test.uf2`, choose Animation, then Marquee, and enter a distinctive
   message and character interval. Power-cycle while it is playing. Verify the
   same message and speed resume with no serial input.
2. Repeat with Random and a custom shuffle interval, and with Clock and Display
   Test as the selected applications. Verify cursor blink restores in Display
   Test. Exit to the launcher, reboot, and verify the last selected app resumes.
3. Set a date/time, leave Clock, and re-enter it; verify time continued advancing.
   Let it run for over a minute, then power-cycle and verify the clock resumes
   from a recent checkpoint, allowing for time spent booting and running
   afterward. Verify an explicit time adjustment supersedes earlier checkpoints.
4. Program the same UF2 again without erasing the chip and verify settings remain.
5. During animation playback, press another effect number and Ctrl+A; verify
   switching and menu display respond promptly. During Clock, press `s`; verify
   the date/time wizard opens. Configure the terminal for CRLF and verify each
   wizard field and marquee prompt accepts the value entered for it.
6. Paste a multiline message into the marquee text prompt. Wait for the speed
   prompt and enter `25`; verify the entire message scrolls with line breaks
   replaced by spaces. Repeat with an oversized message and with an invalid
   speed such as `5000ms`; verify the previous applicable value is retained.
7. At the launcher, press `R` and cancel; verify settings remain unchanged.
   Repeat and confirm with `RESET` followed by Enter. Verify the display clears,
   reboot returns to the launcher, and each application uses its default values.
8. Choose `4`, enter a short line, and verify it remains stationary. Edit with
   `E`, clear with `C`, and try a twenty-character line and an oversized paste.
   Set a line, reboot while the app is selected, and verify it resumes without
   serial input. Verify factory reset restores the static text to `VFD READY`.

## Code review notes

Fixed during this change:

- Settings and the last application/effect were held only in RAM.
- `Clock::init()` reset the clock whenever the application was entered.
- The RTC was read immediately after setting it, before the SDK's documented
  64 microsecond register synchronization interval had elapsed.
- `Vfd::cursor_shift()` could move to index 20, allowing the next character to
  overwrite the display buffer's terminator. Subsequent string operations could
  read beyond the buffer. It now clamps at index 19.
- `Vfd::write_string()` could continue wrapping indefinitely for strings longer
  than the documented twenty-character limit. It now limits input characters.
- Large shuffle values could overflow when converting seconds to milliseconds;
  these inputs are now rejected. Clock years are clamped to the RTC range.

- Waits consumed non-exit keys, dropping animation selections, Ctrl+A, and the
  clock's `s` command. `wait_for_input()` now wakes on any key and preserves it
  for the app's input loop. Effects yield to that loop instead of treating every
  interrupted wait as an exit.
- Separate prompt readers treated CRLF as two Enter presses, sometimes skipping
  fields. All apps now use a shared reader that normalizes CR and LF and skips
  the LF paired with a preceding CR, including across reads and app changes.
- Startup probing consumed the first queued key even when it was a launcher
  selection. It now peeks and consumes only an exit key used to bypass resume.
- Multiline marquee input ended at its first newline, routing subsequent text
  into the speed prompt and animation controls. The text reader now consumes
  the whole paste, joins lines, and explicitly rejects oversized messages.
- The animation numeric reader discarded letters while retaining embedded
  digits, and truncated long numbers. It now rejects mixed text and overflow;
  marquee delay values are bounded to prevent apparently frozen playback.
