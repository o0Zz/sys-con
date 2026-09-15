"""Prunes debug/ without throwing away anything still needed.

Named prune rather than gc because `gc` is a stdlib module, and a
built-in wins over sys.path -- `import gc` here would silently hand back
Python's garbage collector instead of this file.

Each archived build is ~7 MB of ELF, and one lands per iteration, so this is
not optional housekeeping for a rig meant to run overnight.

The one rule that matters: never delete a build an iteration still points at.
A crash report symbolized against the wrong binary produces plausible,
confident, wrong answers -- strictly worse than admitting the ELF is gone.
"""

import json
import os
import shutil

import build as build_mod
import repo


def _dir_size(path):
    total = 0
    for root, _dirs, files in os.walk(path):
        for name in files:
            try:
                total += os.path.getsize(os.path.join(root, name))
            except OSError:
                pass
    return total


def _iteration_dirs():
    if not os.path.isdir(repo.ITERATIONS_DIR):
        return []
    names = [d for d in os.listdir(repo.ITERATIONS_DIR)
             if os.path.isdir(os.path.join(repo.ITERATIONS_DIR, d))]
    # Directory names are UTC timestamps, so lexical order is chronological.
    return sorted(names)


def _build_dirs():
    if not os.path.isdir(repo.BUILDS_DIR):
        return []
    out = []
    for name in os.listdir(repo.BUILDS_DIR):
        full = os.path.join(repo.BUILDS_DIR, name)
        if not os.path.isdir(full):
            continue
        manifest = build_mod.load_manifest(name)
        out.append((name, (manifest or {}).get("built_at", ""), full))
    # Oldest first; missing built_at sorts first and is collected first.
    return sorted(out, key=lambda t: t[1])


def _build_referenced_by(iteration_names):
    """Build IDs named by the iterations we are keeping."""
    referenced = set()
    for name in iteration_names:
        meta = os.path.join(repo.ITERATIONS_DIR, name, "outcome.json")
        try:
            with open(meta, encoding="utf-8") as f:
                data = json.load(f)
        except (OSError, ValueError):
            # Unreadable outcome: assume it matters rather than guess.
            continue
        bid = (data.get("build") or {}).get("build_id")
        if bid:
            referenced.add(bid)
    return referenced


def run(keep_builds=20, keep_iterations=50, dry_run=False):
    repo.ensure_debug_dirs()

    iterations = _iteration_dirs()
    keep_iters = set(iterations[-keep_iterations:]) if keep_iterations else set(iterations)
    drop_iters = [d for d in iterations if d not in keep_iters]

    builds = _build_dirs()
    current = build_mod.current_build_id()
    protected = _build_referenced_by(sorted(keep_iters))
    if current:
        protected.add(current)

    # Newest keep_builds survive regardless; older ones go unless protected.
    keep_by_age = {name for name, _at, _p in builds[-keep_builds:]} if keep_builds else \
        {name for name, _at, _p in builds}
    drop_builds = [(name, p) for name, _at, p in builds
                   if name not in keep_by_age and name not in protected]

    freed = 0
    removed_iters, removed_builds = [], []

    for name in drop_iters:
        full = os.path.join(repo.ITERATIONS_DIR, name)
        freed += _dir_size(full)
        removed_iters.append(name)
        if not dry_run:
            shutil.rmtree(full, ignore_errors=True)

    for name, full in drop_builds:
        freed += _dir_size(full)
        removed_builds.append(name)
        if not dry_run:
            shutil.rmtree(full, ignore_errors=True)

    return {
        "dry_run": dry_run,
        "removed_iterations": removed_iters,
        "removed_builds": removed_builds,
        "kept_iterations": len(keep_iters),
        "kept_builds": len(builds) - len(removed_builds),
        "protected_builds": sorted(protected),
        "freed_bytes": freed,
        "freed_mb": round(freed / (1024 * 1024), 1),
    }
