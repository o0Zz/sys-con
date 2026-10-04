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

- **Two flavours, both working.** `ATMOSPHERE=1`: libstratosphere
  (`src/platform/ams/HidMitm{Module,Service}.cpp`, build with `SYSCON_ATMOSPHERE=1 python
  tools/devtools build`); log lines read `HidMitmService ...`. `ATMOSPHERE=0`: hand-written
  libnx server (`src/platform/libnx/HidMitmServer.cpp`); log lines read `HidMitm: ...`.
  Confirm the binary with `nm src/app/build/sys-con.elf | grep -i hidmitm` before deploying.
- **Both build into `src/app/build`.** `tools/devtools/build.py` forces `make clean` on a
  flavour change; never bypass it. A stale `external/Atmosphere-libs/libstratosphere/build/`
  (`.d` files naming `lib/Atmosphere-libs/...`) fails on `result_get_name.cpp` — clean it.
- **In the libnx server, never touch TLS between a syscall and the use of its message.** The
  logger writes the SD card, which is IPC and reuses the TLS command buffer; a single log line
  after `BuildCmifReply` once sent the applet an fs reply and wedged the whole console
  (ICMP alive, all TCP dead, power-cycle). Requests are snapshotted into `m_request` right
  after the syscall, replies staged in `g_reply` and copied into TLS just before sending —
  keep it that way. Tell: a wrong `aruid` in the log, or `SFCO` inside a request.
- **ams flavour runtime budget:** it must define `__libnx_alloc`/`__libnx_aligned_alloc`/
  `__libnx_free` and call `init::InitializeAllocator` (`AmsRuntime.cpp`; libstratosphere's
  stubs abort, and without them `socketInitialize` → `ENOMEM`), hence
  `-Wl,--require-defined,__libnx_alloc`. Heap stays at 512 KiB (1 MiB → pm refuses the
  launch, `0x00010801`). MITM thread stack stays at 16 KiB (4 KiB overflows in
  `CreateAppletResource` → fatal `0xFFD`, console down).
- **A MITM registration on `hid` survives sys-con's death until reboot.** sm never reclaims
  it and refuses `UninstallMitm` from non-owners, so after `devtools stop`/`restart` the next
  sys-con gets `0x815` and runs without a MITM. Never wrap `RegisterMitmServer` in
  `R_ABORT_UNLESS` (the abort fatals the console). **Test mitm mode after a reboot, never a
  restart.**
- **A MITM only catches processes that open `hid` after it installs** (resolved at
  `sm:GetService`). `boot2.flag` makes it catch qlaunch; otherwise launch something after
  sys-con is up. A run proves nothing unless the log shows a client intercepted
  (`HidMitm: session accepted` / `CreateAppletResource hooked for program`, or
  `HidMitmService creation for program id` at Debug). Good target: the profile-select applet
  `0x0100000000001007`, reached by starting any game.
- **Data plane:** the two fake shared memories (system applets / applications) are allocated
  once at `Start()`. Home and Capture go through the pad's hiddbg device, not npad memory:
  bits 18/19 are `StickLRight`/`StickLDown` there, and `am` reads them from the real hid.
- **Fatals:** use the `hw-triage` skill. `0xFFE` is `std::abort` with the failing Result in
  X[0].

## Hardware test rig

- **Drive the UI with the touch panel:** `python tools/devtools touch X Y` / `swipe X0 Y0 X1
  Y1`, in `devtools screenshot`'s 1280x720 space. It is the only scripted input that reaches
  an intercepted process (pad presses land in a slot the MITM replaces). Handheld only.
- **The test console carries sys-con's `boot2.flag` on purpose.** `devtools doctor`'s boot2
  check is red by design; never run `setup-console --write` (it deletes the flag). As a
  consequence sys-autopilot reports `running: false` for sys-con and `/process/start` fails
  with `0x00010801` — check `/atmosphere/contents/<TID>/flags/` and the mtime of
  `/config/sys-con/log.txt` before chasing a memory budget.
- **sys-autopilot drops `hid:dbg` whenever it launches a sysmodule** (`/input/tap` →
  `0xe401`). To get input back with sys-con running, `POST /process/start
  {"titleId":"0100000000001007"}`: it fails in the location resolver and reopens `hid:dbg` (also what wakes scripted
  input on a fresh boot).
  **Never start/stop Tesla or sys-patch for this — it took the console down.** The
  "press the same button three times" pairing screen needs a human. Known-good binary:
  `C:\dev\sys-autopilot\sys-autopilot-1.5.0-known-good.nsp`.
- **`/process/start` only launches sysmodules under `/atmosphere/contents`;** games are
  launched by driving the Home Menu.
