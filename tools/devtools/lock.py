"""A single-holder lock over the console.

Two runs sharing one console is not a race worth losing: one stops sys-con
while the other is mid-upload, and what lands in exefs.nsp is whatever the
interleaving decided. The result looks like a corrupt build rather than a
collision, which is the worst kind of bug to chase.

fcntl.flock does not exist on Windows and msvcrt.locking has its own quirks,
so this is an O_EXCL lockfile instead -- portable, and the exclusive create is
atomic on every filesystem that matters here. The tradeoff is that a process
killed hard leaves the file behind, which is what the staleness check handles.
"""

import datetime
import errno
import json
import os


class Busy(Exception):
    """Someone else holds the console."""

    code = 43


class Lock:
    def __init__(self, path, purpose="", stale_after=3600):
        self.path = path
        self.purpose = purpose
        self.stale_after = stale_after
        self.fd = None
        self.stolen_from = None

    def _describe(self):
        try:
            with open(self.path, encoding="utf-8") as f:
                return json.load(f)
        except (OSError, ValueError):
            return {}

    def _age(self, info):
        started = info.get("started_at_epoch")
        if not isinstance(started, (int, float)):
            return None
        import time
        return time.time() - started

    def acquire(self):
        os.makedirs(os.path.dirname(self.path), exist_ok=True)
        try:
            self.fd = os.open(self.path, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
        except OSError as e:
            if e.errno != errno.EEXIST:
                raise

            info = self._describe()
            age = self._age(info)

            # A crashed run should not block the rig forever, but silently
            # stealing a live lock is worse than refusing. Only take it when
            # it is provably old.
            if age is not None and age > self.stale_after:
                self.stolen_from = info
                try:
                    os.unlink(self.path)
                    self.fd = os.open(self.path,
                                      os.O_CREAT | os.O_EXCL | os.O_WRONLY)
                except OSError:
                    raise Busy("another run holds %s" % self.path)
            else:
                held = info.get("purpose") or "another run"
                since = info.get("started_at", "unknown time")
                raise Busy(
                    "the console is in use by %s (pid %s, since %s). Wait for "
                    "it, or delete %s if that process is gone."
                    % (held, info.get("pid", "?"), since, self.path))

        import time
        payload = {
            "pid": os.getpid(),
            "purpose": self.purpose,
            "started_at": datetime.datetime.now(
                datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "started_at_epoch": time.time(),
        }
        os.write(self.fd, json.dumps(payload).encode())
        return self

    def release(self):
        if self.fd is not None:
            try:
                os.close(self.fd)
            except OSError:
                pass
            self.fd = None
        try:
            os.unlink(self.path)
        except OSError:
            pass

    def __enter__(self):
        return self.acquire()

    def __exit__(self, *exc):
        self.release()
        return False
