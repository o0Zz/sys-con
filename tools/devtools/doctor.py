"""Preflight: is this machine, and this console, actually set up to run the loop?

Read-only. Every check reports pass / warn / fail with a concrete remedy, so a
broken rig is diagnosed once rather than misread as a test result later.
"""

import os
import re

import autopilot
import config
import repo


def _check(name, ok, detail, remedy=None, level="fail"):
    return {
        "name": name,
        "status": "pass" if ok else level,
        "detail": detail,
        "remedy": None if ok else remedy,
    }


# --- local -------------------------------------------------------------------

def check_local(cfg):
    checks = []

    try:
        tid = repo.title_id()
        checks.append(_check("title_id", True, tid))
    except repo.Fatal as e:
        checks.append(_check("title_id", False, str(e),
                             "run from inside the sys-con repository"))
        tid = None

    checks.append(_check(
        "msys2_bash", os.path.isfile(cfg.msys2_bash), cfg.msys2_bash,
        "install devkitPro's MSYS2 or set SYSCON_MSYS2_BASH; the device build "
        "cannot run without it"))

    try:
        checks.append(_check("addr2line", True, repo.addr2line(cfg.devkitpro_win)))
    except repo.Fatal as e:
        checks.append(_check("addr2line", False, str(e),
                             "set DEVKITPRO, or install devkitA64"))

    checks.append(_check(
        "afe_parser", os.path.isfile(repo.AFE_PARSER), repo.AFE_PARSER,
        "only needed for fatal_errors/*.bin; crash_reports/*.log work without it",
        level="warn"))

    elf_exists = os.path.isfile(repo.ELF)
    checks.append(_check("built_elf", elf_exists, repo.ELF,
                         "run `python tools/devtools build`", level="warn"))

    checks.append(_check_milestones())
    return checks, tid


def _check_milestones():
    """The loop reads boot progress out of log.txt by matching literal strings
    from RunApp. If one is reworded, every run silently becomes a BOOT_HANG --
    so a rename has to fail here, loudly, instead."""
    main_cpp = os.path.join(repo.ROOT, "src", "app", "main.cpp")
    if not os.path.isfile(main_cpp):
        return _check("milestones", False, "%s not found" % main_cpp,
                      "the loop cannot localize a boot hang without it")

    with open(main_cpp, encoding="utf-8", errors="replace") as f:
        source = f.read()

    missing = [name for name, text in config.MILESTONES if text not in source]
    if missing:
        return _check(
            "milestones", False,
            "not found in src/app/main.cpp: %s" % ", ".join(missing),
            "update config.MILESTONES to match the current log lines")
    return _check("milestones", True,
                  "all %d startup milestones match main.cpp" % len(config.MILESTONES))


# --- console -----------------------------------------------------------------

def check_console(cfg, tid):
    """Every console check, or a single 'unreachable' entry."""
    api = autopilot.Autopilot(cfg)
    checks = []

    try:
        status = api.status(attempts=1)
    except (autopilot.Unreachable, autopilot.ApiError) as e:
        return [_check("autopilot", False, "%s: %s" % (cfg.url, e),
                       "power on the console and confirm sys-autopilot is "
                       "installed and its port is reachable")], False

    checks.append(_check(
        "autopilot", True,
        "%s (version %s, firmware %s, up %ss)" % (
            cfg.url, status.get("version"), status.get("firmware"),
            status.get("uptimeSeconds"))))

    if tid:
        checks.append(_check_boot2(api, tid))
        checks.append(_check_exefs(api, tid))
        checks.append(_check_process(api, tid))

    checks.append(_check_fatal_reboot(api))
    checks.append(_check_log_level(api))
    return checks, True


def _check_boot2(api, tid):
    """The crash-loop trap. `make all` creates this flag in out/, so it is a
    realistic accident -- and with it present a build that faults during
    startup takes the console down on every boot, leaving no server to upload
    the fix through."""
    path = config.boot2_flag_path(tid)
    flags = api.list_dir(config.contents_dir(tid) + "flags/")
    present = "boot2.flag" in flags
    return _check(
        "syscon_autostart", not present,
        "%s %s" % (path, "PRESENT" if present else "absent (good)"),
        "run `python tools/devtools setup-console --write` to remove it; "
        "sys-con must be started on demand, not at boot")


