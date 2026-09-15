"""Human-readable output for `--format human`.

The JSON envelope is the contract an agent reads; this is the same data for a
person at a terminal. It deliberately renders from the finished envelope
rather than from inside each command, so the two views can never disagree
about what happened.
"""

STATUS_MARK = {"pass": "ok  ", "warn": "warn", "fail": "FAIL"}

OUTCOME_NOTE = {
    "HEALTHY": "started, stayed up, no new crash reports",
    "CRASHED": "a crash report was produced",
    "BOOT_HANG": "never finished starting up",
    "UNSTABLE": "came up but did not behave",
    "DEPLOY_FAILED": "the build never reached the console intact",
    "BUILD_FAILED": "the device build failed",
    "HOST_TESTS_FAILED": "host tests failed; nothing was deployed",
}


def _kv(lines, label, value):
    if value not in (None, "", [], {}):
        lines.append("  %-16s %s" % (label, value))


def render(cmd, env):
    # A failed command has nothing for its own renderer to describe, and a
    # per-command view built from missing keys prints confident defaults that
    # hide the real problem ("stopped after 0 iterations: ?"). The error is
    # the output in that case.
    if env.get("error") and not env.get("ok"):
        lines = ["error: %s" % env["error"]]
        if env.get("hint"):
            lines.append("hint:  %s" % env["hint"])
        if env.get("error_kind"):
            lines.append("kind:  %s" % env["error_kind"])
        return "\n".join(lines)

    fn = _RENDERERS.get(cmd, _generic)
    try:
        return fn(env)
    except Exception:
        return _generic(env)


def _generic(env):
    lines = []
    if env.get("error"):
        lines.append("error: %s" % env["error"])
        if env.get("hint"):
            lines.append("hint:  %s" % env["hint"])
    skip = {"schema", "cmd", "ok", "started_at", "url", "exit_code",
            "error", "hint", "traceback", "error_kind"}
    for key, value in env.items():
        if key in skip:
            continue
        if isinstance(value, (dict, list)):
            _kv(lines, key, _summarize(value))
        else:
            _kv(lines, key, value)
    return "\n".join(lines) or "(no output)"


def _summarize(value):
    if isinstance(value, list):
        if not value:
            return "(none)"
        if all(not isinstance(v, (dict, list)) for v in value):
            return ", ".join(str(v) for v in value)
        return "%d entries" % len(value)
    keys = list(value)[:4]
    return "{%s%s}" % (", ".join(keys), ", ..." if len(value) > 4 else "")


def _doctor(env):
    lines = []
    for check in env.get("checks", []):
        lines.append("%s %-18s %s" % (STATUS_MARK.get(check["status"], "?"),
                                      check["name"], check["detail"]))
        if check.get("remedy"):
            lines.append("     -> %s" % check["remedy"])
    lines.append("")
    lines.append("%d failed, %d warning(s); console %s"
                 % (env.get("failed", 0), env.get("warnings", 0),
                    "reachable" if env.get("console_reachable") else "unreachable"))
    return "\n".join(lines)


def _iterate(env):
    outcome = env.get("outcome") or "?"
    lines = ["%s  -- %s" % (outcome, OUTCOME_NOTE.get(outcome, ""))]
    s = env.get("signals") or {}
    _kv(lines, "build", (env.get("build") or {}).get("build_id", "")[:16])
    _kv(lines, "version", (env.get("build") or {}).get("version"))
    _kv(lines, "milestones", ", ".join(s.get("milestones_seen", [])) or "(none)")
    if s.get("input_expected"):
        _kv(lines, "input", "sent %s; pad registered: %s"
            % (", ".join(s.get("input_sent", [])) or "nothing",
               "yes" if s.get("input_pad_registered") else "NO"))
    _kv(lines, "error lines", s.get("error_lines"))
    _kv(lines, "new reports", ", ".join(s.get("new_crash_reports", [])))
    crash = env.get("crash") or {}
    _kv(lines, "signature", crash.get("signature"))
    _kv(lines, "symbolized", crash.get("symbolized_path"))
    _kv(lines, "artifacts", env.get("artifacts_dir"))
    if env.get("needs_human"):
        lines.append("")
        lines.append("NEEDS A HUMAN: %s" % env["needs_human"])
    if env.get("error"):
        lines.append("error: %s" % env["error"])
    return "\n".join(lines)


def _loop(env):
    lines = []
    for run in env.get("runs", []):
        lines.append("  #%-3d %-18s %s" % (run["n"], run["outcome"],
                                           run.get("signature") or ""))
    lines.append("")
    lines.append("stopped after %d iteration(s): %s"
                 % (env.get("iterations", 0), env.get("stop_reason", "?")))
    return "\n".join(lines)


def _gc(env):
    lines = []
    _kv(lines, "removed iters", len(env.get("removed_iterations", [])))
    _kv(lines, "removed builds", len(env.get("removed_builds", [])))
    _kv(lines, "kept builds", env.get("kept_builds"))
    _kv(lines, "freed", "%s MB" % env.get("freed_mb"))
    if env.get("dry_run"):
        lines.append("  (dry run -- nothing was deleted)")
    return "\n".join(lines)


def _crashes(env):
    lines = []
    for name, size in (env.get("crash_reports") or {}).items():
        lines.append("  crash_report  %-42s %8s bytes" % (name, size))
    for name, size in (env.get("fatal_errors") or {}).items():
        lines.append("  fatal_error   %-42s %8s bytes" % (name, size))
    if not lines:
        lines.append("  no crash artifacts on the console")
    for path in env.get("pulled", []):
        lines.append("  pulled -> %s" % path)
    return "\n".join(lines)


_RENDERERS = {
    "doctor": _doctor,
    "iterate": _iterate,
    "loop": _loop,
    "gc": _gc,
    "crashes": _crashes,
}
