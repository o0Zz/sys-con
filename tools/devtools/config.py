"""Settings, console-side paths, and timeouts.

Everything is overridable from the environment (SYSCON_* variables), so there
is no config file to edit and accidentally commit.
"""

import os

# --- connection --------------------------------------------------------------

DEFAULT_URL = "http://192.168.10.238:4150"


class Config:
    def __init__(self):
        self.url = os.environ.get("SYSCON_AUTOPILOT_URL", DEFAULT_URL).rstrip("/")
        self.token = os.environ.get("SYSCON_AUTOPILOT_TOKEN", "")
        self.user = os.environ.get("SYSCON_AUTOPILOT_USER", "")
        self.password = os.environ.get("SYSCON_AUTOPILOT_PASS", "")
        # devkitPro lives inside MSYS2 on this machine and `make` is not on the
        # Git Bash PATH, so the device build has to be launched through it.
        self.msys2_bash = os.environ.get(
            "SYSCON_MSYS2_BASH", r"C:\msys64\usr\bin\bash.exe")
        self.devkitpro_win = os.environ.get(
            "SYSCON_DEVKITPRO_WIN", r"C:\msys64\opt\devkitpro")


# --- console-side paths ------------------------------------------------------
# All rooted at the SD card, the way sys-autopilot's /files expects them.

LOG_PATH = "/config/sys-con/log.txt"
CONFIG_PATH = "/config/sys-con/config.ini"
DUMP_DIR = "/config/sys-con/"
CRASH_REPORT_DIR = "/atmosphere/crash_reports/"
FATAL_ERROR_DIR = "/atmosphere/fatal_errors/"
SYSTEM_SETTINGS_PATH = "/atmosphere/config/system_settings.ini"


def contents_dir(title_id):
    return "/atmosphere/contents/%s/" % title_id


def exefs_path(title_id):
    return contents_dir(title_id) + "exefs.nsp"


def boot2_flag_path(title_id):
    return contents_dir(title_id) + "flags/boot2.flag"


# --- timeouts, in seconds ----------------------------------------------------
# A full clean device build measured 13s on this machine, so BUILD is generous
# rather than tuned. REBOOT_WAIT is the one that matters: it has to outlast
# fatal's auto-reboot interval plus a cold boot.

HTTP_CONNECT = 5.0
HTTP_READ = 30.0
HTTP_ATTEMPTS = 3

CTEST = 180
BUILD = 300

STOP_SETTLE = 5      # stop -> /process reports running=false
START_SETTLE = 10    # start -> /process reports running=true
REBOOT_WAIT = 180    # fatal screen -> auto-reboot -> /status answers again
REBOOT_POLL = 5

SOAK_DEFAULT = 120
# Anything touching a stack size, thread priority or heap size deserves this
# instead: those bugs are latent and time-dependent (see psc_module.cpp:20).
SOAK_LONG = 600


# --- scripted input ----------------------------------------------------------
# sys-con's own UDP-driven virtual pad (network_controller=1 in config.ini).
# The VID/PID is synthetic -- it exists only so the pad picks up the [network]
# profile like any real device -- which is what makes the log line below a
# reliable marker that input actually reached the sysmodule.

NETWORK_PAD_PORT = 56789
NETWORK_PAD_VIDPID = "ffff-0001"
NETWORK_INIT_MARKER = "Initializing network controller"
NETWORK_PAD_PLUGGED = "Controller[%s] plugged !" % NETWORK_PAD_VIDPID

# Buttons an input smoke test presses. Deliberately harmless: no HOME (which
# would background whatever is running) and no CAPTURE (which writes to the
# album).
SMOKE_BUTTONS = ["A", "B", "X", "Y", "DPAD_UP", "DPAD_DOWN", "L", "R"]


# --- log parsing -------------------------------------------------------------
# Logger line format is "|L|HH:MM:SS.mmm|TTTTTTTT| message" (src/app/logger.cpp).

LOG_LEVEL_TRACE = 0

# Startup milestones, in order, as logged by syscon::RunApp. Kept as literal
# substrings so `doctor` can grep src/app/main.cpp and fail loudly if one is
# renamed -- otherwise a rename silently turns every run into a BOOT_HANG.
# Note "managment": the typo is in the source and must be matched as-is.
MILESTONES = [
    ("started", "SYS-CON started "),
    ("osversion", "OS version: "),
    ("config", "Initializing configuration ..."),
    ("controllers", "Initializing controllers ..."),
    ("usb", "Initializing USB stack ..."),
    ("psc", "Initializing power supply managment ..."),
]

# The last milestone before RunApp's idle loop; reaching it means "fully up".
UP_MILESTONE = "psc"
