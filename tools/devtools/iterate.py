"""One full iteration: build, deploy, run, watch, classify, recover.

The ordering of the classification matters as much as the checks themselves:
evidence of an actual fault outranks missing progress, because a crash during
init and a boot hang look identical if you only watch for milestones.
"""

import datetime
import hashlib
import json
import os
import time

import autopilot
import build as build_mod
import config
import pad as pad_mod
import repo
import symbolize as symbolize_mod

OUTCOMES = ("HEALTHY", "CRASHED", "BOOT_HANG", "UNSTABLE", "DEPLOY_FAILED",
            "BUILD_FAILED", "HOST_TESTS_FAILED")


def _stamp():
    return datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def _sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


class Iteration:
    def __init__(self, cfg, log):
        self.cfg = cfg
        self.log = log
        self.api = autopilot.Autopilot(cfg)
        self.tid = repo.title_id()
        repo.ensure_debug_dirs()
        self.dir = os.path.join(repo.ITERATIONS_DIR, _stamp())
        os.makedirs(self.dir, exist_ok=True)
        os.makedirs(os.path.join(self.dir, "crash"), exist_ok=True)

    # --- steps ---------------------------------------------------------------

    def snapshot(self):
        """(name, size) of every crash artifact, so new ones can be found by
        set difference afterwards. Names come from the console RTC, which on a
        modchipped console repeats or is wildly wrong -- so never sort by name
        or mtime to find 'the latest'."""
        snap = {
            "crash_reports": self.api.list_dir(config.CRASH_REPORT_DIR),
            "fatal_errors": self.api.list_dir(config.FATAL_ERROR_DIR),
        }
        self._write_json("snapshot.json", snap)
        return snap

    def new_reports(self, before):
        out = {}
        for key, remote_dir in (("crash_reports", config.CRASH_REPORT_DIR),
                                ("fatal_errors", config.FATAL_ERROR_DIR)):
            now = self.api.list_dir(remote_dir)
            old = before.get(key, {})
            out[key] = [n for n, size in now.items()
                        if old.get(n) != size]
        return out

    def quiesce(self):
        running, _ = self.api.process_status(self.tid)
        if not running:
            return True
        self.log("stopping sys-con")
        self.api.process_stop(self.tid)
        return self.api.wait_running(self.tid, False, config.STOP_SETTLE)

    def deploy(self, nsp_path):
        """Upload, then verify with the console's own SHA-256.

        This is the defence against the worst failure mode in the whole rig:
        observing the previous build's behaviour and attributing it to the new
        code. A mismatch is DEPLOY_FAILED, never 'whatever we saw'.
        """
        local_hash = _sha256_file(nsp_path)
        with open(nsp_path, "rb") as f:
            data = f.read()

        remote = config.exefs_path(self.tid)
        self.log("uploading %d bytes -> %s" % (len(data), remote))
        self.api.write_file(remote, data, self.tid)

        info = self.api.hash_file(remote)
        if info.get("hash", "").lower() != local_hash:
            return False, "hash mismatch after upload (local %s, console %s)" % (
                local_hash[:16], str(info.get("hash"))[:16])
        return True, local_hash

    def start(self):
        self.log("starting sys-con")
        self.api.process_start(self.tid)
        return self.api.wait_running(self.tid, True, config.START_SETTLE)

    def exercise_input(self):
        """Drive sys-con's UDP pad and let the log say whether it landed.

        This is what separates "did not crash" from "actually processed
        input", and it needs no physical controller plugged into the console.
        Failures here are reported, never raised: a pad that does not answer
        is a finding about the build, not a reason to abandon the run.
        """
        try:
            host = pad_mod.host_from_url(self.cfg.url)
            self.log("driving the UDP pad: %s" % " ".join(config.SMOKE_BUTTONS))
            return {"sent": pad_mod.tap_sequence(host, config.SMOKE_BUTTONS),
                    "error": None}
        except (repo.Fatal, OSError, ValueError) as e:
            return {"sent": [], "error": str(e)}

    def observe(self, soak):
        """Watch until the soak elapses or something goes wrong.

        Returns (state, detail) where state is one of 'alive', 'died',
        'console_gone'.
        """
        deadline = time.monotonic() + soak
        while time.monotonic() < deadline:
            try:
                running, _ = self.api.process_status(self.tid)
            except autopilot.Unreachable:
                # The server itself stopped answering: fatal screen, hang, or
                # a reboot already under way.
                return "console_gone", "sys-autopilot stopped responding"
            except autopilot.ApiError as e:
                return "console_gone", str(e)

            if not running:
                return "died", "process exited during the run"
            time.sleep(1.0)
        return "alive", "survived %ds" % soak

    def wait_for_reboot(self):
        """After a fatal, the console reboots itself (fatal_auto_reboot_interval)
        and sys-autopilot comes back at boot2. The crash report was already
        written by creport before the reboot, so nothing is lost by waiting."""
        self.log("console gone; waiting up to %ds for it to come back"
                 % config.REBOOT_WAIT)
        waited = self.api.wait_until_alive(
            on_tick=lambda s: self.log("  still down after %ds" % s))
        if waited is None:
            return False
        self.log("console back after ~%ds" % waited)
        return True

    # --- harvesting ----------------------------------------------------------

    def pull_log(self):
        try:
            raw = self.api.read_file(config.LOG_PATH)
        except autopilot.ApiError:
            return None
        path = os.path.join(self.dir, "log.txt")
        with open(path, "wb") as f:
            f.write(raw)
        return path

    def milestones(self, log_path):
        if not log_path or not os.path.isfile(log_path):
            return []
        with open(log_path, encoding="utf-8", errors="replace") as f:
            text = f.read()
        return [name for name, needle in config.MILESTONES if needle in text]

    def error_lines(self, log_path):
        if not log_path or not os.path.isfile(log_path):
            return 0
        with open(log_path, encoding="utf-8", errors="replace") as f:
            return sum(1 for line in f if line.startswith("|E|"))

    def harvest_crashes(self, before, build_id):
        found = self.new_reports(before)
        results = []

        for name in found.get("crash_reports", []):
            raw = self.api.read_file(config.CRASH_REPORT_DIR + name)
            local = os.path.join(self.dir, "crash", name)
            with open(local, "wb") as f:
                f.write(raw)

            elf_path = build_mod.archived_elf(build_id) if build_id else repo.ELF
            if not os.path.isfile(elf_path):
                results.append({"report": name, "error": "no ELF to symbolize against"})
                continue
            try:
                res = symbolize_mod.symbolize(local, elf_path, self.cfg.devkitpro_win)
            except repo.Fatal as e:
                results.append({"report": name, "error": str(e)})
                continue

            out = local + ".symbolized.txt"
            with open(out, "w", encoding="utf-8") as f:
                f.write(symbolize_mod.render(res))
            res["symbolized_path"] = out
            results.append(res)

        # Fatal .bin files are pulled for the record; the .log is richer and is
        # what gets symbolized.
        for name in found.get("fatal_errors", []):
            raw = self.api.read_file(config.FATAL_ERROR_DIR + name)
            with open(os.path.join(self.dir, "crash", name), "wb") as f:
                f.write(raw)

        return found, results

    # --- plumbing ------------------------------------------------------------

    def _write_json(self, name, obj):
        with open(os.path.join(self.dir, name), "w", encoding="utf-8") as f:
            json.dump(obj, f, indent=2)

    def record(self, envelope):
        self._write_json("outcome.json", envelope)
        with open(repo.INDEX_PATH, "a", encoding="utf-8") as f:
            f.write(json.dumps({
                "at": envelope.get("started_at"),
                "outcome": envelope.get("outcome"),
                "build_id": envelope.get("build", {}).get("build_id"),
                "signature_hash": envelope.get("crash", {}).get("signature_hash"),
                "dir": os.path.relpath(self.dir, repo.ROOT).replace("\\", "/"),
            }) + "\n")


