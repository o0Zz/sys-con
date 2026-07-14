# Introduction

This document provides a minimal test plan to validate sys-con before any release.
It covers the main happy-path scenarios to guarantee a minimally working version,
and it re-exercises past regressions to make sure they have not resurfaced.

**Scope:** manual validation on real hardware (Nintendo Switch) before tagging a release.

**Pre-requisites:**
- A Nintendo Switch running a compatible CFW (Atmosphère) with the sys-con build under test installed.
- SD card access (directly or through a file manager such as vgedit) to read/edit
  `/config/sys-con/config.ini` and `/config/sys-con/log.txt`.
- The set of controllers listed in the test cases below.

**Conventions:**
- Config file: `/config/sys-con/config.ini`
- Log file: `/config/sys-con/log.txt`
- Unless stated otherwise, start each test case from a clean boot with the default config.
- Each test case ends with a **If this test fails, investigate:** pointer telling a
  developer which source file(s) most likely hold the defect.

---

# Test plan

## TC1: Log file is auto-created

**Steps:**
1. Power off the Switch.
2. On the SD card, delete `/config/sys-con/log.txt`.
3. Boot the Switch.
4. Inspect `/config/sys-con/`.

**Expected result:**
`/config/sys-con/log.txt` is recreated automatically at boot with at least the
startup entries, at the default log level (Info).

**If this test fails, investigate:**
[logger.cpp](../source/Sysmodule/source/logger.cpp) (`Initialize` / `LogWriteToFile` —
directory creation and file open) and the file-manager backend
[filemanager_ams.h](../source/Sysmodule/source/filemanager_ams.h).

---

## TC2: Auto-add of an unknown controller

**Steps:**
1. Ensure `auto_add_controller=1` in the `[global]` section of `config.ini`.
2. Use a controller whose VID-PID has no `[vid-pid]` section in `config.ini`.
3. Boot the Switch and plug the controller.
4. Unplug the controller and open `config.ini`.

**Expected result:**
- The controller is usable immediately after plugging.
- A new `[vid-pid]` section for the controller is appended to `config.ini`.
- No crash, and the existing config content is preserved (not corrupted/truncated).

**If this test fails, investigate:**
[config_handler.cpp](../source/Sysmodule/source/config_handler.cpp) (auto-add path and
the `.ini` write-back) and [ini.h](../source/Ini/ini.h).

---

## TC3: Xbox 360 wired controller

**Steps:**
1. Boot the Switch.
2. Plug an Xbox 360 wired controller (e.g. VID/PID `045e-028e`).
3. Open the input test menu (Settings → Controllers & Sensors → Test Input Devices).
4. Press every button and move both sticks / triggers.

**Expected result:**
Controller connects, all buttons/sticks/triggers map correctly per the `[xbox360]` profile,
no crash.

**If this test fails, investigate:**
[Xbox360Controller.cpp](../source/ControllerLib/Controllers/Xbox360Controller.cpp).

---

## TC4: Generic HID controller

**Steps:**
1. Boot the Switch.
2. Plug a generic HID gamepad.
3. Open the input test menu and exercise all inputs.

**Expected result:**
Controller is detected as a generic HID device, dpad and sticks work, buttons are
reported. No crash.

**If this test fails, investigate:**
[GenericHIDController.cpp](../source/ControllerLib/Controllers/GenericHIDController.cpp)
(HID report-descriptor parsing) and the shared
[BaseController.cpp](../source/ControllerLib/Controllers/BaseController.cpp).

---

## TC5: Xbox 360 wireless — Plug

**Steps:**
1. Boot the Switch.
2. Plug the Xbox 360 wireless receiver (e.g. `045e-0719`).
3. Power on / sync a wireless pad to the receiver.
4. Exercise the inputs in the input test menu.

**Expected result:**
The paired pad appears as a connected controller and all inputs work. No crash.

**If this test fails, investigate:**
[Xbox360WirelessController.cpp](../source/ControllerLib/Controllers/Xbox360WirelessController.cpp)
and the device-attach path in [usb_module.cpp](../source/Sysmodule/source/usb_module.cpp).

---

## TC6: Xbox 360 wireless — Unplug

**Steps:**
1. Continue from TC5 with a working wireless pad.
2. Power off the wireless pad (or unplug the receiver).

**Expected result:**
The controller disconnects cleanly; log shows the disconnect; the system remains stable.

**If this test fails, investigate:**
The disconnect/teardown path in [usb_module.cpp](../source/Sysmodule/source/usb_module.cpp)
and [controller_handler.cpp](../source/Sysmodule/source/controller_handler.cpp).

---

## TC7: Xbox 360 wireless — Replug

**Steps:**
1. Continue from TC6.
2. Power the pad back on / replug the receiver.
3. Exercise the inputs again.

**Expected result:**
The controller reconnects and works exactly as in TC5. No crash, no duplicate entries.

**If this test fails, investigate:**
[controller_handler.cpp](../source/Sysmodule/source/controller_handler.cpp) (stale state
not released on TC6) and
[Xbox360WirelessController.cpp](../source/ControllerLib/Controllers/Xbox360WirelessController.cpp).

---

## TC8: PS4 controller (largest HID report)

**Steps:**
1. Boot the Switch.
2. Plug a DualShock 4 controller (e.g. `054c-09cc`).
3. Exercise all inputs.

**Expected result:**
Controller connects and all inputs work per the `[dualshock4]` profile. No crash,
no truncated/garbled input.

**If this test fails, investigate:**
Report/buffer sizing in
[GenericHIDController.cpp](../source/ControllerLib/Controllers/GenericHIDController.cpp)
and [ControllerConfig.h](../source/ControllerLib/ControllerConfig.h) (undersized input
buffer would truncate the large DS4 report).

---

## TC9: Missing config.ini does not crash

**Steps:**
1. Power off the Switch.
2. Delete `/config/sys-con/config.ini`.
3. Boot the Switch.
4. Plug a known controller.

**Expected result:**
- sys-con boots without crashing.
- Either a default config is regenerated or built-in defaults are used.
- A plugged controller still works with default mappings.

**If this test fails, investigate:**
[config_handler.cpp](../source/Sysmodule/source/config_handler.cpp) (missing-file
handling / fallback to defaults) and [ini.h](../source/Ini/ini.h).

---

## TC10: Make sure switch sleep/shutdown works

**Steps:**
1. Plug a controller that produces two config entries / two interfaces (to maximise the
   teardown work done on the psc thread).
2. Boot the Switch.
3. Put the Switch to **sleep** (press the power button briefly), then wake it. Repeat a few times.
4. **Restart** the Switch with the controller still connected.
5. **Shut down** the Switch with the controller still connected.

**Expected result:**
Sleep/wake, reboot and shutdown all complete cleanly with the controller(s) connected.
No crash, and on wake the controller keeps working.

**If this test fails, investigate:**
[psc_module.cpp](../source/Sysmodule/source/psc_module.cpp). On each power transition the
`PscThreadFunc` handler runs `controllers::Clear()` on `psc_thread_stack`; its teardown
chains through every connected controller plus stack-heavy `Log*()`/`vsnprintf` calls.
A crash here means `psc_thread_stack` is likely undersized.
