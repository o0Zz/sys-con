"""devtools -- drive sys-con on real hardware from the command line.

    python tools/devtools <command> [options]

Output contract: stdout carries exactly one JSON object, always, including on
failure. Progress and human-readable text go to stderr. That is what lets an
agent run `python tools/devtools iterate 2>/dev/null` and branch on the result
without parsing prose.

`ok` says whether the tool worked; `outcome` says what the experiment found.
`{"ok": true, "outcome": "CRASHED"}` is a successful run that caught a bug.
"""

import argparse
import datetime
import json
import os
import sys
import traceback

import autopilot
import build as build_mod
import config
import doctor as doctor_mod
import fences
import iterate as iterate_mod
import repo
import symbolize as symbolize_mod

EXIT = {
    "HEALTHY": 0,
    "BUILD_FAILED": 10,
    "DEPLOY_FAILED": 11,
    "BOOT_HANG": 12,
    "CRASHED": 13,
    "UNSTABLE": 14,
    "HOST_TESTS_FAILED": 20,
}
EXIT_TRANSPORT = 30
EXIT_USAGE = 40
EXIT_FENCE = 41
EXIT_NEEDS_HUMAN = 42
EXIT_INTERNAL = 70


def log(message):
    print(message, file=sys.stderr, flush=True)


def emit(envelope, exit_code):
    json.dump(envelope, sys.stdout, indent=2, default=str)
    sys.stdout.write("\n")
    sys.stdout.flush()
    return exit_code


def now():
    return datetime.datetime.now(datetime.timezone.utc).strftime(
        "%Y-%m-%dT%H:%M:%SZ")


# --- commands ----------------------------------------------------------------

def cmd_doctor(cfg, args):
    result = doctor_mod.run(cfg)
    for check in result["checks"]:
        mark = {"pass": "ok  ", "warn": "warn", "fail": "FAIL"}[check["status"]]
        log("%s %-18s %s" % (mark, check["name"], check["detail"]))
        if check["remedy"]:
            log("     -> %s" % check["remedy"])
    code = 0
    if not result["console_reachable"]:
        code = EXIT_TRANSPORT
    elif result["failed"]:
        code = EXIT_USAGE
    return result, code


def cmd_setup_console(cfg, args):
    """Applies the two console-side settings the loop depends on.

    Requires --write, and backs up anything it overwrites, because one of
    these files is the console's own Atmosphere configuration.
    """
    api = autopilot.Autopilot(cfg)
    tid = repo.title_id()
    actions = []

    if not args.write:
        return {"dry_run": True,
                "would": ["set fatal_auto_reboot_interval",
                          "remove %s" % config.boot2_flag_path(tid)],
                "note": "re-run with --write to apply"}, 0

    repo.ensure_debug_dirs()

    # 1. fatal_auto_reboot_interval
    try:
        raw = api.read_file(config.SYSTEM_SETTINGS_PATH)
        text = raw.decode("utf-8", "replace")
    except autopilot.ApiError:
        text = ""
    if text:
        backup = os.path.join(repo.CONSOLE_BACKUP_DIR,
                              "system_settings.ini.%s" % now().replace(":", ""))
        with open(backup, "w", encoding="utf-8") as f:
            f.write(text)
        actions.append({"backed_up": backup})

    if "fatal_auto_reboot_interval" in text:
        actions.append({"fatal_auto_reboot_interval": "already present, left alone"})
    else:
        if "[atmosphere]" in text:
            text = text.replace(
                "[atmosphere]",
                "[atmosphere]\nfatal_auto_reboot_interval = u64!0x2710", 1)
        else:
            text += "\n[atmosphere]\nfatal_auto_reboot_interval = u64!0x2710\n"
        api.write_file(config.SYSTEM_SETTINGS_PATH, text.encode(), tid,
                       allow_system_settings=True)
        actions.append({"fatal_auto_reboot_interval": "set to 10000 ms"})

    # 2. sys-con must not autostart
    flag = config.boot2_flag_path(tid)
    flags = api.list_dir(config.contents_dir(tid) + "flags/")
    if "boot2.flag" in flags:
        api.delete_file(flag, tid)
        actions.append({"boot2_flag": "removed"})
    else:
        actions.append({"boot2_flag": "already absent"})

    return {"actions": actions}, 0


def cmd_test(cfg, args):
    ok, detail = build_mod.run_tests(cfg)
    return {"passed": ok, "detail": detail}, 0 if ok else EXIT["HOST_TESTS_FAILED"]


def cmd_build(cfg, args):
    ok, detail = build_mod.run_build(cfg, jobs=args.jobs, clean=args.clean)
    if not ok:
        return {"built": False, "detail": detail}, EXIT["BUILD_FAILED"]
    manifest = build_mod.archive(tests_passed=False)
    log("archived build %s" % manifest["build_id"])
    return {"built": True, "build": manifest}, 0


