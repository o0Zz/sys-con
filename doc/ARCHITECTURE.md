# sys-con architecture

How the sysmodule is put together, and where to look when something breaks.

---

## Where things live

The tree is split by **origin**, not by what kind of code something is:

```
src/         code authored in this repository
external/    code that comes from another repository
```

`external/` holds the three git submodules (`libnx`, `Atmosphere-libs`,
`HIDDataInterpreter`) and the vendored copy of `inih`. Note that HIDDataInterpreter is a
sys-con library too — it simply already lives in its own repo, which is precisely why it is
in `external/` rather than `src/`. The directory name is about *where the code lives*, not
about who wrote it.

That rule also describes the one thing likely to change here: `src/controllerlib/` is a
standalone library that has not been extracted yet. The day it is, it moves to `external/`
and becomes a fourth submodule, exactly as HIDDataInterpreter did.

---

## The layers

```
                   ┌─────────────────────────────────────────────┐
      src/app/     │  usb_module · controller_handler            │  discovery, lifetime
                   │  config_handler · logger · psc_module       │  config, logging, sleep/wake
                   └───────────────┬─────────────────────────────┘
                                   │ IController, ILogger, IFileManager
                   ┌───────────────┴─────────────────────────────┐
src/controllerlib/ │  BaseController  +  9 drivers (drivers/)    │  standalone library.
                   │  IUSBDevice · IUSBInterface · IUSBEndpoint  │  No libnx, no ams, no sys-con.
                   └───────────────┬─────────────────────────────┘
                                   │ implemented by
                   ┌───────────────┴─────────────────────────────┐
   src/platform/   │  SwitchUSBDevice/Interface/Endpoint         │  libnx, both flavours
                   │  SwitchVirtualGamepadHandler                │  the polling thread
                   │  SwitchHDLHandler · SwitchMITMHandler       │  both handlers, both flavours
                   ├─────────────────────────────────────────────┤
     …/libnx/      │  main.cpp · sm_mitm · HidMitmServer         │  ATMOSPHERE=0 only
     …/ams/        │  main_ams.cpp · HidMitmService/Module (AMS) │  ATMOSPHERE=1 only
                   └─────────────────────────────────────────────┘
```

**`src/controllerlib/` is a standalone library, not a layer of sys-con.** It sits under
`src/` only because it has not been extracted to its own repository yet — not because it is
part of the sysmodule. It knows nothing about sys-con, Horizon, libnx or Atmosphere, it
configures and builds entirely on its own
(`cmake -S src/controllerlib -B build-controllerlib`, no flags), and it owns its language
standard, warning set and dependencies. Everything it needs from the outside world arrives
through four pure interfaces (`IUSBDevice`, `IUSBInterface`, `IUSBEndpoint`, `ILogger`),
which `src/platform/` implements.

Sitting next to sys-con's own code makes that easy to erode, so CI builds the library
standalone on every push. Give it a dependency on anything in `src/app/` or `src/platform/`
and that job fails.

Everything the library declares lives in `namespace controllerlib`. sys-con's headers
qualify (`controllerlib::Status`, `controllerlib::IController`); its `.cpp` files open the
namespace with `using namespace controllerlib;` after their includes, so a sys-con header
never re-exports the library into whatever includes it. `src/platform/HorizonResult.h` is
where `controllerlib::Status` becomes a Horizon `Result`, and it is the only place the two
error domains meet.

That is the single most important property of this codebase: it is why the drivers can be
unit-tested on a PC with ordinary gtest mocks, with no emulator and no hardware, and why
the library could be lifted into its own repository without touching a line of it. **Do not
introduce a libnx — or a sys-con — dependency into `src/controllerlib/`.** In particular the
root `CMakeLists.txt` must not reach into it with `syscon_*` helpers; it only says where
HIDDataInterpreter lives, calls `add_subdirectory()`, and links the `ControllerLib` target.
See [src/controllerlib/README.md](../src/controllerlib/README.md).

`src/companion/` is a separate homebrew NRO used to eyeball raw stick/rumble values while
debugging. It shares nothing with the sysmodule but the repo.

---

## The input pipeline

One pass of this runs per controller, per poll, on that controller's own thread:

```
 USB endpoint
     │  SwitchUSBEndpoint::Read()
     ▼
 BaseController::ReadNextBuffer()        pick an endpoint, drain to the freshest report
     │                                   (round-robin across m_inPipe; "keep latest")
     ▼  raw bytes
 <Driver>::ParseData()                   per-controller: decode the wire format
     │                                   pure virtual — this is what a new driver implements
     ▼  RawInputData        buttons indexed by PHYSICAL PIN, analog by axis
 BaseController::MapRawInputToNormalized()
     │                                   deadzone → factor → pin-to-button mapping →
     │                                   analog-as-digital → combo simulation
     ▼  NormalizedButtonData   buttons indexed by ControllerButton, sticks as floats
 SwitchVirtualGamepadHandler::UpdateInput()
     │
     ▼
 SwitchHDLHandler / SwitchMITMHandler    publish to the console as a virtual pad
```

### The two button index spaces

This trips up nearly everyone, so it is worth stating plainly. There are **two different
arrays of booleans**, both called `buttons`, both sized `MAX_CONTROLLER_BUTTONS`:

| Array | Indexed by | Meaning |
|---|---|---|
| `RawInputData::buttons` | physical pin id | "button 5 on the wire is pressed" |
| `NormalizedButtonData::buttons` | `ControllerButton` | "the Switch's `A` is pressed" |

`ControllerConfig::buttonsPin` is the mapping between them, and it is what a user edits in
`config.ini` (`a=2` means "Switch A comes from pin 2"). Pin `0` means *unmapped*.

Two consequences to be careful about:

- Drivers are not consistent about whether pins are 0- or 1-based. Most start writing at
  `buttons[1]`; `GenericHIDController` starts at `buttons[0]`, which collides with the
  "unmapped" sentinel.
- The D-pad occupies a *third* numbering: `DPAD_UP_BUTTON_ID`..`DPAD_LEFT_BUTTON_ID`
  (32–35), separate from `ControllerButton::DPAD_UP` (21).

---

## Threads

| Thread | Created in | Priority / core | Does |
|---|---|---|---|
| main | `main.cpp` | — | init, then waits |
| USB event | `usb_module.cpp` | `0x3A`, any core | waits on USB events, probes interfaces, builds controllers |
| USB interface change | `usb_module.cpp` | `0x2C`, any core | notices unplugs, calls `RemoveAllNonPlugged` |
| per-controller polling | `SwitchVirtualGamepadHandler::InitThread` | config `polling_thread_priority`, **core 3** | the pipeline above, one thread per controller |
| PSC | `psc_module.cpp` | `0x2C`, any core | sleep/wake; calls `controllers::Clear()` |
| HID MITM | `HidMitmServer.cpp` (libnx) / `HidMitmModule.cpp` (ams); `mode=mitm` only | `0x20`/`20`, core 3 | serves the MITM'd `hid` IPC |

Shared state and its lock:

- `controllerHandlers` (the live controller set) is guarded by `controllerMutex` in
  `controller_handler.cpp`. It is touched by the USB event thread (insert), the interface
  change thread (remove), the PSC thread (clear) and main (exit).
- All USB access is serialised by `SwitchUSBLock`, a RAII wrapper over one process-wide
  `std::recursive_mutex`.
- Polling threads run on **core 3**, which is the core Horizon reserves for input; this is
  deliberate and affects latency.

**Stack sizes are tight.** Polling and PSC threads get 16 KiB. Config parsing puts inih's
~1.3 KiB line buffer on the caller's stack, and the logger formats through `vsnprintf`.
A stack overflow here presents as a hang or a crash on sleep/wake — see TC10 below.

---

## Configuration

`config.ini` is read from `/config/sys-con/`. Settings are applied in layers, each
overriding the last:

```
[global]      → polling rate, log level, discovery mode
[default]     → baseline mapping for every controller
[profile]     → a named reusable mapping, selected by `profile=`
[VID-PID]     → per-device overrides, e.g. [045e-02dd]
```

`LoadControllerConfig()` walks these in order. The file is parsed by `inih`
(vendored at `external/ini/`) through `IFileManager`, never through `fopen` directly — which
is what lets the host test build feed it an in-memory file.

---

## The two build flavours

Selected by the `ATMOSPHERE` make variable. **`ATMOSPHERE=0` is the default and the one CI
builds and ships.**

| | `ATMOSPHERE=0` | `ATMOSPHERE=1` |
|---|---|---|
| entry point | `main.cpp` | `main_ams.cpp` |
| runtime | libnx | libnx + libstratosphere |
| file I/O | `StdFileManager` | `AMSFileManager` |
| MITM server framework | hand-written (`sm_mitm` + `HidMitmServer`) | libstratosphere (`HidMitmService`/`Module`) |

