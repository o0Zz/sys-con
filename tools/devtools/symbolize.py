"""Turns an Atmosphere crash report into a symbolized stack trace.

sys-con is linked PIE at vaddr 0, so symbolization is just
`addr2line(pc - module_base)`, and creport records the module base. That last
part is what makes this automatable at all: nothing has to stop and ask a
human which address the module was loaded at.

The one rule that makes the output trustworthy: bounds-check every offset
against the ELF's image size first. addr2line never fails. Handed an address
from libnx or the kernel it prints `??`, or worse, the nearest preceding
symbol in sys-con -- a confident, wrong, actionable-looking answer. Frames
outside the image are reported as such instead.

Note on the creport text format: it is parsed leniently (labelled hex, section
headings) rather than against a fixed grammar, because the exact layout has
not yet been checked against a report from this console. `parse_crash_report`
reports what it found so a caller can tell "no frames" from "many frames".
"""

import hashlib
import os
import re
import subprocess

import elf
import repo

HEX = r"0x([0-9A-Fa-f]{6,16})"


def parse_crash_report(text):
    """Extracts what is needed from an /atmosphere/crash_reports/*.log."""
    out = {
        "program_id": None,
        "result": None,
        "module_name": None,
        "module_base": None,
        "module_end": None,
        "build_id": None,
        "frames": [],
    }

    m = re.search(r"Program ID:\s*([0-9A-Fa-f]{16})", text)
    if m:
        out["program_id"] = m.group(1).lower()

    m = re.search(r"Result:\s*(0x[0-9A-Fa-f]+)", text)
    if m:
        out["result"] = m.group(1)

    # Module list: a name, then an address range, then optionally a build ID.
    # The module we want is the one whose range contains the fault, but the
    # sys-con entry is also identifiable by name.
    for block in re.split(r"\n\s*Module\s+\d+", text)[1:]:
        name = re.search(r"Name:\s*(\S+)", block)
        rng = re.search(r"Address:\s*" + HEX + r"\s*-\s*" + HEX, block)
        bid = re.search(r"Build ID:\s*([0-9A-Fa-f]{16,40})", block)
        if not rng:
            continue
        base, end = int(rng.group(1), 16), int(rng.group(2), 16)
        is_syscon = name and "sys-con" in name.group(1).lower()
        if is_syscon or out["module_base"] is None:
            out["module_name"] = name.group(1) if name else None
            out["module_base"] = base
            out["module_end"] = end
            out["build_id"] = bid.group(1).lower() if bid else None
        if is_syscon:
            break

    # Registers worth resolving, then the stack trace itself.
    for label in ("PC", "LR"):
        m = re.search(r"\b%s:\s*" % label + HEX, text)
        if m:
            out["frames"].append({"label": label, "addr": int(m.group(1), 16)})

    m = re.search(r"Stack Trace:(.*?)(?:\n\s*\n|\Z)", text, re.S)
    if m:
        for i, addr in enumerate(re.findall(HEX, m.group(1))):
            out["frames"].append({"label": "bt%02d" % i, "addr": int(addr, 16)})

    return out


def _addr2line(elf_path, offsets, devkitpro_win):
    """One batched call for every in-image offset.

    -a is not optional: -i emits a variable number of lines per address, so
    without the echoed address the output cannot be split back apart.
    """
    if not offsets:
        return {}

    tool = repo.addr2line(devkitpro_win)
    args = [tool, "-f", "-C", "-i", "-a", "-e", elf_path]
    args += ["0x%x" % o for o in offsets]

    p = subprocess.run(args, capture_output=True, text=True, timeout=120)
    if p.returncode != 0:
        raise repo.Fatal("addr2line failed: %s" % (p.stderr or "").strip())

    resolved, current = {}, None
    pending = []
    for line in p.stdout.splitlines():
        line = line.rstrip()
        if re.match(r"^0x[0-9A-Fa-f]+$", line):
            if current is not None:
                resolved[current] = pending
            current = int(line, 16)
            pending = []
        elif current is not None:
            pending.append(line)
    if current is not None:
        resolved[current] = pending

    # addr2line alternates function name / "file:line" per inline level.
    out = {}
    for addr, lines in resolved.items():
        pairs = []
        for i in range(0, len(lines) - 1, 2):
            pairs.append((lines[i], lines[i + 1]))
        out[addr] = pairs
    return out


