"""HTTP client for sys-autopilot.

Only the endpoints this loop needs. Response shapes were read out of
sys-autopilot's source rather than guessed:

    GET    /status                  -> {"version","firmware","uptimeSeconds","keepAwake",...}
    GET    /files?path=P            -> raw bytes; &offset=-N tails the last N
    GET    /files?path=DIR/         -> {"path","entries":[{name,type,size,mtime}]}
    GET    /files/hash?path=P       -> {"path","algorithm","hash","size"}
    PUT    /files?path=P            -> 201 {"written":N,"path":P}
    DELETE /files?path=P            -> {"deleted":P}
    GET    /process?titleId=T       -> {"titleId","running":bool,"pid":"N"}
    POST   /process/{start,stop,restart} {"titleId":"T"}
    POST   /input/touch             {"x":N,"y":N,"durationMs":N}
    POST   /input/swipe             {"fromX":N,"fromY":N,"toX":N,"toY":N,...}
    POST   /power/restart           -> {"ok":true,...} then the console goes down
"""

import base64
import json
import socket
import time
import urllib.error
import urllib.parse
import urllib.request

import config
import fences


class Unreachable(Exception):
    """The console did not answer at all. Distinct from an HTTP error, because
    it is the signal the loop uses to detect a fatal screen or a reboot."""

    code = 30


class ApiError(Exception):
    """The server answered, but refused."""

    code = 30

    def __init__(self, status, message):
        super().__init__("HTTP %d: %s" % (status, message))
        self.status = status
        self.message = message


