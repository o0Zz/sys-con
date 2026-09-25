#!/usr/bin/env python3
"""Drive sys-con's network controller over UDP.

Enable it first, in /config/sys-con/config.ini on the console's SD card:

    [global]
    network_controller=1
    network_controller_port=56789

then reboot. The pad appears to the console the moment the first packet arrives, and
disappears when you send one with connected=0 (or `disconnect` below).

Use it as a library:

    from networkpad import NetworkPad
    with NetworkPad("192.168.1.42") as pad:
        pad.tap("A")
        pad.stick("left", 0.0, 1.0, hold=0.5)

or from the shell:

    python networkpad.py --host 192.168.1.42 tap A
    python networkpad.py --host 192.168.1.42 --hold 2 press ZL
    python networkpad.py --host 192.168.1.42 --hold 1 stick left 0 1
    python networkpad.py --host 192.168.1.42 sequence A B DPAD_UP
    python networkpad.py --host 192.168.1.42 --hold 2 motion --gyro 0 3.14 0
    python networkpad.py --host 192.168.1.42

--hold is a top-level option, so it goes before the command.

With no command it goes interactive: the terminal is put in raw mode and your keyboard
becomes the pad -- arrows are the d-pad, WASD/ZQSD the left stick, IJKL the right one,
Enter is A and Backspace is B. Press ? there for the full map.

The wire format is defined by NetworkPadReport in
src/controllerlib/drivers/NetworkController.h, which is also where the button numbering
below comes from. The two have to agree; test_network_report_is_44_bytes pins the size.

Motion uses SDL's sensor convention (+X right, +Y up, +Z toward the player; accel in
m/s^2, gyro in rad/s). At rest the pad reports lying flat: 9.8 m/s^2 up, no rotation.
"""

import argparse
import socket
import struct
import sys
import threading
import time

MAGIC = 0x504E4353  # 'SCNP'
VERSION = 1
DEFAULT_PORT = 56789

# GamepadButton, from src/controllerlib/GamepadButton.h. The value is both the bit
# position in the packet and the pin number the [network] profile maps.
BUTTONS = {
    "X": 1,
    "A": 2,
    "B": 3,
    "Y": 4,
    "LSTICK_CLICK": 5,
    "RSTICK_CLICK": 10,
    "L": 15,
    "R": 16,
    "ZL": 17,
    "ZR": 18,
    "MINUS": 19,
    "PLUS": 20,
    "DPAD_UP": 21,
    "DPAD_RIGHT": 22,
    "DPAD_DOWN": 23,
    "DPAD_LEFT": 24,
    "CAPTURE": 25,
    "HOME": 26,
}

# UDP is lossy and the pad holds whatever it last received, so a state change that goes
# missing sticks until the next one. Sending each change a few times is cheap insurance.
DEFAULT_REPEAT = 3

STANDARD_GRAVITY = 9.80665
REST_ACCEL = (0.0, STANDARD_GRAVITY, 0.0)
NO_ROTATION = (0.0, 0.0, 0.0)

# Matches a real pad's report rate closely enough that held inputs look continuous.
FRAME_RATE_HZ = 60


def _axis_to_i16(value):
    """Clamp a -1.0..1.0 float onto the wire's int16 range."""
    value = max(-1.0, min(1.0, float(value)))
    return max(-32768, min(32767, int(round(value * 32767))))


