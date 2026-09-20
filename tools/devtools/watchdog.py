"""Recovery state that outlives a single command.

The rule worth persisting is this one: when the same crash signature comes
back, retrying is pure cost. The answer is a code change, and another reboot
produces no new information. That decision is worthless if it only holds
inside one `iterate` call, because the usual way this gets run is a series of
separate invocations.

Kept as one small JSON file rather than a database: it has to be readable by a
human deciding whether the rig has gone in circles overnight.
"""

import datetime
import json
import os

import repo

MAX_HISTORY = 40


def _now():
    return datetime.datetime.now(datetime.timezone.utc).strftime(
        "%Y-%m-%dT%H:%M:%SZ")


def path():
    return os.path.join(repo.DEBUG_DIR, "watchdog.json")


def load():
    try:
        with open(path(), encoding="utf-8") as f:
            state = json.load(f)
    except (OSError, ValueError):
        state = {}
    state.setdefault("consecutive_failures", 0)
    state.setdefault("consecutive_healthy", 0)
    state.setdefault("last_signature_hash", None)
    state.setdefault("signature_streak", 0)
    state.setdefault("starts_since_boot", 0)
    state.setdefault("last_uptime", 0)
    state.setdefault("history", [])
    return state


def save(state):
    os.makedirs(repo.DEBUG_DIR, exist_ok=True)
    state["history"] = state.get("history", [])[-MAX_HISTORY:]
    with open(path(), "w", encoding="utf-8") as f:
        json.dump(state, f, indent=2)


def record(state, outcome, signature_hash=None, build_id=None):
    """Folds one iteration's result into the state and returns it."""
    if outcome == "HEALTHY":
        state["consecutive_healthy"] += 1
        state["consecutive_failures"] = 0
    else:
        state["consecutive_failures"] += 1
        state["consecutive_healthy"] = 0

    if signature_hash and signature_hash == state.get("last_signature_hash"):
        state["signature_streak"] += 1
    elif signature_hash:
        state["signature_streak"] = 1
    else:
        state["signature_streak"] = 0
    state["last_signature_hash"] = signature_hash

    state["history"].append({
        "at": _now(),
        "outcome": outcome,
        "signature_hash": signature_hash,
        "build_id": build_id,
    })
    return state


def should_retry(state, outcome, signature_hash):
    """Whether another attempt at the same thing can tell us anything new.

    Returns (retry, reason).
    """
    if outcome not in ("CRASHED", "UNSTABLE"):
        return False, "outcome is not retryable"
    if signature_hash and signature_hash == state.get("last_signature_hash"):
        return False, ("same crash signature as the previous run (%s); this is "
                       "a code bug, not a flaky console" % signature_hash)
    return True, "new failure, worth one more attempt"


def reset():
    state = {"consecutive_failures": 0, "consecutive_healthy": 0,
             "last_signature_hash": None, "signature_streak": 0, "history": []}
    save(state)
    return state
