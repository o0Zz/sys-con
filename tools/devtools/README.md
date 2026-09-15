# devtools

Drives sys-con on a real console: build, deploy, run, watch, collect, symbolize.

It talks to [sys-autopilot](https://github.com/o0Zz/sys-autopilot) over HTTP —
which supplies file transfer, on-console SHA-256, screenshots, and (as of
`feat/process-control`) starting and stopping a sysmodule by title id. That is
what makes an iteration possible without rebooting.

```sh
python tools/devtools doctor            # is this machine, and the console, set up?
python tools/devtools iterate           # one full build -> deploy -> run -> classify
```

## Setup

The console needs three things; `doctor` checks all of them and names what is
missing.

1. **sys-autopilot installed and autostarting** (its own `boot2.flag`), with a
   token set in `/config/sys-autopilot/config.ini`.
2. **`fatal_auto_reboot_interval` set** in `/atmosphere/config/system_settings.ini`:

   ```ini
   [atmosphere]
   fatal_auto_reboot_interval = u64!0x2710   ; 10s
   ```

   Without it the console sits on the fatal screen waiting for a human and the
   loop cannot recover on its own. With it, a crash self-heals: fatal → reboot
   → CFW → sys-autopilot answers again, and the crash report creport wrote
   before the reboot is still on the card.

3. **sys-con *not* autostarting** — `/atmosphere/contents/<TID>/flags/boot2.flag`
   must be absent. The loop starts it on demand, so a build that faults during
   startup cannot take the console down before anything is reachable. Note
   `make all` recreates that flag in `out/`, which is correct for release zips
   and wrong for a test console: never sync `out/` wholesale, and the fence
   refuses to upload the flag.

`python tools/devtools setup-console --write` applies 2 and 3, backing up
anything it overwrites into `debug/console-backup/`.

Also worth setting `log_level=0` (Trace) in `/config/sys-con/config.ini`. The
startup milestones are logged at Debug, so at the shipped `log_level=3` the log
shows the banner and then nothing, and a boot hang cannot be attributed to a
step. `doctor` warns when it is above Trace.

## Configuration

All configuration is environment variables:

| Variable | Default |
|---|---|
| `SYSCON_AUTOPILOT_URL` | `http://192.168.10.238:4150` |
| `SYSCON_AUTOPILOT_TOKEN` | *(none)* |
| `SYSCON_AUTOPILOT_USER` / `_PASS` | *(none; used for HTTP Basic)* |
| `SYSCON_MSYS2_BASH` | `C:\msys64\usr\bin\bash.exe` |
| `SYSCON_DEVKITPRO_WIN` | `C:\msys64\opt\devkitpro` |

The device build runs through MSYS2 because `make` and devkitPro are not on the
Windows PATH. The title ID always comes from `make print-title-id`, with the
Makefile literal as a fallback so this works without a toolchain.

## Scripted input

sys-con can create a virtual pad driven over UDP, so the loop can press buttons
with no controller plugged into the console:

```sh
python tools/devtools input                 # the smoke-test set
python tools/devtools input A B DPAD_UP --hold 0.2
python tools/devtools iterate --exercise-input
```

Enable it first on the console — it is off by default, and deliberately so,
since anyone on the network can press buttons while it is on:

```ini
[global]
network_controller=1
network_controller_port=56789
```

`doctor` warns when it is disabled or on a different port. The wire format
comes from `tools/networkpad.py`, which is imported rather than reimplemented:
the packet layout has to agree with `NetworkController.h`, and two copies would
drift.

With `--exercise-input`, an iteration presses the smoke-test set after startup
and then checks the log for `Controller[ffff-0001] plugged !`. If the pad never
registers, the outcome is `UNSTABLE` rather than `HEALTHY` — the sysmodule is
up but not doing its job. That is the difference between proving it did not
crash and proving it works.

`HOME` and `CAPTURE` are left out of the smoke set on purpose: one backgrounds
whatever is running, the other writes to the album.

## Output contract

stdout is exactly one JSON object, always, including on failure. Progress and
human text go to stderr, so an agent can run
`python tools/devtools iterate 2>/dev/null` and branch on the result.

`ok` says whether the tool worked; `outcome` says what the experiment found —
`{"ok": true, "outcome": "CRASHED"}` is a successful run that caught a bug.

| Exit | Meaning |
|---|---|
| 0 | `HEALTHY` |
| 10 / 11 | `BUILD_FAILED` / `DEPLOY_FAILED` |
| 12 / 13 / 14 | `BOOT_HANG` / `CRASHED` / `UNSTABLE` |
| 20 | host tests failed (never deploys) |
| 30 | console unreachable — *not* a crash |
| 40 / 41 / 42 | usage / guardrail refused / needs a human |

## How an iteration decides

Evidence of a fault outranks missing progress, because a crash during init and
a boot hang look identical if you only watch for milestones:

```
new crash report, or the server vanished   -> CRASHED
process exited, having reached psc         -> CRASHED
process exited, having not                 -> BOOT_HANG
alive but never finished starting          -> BOOT_HANG
|E| lines in the log                       -> UNSTABLE
otherwise                                  -> HEALTHY
```

When the server vanishes mid-run the loop waits out the auto-reboot (up to
180s), then harvests the report. A retry is attempted unless the crash
signature repeats: **the same crash twice is a code bug, and rebooting harder
produces no new information.**

## Things that will bite you

- **Stale-build attribution** is the nastiest failure here: observing the old
  build and blaming the new code. Every deploy is verified by comparing the
  console's own SHA-256 against the local file, and a mismatch is
  `DEPLOY_FAILED` rather than "whatever we saw".
- **Crash report filenames are not ordered.** They come from the console RTC,
  which on a modchipped console repeats or is wildly wrong. New reports are
  found by `(name, size)` set difference — never by sorting or mtime.
- **`log.txt` wraps.** 128 KiB cap, truncated only at startup, so at Trace with
  a pad attached the boot record is the first thing lost. The log is pulled
  right after boot, not only at the end of a soak.
- **The ELF is archived per build** under `debug/builds/<build-id>/`, because
  `make all` overwrites it and a crash report can arrive an iteration late.
  Symbolizing against the wrong ELF produces plausible garbage.

## Not yet verified against hardware

- The creport text parser in `symbolize.py` is written leniently but has not
  been checked against a report from this console. The offset arithmetic,
  bounds check and addr2line batching are all verified against the real ELF.
- Whether Atmosphère's `ldr` re-reads `exefs.nsp` on a relaunch within one
  boot. If it caches, every iteration needs a reboot (~35s) instead of a
  restart (~3s). The loop handles both, but the cost differs.
