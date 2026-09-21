# CLAUDE.md

## Git

- **Never commit and never push changes.** Leave all modifications in the working tree for the user to review and commit themselves. This applies even if a task seems to naturally end in a commit — stop before it and report what changed instead. Only commit or push if the user explicitly asks in that message.

## Code style

- **No fallbacks, no compatibility shims.** Implement exactly one path — the correct one. Do
  not add "if the new way fails, try the old way" branches, legacy aliases, defensive
  defaults that paper over a missing value, or dead configuration kept around for
  compatibility. If something is wrong or absent, fail loudly at the point it is detected.
- **Breaking changes are fine — prefer them over accretion.** Rename, change signatures,
  delete config keys and drop old behaviour when that yields the cleaner design. There is no
  external API to preserve and no deprecation period; update every call site in the same
  change and say in the report what broke.
- **Avoid comments.** Code must speak for itself — use clear names and structure instead of explaining what the code does. Only comment non-obvious things: a hardware quirk, a protocol constraint, a workaround, or a "why" that cannot be read off the code. Do not add comments that restate the line below them, and do not leave narration of changes in the code.

## Working with me

- **I'm an engineer.** Skip the beginner explanations and go straight to the technical point.
- **Keep plans minimal.** Write the shortest plan that is still executable — the change, the files touched, how to verify. No long write-ups, no restating what I already said, no listing alternatives I did not ask for.

## HID MITM (`mode=mitm`)

- **Both flavours have a working MITM.** `ATMOSPHERE=1` uses libstratosphere's
  (`src/platform/ams/HidMitm{Module,Service}.cpp`; build with `SYSCON_ATMOSPHERE=1 python
  tools/devtools build`). `ATMOSPHERE=0` uses the hand-written libnx server
  (`src/platform/libnx/HidMitmServer.cpp`), verified 2026-09-20: the profile-select applet is
  intercepted, gets the fake shared memory, and its pointer moves under the UDP pad.
- **In the MITM server, never touch TLS between a syscall and the use of its message.**
  `syscon::logger` writes to the SD card, an SD write is IPC, and IPC reuses the thread's TLS
  command buffer — so one log line between `svcReplyAndReceive` and reading the request, or
  between building a reply and sending it, replaces the message with an fs reply. That single
  `LogInfo` after `BuildCmifReply` in `CreateAppletResource` is what used to wedge the console
  (the applet got an fs reply as its `IAppletResource`, hung, and took `am`/`hid`/`sm` with
  it: alive on ICMP, every TCP port dead, nothing on the card, physical power-cycle needed).
  The server now snapshots every request into `m_request` right after the syscall, stages
  every reply in `g_reply`, and copies into TLS only immediately before the reply syscall.
  Keep it that way; it is not about the domain layer, and a wrong `aruid` in the log is the
  tell that something is reading TLS after a log.
- **Tell the two apart in the log by wording.** `HidMitmService ...` is the ams
  implementation; a bare `HidMitm: ...` is the libnx one. Before deploying, confirm with
  `nm src/app/build/sys-con.elf | grep -i hidmitm`.
- **Both flavours compile into the same `src/app/build`.** `make` cannot tell them apart, so
  building one after the other silently relinks the wrong binary. `tools/devtools/build.py`
  forces `make clean` when the flavour changes; never bypass that.
- **`ATMOSPHERE=1` needs `external/Atmosphere-libs/libstratosphere/build/` clean** if the
  checkout predates the move of third-party code to `external/`. Stale `.d` files still name
  `lib/Atmosphere-libs/...` and the build fails on a missing `result_get_name.cpp`.
- **The ams flavour must supply its own allocators.** libstratosphere ships only weak stubs
  that abort, and its `malloc` returns `nullptr` until a region is registered — that is why
  `-Wl,--require-defined,__libnx_alloc` is in its build config. `AmsRuntime.cpp` defines
  `__libnx_alloc` / `__libnx_aligned_alloc` / `__libnx_free` and calls
  `init::InitializeAllocator`. Without them libnx's socket driver fails
  (`socketInitialize` → `LibnxError_OutOfMemory`, then `socket()` → `ENOMEM`).
- **Keep the ams heap at 512 KiB.** It is static storage, so it counts against the memory the
  kernel reserves for the process; at 1 MiB pm refuses to launch the module at all
  (`0x00010801` LimitReached).
- **The ams MITM thread needs 16 KiB of stack, not 4 KiB.** libstratosphere's CMIF dispatch is
  ten frames deep before a handler body runs, and `HidMitmService::CreateAppletResource` then
  adds the logger's 512-byte line buffer plus newlib's `vsnprintf`. At `0x1000` it overflows
  and Atmosphere fatals with descriptor **`0xFFD`** (`StackOverflowErrorDesc`, see
  `libvapours/.../ams_fatal_error_context.hpp`), taking the whole console down. libstratosphere
  gives its own mitm query server 16 KiB for the same reason. This only bites once an applet
  actually reaches `CreateAppletResource`, which is why it surfaced the moment `boot2.flag`
  made the MITM catch `qlaunch` (`0x…1000`) and `overlayDisp` (`0x…100C`) at boot.
