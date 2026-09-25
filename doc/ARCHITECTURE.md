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
      src/app/     │  main · usb_module · controller_handler     │  program body, discovery
                   │  config_handler · logger · psc_module       │  config, logging, sleep/wake
                   │  network_module                             │  the network pad (opt-in)
                   └───────────────┬─────────────────────────────┘
                                   │ IController, ILogger, IFileManager
                   ┌───────────────┴─────────────────────────────┐
src/controllerlib/ │  BaseController  +  11 drivers (drivers/)   │  standalone library.
                   │  IUSBDevice · IUSBInterface · IUSBEndpoint  │  No libnx, no ams, no sys-con.
                   └───────────────┬─────────────────────────────┘
                                   │ implemented by
                   ┌───────────────┴─────────────────────────────┐
   src/platform/   │  SwitchUSBDevice/Interface/Endpoint         │  libnx, both flavours
                   │  UdpDevice · UdpInterface · UdpEndpoint     │  a socket, shaped like USB
                   │  SwitchVirtualGamepadHandler                │  the polling thread
                   │  SwitchHDLHandler · SwitchMITMHandler       │  both handlers, both flavours
                   ├─────────────────────────────────────────────┤
     …/libnx/      │  LibnxRuntime · sm_mitm · HidMitmServer     │  ATMOSPHERE=0 only
     …/ams/        │  AmsRuntime · HidMitmService/Module (AMS)   │  ATMOSPHERE=1 only
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
     ▼  NormalizedButtonData   buttons indexed by ControllerButton, sticks as floats,
     │                         motion in SDL's frame (see NormalizedMotion)
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
| runtime overhead | `LibnxRuntime.cpp` | `AmsRuntime.cpp` |
| entry point | `int main` (own `__appInit`) | `ams::Main` + `ams::init::*` |
| runtime | libnx | libnx + libstratosphere |
| file I/O | `StdFileManager` | `AMSFileManager` |
| MITM server framework | hand-written (`sm_mitm` + `HidMitmServer`) | libstratosphere (`HidMitmService`/`Module`) |

The flavour-agnostic program — the shared bring-up helpers (`ReadFirmwareVersion`,
`InitializeModules`/`FinalizeModules`) and the application body (`RunApp`) — lives once in
`src/app/main.cpp` (device-build only; `src/app/CMakeLists.txt` does not list it, so
`<switch.h>` never reaches the host tests). Each flavour's runtime file is the *overhead* that
genuinely differs and calls into it. The entry point is the important asymmetry: the libnx
build owns `__appInit`/`main`, whereas the Atmosphère build must **not** — libstratosphere's
`init_libnx_shim` already defines `__appInit`/`main` and calls `ams::Main()` plus the
`ams::init::*` hooks, which `AmsRuntime.cpp` supplies. Both call the same
`InitializeModules()` (hiddbg, usbHs, pscm) and `ReadFirmwareVersion()`, so only the SM/FS
bring-up and the ams heap/allocator are written per flavour.

Both virtual-pad handlers — `SwitchHDLHandler` (hiddbg HDLS) and `SwitchMITMHandler` (fake
HID shared memory) — plus the shared `SwitchMITMManager` data plane compile into **both**
flavours; the config `mode` (`hiddbg`, `mitm` or `disabled`, default `hiddbg`) picks one at runtime via
`controllers::SetMode`. Only the MITM *server framework* differs per flavour, because
libstratosphere is unavailable in the `ATMOSPHERE=0` build. The libnx MITM installs on `hid`
through Atmosphère's `sm` tipc extensions (`sm_mitm.c`, a port of libstratosphere's
`sm_ams.os.horizon.c`) — so Atmosphère is required at runtime for either flavour's MITM.

`IFileManager` (`src/platform/IFileManager.h`) is the clean seam between them: one
interface, two implementations, one of which each flavour's runtime file passes to `RunApp`
as a factory at startup.

The variant is selected **by directory**, not by filtering filenames. `src/app/Makefile`
adds `../platform` (shared by both) plus exactly one of `../platform/libnx` or
`../platform/ams`, and each of those carries that flavour's runtime file (`LibnxRuntime.cpp` /
`AmsRuntime.cpp`) and its entry point. Nothing has to be subtracted by name, and neither
flavour compiles the other's code — so the ams build never sees libnx's `__appInit`, and vice
versa.

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

For crashes, `tools/devtools` pulls the Atmosphère report off the console and symbolises it
against the build's archived `sys-con.elf`:

```sh
python tools/devtools crashes --pull
python tools/devtools symbolize --report debug/crashes/<report>.log
```

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
  config parsing (against the *real shipped* `src/app/config.ini`), and the
  INI line reader.
- **Not covered:** `usb_module`, `controller_handler`, `psc_module`, `network_module` and all of
  `src/platform/` — they pull in libnx and are only compiled by the device build. Changes
  there can only be verified by building the NSP and testing on hardware.

Test doubles live in `tests/mocks/`: `MockDevice`/`MockUSBInterface`/`MockUSBEndpoint` for
the USB interfaces, `MockLogger`, and `MemoryFileManager` for an in-memory `IFileManager`.

---

## The network controller (UDP)

Turned on with `network_controller=1` in `[global]`, off otherwise. It exists so that input can
be scripted from a PC — see [tools/networkpad.py](../tools/networkpad.py) — without a controller
plugged in, which is the only way to test anything below `src/controllerlib/` automatically.

It is **not** a special case in the pipeline. It is a UDP socket wearing the `IUSBDevice` /
`IUSBInterface` / `IUSBEndpoint` interfaces (`src/platform/UdpDevice.h`), feeding an
ordinary `BaseController` subclass (`src/controllerlib/drivers/NetworkController.h`) that
decodes a 20-byte packet. So it goes through the same `ReadNextBuffer` → `ParseData` →
`MapRawInputToNormalized` → handler path as a real pad, and `usb_module`, `controller_handler`
and both virtual-pad handlers need to know nothing about it.

Its pin numbers are the identity — bit N of the packet is pin N, and N is a `GamepadButton`
value — because nothing physical dictates them. The shipped `[network]` profile writes that
mapping out in full, and `tests/app/test_network_profile.cpp` checks the profile and the decoder
still agree, against the real `config.ini`.

Three things about it are easy to get wrong:

- **The socket is UDP-only on purpose.** `socketInitializeDefault()` asks bsd for ~2.2 MiB of
  transfer memory and `tmemCreate` takes it from the process heap, which is 512 KiB in total —
  so it does not merely waste memory, it fails. Zeroing the TCP buffers and setting
  `sb_efficiency = 1` brings it to 12 KiB. Do not "fix" a socket problem by enlarging this.
- **The pad opts out of `RemoveAllNonPlugged`** (`SwitchVirtualGamepadHandler::SetRemovable`).
  That function decides "unplugged" by looking for usbHs interface IDs, which this pad has
  none of, so it would otherwise be destroyed the first time a real device was plugged in.
- **Sleep destroys it and nothing re-creates it.** Real controllers come back because
  re-acquiring them raises a USB event; this one has no such event, so `psc_module` calls
  `networkpad::OnWake()` explicitly.
