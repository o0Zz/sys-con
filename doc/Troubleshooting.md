# Troubleshooting guide

## My controller buttons are not mapped correctly
It's expected, you need to map the buttons by yourself - There are thousands of controllers in the world, it's not possible for sys-con to map all of them.
This means, if you see that your controller works partially (stick/joystick works) but the buttons are not mapped correctly, you need to update the button mapping yourself.
See the [README](https://github.com/o0Zz/sys-con?tab=readme-ov-file#configure-a-controller) for more details.

## My controller right sticks axis are reversed (Or don't works at all)
If your right joystick doesn't behave as it should (for example, you press up and it goes right) - Or don't works at all.
This means you need to map your joystick as well.
See the [README](https://github.com/o0Zz/sys-con?tab=readme-ov-file#configure-a-controller) 

And especially these values:
```
[vid-pid]
rstick_left=-Z
rstick_right=+Z
rstick_up=+Rz
rstick_down=-Rz
```
Where rstick_xxxx could be: Z, -Z, Rz, -Rz, Rx, -Rx, Ry, -Ry, Slider, -Slider, Dial, -Dial, Brake, -Brake, Accelerator, -Accelerator (try all combinations to find the right one)

##  ZL/ZR don't work or can't be bound
First, remove any existing ZL/ZR bindings from your [vid-pid] section.
In most cases, these triggers are internally associated with Rz or -Rz, and sys-con binds them automatically.

If that doesn’t work, try binding them manually to regular buttons (numbers 0–15) to check if they’re detected as digital inputs.

If they still don’t respond, it likely means that ZL/ZR are mapped as analog inputs. In that case, you will need to test them using analog axes until you find the correct mapping.

Example:
```
[vid-pid]
zl=Brake
zr=Accelerator
```
Possible analog inputs include:
`Z, -Z, Rz, -Rz, Rx, -Rx, Ry, -Ry, Slider, -Slider, Dial, -Dial, Brake, -Brake, Accelerator, -Accelerator`

Try these combinations to determine which ones correspond to your controller’s ZL and ZR triggers.

## My Xbox One S/X Controller is not detected or takes ~1 minute to be detected (Dock Mode)
If your Xbox One S/X controller is not being detected by the Switch or takes a long time (~1 minute) to connect, please follow the guidance below.
 - The controller must be powered on before it is connected to the Switch. If not, it will not be detected.
 - When using the controller in dock mode, the Switch may take approximately 1 minute to detect it.
 - To reduce detection time, consider using a USB-C to USB-C adapter and connecting the controller in handheld mode.
(For additional context, refer to Issue #66.)

Recommended procedure to ensure proper detection:
 1. Disconnect the controller from the Switch
 2. Power off the controller
 3. Turn the controller back on
 4. Connect the controller to the Switch (Ensure the Switch is wakeup)
 5. Wait up to 1 minute for the controller to be recognized

## I got error: "Failed to acquire USB interface - Error: 0x25A8C ..."
This error occur when two drivers try to acquire the controller.
Most of the time, this issue occured when you plug an official switch controller and the switch itself try to open it.
To fix the problem:

1. Edit the `/config/sys-con/config.ini` and change `discovery_mode=0` to `discovery_mode=2`
2. Reboot the switch.

(This should solve your issue, but it will only detect Xbox controllers and any controllers listed in `discovery_vidpid=`)

## I got error: "Failed to initialize controller: Error: 0x6B ..."
This error happens when your USB device is not an HID controller. 
For example, it can occur if you plug in a USB audio device instead of a controller.
This is because Orizon OS’s USB filter is not powerful enough to handle filter-out some peripherals correctly.
To fix it:

1. Edit /config/sys-con/config.ini and change `discovery_mode=0` to `discovery_mode=1`
2. Reboot the Switch.

(This should solve your issue, but it will only detect Xbox controllers and any controllers listed in `discovery_vidpid=`)

## My controller don't have Home or Capture button, how to simulate home or capture button ?
By default, all controllers are configured to simulate home and capture with following binding: Minus+DPAD_UP for capture and Minus+DPAD_DOWN for home.
If you want to change it, consider updating the configuration file, edit your controller and add `simulate_home=` `simulate_capture=` followed by the buttons you want to use to simulate it.

Example:
```
[vid-pid]
simulate_capture=minus+dpad_up
simulate_home=minus+dpad_down

[vid-pid]
simulate_home=plus+dpad_up
simulate_capture=plus+dpad_down
```

## My XBOX360 wireless controller with USB cable is not detected 
The Xbox 360 Wireless Controller USB cable supplies power to the controller, not USB data.
That's why it's not detected. It will behave the same way on your PC.
To use an XBOX360 wireless controller with the switch, you need to buy this adapter (or equivalent)
https://www.amazon.com/Mcbazel-Wireless-Receiver-Microsoft-Xbox-360/dp/B076GZFLR3/

## My controller is detected but don't works at all
This probably means that your controller is not configure with the correct driver.
In the `/config/sys-con/config.ini`, you need to find your controller (\[VID/PID\]) and edit the `driver=` key with one of the following:
 - generic (default if nothing is set)
 - xbox360
 - xbox
 - xboxone
 - dualshock4
 - switch

Typically, if you know your controller is an xboxone controller, just add
```
[vid-pid]
driver=xboxone
```

## My Nintendo Switch crash during boot
If you see a crash with Program ID: 690000000000000D, it means something is wrong with sys-con.
Most of the time it's one of below reasons:

1) You selected the wrong build: Try to use the build that match your atmosphere version.
2) Something is wrong with your configuration file. Even if sys-con try to be as robust as possible, you might have an issue with your configuration. Try to restore the default one.
3) You have the latest version of atmosphere (Not yet supported by sys-con) - Open a ticket.
4) All other cases - Open a ticket