- **A stale MITM registration on `hid` survives until the console reboots, and sys-con must
  not abort over it.** sm never reclaims a MITM registration when its owner is killed, a
  terminated sys-con never runs `Finalize`, and sm refuses `UninstallMitm` from any process
  that is not the owner — all verified on device. So after any `devtools stop`/`restart`,
  the next sys-con runs *without* a MITM until a reboot; only a clean boot gets one.
  `RegisterMitmServer` therefore must never be wrapped in `R_ABORT_UNLESS`: it returns
  `0x815` (`sm::ResultAlreadyRegistered`) in that state, and aborting fatals the whole
  console. **Restarting sys-con is not a valid way to test mitm mode — reboot instead.**
- **Decoding a fatal: `tools/AFE_Parser.exe -report <bin> -elf src/app/build/sys-con.elf
  -addr2line C:/msys64/opt/devkitpro/devkitA64/bin/aarch64-none-elf-addr2line.exe`.** The
  default addr2line path in the tool is wrong on this machine and the trace comes out
  unsymbolized without the flag. Descriptor `0xFFE` is `std::abort` (a failed
  `R_ABORT_UNLESS`; the failing Result is in X[0] — e.g. `0x815` = `sm::ResultAlreadyRegistered`
  from `InstallMitm("hid")` when a second sys-con instance starts while the first holds it).
- **A MITM only catches processes that open `hid` after it installs.** Atmosphere resolves it
  at `sm:GetService` time, so nothing already running — qlaunch included — is ever
  intercepted. In production that is what `boot2.flag` solves. On the test console, where
  that flag is forbidden, an experiment has to launch an applet or game *after* sys-con is
  up, and a mitm-mode run with no `session accepted` line in the log proves nothing.
- **Good test target: the profile-select applet** (`0x0100000000001007`), reached by starting
  any game. It is a separate process, so it gets MITM'd, and its selection visibly moves
  under the UDP pad.
- **Data plane** (`SwitchMITMManager`, shared by both flavours): the manager thread mirrors
  the whole real shared memory into the fake one at 200 Hz, skipping the npad slots sys-con
  owns and slots empty on both sides; `Update` only stores the latest pad state and the
  manager publishes it every tick, so the pad keeps sampling like real hardware. Slot
  allocation avoids npads the real hid is using.

## Hardware test rig

- **Drive the console's UI with the touch panel, not with buttons.**
  `python tools/devtools touch X Y` (and `swipe X0 Y0 X1 Y1`) taps the screen
  through sys-autopilot's `/input/touch`, which injects into hiddbg's touch
  auto-pilot. Coordinates are pixels in the same 1280x720 space `devtools
  screenshot` returns, so read the target off a capture — on-screen button hints
  ("B Retour") are tappable too. This is the only scripted input that reaches an
  intercepted process in `mode=mitm`: a pad press lands in an npad slot the MITM
  replaces, while the touch section of shared memory is mirrored through.
  Verified 2026-09-21 against qlaunch while the log showed `CreateAppletResource
  hooked for program 0x0100000000001000` — two taps walked the post-boot
  "Ⓐ Commencer" screen into controller pairing and back out. Handheld only:
  docked, the panel is off and the tap succeeds with nothing happening.
- **sys-autopilot only knows the processes it launched itself.**
  `GET /process?titleId=...` resolves the pid it kept at launch, so a sysmodule
  started by its own `boot2.flag` always reports `running: false`, and
  `POST /process/start` on it then fails with **`0x00010801`** — the same
  LimitReached that a too-large heap gives, because a second instance cannot
  fit. Before chasing a memory budget, check `/atmosphere/contents/<TID>/flags/`
  and whether `/config/sys-con/log.txt` has a fresh mtime. The test console
  currently *does* carry sys-con's `boot2.flag` (mitm needs it to catch
  qlaunch), which contradicts `tools/devtools/README.md`'s setup rule.
- **sys-autopilot gives up `hid:dbg` whenever it launches a sysmodule**, so its scripted input
  dies for as long as sys-con runs (`/input/tap` → `0xe401`). It takes it back only in
  `process_stop`. Its `process.c` was patched with `reopen_hiddbg()`, which wraps
  `hiddbgInitialize()` in `smInitialize()/smExit()` — `__appInit` closes the sm session, so
  the bare call silently left the service unopened. `reopen_hiddbg()` also runs on the
  *failed*-launch branch, so `POST /process/start {"titleId":"0100000000001007"}` (any
  non-sysmodule id) fails in the location resolver and hands `hid:dbg` back with sys-con still
  running — that is the way to get input back after `devtools start`, and also what wakes
  scripted input on a fresh boot. Do not start/stop Tesla or sys-patch for this; starting
  Tesla took the console down. The controller-pairing screen ("press the same button three
  times") does not accept the HDLS pad and needs a human. Known-good 1.5.0 binary:
  `C:\dev\sys-autopilot\sys-autopilot-1.5.0-known-good.nsp`.
- **`/process/start` only launches sysmodules under `/atmosphere/contents`.** Real titles fail
  in the location resolver, so games have to be launched by driving the Home Menu.
