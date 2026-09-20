"""Host tests, the device build, and per-build artifact archiving."""

import datetime
import hashlib
import json
import os
import shutil
import subprocess

import config
import elf
import repo


def _sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def _utcnow():
    return datetime.datetime.now(datetime.timezone.utc).strftime(
        "%Y-%m-%dT%H:%M:%SZ")


# --- host tests --------------------------------------------------------------

def run_tests(cfg, log_path=None):
    """cmake + ctest. Nothing here touches the console.

    Returns (ok, detail). A failure must stop the caller before deploying:
    pushing a build whose host tests fail wastes a hardware iteration and
    muddies the crash signal.
    """
    build_dir = os.path.join(repo.ROOT, "build")
    steps = []

    if not os.path.isfile(os.path.join(build_dir, "CMakeCache.txt")):
        steps.append(["cmake", "-B", "build", "-DCMAKE_BUILD_TYPE=Release"])
    steps.append(["cmake", "--build", "build", "--target", "SysConTests", "-j"])
    steps.append(["ctest", "--test-dir", "build", "--output-on-failure"])

    output = []
    for step in steps:
        output.append("$ " + " ".join(step))
        try:
            p = subprocess.run(step, cwd=repo.ROOT, capture_output=True,
                               text=True, timeout=config.CTEST)
        except subprocess.TimeoutExpired:
            output.append("TIMEOUT after %ds" % config.CTEST)
            _write(log_path, output)
            return False, "timed out running: " + " ".join(step)
        except OSError as e:
            output.append(str(e))
            _write(log_path, output)
            return False, "could not run %s: %s" % (step[0], e)

        output.append(p.stdout)
        output.append(p.stderr)
        if p.returncode != 0:
            _write(log_path, output)
            return False, "%s failed (exit %d)" % (step[0], p.returncode)

    _write(log_path, output)
    return True, "host tests passed"


def _write(path, chunks):
    if not path:
        return
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", errors="replace") as f:
        f.write("\n".join(c for c in chunks if c))


# --- device build ------------------------------------------------------------

def run_build(cfg, jobs=4, clean=False, log_path=None):
    """`make all` through the MSYS2 login shell, which is where devkitPro lives.

    ATMOSPHERE=0 is the shipped flavour and the one CI builds, so it is what
    the loop tests by default. SYSCON_ATMOSPHERE=1 switches to the
    libstratosphere flavour, which is the only one with a working HID MITM.
    Note `make clean` also deletes src/app/build/sys-con.elf, which is exactly
    why archiving happens immediately after a build.
    """
    ams = os.environ.get("SYSCON_ATMOSPHERE", "0") == "1"
    root = repo.to_msys_path(repo.ROOT)
    cmd = "cd %s && " % root
    if clean:
        cmd += "make clean && "
    cmd += "make all ATMOSPHERE=%d ATMOSPHERE_BUILD_ENABLED=%d -j%d" % (ams, ams, jobs)

    try:
        p = repo.run_in_msys2(cfg, cmd, timeout=config.BUILD)
    except subprocess.TimeoutExpired:
        _write(log_path, ["$ " + cmd, "TIMEOUT after %ds" % config.BUILD])
        return False, "build timed out after %ds" % config.BUILD
    except repo.Fatal:
        raise

    _write(log_path, ["$ " + cmd, p.stdout, p.stderr])
    if p.returncode != 0:
        tail = (p.stderr or p.stdout or "").strip().splitlines()[-5:]
        return False, "make failed (exit %d): %s" % (p.returncode, " / ".join(tail))

    for required in (repo.ELF, repo.NSP):
        if not os.path.isfile(required):
            return False, "build reported success but %s is missing" % required
    return True, "build ok"


# --- archiving ---------------------------------------------------------------

def archive(tests_passed):
    """Copies this build's artifacts into debug/builds/<build-id>/.

    Non-negotiable, because `make all` overwrites the ELF and crash reports
    routinely arrive an iteration late -- the console sits on a fatal screen
    until it reboots. Symbolizing against the wrong ELF yields plausible
    garbage, which is worse than no answer because it gets acted on.
    """
    repo.ensure_debug_dirs()

    binary = elf.Elf(repo.ELF)
    build_id = binary.build_id()
    if not build_id:
        # Fall back to content hash so the archive still has a stable key.
        build_id = "nobuildid-" + _sha256(repo.ELF)[:16]

    dest = os.path.join(repo.BUILDS_DIR, build_id)
    os.makedirs(dest, exist_ok=True)

    for src in (repo.ELF, repo.NSP, repo.MAP):
        if os.path.isfile(src):
            shutil.copy2(src, os.path.join(dest, os.path.basename(src)))

    manifest = {
        "build_id": build_id,
        "version": repo.version(),
        "built_at": _utcnow(),
        "elf_sha256": _sha256(repo.ELF),
        "nsp_sha256": _sha256(repo.NSP),
        "elf_max_vaddr": "0x%x" % binary.max_vaddr(),
        "host_tests_passed": bool(tests_passed),
        "deployed_at": [],
    }
    manifest.update(repo.git_state())

    with open(os.path.join(dest, "manifest.json"), "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)
    with open(os.path.join(repo.BUILDS_DIR, "current.txt"), "w",
              encoding="utf-8") as f:
        f.write(build_id + "\n")

    return manifest


def load_manifest(build_id):
    path = os.path.join(repo.BUILDS_DIR, build_id, "manifest.json")
    if not os.path.isfile(path):
        return None
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def current_build_id():
    path = os.path.join(repo.BUILDS_DIR, "current.txt")
    if not os.path.isfile(path):
        return None
    with open(path, encoding="utf-8") as f:
        return f.read().strip() or None


def record_deploy(build_id):
    manifest = load_manifest(build_id)
    if not manifest:
        return
    manifest.setdefault("deployed_at", []).append(_utcnow())
    with open(os.path.join(repo.BUILDS_DIR, build_id, "manifest.json"), "w",
              encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)


def archived_nsp(build_id):
    return os.path.join(repo.BUILDS_DIR, build_id, "sys-con.nsp")


def archived_elf(build_id):
    return os.path.join(repo.BUILDS_DIR, build_id, "sys-con.elf")
