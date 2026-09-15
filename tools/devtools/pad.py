"""Scripted input through sys-con's own UDP network controller.

sys-con can create a virtual pad that is driven over UDP (`network_controller=1`
in config.ini). The wire format lives in tools/networkpad.py, which is imported
rather than reimplemented here: the packet layout has to agree with
src/controllerlib/drivers/NetworkController.h, and two copies of that struct
would drift.

What this buys the loop is a much stronger definition of "healthy". Without it
a passing iteration only means the sysmodule did not fall over. With it the run
also proves sys-con registered a controller and processed real input -- and it
does so with no physical pad plugged into the console, which is what makes the
input path testable unattended at all.
"""

import os
import sys

import config
import repo

# networkpad.py sits in tools/, one level up from this package.
_TOOLS = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
if _TOOLS not in sys.path:
    sys.path.insert(0, _TOOLS)


def _import():
    try:
        import networkpad
        return networkpad
    except ImportError as e:
        raise repo.Fatal("could not import tools/networkpad.py: %s" % e, code=40)


def buttons():
    return sorted(_import().BUTTONS)


def open_pad(host, port=None):
    """A NetworkPad aimed at the console.

    The pad appears to the console the moment the first packet lands, and the
    matching `Controller[ffff-0001] plugged !` line in sys-con's log is how the
    loop confirms the input actually arrived.
    """
    np = _import()
    return np.NetworkPad(host, port or config.NETWORK_PAD_PORT)


def host_from_url(url):
    """Extracts the console's address from the sys-autopilot base URL, so the
    pad does not need its own separate configuration."""
    from urllib.parse import urlparse

    parsed = urlparse(url)
    if not parsed.hostname:
        raise repo.Fatal("could not read a host out of %r" % url, code=40)
    return parsed.hostname


def tap_sequence(host, names, port=None, hold=0.12, gap=0.12):
    """Taps each button in turn. Returns the names actually sent."""
    import time

    np = _import()

    # Validate the whole list before opening the socket, so a typo late in the
    # sequence cannot leave half of it already pressed on the console.
    keys = []
    for name in names:
        key = name.upper()
        if key not in np.BUTTONS:
            raise repo.Fatal(
                "unknown button %r; known: %s" % (name, ", ".join(buttons())),
                code=40)
        keys.append(key)

    sent = []
    with open_pad(host, port) as pad:
        for key in keys:
            pad.tap(key, hold=hold)
            sent.append(key)
            time.sleep(gap)
        # Leave the pad detached so it does not linger as a phantom controller
        # between iterations.
        pad.disconnect()
    return sent
