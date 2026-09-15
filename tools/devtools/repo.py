"""Repository layout, the MSYS2 build shell, and devkitPro tool discovery."""

import functools
import os
import re
import shutil
import subprocess

import config


class Fatal(Exception):
    """An error worth reporting to the caller rather than a traceback."""

    def __init__(self, message, code=70):
        super().__init__(message)
        self.code = code


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))

MAKEFILE = os.path.join(ROOT, "Makefile")
DEBUG_DIR = os.path.join(ROOT, "debug")
BUILDS_DIR = os.path.join(DEBUG_DIR, "builds")
ITERATIONS_DIR = os.path.join(DEBUG_DIR, "iterations")
CONSOLE_BACKUP_DIR = os.path.join(DEBUG_DIR, "console-backup")
INDEX_PATH = os.path.join(DEBUG_DIR, "index.jsonl")
LOCK_PATH = os.path.join(DEBUG_DIR, ".lock")

# Build artifacts. Note these moved into build/ subdirectories in ebdc4a7; the
# .elf in particular is what every crash report has to be symbolized against.
ELF = os.path.join(ROOT, "src", "app", "build", "sys-con.elf")
MAP = os.path.join(ROOT, "src", "app", "build", "sys-con.map")
NSP = os.path.join(ROOT, "src", "app", "build", "sys-con.nsp")

AFE_PARSER = os.path.join(ROOT, "tools", "AFE_Parser.exe")


def _run(args, timeout=60):
    return subprocess.run(args, capture_output=True, text=True, timeout=timeout,
                          cwd=ROOT)


@functools.cache
def title_id():
    """The sysmodule's title ID, from the Makefile -- its single source of truth.

    Asks make first, falling back to reading the literal so this still works
    from a shell with no build toolchain -- which is the normal case here,
    since make lives inside MSYS2 and not on the Windows PATH.
    """
    try:
        out = _run(["make", "-s", "-C", ROOT, "print-title-id"]).stdout.strip()
        if re.fullmatch(r"[0-9A-Fa-f]{16}", out):
            return out.upper()
    except (OSError, subprocess.SubprocessError):
        pass

    with open(MAKEFILE, encoding="utf-8") as f:
        m = re.search(r"^TITLE_ID\s*:=\s*([0-9A-Fa-f]{16})", f.read(), re.M)
    if not m:
        raise Fatal("could not determine TITLE_ID from %s" % MAKEFILE, code=40)
    return m.group(1).upper()


@functools.cache
def version():
    """The release version string, matching `make print-version`.

    `make` is not on the PATH outside the MSYS2 shell, so the Makefile's own
    git incantation is reproduced here rather than shelling out to make just
    for a string. Keep in sync with GIT_TAG / GIT_TAG_COMMIT_COUNT at the top
    of the root Makefile.
    """
    try:
        out = _run(["make", "-s", "-C", ROOT, "print-version"]).stdout.strip()
        if out:
            return out
    except (OSError, subprocess.SubprocessError):
        pass

    newest = git("rev-list", "--tags", "--max-count=1")
    if not newest:
        return "untagged"
    tag = git("describe", "--tags", newest)
    if not tag:
        return "untagged"
    count = git("rev-list", "%s..HEAD" % newest, "--count")
    if count and count != "0":
        return "%s+%s" % (tag, count)
    return tag


def git(*args):
    try:
        return _run(["git"] + list(args)).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return ""


def git_state():
    """Identity of the tree being built.

    The commit alone is not enough: this loop runs almost entirely on
    uncommitted trees, so the diff is hashed too.
    """
    import hashlib

    sha = git("rev-parse", "--short", "HEAD")
    diff = git("diff", "HEAD")
    return {
        "git_sha": sha,
        "git_dirty": bool(diff),
        "git_diff_sha256": hashlib.sha256(diff.encode()).hexdigest() if diff else None,
    }


# --- MSYS2 -------------------------------------------------------------------

def msys2_bash(cfg):
    path = cfg.msys2_bash
    if not os.path.isfile(path):
        raise Fatal(
            "MSYS2 bash not found at %s -- the device build needs it "
            "(set SYSCON_MSYS2_BASH)" % path, code=40)
    return path


def to_msys_path(win_path):
    """C:\\dev\\sys-con -> /c/dev/sys-con"""
    p = os.path.abspath(win_path).replace("\\", "/")
    if len(p) > 1 and p[1] == ":":
        p = "/" + p[0].lower() + p[2:]
    return p


def run_in_msys2(cfg, command, timeout):
    """Runs a shell command in the MSYS2 login shell, which is where devkitPro
    and make live. A login shell (-l) is required: it is what sets DEVKITPRO."""
    return subprocess.run(
        [msys2_bash(cfg), "-lc", command],
        capture_output=True, text=True, timeout=timeout, cwd=ROOT)


# --- devkitPro ---------------------------------------------------------------

@functools.cache
def addr2line(devkitpro_win):
    """Locates aarch64-none-elf-addr2line.

    DEVKITPRO is typically unset outside the MSYS2 shell, so the known install
    location is tried too rather than failing on a missing variable.
    """
    roots = [os.environ.get("DEVKITPRO"), devkitpro_win, "/opt/devkitpro"]
    for root in roots:
        if not root:
            continue
        for ext in (".exe", ""):
            p = os.path.join(root, "devkitA64", "bin",
                             "aarch64-none-elf-addr2line" + ext)
            if os.path.isfile(p):
                return p
    found = shutil.which("aarch64-none-elf-addr2line")
    if found:
        return found
    raise Fatal("aarch64-none-elf-addr2line not found (set DEVKITPRO)", code=40)


def ensure_debug_dirs():
    for d in (DEBUG_DIR, BUILDS_DIR, ITERATIONS_DIR, CONSOLE_BACKUP_DIR):
        os.makedirs(d, exist_ok=True)