class NetworkPad:
    def __init__(self, host, port=DEFAULT_PORT):
        self.host = host
        self.port = port
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self._held = set()
        self._sticks = {"left": (0.0, 0.0), "right": (0.0, 0.0)}
        self._accel = REST_ACCEL
        self._gyro = NO_ROTATION
        self._connected = True

    # -- context manager -------------------------------------------------------------

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()
        return False

    def close(self):
        self._socket.close()

    # -- state -----------------------------------------------------------------------

    def _mask(self):
        mask = 0
        for name in self._held:
            mask |= 1 << BUTTONS[name]
        return mask

    def _packet(self):
        lx, ly = self._sticks["left"]
        rx, ry = self._sticks["right"]
        return struct.pack(
            "<IBBBBIhhhh6f",
            MAGIC,
            VERSION,
            0,  # pad_index
            1 if self._connected else 0,
            0,  # reserved
            self._mask(),
            _axis_to_i16(lx),
            _axis_to_i16(ly),
            _axis_to_i16(rx),
            _axis_to_i16(ry),
            *self._accel,
            *self._gyro,
        )

    def send(self, repeat=DEFAULT_REPEAT):
        """Resend the current state."""
        packet = self._packet()
        for _ in range(repeat):
            self._socket.sendto(packet, (self.host, self.port))
        return self

    def _hold(self, seconds):
        """Resend at the frame rate for `seconds`, so the state survives packet loss."""
        deadline = time.monotonic() + seconds
        interval = 1.0 / FRAME_RATE_HZ
        while time.monotonic() < deadline:
            self.send(repeat=1)
            time.sleep(interval)

    # -- input -----------------------------------------------------------------------

    @staticmethod
    def _normalize(names):
        resolved = []
        for name in names:
            key = name.upper()
            if key not in BUTTONS:
                raise ValueError(
                    "unknown button %r; known: %s" % (name, ", ".join(sorted(BUTTONS)))
                )
            resolved.append(key)
        return resolved

    def press(self, *buttons, hold=0.0):
        self._held.update(self._normalize(buttons))
        self.send()
        if hold > 0:
            self._hold(hold)
        return self

    def release(self, *buttons):
        if buttons:
            self._held.difference_update(self._normalize(buttons))
        else:
            self._held.clear()
        self.send()
        return self

    def tap(self, *buttons, hold=0.1):
        self.press(*buttons)
        self._hold(hold)
        self.release(*buttons)
        return self

    def stick(self, side, x, y, hold=0.0):
        if side not in self._sticks:
            raise ValueError("side must be 'left' or 'right', got %r" % (side,))
        self._sticks[side] = (x, y)
        self.send()
        if hold > 0:
            self._hold(hold)
            self._sticks[side] = (0.0, 0.0)
            self.send()
        return self

    def motion(self, accel=REST_ACCEL, gyro=NO_ROTATION, hold=0.0):
        """Report accel (m/s^2) and gyro (rad/s); with `hold`, go back to rest afterwards."""
        self._accel = tuple(float(v) for v in accel)
        self._gyro = tuple(float(v) for v in gyro)
        self.send()
        if hold > 0:
            self._hold(hold)
            self._accel = REST_ACCEL
            self._gyro = NO_ROTATION
            self.send()
        return self

    def neutral(self):
        """Release everything, centre both sticks and put the pad back at rest."""
        self._held.clear()
        self._sticks = {"left": (0.0, 0.0), "right": (0.0, 0.0)}
        self._accel = REST_ACCEL
        self._gyro = NO_ROTATION
        self.send()
        return self

    def disconnect(self):
        """Report the pad as unplugged; the console detaches it."""
        self._connected = False
        self.send()
        return self

    def connect(self):
        self._connected = True
        self.send()
        return self

    def is_connected(self):
        return self._connected

    def set_state(self, buttons=(), left=(0.0, 0.0), right=(0.0, 0.0), repeat=1):
        """Replace the whole state in one go, for a caller that owns its own frame loop."""
        self._held = set(self._normalize(buttons))
        self._sticks = {"left": tuple(left), "right": tuple(right)}
        self.send(repeat=repeat)
        return self


# -- interactive mode --------------------------------------------------------------------

# A terminal reports key presses but never releases. Each press is published as a short
# pulse, and two presses never merge: the second is queued behind a release gap so the
# console sees two distinct presses instead of one long one -- a long one would trip the
# console's own menu auto-repeat and move the cursor several cells for a single tap.
# Holding a key still becomes a real hold, because the OS auto-repeat stream is
# recognisable: one event, the repeat delay, then a fast steady stream. Only an event that
# arrives within REPEAT_GAP *and* follows a gap at least REPEAT_DELAY long turns the press
# into a sustained one; mashing, which has no such delay, stays a run of distinct presses.
TAP_SECONDS = 0.08
RELEASE_SECONDS = 0.05
REPEAT_GAP = 0.2
REPEAT_DELAY = 0.25
MAX_QUEUE_SECONDS = 0.5
MEMORY_SECONDS = 1.2
DEFAULT_KEY_HOLD = 0.15