Both virtual-pad handlers — `SwitchHDLHandler` (hiddbg HDLS) and `SwitchMITMHandler` (fake
HID shared memory) — plus the shared `SwitchMITMManager` data plane compile into **both**
flavours; the config `mode` (`hiddbg` or `mitm`, default `hiddbg`) picks one at runtime via
`controllers::SetMode`. Only the MITM *server framework* differs per flavour, because
libstratosphere is unavailable in the `ATMOSPHERE=0` build. The libnx MITM installs on `hid`
through Atmosphère's `sm` tipc extensions (`sm_mitm.c`, a port of libstratosphere's
`sm_ams.os.horizon.c`) — so Atmosphère is required at runtime for either flavour's MITM.

`IFileManager` (`src/platform/IFileManager.h`) is the clean seam between them: one
interface, two implementations, injected at startup by whichever `main` is compiled in.

The variant is selected **by directory**, not by filtering filenames. `src/app/Makefile`
adds `../platform` (shared by both) plus exactly one of `../platform/libnx` or
`../platform/ams`, and each of those carries that flavour's entry point. Nothing has to be
subtracted by name, and neither flavour compiles the other's code.

Caveats worth knowing before you touch the MITM path:

- The two handlers are not feature-equivalent: the MITM path hardcodes its npad identity
  and ignores the per-controller colours and `controllerType` the HDL path honours.
- `MITM_CONFIG_GC_ENABLED` is 0, so the shared-memory entry list is never pruned.
- The libnx `HidMitmServer` forwards non-hooked commands without domain-object tracking
  (unlike libstratosphere). This is fine for `hid` — clients do not domain-convert the hid
  session — but is a genuine limitation if the hooked interface ever changes.

---

## Where to look when something breaks

Condensed from [TestPlan.md](TestPlan.md), which carries the full manual test matrix.

| Symptom | Start here |
|---|---|
| No log file / empty log | `logger.cpp` (`Initialize`, `LogWriteToFile`), `filemanager_*.h` |
| Controller works but mapping is wrong | `config_handler.cpp`, then `BaseController::MapRawInputToNormalized` |
| Unknown pad not auto-added to `config.ini` | `config_handler.cpp` auto-add path, `ini.h` |
| A specific pad misreports buttons/axes | that driver's `ParseData`, then `BaseController` |
| Generic/HID pad not detected | `GenericHIDController.cpp` (report-descriptor parsing) |
| Large reports truncated (e.g. DS4) | `CONTROLLER_INPUT_BUFFER_SIZE`, `GetMaxInputBufferSize()` |
| Pad not detected on plug | `usb_module.cpp` attach path |
| Pad stays after unplug / ghost pad | `usb_module.cpp` teardown, `controller_handler.cpp` |
| Replug leaves stale state | `controller_handler.cpp`, `Xbox360WirelessController.cpp` |
| Missing `config.ini` doesn't fall back | `config_handler.cpp`, `ini.h` |
| **Crash or hang on sleep/wake** | `psc_module.cpp`. Each power transition runs `controllers::Clear()` on `psc_thread_stack`; teardown chains through every controller plus stack-heavy `Log*()`/`vsnprintf`. A crash here usually means that stack is undersized. |

For crashes, `sys-con.sh` can pull the Atmosphère report off the console and symbolise it
against `sys-con.elf`.

---

## Testing

The host build (CMake) compiles `src/controllerlib/` plus `config_handler`/`logger`/`ini` and
runs them under gtest — no hardware, no emulator:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target SysConTests -j
ctest --test-dir build --output-on-failure
```

What is and is not covered:

- **Covered:** every driver's `ParseData`, the normalization pipeline, deadzone/factor,
  config parsing (against the *real shipped* `dist/config/sys-con/config.ini`), and the
  INI line reader.
- **Not covered:** `usb_module`, `controller_handler`, `psc_module` and all of
  `src/platform/` — they pull in libnx and are only compiled by the device build. Changes
  there can only be verified by building the NSP and testing on hardware.

Test doubles live in `tests/mocks/`: `MockDevice`/`MockUSBInterface`/`MockUSBEndpoint` for
the USB interfaces, `MockLogger`, and `MemoryFileManager` for an in-memory `IFileManager`.