## My official Switch controller don't works when sys-con is enabled
This problem occurs because sys-con detects all connected USB devices and try to map them.
So you need to configure sys-con to detect only specific controllers.

1. Edit the `/config/sys-con/config.ini` and change `discovery_mode=0` to `discovery_mode=1`.
2. Update `discovery_vidpid=` to add the controllers you want to discover (Other than the official switch ones). 
3. Reboot the switch.

## I want to bind a button to multiple controller buttons 
It's possible to bind a button (Example: X) to multiple controller button (Example: 13 and 2):

```
[vid-pid]
x=13,2
```

## I want to bind a button to dpad or stick
It's possible to map a button (Example: A) to dpad/sticks/...

```
[vid-pid]
a=32
```
Note: 32 is a magic value representing dpad_up

```
[vid-pid]
zr=X
zl=-X
```
Note: The button is considered pressed only on positive value.

It's also possible to combinate analog and controller button:

```
[vid-pid]
x=13,2,+X
```

## I don't see any logs in /config/sys-con/log.txt
If you don't see any logs in /config/sys-con/log.txt, it indicates that sys-con isn't loading correctly. Sys-con should automatically generate a log entry upon boot, similar to the following:
```
|I|00:00:08.383|5E953330| SYS-CON started 1.3.0+6-411aff0 (Build date: Aug 26 2024 22:22:53)
```

1. Verify Installation Files: Ensure that the following files are installed correctly:
```
atmosphere\contents\690000000000000D\toolbox.json
atmosphere\contents\690000000000000D\exefs.nsp
atmosphere\contents\690000000000000D\flags\boot2.flag
```
2. Check for Conflicting Tools: Confirm that no other tool is blocking sys-con from running.