def _check_exefs(api, tid):
    path = config.exefs_path(tid)
    try:
        info = api.hash_file(path)
        return _check("exefs", True, "%s (%s bytes)" % (path, info.get("size")))
    except autopilot.ApiError as e:
        if e.status == 404:
            return _check("exefs", False, "%s not found" % path,
                          "run `python tools/devtools deploy`")
        raise


def _check_process(api, tid):
    running, pid = api.process_status(tid)
    return _check("process_control", True,
                  "sys-con %s" % ("running (pid %s)" % pid if running
                                  else "not running"))


def _check_fatal_reboot(api):
    """Without this, a fatal screen waits forever for a human and the loop
    stops dead. With it, a crash self-heals: fatal -> reboot -> CFW ->
    sys-autopilot answers again."""
    try:
        raw = api.read_file(config.SYSTEM_SETTINGS_PATH)
    except autopilot.ApiError as e:
        if e.status == 404:
            return _check("fatal_auto_reboot", False,
                          "%s not found" % config.SYSTEM_SETTINGS_PATH,
                          "create it with [atmosphere] "
                          "fatal_auto_reboot_interval = u64!0x2710")
        raise

    text = raw.decode("utf-8", "replace")
    m = re.search(r"^\s*fatal_auto_reboot_interval\s*=\s*u64!(0x[0-9A-Fa-f]+|\d+)",
                  text, re.M)
    if not m:
        return _check("fatal_auto_reboot", False, "not set",
                      "add `fatal_auto_reboot_interval = u64!0x2710` under "
                      "[atmosphere]; without it the console waits on the fatal "
                      "screen and the loop cannot recover unattended")

    raw_value = m.group(1)
    value = int(raw_value, 16) if raw_value.startswith("0x") else int(raw_value)
    if value == 0:
        return _check("fatal_auto_reboot", False, "set to 0 (waits forever)",
                      "set it to u64!0x2710 (10s)")
    return _check("fatal_auto_reboot", True, "%d ms" % value)


def _check_log_level(api):
    """The startup milestones are LogDebug, but the shipped config.ini ships
    log_level=3 (Info) -- at which point log.txt shows the banner and then
    silence, and a boot hang cannot be attributed to a step."""
    try:
        raw = api.read_file(config.CONFIG_PATH)
    except autopilot.ApiError as e:
        if e.status == 404:
            return _check("log_level", False,
                          "%s not found" % config.CONFIG_PATH,
                          "copy src/app/config.ini to the console")
        raise

    text = raw.decode("utf-8", "replace")
    # Only the [global] section defines log_level.
    section = re.split(r"^\s*\[", text, flags=re.M)
    level = None
    for block in section:
        if block.lower().startswith("global]"):
            m = re.search(r"^\s*log_level\s*=\s*(\d+)", block, re.M)
            if m:
                level = int(m.group(1))
            break

    if level is None:
        return _check("log_level", False, "no log_level in [global]",
                      "set log_level=0 (Trace) on the test console")
    if level != config.LOG_LEVEL_TRACE:
        names = {0: "Trace", 1: "Debug", 2: "Perf", 3: "Info", 4: "Warning",
                 5: "Error"}
        return _check(
            "log_level", False,
            "log_level=%d (%s), expected 0 (Trace)" % (level, names.get(level, "?")),
            "set log_level=0 in /config/sys-con/config.ini: above Trace the "
            "startup milestones are invisible (they are LogDebug), so a boot "
            "hang cannot be localized",
            level="warn")
    return _check("log_level", True, "log_level=0 (Trace)")


# --- entry point -------------------------------------------------------------

def run(cfg):
    checks, tid = check_local(cfg)
    console, reachable = check_console(cfg, tid)
    checks += console

    failed = [c for c in checks if c["status"] == "fail"]
    warned = [c for c in checks if c["status"] == "warn"]
    return {
        "checks": checks,
        "console_reachable": reachable,
        "failed": len(failed),
        "warnings": len(warned),
    }