BTN = "btn"
AXIS = "axis"

KEYMAP = {
    "\r": (BTN, "A"),
    "\n": (BTN, "A"),
    "\x08": (BTN, "B"),
    "\x7f": (BTN, "B"),
    "x": (BTN, "X"),
    "y": (BTN, "Y"),
    "up": (BTN, "DPAD_UP"),
    "down": (BTN, "DPAD_DOWN"),
    "left": (BTN, "DPAD_LEFT"),
    "right": (BTN, "DPAD_RIGHT"),
    "1": (BTN, "L"),
    "2": (BTN, "R"),
    "3": (BTN, "ZL"),
    "4": (BTN, "ZR"),
    "-": (BTN, "MINUS"),
    "+": (BTN, "PLUS"),
    "=": (BTN, "PLUS"),
    "h": (BTN, "HOME"),
    "c": (BTN, "CAPTURE"),
    "n": (BTN, "LSTICK_CLICK"),
    "m": (BTN, "RSTICK_CLICK"),
    # QWERTY and AZERTY at once: w/z are the same physical key swapped, as are a/q.
    "w": (AXIS, "left", 1, 1.0),
    "z": (AXIS, "left", 1, 1.0),
    "s": (AXIS, "left", 1, -1.0),
    "a": (AXIS, "left", 0, -1.0),
    "q": (AXIS, "left", 0, -1.0),
    "d": (AXIS, "left", 0, 1.0),
    "i": (AXIS, "right", 1, 1.0),
    "k": (AXIS, "right", 1, -1.0),
    "j": (AXIS, "right", 0, -1.0),
    "l": (AXIS, "right", 0, 1.0),
}

HELP = """\
  arrows ......... d-pad                 w a s d / z q s d ... left stick
  Enter ... A     Backspace ... B        i j k l ............. right stick
  x ....... X     y ........... Y        n / m ............... stick clicks
  1 2 3 4 ........ L R ZL ZR             - / + ............... minus / plus
  h ....... HOME  c ........... CAPTURE

  SHIFT+letter ... latch that input on or off   space ... release everything
  , / . .......... shorter / longer repeat hold p ....... unplug / replug the pad
  [ / ] .......... stick tilt                   ? ....... this help
  Esc or Ctrl+C .. quit
"""


def _read_keys():
    """Yield one logical key name per press, with the terminal in raw mode."""
    if sys.platform == "win32":
        import msvcrt

        arrows = {"H": "up", "P": "down", "K": "left", "M": "right"}
        while True:
            char = msvcrt.getwch()
            if char in ("\x00", "\xe0"):
                yield arrows.get(msvcrt.getwch(), "")
            else:
                yield char
    else:
        import select
        import termios
        import tty

        fd = sys.stdin.fileno()
        saved = termios.tcgetattr(fd)
        arrows = {"A": "up", "B": "down", "C": "right", "D": "left"}
        try:
            tty.setcbreak(fd)
            while True:
                char = sys.stdin.read(1)
                if char != "\x1b":
                    yield char
                elif not select.select([fd], [], [], 0.05)[0]:
                    yield "\x1b"
                elif sys.stdin.read(1) != "[":
                    yield ""
                else:
                    yield arrows.get(sys.stdin.read(1), "")
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, saved)


