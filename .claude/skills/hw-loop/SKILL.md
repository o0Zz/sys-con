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

Add `--exercise-input` to also prove input works (see §4). Without it a pass
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
| `UNSTABLE` | 14 | Repeat once before concluding anything. With `--exercise-input`, check `signals.input_pad_registered` first — see §4. |

### Reading a crash

`crash.signature_hash` is the dedup key. **If it matches the previous run, your
last fix did not work — do not try the same fix again.** Say so plainly and
change approach.

Frames listed under `frames_outside_image` are libnx or kernel addresses, not
sys-con. Do not chase them and do not override the module base to force them to
resolve; a confident wrong answer is worse than an unresolved frame.

## 4. Simulating a controller over UDP

sys-con can create a virtual pad driven over UDP, so input is testable with
nothing plugged into the console. Use it whenever the change touches the input
path — `src/app/controller_handler.cpp`, `src/controllerlib/**`,
`usb_module.cpp`, the profile/config handling — because a `HEALTHY` run without
it says nothing about whether sys-con still reads a controller.

It needs `network_controller=1` in `/config/sys-con/config.ini`; `doctor`
reports this as the `network_pad` check, and `setup-console --write` turns it
on. A disabled pad makes `--exercise-input` report `UNSTABLE` for a build that
is actually fine, so check `doctor` before believing that result.

Inside a run:

```sh
python tools/devtools iterate --soak 120 --exercise-input 2>/dev/null
python tools/devtools loop --iterations 10 --exercise-input 2>/dev/null
```

The iteration taps the smoke-test set (`A B X Y DPAD_UP DPAD_DOWN L R`, no
HOME, no CAPTURE) after startup and retries up to three times, because sys-con
reports running before its network controller has bound the socket.

Standalone, against a console already running sys-con:

```sh
python tools/devtools input 2>/dev/null                    # smoke-test set
python tools/devtools input A B DPAD_UP --hold 0.2 2>/dev/null
python tools/devtools logs --tail 4096 2>/dev/null         # look for the pad
```

Read the result from `signals`, not from the fact that the command returned:

| Signal | Means |
|---|---|
| `input_sent` | what the host transmitted. **UDP is unacknowledged — this is not evidence it arrived.** |
| `network_module_up` | sys-con started its network controller at all. |
| `input_pad_registered` | `Controller[ffff-0001] plugged !` in the log. This is the only proof input reached the sysmodule. |
| `input_attempts` | how many tries it took; >1 means the socket was late, not broken. |
| `input_error` | the host side failed (no route, pad import); fix the rig, do not read it as a code finding. |

`input_expected` true with `input_pad_registered` false is classified
`UNSTABLE`: the sysmodule is up but not doing its job. Treat it as a finding
about the build once `doctor` says the pad is enabled.

The wire format comes from `tools/networkpad.py` and must agree with
`src/controllerlib/drivers/NetworkController.h` — if a pad stops registering
after a change to that header, suspect the struct, not the console.

## 5. Repeating

```sh
python tools/devtools loop --iterations 10 --exercise-input 2>/dev/null
```

It stops on its own: build/test failure, deploy failure, 3 consecutive
`HEALTHY`, or the same crash signature 3 times. Prefer this over calling
`iterate` in a shell loop.

## The console only has two sound starts per boot

`hiddbgInitialize` leaks across launches. By the **third** start in a boot the
call quietly does nothing, and sys-con either fails to start or comes up unable
to drive a virtual controller.

The second failure mode is the dangerous one: the process runs, logs a clean
startup, reports `Controller[ffff-0001] plugged !` — and no input ever reaches
the console. That is indistinguishable from a real input bug, so a result
gathered on a third start is worse than no result.

`iterate` enforces this itself (`MAX_STARTS_PER_BOOT` in `config.py`): it
tracks starts against the console's uptime and reboots before spending a start
it cannot trust. **When starting sys-con by hand, count your starts** —
`devtools start` does not, and two is the budget.

Symptoms that mean you have already overrun it, not that you found a bug:

- `process/start` returning `LimitReached` (`rc=0x00010801`) when the same
  build started fine minutes earlier
- the pad registering in the log while nothing moves on screen
- `devtools input` succeeding while the log shows no receive activity

The fix is always `power/restart`, never another start. Budget one reboot
(~60 s) per two experiments and plan the session around that, rather than
retrying and reading the wreckage.

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
console is on and sys-autopilot is answering. A console that went to sleep
looks exactly the same: run `doctor` and read its `keep_awake` check.

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
