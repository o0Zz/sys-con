---
name: hw-loop
description: Run sys-con on the real Nintendo Switch — build, deploy, start, watch, classify, and symbolize any crash — using tools/devtools. Use when asked to test on hardware, run the hardware loop, chase a crash on device, or check whether a change actually works on the console.
---

# sys-con hardware loop

Everything goes through one CLI: `python tools/devtools <command>`. It prints
exactly one JSON object on stdout (progress goes to stderr), so run it as
`python tools/devtools <cmd> 2>/dev/null` and parse the result. Add
`--format human` when reporting to the user.

`ok` says whether the tool worked. `outcome` says what the experiment found.
`{"ok": true, "outcome": "CRASHED"}` is a **successful run that caught a bug** —
report it as a finding, not as a failure of the tooling.

## 1. Before touching the console

```sh
python tools/devtools doctor 2>/dev/null
```

Non-zero: stop and report what failed. A rig with a red check produces results
that mean nothing — do not "try anyway".

## 2. Decide whether hardware is needed at all

Run `python tools/devtools test` and stop there when the change is confined to
code the host suite covers:

- `src/controllerlib/**`, `src/app/config_handler.cpp`, `external/ini/**`
- `tests/**`, `doc/**`, `README.md`, CMake files

Hardware is required when the change touches code with no host coverage:

- `src/app/usb_module.cpp`, `psc_module.cpp`, `controller_handler.cpp`,
  `logger.cpp`, `main.cpp`
- anything under `src/platform/**` — highest risk, and the area that can hang
  the whole console
- `src/app/sys-con.json` (NPDM), `Makefile`, `src/app/Makefile`, `toolbox.json`

Any change to a **stack size, thread priority or heap size** needs
`--soak 600`, not the default. Those bugs are latent and time-dependent.

## 3. One iteration

```sh
python tools/devtools iterate --soak 120 2>/dev/null
```

Add `--exercise-input` to also prove input works: it drives sys-con's UDP pad
and checks the log for `Controller[ffff-0001] plugged !`. Without it a pass
only means "did not fall over".

Branch on `.outcome`:

| Outcome | Exit | Do |
|---|---|---|
| `HEALTHY` | 0 | Report and stop. |
| `HOST_TESTS_FAILED` | 20 | Read `<artifacts_dir>/test.log`, fix, re-run. **Never deploy.** |
| `BUILD_FAILED` | 10 | Read `<artifacts_dir>/build.log`, fix, re-run. Do not touch the console. |
| `DEPLOY_FAILED` | 11 | Transport problem, **not** a code problem. Do not change code. Retry once; on a second failure stop and tell the user. |
| `CRASHED` | 13 | Read the file at `crash.symbolized_path`. |
| `BOOT_HANG` | 12 | `signals.milestones_seen` names the last startup step reached — suspect the step *after* it, in `src/app/main.cpp`. |
| `UNSTABLE` | 14 | Repeat once before concluding anything. |

### Reading a crash

`crash.signature_hash` is the dedup key. **If it matches the previous run, your
last fix did not work — do not try the same fix again.** Say so plainly and
change approach.

Frames listed under `frames_outside_image` are libnx or kernel addresses, not
sys-con. Do not chase them and do not override the module base to force them to
resolve; a confident wrong answer is worse than an unresolved frame.

## 4. Repeating

```sh
python tools/devtools loop --iterations 10 --exercise-input 2>/dev/null
```

It stops on its own: build/test failure, deploy failure, 3 consecutive
`HEALTHY`, or the same crash signature 3 times. Prefer this over calling
`iterate` in a shell loop.

## Hard stops

Stop, report, and do **not** retry on:

- exit `41` — a guardrail refused a path. Never work around it.
- exit `42` — needs a human (the console did not come back; it may need a
  power cycle, or the SD card read directly).
- exit `43` — another run holds the console lock.
- three runs with the same `signature_hash`.
- three consecutive `HEALTHY`.

**Exit `30` means the console is unreachable — that is not a crash.** It is the
one failure that must never be reported as a finding about the code. Check the
console is on and sys-autopilot is answering.

## Never

- Re-create `/atmosphere/contents/690000000000000D/flags/boot2.flag`. sys-con
  is started on demand; with that flag a startup crash locks the console on
  every boot and there is no server left to upload a fix through. The fence
  refuses it — do not look for another way.
- Sync `out/` wholesale to the console. `make all` puts `boot2.flag` in it.
- Deploy a build whose host tests failed.
- `make distclean` or `mrproper` — they can `git reset --hard` the
  Atmosphere-libs submodule.
- Commit or push (see CLAUDE.md).

## Housekeeping

`python tools/devtools gc` prunes `debug/` (~7 MB of ELF per iteration). It
never removes the current build or one an iteration still references.

Full reference: `tools/devtools/README.md`.