**Important Note:** During an upgrade, you may need to disable sysmodules. If you have disabled them, remember to re-enable sys-con using [ovl-sysmodules](https://github.com/WerWolv/ovl-sysmodules).

## Stopping sys-con with ovl-sysmodule does not restore memory and stops working after 3 attempts
This is a known limitation of Horizon OS and ovl-sysmodule. 
The issue arises because ovl-sysmodule terminates the software without allowing it to clean up resources properly. 
This leads to the following side effects:
1. **Resource Release Failure:** sys-con is unable to fully release the resources it has allocated.
2. **System Call Rejection:** After three attempts, Horizon OS rejects the `hiddbgAttachHdlsWorkBuffer` system call, preventing sys-con from loading.

To resolve this issue, you must restart the Switch after three attempts to restore functionality.

## My Controller isn't detected — What should I do?
1) Use a Direct USB-C OTG Connection: Connect your controller directly to the Switch using a USB-C OTG cable, bypassing the Dock. Some controllers may not works when connected through a USB hub, such as the Switch Dock.
2) Test with another officially supported controller: Try using a different controller that is officially supported. This will help you determine whether the issue is with your Switch setup (Broken USB-C, ...) or your controller.
3) Try to use a Powered Hub. Sometimes the switch is not powerfull enough to power-on your controller (Especially with the Dock)
4) Enable and check logs: Turn on logging in Trace mode by following the instructions [here](https://github.com/o0Zz/sys-con?tab=readme-ov-file#logs). Carefully review the logs for any errors or clues about what might be causing the problem.

A typically working flow will look like:

```
|I|00:00:08.362|5E953330| -----------------------------------------------------
|I|00:00:08.383|5E953330| SYS-CON started 1.3.0+6-411aff0 (Build date: Aug 26 2024 22:22:53)
|I|00:00:08.397|5E953330| OS version: 17.0.1 (Built with Atmosphere 1.7.x-0e4ac884)
|D|00:00:08.572|5E953330| Initializing controllers ...
|D|00:00:08.590|5E953330| Polling frequency: 500 ms
|D|00:00:08.606|5E953330| Initializing USB stack ...
|I|00:00:08.625|5E953330| USB configuration: Discovery mode(0), Auto add controller(true)
|D|00:00:08.649|5E953330| Adding event with filter: XBOX (1/3)...
|D|00:00:08.664|5E953330| Adding event with filter: USB_CLASS_HID (2/3)...
|D|00:00:08.679|5E953330| Initializing power supply managment ...
|D|00:00:08.719|5E94C0D0| New USB device detected (Or polling timeout), checking for controllers ...
|D|00:00:08.930|5E94C0D0| No HID or XBOX interfaces found !
|D|00:00:36.401|5E94C0D0| New USB device detected (Or polling timeout), checking for controllers ...
|I|00:00:36.417|5E94C0D0| Trying to initialize USB device: [057e-2009] (Class: 0x00, SubClass: 0x00, Protocol: 0x00, bcd: 0x0200)...
|D|00:00:36.434|5E94C0D0| Loading controller config: 'sdmc:/config/sys-con/config.ini' [default] ...
|D|00:00:36.723|5E94C0D0| Loading controller config: 'sdmc:/config/sys-con/config.ini' [057e-2009] ...
|D|00:00:37.013|5E94C0D0| Loading controller config: 'sdmc:/config/sys-con/config.ini' (Profile: [switch]) ... 
|I|00:00:37.572|5E94C0D0| Controller successfully loaded (B=3, A=4, Y=1, X=2, ...) !
|I|00:00:37.589|5E94C0D0| Initializing Switch (Interface count: 1) ...
|D|00:00:37.603|5E94C0D0| Controller[057e-2009] Created !
|D|00:00:37.618|5E94C0D0| SwitchHDLHandler[057e-2009] Initializing ...
|D|00:00:37.630|5E94C0D0| Controller[057e-2009] Initializing ...
|D|00:00:37.647|5E94C0D0| Controller[057e-2009] Opening interfaces ...
|D|00:00:37.685|5E94C0D0| Controller[057e-2009] Opening interface idx=0 ...
|D|00:00:37.703|5E94C0D0| SwitchUSBInterface[057e-2009] Openning ...
|D|00:00:37.719|5E94C0D0| SwitchUSBInterface[057e-2009] Input endpoint found 0x81 (Idx: 0)
|D|00:00:37.730|5E94C0D0| SwitchUSBInterface[057e-2009] Output endpoint found 0x1 (Idx: 0)
|D|00:00:37.745|5E94C0D0| SwitchUSBEndpoint Opening 0x81 (Pkt size: 64)...
|D|00:00:37.767|5E94C0D0| SwitchUSBEndpoint successfully opened!
|D|00:00:37.785|5E94C0D0| SwitchUSBEndpoint Opening 0x1 (Pkt size: 64)...
|D|00:00:37.800|5E94C0D0| SwitchUSBEndpoint successfully opened!
|D|00:00:37.813|5E94C0D0| Controller[057e-2009] successfully opened !
```

Search for logs starting with `|E|`, If you find one, this is an error and it might give you a hint about the issue.

## How to attach a logs to a ticket ?
- Edit `/config/sys-con/config.ini`
- change `log_level=` to `log_level=0`
- Reboot your console
- Wait for the console to boot
- Connect your controllers
- Wait for the issue
- Download the logs from `/config/sys-con/log.txt`

**Note**: `log_level=0` introduces significant input lag. Use it only for debugging.