def run(cfg, log, soak=None, do_build=True, do_test=True, max_retries=2,
        exercise_input=False):
    """Runs iterations until one concludes, retrying across a crash-reboot."""
    soak = soak if soak is not None else config.SOAK_DEFAULT
    started = datetime.datetime.now(datetime.timezone.utc).strftime(
        "%Y-%m-%dT%H:%M:%SZ")

    # Establish that the console is there before anything else. Without this,
    # a console that is simply switched off looks identical to one that
    # crashed and failed to come back -- and would be reported as CRASHED,
    # sending the reader hunting a bug that does not exist. Only a
    # disappearance *after* a confirmed-alive start is a fatal.
    probe = autopilot.Autopilot(cfg)
    if not probe.alive():
        raise autopilot.Unreachable(
            "%s did not respond before the run started; the console is not "
            "reachable, which is not a crash" % cfg.url)

    it = Iteration(cfg, log)
    envelope = {"started_at": started, "outcome": None, "signals": {},
                "artifacts_dir": os.path.relpath(it.dir, repo.ROOT).replace("\\", "/")}

    # 1-2. Host tests, then the device build. A failure in either never
    # reaches the console.
    if do_test:
        ok, detail = build_mod.run_tests(cfg, os.path.join(it.dir, "test.log"))
        if not ok:
            envelope.update(outcome="HOST_TESTS_FAILED", error=detail)
            it.record(envelope)
            return envelope
    if do_build:
        ok, detail = build_mod.run_build(cfg, log_path=os.path.join(it.dir, "build.log"))
        if not ok:
            envelope.update(outcome="BUILD_FAILED", error=detail)
            it.record(envelope)
            return envelope

    manifest = build_mod.archive(tests_passed=do_test)
    build_id = manifest["build_id"]
    envelope["build"] = manifest
    nsp = build_mod.archived_nsp(build_id)

    attempt = 0
    while True:
        attempt += 1
        envelope["attempt"] = attempt

        try:
            before = it.snapshot()
            if not it.quiesce():
                envelope.update(outcome="DEPLOY_FAILED",
                                error="sys-con did not stop within %ds"
                                      % config.STOP_SETTLE)
                it.record(envelope)
                return envelope

            ok, detail = it.deploy(nsp)
            if not ok:
                envelope.update(outcome="DEPLOY_FAILED", error=detail)
                it.record(envelope)
                return envelope
            build_mod.record_deploy(build_id)

            started_ok = it.start()
            state, why = ("died", "never reported running") if not started_ok \
                else it.observe(soak)

        except autopilot.Unreachable as e:
            state, why = "console_gone", str(e)
            input_result = locals().get("input_result")
        except autopilot.ApiError as e:
            envelope.update(outcome="DEPLOY_FAILED", error=str(e))
            it.record(envelope)
            return envelope

        # The console vanished: almost always a fatal screen. Wait it out,
        # then collect the report creport wrote before the reboot.
        if state == "console_gone":
            if not it.wait_for_reboot():
                envelope.update(
                    outcome="CRASHED", error=why,
                    needs_human="console did not return within %ds; power-cycle "
                                "it, or read the SD card directly" % config.REBOOT_WAIT)
                it.record(envelope)
                return envelope

        log_path = it.pull_log()
        found, crashes = it.harvest_crashes(before, build_id)
        seen = it.milestones(log_path)

        envelope["signals"] = {
            "state": state,
            "detail": why,
            "milestones_seen": seen,
            "reached_up": config.UP_MILESTONE in seen,
            "error_lines": it.error_lines(log_path),
            "new_crash_reports": found.get("crash_reports", []),
            "new_fatal_errors": found.get("fatal_errors", []),
            "input_expected": bool(exercise_input),
        }
        if input_result is not None:
            log_text = ""
            if log_path and os.path.isfile(log_path):
                with open(log_path, encoding="utf-8", errors="replace") as f:
                    log_text = f.read()
            envelope["signals"].update({
                "input_sent": input_result["sent"],
                "input_error": input_result["error"],
                "network_module_up": config.NETWORK_INIT_MARKER in log_text,
                "input_pad_registered": config.NETWORK_PAD_PLUGGED in log_text,
            })
        if crashes:
            envelope["crash"] = crashes[0]
            envelope["crashes"] = crashes

        outcome = classify(envelope)
        envelope["outcome"] = outcome

        # A repeated signature means the last fix did not work. Retrying is
        # pure cost: the answer is a code change, not another reboot.
        sig = envelope.get("crash", {}).get("signature_hash")
        retryable = outcome in ("CRASHED", "UNSTABLE")
        if retryable and attempt <= max_retries and sig != envelope.get("_last_sig"):
            envelope["_last_sig"] = sig
            log("outcome %s (attempt %d); retrying" % (outcome, attempt))
            continue

        envelope.pop("_last_sig", None)
        it.record(envelope)
        return envelope


def classify(envelope):
    s = envelope["signals"]

    # Evidence of a real fault beats any inference from missing progress.
    if s["new_crash_reports"] or s["new_fatal_errors"]:
        return "CRASHED"
    if s["state"] == "console_gone":
        return "CRASHED"
    if s["state"] == "died":
        return "CRASHED" if s["reached_up"] else "BOOT_HANG"
    if not s["reached_up"]:
        # Alive but never finished starting up. Note this is only meaningful
        # at log_level=0; doctor warns when the console is above Trace.
        return "BOOT_HANG"
    if s["error_lines"] > 0:
        return "UNSTABLE"
    # Asked to prove input works and it did not: the sysmodule is up but not
    # doing its job, which is a finding, not a pass.
    if s.get("input_expected") and not s.get("input_pad_registered"):
        return "UNSTABLE"
    return "HEALTHY"
