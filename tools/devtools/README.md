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

   **Exception: the current test console carries sys-con's `boot2.flag` on
   purpose**, because `mode=mitm` only intercepts processes that open `hid`
   after the MITM is up, and catching qlaunch needs sys-con at boot. On that
   console `doctor`'s `syscon_autostart` check fails by design, and
   `setup-console --write` would delete the flag mitm testing relies on — do
   not run it there unless you restore the flag afterwards. The tooling itself
   still enforces the rule above.

`python tools/devtools setup-console --write` applies 2 and 3 and also enables
`network_controller=1` in `/config/sys-con/config.ini` (adding it, with
`network_controller_port`, to `[global]` if the key is missing), backing up
anything it overwrites into `debug/console-backup/`.

The console must also stay awake: sleep powers down the WLAN module, so every
call turns into an unreachable error (exit 30) that reads like a crash but is
not one. sys-autopilot holds auto-sleep off while `keep_awake = true` under
`[power]` in `/config/sys-autopilot/config.ini` (the default); `doctor` warns
when `/status` reports `keepAwake` false.

Also worth setting `log_level=0` (Trace) in `/config/sys-con/config.ini`. The
startup milestones are logged at Debug, so at the shipped `log_level=3` the log
shows the banner and then nothing, and a boot hang cannot be attributed to a
step. `doctor` warns when it is above Trace.

## Configuration

All configuration is environment variables:

| Variable | Default |
|---|---|
| `SYSCON_AUTOPILOT_URL` | `http://192.168.10.253:4150` |
| `SYSCON_AUTOPILOT_TOKEN` | *(none)* |
| `SYSCON_AUTOPILOT_USER` / `_PASS` | *(none; used for HTTP Basic)* |
| `SYSCON_MSYS2_BASH` | `C:\msys64\usr\bin\bash.exe` |
| `SYSCON_DEVKITPRO_WIN` | `C:\msys64\opt\devkitpro` |
| `SYSCON_ATMOSPHERE` | `0`; `1` builds the libstratosphere (`ATMOSPHERE=1`) flavour |

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
and then checks the log for `Controller[ffff-0001] plugged !`, retrying up to
three times — the process reports running before the network controller has
bound its socket, and UDP gives no hint that the first packets went nowhere.
If the pad never registers, the outcome is `UNSTABLE` rather than `HEALTHY` — the sysmodule is
up but not doing its job. That is the difference between proving it did not
crash and proving it works.

`HOME` and `CAPTURE` are left out of the smoke set on purpose: one backgrounds
whatever is running, the other writes to the album.

## Driving the UI: the touch panel

`input` presses buttons on sys-con's own pad, which is the wrong tool for
walking the console's own menus. Use the touch panel instead:

```sh
python tools/devtools screenshot --out debug/before.jpg
python tools/devtools touch 819 345          # pixel in the screenshot
python tools/devtools swipe 640 600 640 150 --duration 250
```

Coordinates are pixels in the panel's 1280x720 space, which is exactly the
space `screenshot` returns, so a target is read straight off a capture — button
hints at the bottom of a system screen ("B Retour") are tappable too. Verified
on hardware 2026-09-21: two taps walked the post-boot "A Commencer" screen
forward into controller pairing and back out.

sys-autopilot injects this through `hiddbg`'s touch auto-pilot, not through the
HDLS virtual pad, so it needs no controller attached and no `network_controller`.
Handheld mode only: docked, the panel is off and a tap succeeds while nothing
moves on screen.

## Commands

```
doctor                       environment + console preflight, read-only
setup-console --write        fatal_auto_reboot_interval, remove sys-con boot2.flag, network_controller=1
build / test                 device build (+ archive) / host ctest
deploy                       upload exefs.nsp, verify by on-console SHA-256
start / stop / restart / status
logs / crashes / dumps       pull artifacts off the console
symbolize --report FILE      crash report -> symbolized stack trace
input [BUTTONS...]           press buttons via the UDP pad
touch X Y                    tap the touch panel (drives the console's menus)
swipe X0 Y0 X1 Y1            drag across the touch panel
screenshot                   capture the screen
iterate                      one build -> deploy -> run -> classify cycle
loop                         repeat iterate until a stop condition
gc                           prune debug/ without orphaning a crash report
```

`loop` stops on the first thing that makes further runs pointless: a build or
host-test failure, a deploy failure, `--until-healthy` consecutive HEALTHY runs
(default 3), or the same crash signature `--same-crash-limit` times (default 3).
That last one matters most — the same crash twice is a code bug, and another
reboot produces no new information.

Commands that change console state take a lock (`debug/.lock`) so two runs
cannot interleave a stop with an upload and leave a half-written `exefs.nsp`.
A second run exits 43 rather than waiting; a lock older than an hour is
treated as stale and taken.

`gc` never deletes the build named by `builds/current.txt` or one an
iteration still references, because symbolizing against the wrong ELF produces
confident wrong answers. `--dry-run` shows what would go.

Recovery state lives in `debug/watchdog.json` and persists across invocations —
the "don't retry the same signature" rule is useless if it resets every time,
since the normal way this runs is a series of separate commands.

## Output contract

stdout carries exactly one representation of the result: the JSON envelope by
default, or a human rendering of that same envelope with `--format human`
(accepted before or after the subcommand). Progress goes to stderr either way,
so an agent can run `python tools/devtools iterate 2>/dev/null` and branch on
the result.

`ok` says whether the tool worked; `outcome` says what the experiment found —
`{"ok": true, "outcome": "CRASHED"}` is a successful run that caught a bug.

| Exit | Meaning |
|---|---|
| 0 | `HEALTHY` |
| 10 / 11 | `BUILD_FAILED` / `DEPLOY_FAILED` |
| 12 / 13 / 14 | `BOOT_HANG` / `CRASHED` / `UNSTABLE` |
| 20 | host tests failed (never deploys) |
| 30 | console unreachable — *not* a crash (asleep, off, or sys-autopilot down) |
| 40 / 41 / 42 | usage / guardrail refused / needs a human |
| 43 | another run holds the console |

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

## One sound start per boot

`hiddbgInitialize` leaks across launches. On this console the second start in
a boot already fails with `LimitReached`, so the budget is one
(`MAX_STARTS_PER_BOOT = 1`). A start past the budget either fails outright
(`LimitReached`, `rc=0x00010801`) or — worse — succeeds into a process that logs a clean startup and registers its pad while no input ever
reaches the console. That reads exactly like an input bug in the build under
test, which is why this is enforced rather than merely documented:
`MAX_STARTS_PER_BOOT` in `config.py`, applied by `iterate` through the
`starts_since_boot` counter in `debug/watchdog.json`, reset whenever the
console's uptime goes backwards.

`devtools start` does **not** enforce it. Counting starts is on you when
driving by hand, and the remedy is always a reboot, never another start.

## Things that will bite you

- **Stale-build attribution** is the nastiest failure here: observing the old
  build and blaming the new code. Every deploy is verified by comparing the
  console's own SHA-256 against the local file, and a mismatch is
  `DEPLOY_FAILED` rather than "whatever we saw".
- **Crash report filenames are not ordered.** They come from the console RTC,
  which on a modchipped console repeats or is wildly wrong. New reports are
  found by `(name, size)` set difference — never by sorting or mtime.
- **`log.txt` is only reset at startup.** `logger::Initialize` deletes it if it
  is already at least 128 KiB (`LOG_FILE_SIZE_MAX`), otherwise appends; nothing
  caps it during a run. So one boot's log can start with the tail of earlier
  boots, or lose them entirely. The log is pulled right after boot, not only at
  the end of a soak.
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