def cmd_deploy(cfg, args):
    tid = repo.title_id()
    build_id = args.build_id or build_mod.current_build_id()
    if not build_id:
        return {"error": "no archived build; run `build` first"}, EXIT_USAGE

    manifest = build_mod.load_manifest(build_id)
    nsp = build_mod.archived_nsp(build_id)
    if not os.path.isfile(nsp):
        return {"error": "archived nsp missing: %s" % nsp}, EXIT_USAGE

    it = iterate_mod.Iteration(cfg, log)
    if not it.quiesce():
        return {"error": "sys-con did not stop"}, EXIT["DEPLOY_FAILED"]
    ok, detail = it.deploy(nsp)
    if not ok:
        return {"deployed": False, "error": detail}, EXIT["DEPLOY_FAILED"]
    build_mod.record_deploy(build_id)
    return {"deployed": True, "build_id": build_id, "sha256": detail,
            "version": (manifest or {}).get("version")}, 0


def cmd_process(cfg, args):
    api = autopilot.Autopilot(cfg)
    tid = repo.title_id()
    if args.command == "start":
        api.process_start(tid)
        ok = api.wait_running(tid, True, config.START_SETTLE)
    elif args.command == "stop":
        api.process_stop(tid)
        ok = api.wait_running(tid, False, config.STOP_SETTLE)
    elif args.command == "restart":
        api.process_restart(tid)
        ok = api.wait_running(tid, True, config.START_SETTLE)
    else:
        ok = True
    running, pid = api.process_status(tid)
    return {"titleId": tid, "running": running, "pid": pid, "settled": ok}, 0


def cmd_status(cfg, args):
    api = autopilot.Autopilot(cfg)
    status = api.status()
    running, pid = api.process_status(repo.title_id())
    return {"server": status, "syscon": {"running": running, "pid": pid}}, 0


def cmd_logs(cfg, args):
    api = autopilot.Autopilot(cfg)
    raw = api.read_file(config.LOG_PATH,
                        offset=-args.tail if args.tail else None)
    text = raw.decode("utf-8", "replace")
    log(text)
    return {"path": config.LOG_PATH, "bytes": len(raw),
            "lines": text.count("\n")}, 0


def cmd_crashes(cfg, args):
    api = autopilot.Autopilot(cfg)
    reports = api.list_dir(config.CRASH_REPORT_DIR)
    fatals = api.list_dir(config.FATAL_ERROR_DIR)
    out = {"crash_reports": reports, "fatal_errors": fatals}

    if args.pull:
        repo.ensure_debug_dirs()
        dest = os.path.join(repo.DEBUG_DIR, "crashes")
        os.makedirs(dest, exist_ok=True)
        pulled = []
        for name in reports:
            raw = api.read_file(config.CRASH_REPORT_DIR + name)
            path = os.path.join(dest, name)
            with open(path, "wb") as f:
                f.write(raw)
            pulled.append(path)
        out["pulled"] = pulled
    return out, 0


def cmd_dumps(cfg, args):
    """HID memory captures written by the MITM path, for tools/HID_Parser.

    They accumulate on the card, so --pull offers to clear them afterwards --
    but only after every file has been written locally, never before.
    """
    api = autopilot.Autopilot(cfg)
    tid = repo.title_id()
    listing = api.list_dir(config.DUMP_DIR)
    dumps = {n: s for n, s in listing.items() if n.lower().endswith(".dmp")}
    out = {"dumps": dumps}

    if args.pull and dumps:
        repo.ensure_debug_dirs()
        dest = os.path.join(repo.DEBUG_DIR, "dumps")
        os.makedirs(dest, exist_ok=True)
        pulled = []
        for name in dumps:
            raw = api.read_file(config.DUMP_DIR + name)
            path = os.path.join(dest, name)
            with open(path, "wb") as f:
                f.write(raw)
            pulled.append(path)
        out["pulled"] = pulled

        if args.delete:
            removed = []
            for name in dumps:
                api.delete_file(config.DUMP_DIR + name, tid)
                removed.append(name)
            out["deleted"] = removed

    return out, 0


def cmd_symbolize(cfg, args):
    build_id = args.build_id or build_mod.current_build_id()
    elf_path = args.elf or (build_mod.archived_elf(build_id) if build_id else repo.ELF)
    if not os.path.isfile(elf_path):
        return {"error": "no ELF at %s" % elf_path}, EXIT_USAGE

    base = int(args.module_base, 16) if args.module_base else None
    result = symbolize_mod.symbolize(args.report, elf_path, cfg.devkitpro_win,
                                     module_base=base)
    text = symbolize_mod.render(result)
    log(text)
    if args.out:
        with open(args.out, "w", encoding="utf-8") as f:
            f.write(text)
        result["symbolized_path"] = args.out
    return result, 0


def cmd_screenshot(cfg, args):
    api = autopilot.Autopilot(cfg)
    data = api.screenshot()
    out = args.out or os.path.join(repo.DEBUG_DIR, "screenshot.jpg")
    os.makedirs(os.path.dirname(out), exist_ok=True)
    with open(out, "wb") as f:
        f.write(data)
    return {"path": out, "bytes": len(data)}, 0


