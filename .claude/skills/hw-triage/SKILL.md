---
name: hw-triage
description: Symbolize an Atmosphère crash report from sys-con into a readable stack trace, picking the right archived ELF. Use when asked to explain a crash report, decode a fatal error, or work out where sys-con crashed — including from an old report, with no console attached.
---

# Triaging a sys-con crash report

Works with no console when the report is already on disk.

## Get a report

Already local? It is under `debug/iterations/<run>/crash/` or `debug/crashes/`.

Otherwise, pull from the console:

```sh
python tools/devtools crashes --pull 2>/dev/null
```

Prefer `/atmosphere/crash_reports/*.log` over `fatal_errors/*.bin`: creport
writes it unconditionally and it is far richer — every thread, every stack, and
the module list with base addresses.

## Symbolize

```sh
python tools/devtools symbolize --report debug/crashes/<name>.log 2>/dev/null
```

It picks the ELF from `debug/builds/<build-id>/` automatically. Override with
`--build-id` or `--elf` when you know the report belongs to an older build. If
the report has no module base, pass `--module-base 0xHEX` — the "Start Address"
shown on the fatal screen.

## Reading the result

- `signature` / `signature_hash` — `func@file:line` of the top in-module frame.
  The hash is how you tell "same crash again" from "new crash".
- `frames` — resolved, innermost first, with inlined frames marked.
- `frames_outside_image` — addresses outside sys-con's image: libnx or the
  kernel. **These are not sys-con frames.** Do not chase them, and do not
  invent a module base that makes them resolve. `addr2line` never fails; handed
  a foreign address it returns the nearest preceding symbol, which looks
  authoritative and is wrong.
- `result` — the Horizon result code, e.g. `0x0000ce01`.

## Attribute it honestly

Check the ELF actually matches the report. `report_build_id` against the
archived build's `manifest.json`: if they differ, you are symbolizing against
the wrong binary and every line is fiction. Say so rather than presenting the
output.

`make all` overwrites `src/app/build/sys-con.elf`, which is why builds are
archived per build-id — a report can arrive an iteration late, because the
console sits on the fatal screen until it reboots.

## Then

Point at the file and line, explain what the crashing code was doing, and name
the most likely cause. Common ones recorded in `doc/ARCHITECTURE.md`: a crash
during sleep/wake is usually `psc_thread_stack` being undersized, and anything
in `src/platform/**` involving the `hid` MITM can take the whole console down
before it can report.

If nothing resolves inside the image, say that plainly — the crash was probably
not in sys-con's own code.