def symbolize(report_path, elf_path, devkitpro_win, module_base=None):
    with open(report_path, encoding="utf-8", errors="replace") as f:
        text = f.read()

    info = parse_crash_report(text)
    base = module_base if module_base is not None else info["module_base"]
    if base is None:
        raise repo.Fatal(
            "no module base in %s; pass --module-base 0xHEX (the 'Start "
            "Address' shown on the fatal screen)" % os.path.basename(report_path),
            code=40)

    image_size = elf.Elf(elf_path).max_vaddr()

    in_image, out_of_image = [], []
    for frame in info["frames"]:
        off = frame["addr"] - base
        if 0 <= off < image_size:
            in_image.append(dict(frame, offset=off))
        else:
            out_of_image.append(frame)

    syms = _addr2line(elf_path, [f["offset"] for f in in_image], devkitpro_win)

    for frame in in_image:
        frame["symbols"] = [
            {"function": fn, "location": loc}
            for fn, loc in syms.get(frame["offset"], [])
        ]

    signature, sig_hash = _signature(in_image)
    return {
        "report": os.path.basename(report_path),
        "elf": elf_path,
        "program_id": info["program_id"],
        "result": info["result"],
        "module_name": info["module_name"],
        "module_base": "0x%x" % base,
        "image_size": "0x%x" % image_size,
        "report_build_id": info["build_id"],
        "frames": in_image,
        "frames_outside_image": [
            {"label": f["label"], "addr": "0x%x" % f["addr"]} for f in out_of_image
        ],
        "signature": signature,
        "signature_hash": sig_hash,
    }


def _signature(frames):
    """A stable key for 'is this the same crash as last time?'.

    The loop compares this instead of re-reading two stack traces, and refuses
    to retry when it repeats -- the same crash twice is a code bug, and
    rebooting harder produces no new information.
    """
    for frame in frames:
        if frame.get("symbols"):
            fn = frame["symbols"][0]["function"]
            # Drop the parameter list: demangled C++ signatures run to
            # hundreds of characters, and the qualified name plus the source
            # location already identifies the frame.
            fn = fn.split("(", 1)[0].strip() or fn
            loc = os.path.basename(frame["symbols"][0]["location"])
            sig = "%s@%s" % (fn, loc)
            return sig, hashlib.sha256(sig.encode()).hexdigest()[:8]
    return None, None


def render(result):
    """The single text file a human (or agent) actually reads."""
    lines = [
        "sys-con crash report",
        "  report       %s" % result["report"],
        "  elf          %s" % result["elf"],
        "  program id   %s" % (result["program_id"] or "?"),
        "  result       %s" % (result["result"] or "?"),
        "  module base  %s   image size %s" % (result["module_base"],
                                               result["image_size"]),
        "",
    ]
    if not result["frames"]:
        lines.append("  no frames resolved inside the sys-con image")
    for frame in result["frames"]:
        lines.append("%-4s 0x%016x  ->  +0x%x" % (frame["label"], frame["addr"],
                                                  frame["offset"]))
        if not frame["symbols"]:
            lines.append("       <no debug info at this offset>")
        for i, sym in enumerate(frame["symbols"]):
            prefix = "       " if i == 0 else "       (inlined) "
            lines.append("%s%s" % (prefix, sym["function"]))
            lines.append("         at %s" % sym["location"])
    if result["frames_outside_image"]:
        lines.append("")
        lines.append("frames outside the sys-con image (libnx/kernel):")
        for frame in result["frames_outside_image"]:
            lines.append("  %-4s %s" % (frame["label"], frame["addr"]))
    lines += ["", "signature      %s" % (result["signature"] or "-"),
              "signature_hash %s" % (result["signature_hash"] or "-")]
    return "\n".join(lines) + "\n"