class Autopilot:
    def __init__(self, cfg):
        self.cfg = cfg
        self.url = cfg.url

    # --- plumbing ------------------------------------------------------------

    def _headers(self):
        h = {"Accept": "application/json"}
        if self.cfg.token:
            h["Authorization"] = "Bearer " + self.cfg.token
        elif self.cfg.user or self.cfg.password:
            raw = ("%s:%s" % (self.cfg.user, self.cfg.password)).encode()
            h["Authorization"] = "Basic " + base64.b64encode(raw).decode()
        return h

    def _request(self, method, path, query=None, body=None, content_type=None,
                 attempts=None, read_timeout=None):
        url = self.url + path
        if query:
            url += "?" + urllib.parse.urlencode(query)

        headers = self._headers()
        if content_type:
            headers["Content-Type"] = content_type

        attempts = attempts if attempts is not None else config.HTTP_ATTEMPTS
        timeout = read_timeout if read_timeout is not None else config.HTTP_READ

        last = None
        for attempt in range(attempts):
            req = urllib.request.Request(url, data=body, method=method,
                                         headers=headers)
            try:
                with urllib.request.urlopen(req, timeout=timeout) as resp:
                    return resp.status, resp.read()
            except urllib.error.HTTPError as e:
                # The server answered; retrying will not change its mind.
                detail = ""
                try:
                    detail = e.read().decode("utf-8", "replace")[:400]
                except Exception:
                    pass
                raise ApiError(e.code, detail or e.reason)
            except (urllib.error.URLError, socket.timeout, OSError) as e:
                last = e
                if attempt + 1 < attempts:
                    time.sleep(1.0)
        raise Unreachable("%s %s: %s" % (method, url, last))

    def _json(self, method, path, query=None, body=None, content_type=None, **kw):
        if content_type is None and body is not None:
            content_type = "application/json"
        _, raw = self._request(method, path, query=query, body=body,
                               content_type=content_type, **kw)
        if not raw:
            return {}
        try:
            return json.loads(raw.decode("utf-8", "replace"))
        except ValueError:
            raise ApiError(200, "response was not JSON: %r" % raw[:200])

    # --- server --------------------------------------------------------------

    def status(self, attempts=None, read_timeout=None):
        return self._json("GET", "/status", attempts=attempts,
                          read_timeout=read_timeout)

    def alive(self):
        """One quick probe, no retries. Used to tell a fatal screen (server
        gone) from a sysmodule that merely died (server still answering)."""
        try:
            self.status(attempts=1, read_timeout=config.HTTP_CONNECT)
            return True
        except (Unreachable, ApiError):
            return False

    def wait_until_alive(self, timeout=config.REBOOT_WAIT,
                         poll=config.REBOOT_POLL, on_tick=None):
        """Waits out a reboot. Returns the seconds waited, or None on timeout."""
        # Count real elapsed time: each alive() probe can itself burn most of
        # the poll interval, so adding the interval under-reports the wait and
        # gives up well before the timeout.
        started = time.monotonic()
        deadline = started + timeout
        while time.monotonic() < deadline:
            if self.alive():
                return time.monotonic() - started
            if on_tick:
                on_tick(time.monotonic() - started)
            time.sleep(poll)
        return None

    def screenshot(self):
        _, raw = self._request("GET", "/screenshot", read_timeout=60)
        return raw

    # --- files ---------------------------------------------------------------

    def read_file(self, path, offset=None):
        path = fences.check_read(path)
        q = {"path": path}
        if offset is not None:
            q["offset"] = str(offset)
        _, raw = self._request("GET", "/files", query=q)
        return raw

    def list_dir(self, path):
        """Returns {name: size} for files, ignoring subdirectories.

        Sizes matter: they are how a new crash report is detected. Report names
        come from the console RTC, which on a modchipped console can repeat or
        be wildly wrong, so (name, size) set-difference is the only reliable
        signal -- never mtime, never sorting by name.
        """
        path = fences.check_read(path)
        if not path.endswith("/"):
            path += "/"
        try:
            data = self._json("GET", "/files", query={"path": path})
        except ApiError as e:
            if e.status == 404:
                return {}
            raise
        out = {}
        for entry in data.get("entries", []):
            if entry.get("type") == "file":
                out[entry["name"]] = int(entry.get("size", 0))
        return out

    def hash_file(self, path):
        path = fences.check_read(path)
        return self._json("GET", "/files/hash", query={"path": path})

    def write_file(self, path, data, title_id, allow_config=False,
                   allow_system_settings=False):
        path = fences.check_write(path, title_id, allow_config=allow_config,
                                  allow_system_settings=allow_system_settings)
        # Uploads are one shot: a retry would re-send the whole body, and a
        # half-written exefs.nsp is worse than a failed deploy.
        return self._json("PUT", "/files", query={"path": path}, body=data,
                          content_type="application/octet-stream",
                          attempts=1, read_timeout=120)

    def delete_file(self, path, title_id):
        path = fences.check_delete(path, title_id)
        return self._json("DELETE", "/files", query={"path": path})

    # --- process -------------------------------------------------------------

    def process_status(self, title_id):
        d = self._json("GET", "/process", query={"titleId": title_id})
        return bool(d.get("running")), d.get("pid")

    def process_start(self, title_id):
        return self._json("POST", "/process/start",
                          body=json.dumps({"titleId": title_id}).encode())

    def process_stop(self, title_id):
        return self._json("POST", "/process/stop",
                          body=json.dumps({"titleId": title_id}).encode())

    def process_restart(self, title_id):
        return self._json("POST", "/process/restart",
                          body=json.dumps({"titleId": title_id}).encode())

    def wait_running(self, title_id, want, timeout):
        """Polls until running == want. Returns True on success."""
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                running, _ = self.process_status(title_id)
            except (Unreachable, ApiError):
                return False
            if running == want:
                return True
            time.sleep(0.25)
        return False

    # --- touch screen --------------------------------------------------------
    # hiddbg's touch auto-pilot, not sys-con's UDP pad. It is the only scripted
    # input that survives sys-con running: /input/tap drives an HDLS pad, whose
    # npad slots the MITM replaces, while the touch panel is mirrored through
    # to the intercepted process untouched.

    def input_touch(self, x, y, duration_ms=None):
        body = {"x": x, "y": y}
        if duration_ms is not None:
            body["durationMs"] = duration_ms
        return self._json("POST", "/input/touch", body=json.dumps(body).encode(),
                          attempts=1, read_timeout=config.TOUCH_READ)

    def input_swipe(self, from_x, from_y, to_x, to_y, duration_ms=None):
        body = {"fromX": from_x, "fromY": from_y, "toX": to_x, "toY": to_y}
        if duration_ms is not None:
            body["durationMs"] = duration_ms
        return self._json("POST", "/input/swipe", body=json.dumps(body).encode(),
                          attempts=1, read_timeout=config.TOUCH_READ)

    # --- power ---------------------------------------------------------------

    def power_restart(self):
        # The console goes down right after answering, so one attempt only and
        # a short read: a timeout here usually means it already rebooted.
        try:
            return self._json("POST", "/power/restart", body=b"{}",
                              attempts=1, read_timeout=10)
        except Unreachable:
            return {"ok": True, "note": "no response; console likely already down"}
