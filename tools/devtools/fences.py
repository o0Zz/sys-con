"""Path fences for everything this tool writes to or deletes from the SD card.

These are enforced, not documented. The console's boot chain lives on the same
card as the sysmodule under test, and damaging it means opening the console
again to re-flash the modchip -- so the rule is an explicit allowlist, and
anything not named here is refused.

sys-autopilot does its own `..` rejection server-side; this is the second lock,
on the client, so a bug here cannot be talked into a bad path by a redirect or
a mistyped argument.
"""

import re

import config


class FenceError(Exception):
    """Refused by the path fence. Always reported as exit code 41."""

    code = 41


# Never touched, for any reason. Ordered roughly by how bad it would be.
FORBIDDEN = [
    r"^/bootloader(/|$)",
    r"^/payload\.bin$",
    r"^/boot\.dat$",
    r"^/hekate",
    r"^/atmosphere/package3$",
    r"^/atmosphere/stratosphere\.romfs$",
    r"^/atmosphere/reboot_payload\.bin$",
    r"^/Nintendo(/|$)",
    r"^/emu(MMC|mmc)(/|$)",
    r"^/switch(/|$)",
]


def normalize(path):
    """Validates and canonicalizes a console-side path.

    Rejects the shapes that turn a relative mistake into an absolute one:
    traversal, Windows separators, and drive letters.
    """
    if not path or not isinstance(path, str):
        raise FenceError("empty path")
    if "\\" in path:
        raise FenceError("backslash in console path: %r" % path)
    if re.match(r"^[A-Za-z]:", path):
        raise FenceError("drive letter in console path: %r" % path)
    if not path.startswith("/"):
        raise FenceError("console path must be absolute: %r" % path)

    parts = [p for p in path.split("/") if p not in ("", ".")]
    if any(p == ".." for p in parts):
        raise FenceError("path traversal rejected: %r" % path)

    out = "/" + "/".join(parts)
    if path.endswith("/") and out != "/":
        out += "/"
    return out


def _forbidden(path):
    for pattern in FORBIDDEN:
        if re.search(pattern, path):
            return pattern
    return None


def _other_contents(path, title_id):
    """True for /atmosphere/contents/<someone else>/..., which includes
    sys-autopilot itself and sys-ftpd. Breaking those costs us the very
    access we are using to work."""
    m = re.match(r"^/atmosphere/contents/([0-9A-Fa-f]{16})/", path)
    return bool(m) and m.group(1).upper() != title_id.upper()


def check_read(path):
    """Reads are unrestricted apart from the shape checks: knowing what is on
    the card is never the dangerous part."""
    return normalize(path)


def check_write(path, title_id, allow_config=False, allow_system_settings=False):
    path = normalize(path)

    pattern = _forbidden(path)
    if pattern:
        raise FenceError("refusing to write %s (matches %s)" % (path, pattern))
    if _other_contents(path, title_id):
        raise FenceError(
            "refusing to write %s: it belongs to another sysmodule" % path)

    # The boot2 flag is the crash-loop trap: with it present, a build that
    # faults during startup takes the console down on every boot and there is
    # no server left to upload the fix through. `make all` creates it in out/,
    # so this is a realistic accident, not a theoretical one.
    if path == config.boot2_flag_path(title_id):
        raise FenceError(
            "refusing to create %s: sys-con must not autostart on the test "
            "console, or a startup crash will loop" % path)

    if path == config.exefs_path(title_id):
        return path
    if path == config.contents_dir(title_id) + "toolbox.json":
        return path
    if path == config.CONFIG_PATH:
        if not allow_config:
            raise FenceError(
                "refusing to overwrite %s without an explicit opt-in" % path)
        return path
    if path == config.SYSTEM_SETTINGS_PATH:
        if not allow_system_settings:
            raise FenceError(
                "refusing to write %s outside `setup-console --write`" % path)
        return path

    raise FenceError("%s is not a writable path" % path)


def check_delete(path, title_id):
    path = normalize(path)

    pattern = _forbidden(path)
    if pattern:
        raise FenceError("refusing to delete %s (matches %s)" % (path, pattern))
    if _other_contents(path, title_id):
        raise FenceError(
            "refusing to delete %s: it belongs to another sysmodule" % path)

    deletable = [
        r"^/config/sys-con/log\.txt$",
        r"^/config/sys-con/[^/]+\.dmp$",
        r"^/atmosphere/fatal_errors/[^/]+\.bin$",
        r"^/atmosphere/crash_reports/[^/]+\.log$",
        # Removing this one is the whole point of `setup-console`.
        r"^/atmosphere/contents/%s/flags/boot2\.flag$" % re.escape(title_id),
    ]
    for pattern in deletable:
        if re.match(pattern, path, re.IGNORECASE):
            return path

    raise FenceError("%s is not a deletable path" % path)