def cmd_iterate(cfg, args):
    envelope = iterate_mod.run(cfg, log, soak=args.soak,
                               do_build=not args.no_build,
                               do_test=not args.no_test,
                               max_retries=args.max_retries)
    code = EXIT.get(envelope.get("outcome"), EXIT_INTERNAL)
    if envelope.get("needs_human"):
        code = EXIT_NEEDS_HUMAN
    log("outcome: %s" % envelope.get("outcome"))
    return envelope, code


# --- argument parsing --------------------------------------------------------

def build_parser():
    p = argparse.ArgumentParser(prog="devtools", description=__doc__)
    p.add_argument("--format", choices=["json", "human"], default="json")
    sub = p.add_subparsers(dest="command", required=True)

    sub.add_parser("doctor", help="environment + console preflight (read-only)")

    sc = sub.add_parser("setup-console", help="apply required console settings")
    sc.add_argument("--write", action="store_true",
                    help="actually apply the changes (default: dry run)")

    sub.add_parser("test", help="host unit tests (no console)")

    b = sub.add_parser("build", help="device build + archive")
    b.add_argument("--jobs", type=int, default=4)
    b.add_argument("--clean", action="store_true")

    d = sub.add_parser("deploy", help="upload exefs.nsp and verify")
    d.add_argument("--build-id")

    for name in ("start", "stop", "restart"):
        sub.add_parser(name, help="%s sys-con" % name)
    sub.add_parser("status", help="server and sys-con status")

    lg = sub.add_parser("logs", help="read sys-con's log")
    lg.add_argument("--tail", type=int, default=8192,
                    help="bytes from the end (0 for the whole file)")

    cr = sub.add_parser("crashes", help="list/pull crash artifacts")
    cr.add_argument("--pull", action="store_true")

    dm = sub.add_parser("dumps", help="list/pull HID dumps (for tools/HID_Parser)")
    dm.add_argument("--pull", action="store_true")
    dm.add_argument("--delete", action="store_true",
                    help="with --pull, clear them from the console afterwards")

    sy = sub.add_parser("symbolize", help="symbolize a crash report")
    sy.add_argument("--report", required=True)
    sy.add_argument("--elf")
    sy.add_argument("--build-id")
    sy.add_argument("--module-base")
    sy.add_argument("--out")

    ss = sub.add_parser("screenshot", help="capture the screen")
    ss.add_argument("--out")

    it = sub.add_parser("iterate", help="one full build/deploy/run cycle")
    it.add_argument("--soak", type=int, default=config.SOAK_DEFAULT)
    it.add_argument("--max-retries", type=int, default=2)
    it.add_argument("--no-build", action="store_true")
    it.add_argument("--no-test", action="store_true")

    return p


HANDLERS = {
    "doctor": cmd_doctor,
    "setup-console": cmd_setup_console,
    "test": cmd_test,
    "build": cmd_build,
    "deploy": cmd_deploy,
    "start": cmd_process,
    "stop": cmd_process,
    "restart": cmd_process,
    "status": cmd_status,
    "logs": cmd_logs,
    "crashes": cmd_crashes,
    "dumps": cmd_dumps,
    "symbolize": cmd_symbolize,
    "screenshot": cmd_screenshot,
    "iterate": cmd_iterate,
}


def main(argv):
    args = build_parser().parse_args(argv)
    cfg = config.Config()

    envelope = {"schema": 1, "cmd": args.command, "ok": True,
                "started_at": now(), "url": cfg.url}

    try:
        result, code = HANDLERS[args.command](cfg, args)
        envelope.update(result if isinstance(result, dict) else {"result": result})
        envelope["ok"] = code == 0 or "outcome" in envelope
        envelope["exit_code"] = code
        return emit(envelope, code)

    except fences.FenceError as e:
        envelope.update(ok=False, error=str(e), error_kind="guardrail",
                        exit_code=EXIT_FENCE)
        log("REFUSED: %s" % e)
        return emit(envelope, EXIT_FENCE)

    except autopilot.Unreachable as e:
        envelope.update(ok=False, error=str(e), error_kind="unreachable",
                        exit_code=EXIT_TRANSPORT,
                        hint="is the console on, and sys-autopilot running?")
        log("console unreachable: %s" % e)
        return emit(envelope, EXIT_TRANSPORT)

    except autopilot.ApiError as e:
        envelope.update(ok=False, error=str(e), error_kind="api",
                        exit_code=EXIT_TRANSPORT)
        log("api error: %s" % e)
        return emit(envelope, EXIT_TRANSPORT)

    except repo.Fatal as e:
        envelope.update(ok=False, error=str(e), error_kind="setup",
                        exit_code=e.code)
        log("error: %s" % e)
        return emit(envelope, e.code)

    except KeyboardInterrupt:
        envelope.update(ok=False, error="interrupted", exit_code=EXIT_INTERNAL)
        return emit(envelope, EXIT_INTERNAL)

    except Exception as e:  # never let a traceback be the only output
        envelope.update(ok=False, error=repr(e), error_kind="internal",
                        traceback=traceback.format_exc(), exit_code=EXIT_INTERNAL)
        log(traceback.format_exc())
        return emit(envelope, EXIT_INTERNAL)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