class _Press:
    """The pulses one key owes the pad, plus what the key's event timing looks like."""

    __slots__ = ("pulses", "last", "gap", "repeating")

    def __init__(self, now):
        self.pulses = [[now, now + TAP_SECONDS]]
        self.last = now
        self.gap = 0.0
        self.repeating = False

    def sustain(self, now, seconds):
        if self.pulses:
            self.pulses[-1][1] = max(self.pulses[-1][1], now + seconds)
        else:
            self.pulses.append([now, now + seconds])

    def queue(self, now):
        start = max(now, self.pulses[-1][1] + RELEASE_SECONDS) if self.pulses else now
        if start <= now + MAX_QUEUE_SECONDS:
            self.pulses.append([start, start + TAP_SECONDS])

    def is_down(self, now):
        self.pulses = [p for p in self.pulses if p[1] > now]
        return bool(self.pulses) and self.pulses[0][0] <= now

    def is_spent(self, now):
        return not self.pulses and now - self.last > MEMORY_SECONDS


class InteractiveSession:
    """Turns key presses into a pad state that a frame thread keeps publishing."""

    def __init__(self, pad, key_hold=DEFAULT_KEY_HOLD, tilt=1.0):
        self.pad = pad
        self.key_hold = key_hold
        self.tilt = tilt
        self.running = True
        self._lock = threading.Lock()
        self._armed = {}
        self._latched = set()

    # -- state -------------------------------------------------------------------------

    def _arm(self, action):
        now = time.monotonic()
        with self._lock:
            press = self._armed.get(action)
            if press is None:
                self._armed[action] = _Press(now)
                return
            gap = now - press.last
            if gap > REPEAT_GAP:
                press.repeating = False
                press.queue(now)
            elif press.repeating or press.gap >= REPEAT_DELAY:
                press.repeating = True
                press.sustain(now, self.key_hold)
            else:
                press.queue(now)
            press.gap = gap
            press.last = now

    def _latch(self, action):
        with self._lock:
            self._latched ^= {action}

    def _clear(self):
        with self._lock:
            self._armed.clear()
            self._latched.clear()

    def _state(self):
        now = time.monotonic()
        with self._lock:
            active = {a for a, p in self._armed.items() if p.is_down(now)}
            self._armed = {a: p for a, p in self._armed.items() if not p.is_spent(now)}
            active |= self._latched
        buttons = set()
        sticks = {"left": [0.0, 0.0], "right": [0.0, 0.0]}
        for action in active:
            if action[0] == BTN:
                buttons.add(action[1])
            else:
                _, side, index, sign = action
                sticks[side][index] = max(-1.0, min(1.0, sticks[side][index] + sign * self.tilt))
        return buttons, sticks

    # -- loops -------------------------------------------------------------------------

    def _frames(self):
        interval = 1.0 / FRAME_RATE_HZ
        next_draw = 0.0
        while self.running:
            buttons, sticks = self._state()
            self.pad.set_state(buttons, sticks["left"], sticks["right"])
            now = time.monotonic()
            if now >= next_draw:
                self._draw(buttons, sticks)
                next_draw = now + 0.05
            time.sleep(interval)

    def _draw(self, buttons, sticks):
        line = "L(%+.2f,%+.2f) R(%+.2f,%+.2f)  %s  hold %.2fs  tilt %.2f  %s" % (
            sticks["left"][0],
            sticks["left"][1],
            sticks["right"][0],
            sticks["right"][1],
            "plugged" if self.pad.is_connected() else "UNPLUGGED",
            self.key_hold,
            self.tilt,
            " ".join(sorted(buttons)) or "-",
        )
        sys.stdout.write("\r" + line.ljust(110)[:110])
        sys.stdout.flush()

    def run(self):
        print("interactive pad -> %s:%d" % (self.pad.host, self.pad.port))
        print(HELP)
        frames = threading.Thread(target=self._frames, daemon=True)
        frames.start()
        try:
            for key in _read_keys():
                if not self._handle(key):
                    break
        except KeyboardInterrupt:
            pass
        finally:
            self.running = False
            frames.join(timeout=1.0)
            self._clear()
            self.pad.connect()
            self.pad.neutral()
            sys.stdout.write("\n")

    def _handle(self, key):
        """False ends the session."""
        if key in ("\x1b", "\x03"):
            return False
        if key == " ":
            self._clear()
        elif key == "?":
            sys.stdout.write("\n" + HELP)
        elif key == "p":
            self.pad.disconnect() if self.pad.is_connected() else self.pad.connect()
        elif key == ",":
            self.key_hold = max(0.05, self.key_hold - 0.05)
        elif key == ".":
            self.key_hold = min(5.0, self.key_hold + 0.05)
        elif key == "[":
            self.tilt = max(0.1, self.tilt - 0.1)
        elif key == "]":
            self.tilt = min(1.0, self.tilt + 0.1)
        elif key.isupper() and key.lower() in KEYMAP:
            self._latch(KEYMAP[key.lower()])
        elif key in KEYMAP:
            self._arm(KEYMAP[key])
        return True


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", required=True, help="the console's IP address")
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--hold", type=float, default=None, help="seconds to hold (default: 0.1 for tap, 0 otherwise)")

    sub = parser.add_subparsers(dest="command")
    parser.set_defaults(command="interactive", key_hold=DEFAULT_KEY_HOLD, tilt=1.0)

    p_tap = sub.add_parser("tap", help="press and release")
    p_tap.add_argument("buttons", nargs="+")

    p_press = sub.add_parser("press", help="press and leave held")
    p_press.add_argument("buttons", nargs="+")

    p_release = sub.add_parser("release", help="release named buttons, or all of them")
    p_release.add_argument("buttons", nargs="*")

    p_stick = sub.add_parser("stick", help="move a stick")
    p_stick.add_argument("side", choices=["left", "right"])
    p_stick.add_argument("x", type=float)
    p_stick.add_argument("y", type=float)

    p_motion = sub.add_parser("motion", help="report accelerometer and gyro values")
    p_motion.add_argument("--accel", type=float, nargs=3, default=list(REST_ACCEL), metavar=("X", "Y", "Z"),
                          help="m/s^2 (default: lying flat)")
    p_motion.add_argument("--gyro", type=float, nargs=3, default=list(NO_ROTATION), metavar=("X", "Y", "Z"),
                          help="rad/s (default: still)")

    p_seq = sub.add_parser("sequence", help="tap each button in turn")
    p_seq.add_argument("buttons", nargs="+")

    p_interactive = sub.add_parser("interactive", help="drive the pad from the keyboard")
    p_interactive.add_argument("--key-hold", type=float, default=DEFAULT_KEY_HOLD,
                               help="seconds an auto-repeat event keeps an input down")
    p_interactive.add_argument("--tilt", type=float, default=1.0,
                               help="stick deflection a direction key applies, 0..1")

    sub.add_parser("neutral", help="release everything and centre the sticks")
    sub.add_parser("disconnect", help="report the pad as unplugged")
    sub.add_parser("connect", help="report the pad as plugged in")
    sub.add_parser("buttons", help="list the button names")

    args = parser.parse_args(argv)

    if args.command == "buttons":
        for name in sorted(BUTTONS, key=lambda n: BUTTONS[n]):
            print("%-14s bit %d" % (name, BUTTONS[name]))
        return 0

    try:
        with NetworkPad(args.host, args.port) as pad:
            if args.command == "tap":
                pad.tap(*args.buttons, hold=args.hold if args.hold is not None else 0.1)
            elif args.command == "press":
                pad.press(*args.buttons, hold=args.hold or 0.0)
            elif args.command == "release":
                pad.release(*args.buttons)
            elif args.command == "stick":
                pad.stick(args.side, args.x, args.y, hold=args.hold or 0.0)
            elif args.command == "motion":
                pad.motion(args.accel, args.gyro, hold=args.hold or 0.0)
            elif args.command == "sequence":
                for button in args.buttons:
                    pad.tap(button, hold=args.hold if args.hold is not None else 0.1)
                    time.sleep(0.05)
            elif args.command == "interactive":
                InteractiveSession(pad, key_hold=args.key_hold, tilt=args.tilt).run()
            elif args.command == "neutral":
                pad.neutral()
            elif args.command == "disconnect":
                pad.disconnect()
            elif args.command == "connect":
                pad.connect()
    except ValueError as error:
        print("error: %s" % (error,), file=sys.stderr)
        return 2

    return 0


if __name__ == "__main__":
    sys.exit(main())
