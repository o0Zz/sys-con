# Sys-con

#### Connect any USB controller to your Nintendo Switch ! 
***Support any controller***: PC controllers, Wheels, Dualshock 3, Dualshock 4, Dualsense (PS5), XBOX, XBOX360, XBOXONE, ...

## Description
Sys-con is a Nintendo Switch module that adds support for all HID and XID joysticks and gamepads to the Nintendo Switch.
Only **USB** connection is supported (For Bluetooth connection prefer to use ndeadly's [MissionControl](https://github.com/ndeadly/MissionControl))

## Installation
Download the latest zip from the [releases page](https://github.com/o0zz/sys-con/releases). Extract it to your SD card root folder and boot/reboot your switch.

## Configuration
sys-con comes with a configuration folder located in `/config/sys-con/`. It contains configuration for controllers (Button mappings, sticks configuration, triggers configuration, deadzones...).

The configuration is loaded in the following way:
- The `[global]` section is only loaded once, when the switch boots, so if you want to apply a setting, you have to reboot the switch.
- Other sections are for controller configuration, they are loaded each time you plug a controller, so if you want to apply a setting you will have to unplug and then replug your controller to apply it.

When a new controller is plugged, the configuration is loaded in below order
1. First it load the `[default]` section
2. Then it will load the `[VID-PID]` section
3. If `[VID-PID]` contains a `[profile]`, it will load the `[profile]` then load `[VID-PID]`.

In other words, the loading order is: `[Default]` `[Profile]` `[VID-PID]`.
If you want to override a setting for only 1 controller, it's adviced to change the configuration in `[VID-PID]` in order to not impact others controllers

## Logs
In case of issue, you can look at the logs in `/config/sys-con/log.txt` (On your SDCard).
The logs are automatically created with a log level equal to Info.
For more verbose logs, edit `/config/sys-con/config.ini` and set:

```
[global]
log_level=0
```

Reboot the Nintendo Switch.

## Features
- [x] HID joystick/gamepad/wheels supported (PC Controller compatible)
- [x] PS and XBOXs controllers supported
- [x] Custom key mapping using VID/PID and profile.
- [x] Automatically add new controllers (Try to determine the best driver)
- [x] Configurable deadzone
- [x] Configurable polling frequency
- [x] Configurable controller color using #RGBA
- [ ] Rumble
- [ ] HID keyboard / mouse support

## Supported controller
- [x] All PC Controllers
- [x] All Playstation Controllers
- [x] All Xbox Controllers
- [x] Wheels

A complete list of tested controller is available 
[here](https://github.com/o0Zz/sys-con/blob/master/doc/TestedControllers.md)

## Configure a controller
When a new controller is connected, sys-con tries to determine the best profile for this new controller.
In most cases the dpad and joystick will work fine, the buttons may not be mapped correctly by default (and in rare cases the right stick may be reversed or not working). If this is the case, you will need to map the buttons yourself using the procedure below:

### Method 1: Configure Directly on the Switch (Recommended)

1. Connect your controller to the Nintendo Switch.

2. Open the input test menu: 
   On the Switch, go to:  
   Settings → Controllers & Sensors → Test Input Devices

3. Test each button.**
   Press every button on the controller and write down or memorize how the Switch reports each input.  
   Your goal here is to understand the correct mapping for your specific controller.

4. Edit your sys-con configuration.**
   On the SD card, open:  
   ```
   /config/sys-con/config.ini
   ```  
   Locate the section corresponding to your controller’s **VID-PID**, which looks like:  
   ```
   [vid-pid]
   ```  
   This section is usually added automatically when sys-con detects an unknown controller.  
   Most of the time, it will be at the bottom of the file.

#### Tips for Faster Iteration

The `config.ini` file is reloaded **every time you plug in a controller**.  
To speed up testing:

1. Install **vgedit** on your Switch:  
   https://github.com/vgmoose/vgedit/releases  
2. **Unplug the controller**  
3. Edit `config.ini` directly from the Switch using vgedit  
4. **Re-plug the controller** to reload the config  
5. Test your changes immediately  

This allows very quick trial-and-error cycles.

#### Typical configuration
```
[0810-0001]
B=1
A=2
Y=3
X=4
L=5
R=6
ZL=7
ZR=8
minus=9
plus=10
home=11
capture=12
rstick_left=-Rz
rstick_right=+Rz
rstick_up=+Z
rstick_down=-Z
``` 
Where ButtonName (A, B, Y, X ...) need to be assign to a ButtonID (1,2,3,4,5 ...)
Now, according to what you found in step 2, you need to match ButtonName with ButtonID.
For example, if you found that 'B' is reversed with 'A', just swap the ButtonID:
```
B=2
A=1
```

### Method 2 (From a windows PC)
__Note:__ Button mapping can differ between windows and the switch
1. Connect your controller to your PC
2. Go to 'Control Panel' > 'Device Manager' and find your USB device under 'Human Interface Devices'.
3. Double click on the device or right click and select Properties.
4. Go to the 'Details' tab and select 'Hardware IDs' to view its PID and VID. The PID/VID should look like "HID\VID_0810&PID_0001&...", which becomes: `[0810-0001]`.
5. Open "joy.cpl" (either from Win+R or directly from the Start menu). 
6. Select your controller and click on "Properties"
7. Here you should see a panel with ButtonID (1, 2, 3 ...), press the button and make a note of them (which button is assigned to which ID).
8. Now edit `/config/sys-con/config.ini` on your switch sdcard and edit your controller's vid-pid section:

```
[0810-0001]
B=3
A=2
Y=4
X=1
L=7
R=8
ZL=5
ZR=6
minus=9
plus=10
home=11
capture=12
rstick_left=-Z
rstick_right=+Z
rstick_up=+Rz
rstick_down=-Rz
```
Where ButtonID (1,2,3,4, ...) is the key ID noted in step 7.
Note: Depending to the controller, this windows procedure might not works. If the mapping is incorrect, switch to Method 1

### Button mapping

The following is the complete list of mappable buttons for the Switch controller:
```
lstick_left=
lstick_right=
lstick_up=
lstick_down=
rstick_left=
rstick_right=
rstick_up=
rstick_down=
B=
A=
Y=
X=
L=
R=
ZL=
ZR=
minus=
plus=
lstick_click=
rstick_click=
dpad_up=
dpad_down=
dpad_left=
dpad_right=
capture=
home=
simulate_<BUTTON>=
```

**Possible Values:**

 - **1 to 31**: Represent the Button ID of the controller
 - **Z, -Z, Rz, -Rz, Rx, -Rx, Ry, -Ry, Slider, -Slider, Dial, -Dial, Brake, -Brake, Accelerator, -Accelerator**: Represents the analog inputs (e.g., joystick, slider).
 - **32 to 35**:  Represents the hat switch (D-Pad) directions:
   - `32` — `dpad_up`
   - `33` — `dpad_down`
   - `34` — `dpad_left`
   - `35` — `dpad_right`

### Deadzone & Factor configuration

You can adjust the deadzone and factor for each analog input to fine-tune the controller response.
 - **Deadzone:** Defines a region around the neutral position of an analog stick where inputs are ignored, preventing unintended movements.
 - **Factor:** Adjusts the input scaling, allowing for compensation if the controller isn't reaching its full range of motion.

```
deadzone_x=20
deadzone_y=20
deadzone_z=20
deadzone_rz=20
deadzone_rx=5
deadzone_ry=5
deadzone_slider=20
deadzone_dial=20

factor_x=100
factor_y=100
factor_z=100
factor_rz=100
factor_rx=100
factor_ry=100
factor_slider=100
factor_dial=100
```

All these values are in percentages
- Typical deadzone range: 0% to 30%
- Typical Factor range: 100% to 150%

### Home & Capture shortcuts
By default, the **HOME** and **CAPTURE** can be triggered by pressing `Minus + DPAD_UP` and `Minus + DPAD_DOWN`, respectively. Minus button is often mapped to the select button.

### Simulating buttons
You can simulate buttons by combining multiple button presses.
For example:
```
simulate_L=minus+plus
simulate_A=L+R
simulate_rstick_click=ZL
```
This configuration allows you to trigger specific buttons using combinations of other buttons, offering more flexibility in custom mappings.

## Troubleshooting
For common issues a troubleshooting guide is available: [Troubleshooting](https://github.com/o0Zz/sys-con/blob/master/doc/Troubleshooting.md)

## Contribution
All contributions are welcome, you can be a simple user or developer, if you did some mapping work in the config.ini or if you have any feedback, feel free to share it in [Discussions](https://github.com/o0Zz/sys-con/discussions) or submit a [Pull request](https://github.com/o0Zz/sys-con/pulls)

## Building (For developers)

Don't download the project as ZIP as it will not copy submodules properly, prefer a git clone:
`git clone --recurse-submodules -j8 https://github.com/o0Zz/sys-con.git`

Like all other switch projects, you will need [devkitA64](https://switchbrew.org/wiki/Setting_up_Development_Environment) set up on your system.

### Setup your environment on windows:
Full procedure here: https://devkitpro.org/wiki/devkitPro_pacman

1) Add these variables to your environement system
```
DEVKITPRO=/opt/devkitpro
DEVKITARM=/opt/devkitpro/devkitARM
DEVKITPPC=/opt/devkitpro/devkitPPC
```

2) Download and install msys64: https://www.msys2.org/#installation
3) Open msys and type:
```
pacman-key --recv BC26F752D25B92CE272E0F44F7FD5492264BB9D0 --keyserver keyserver.ubuntu.com
pacman-key --lsign BC26F752D25B92CE272E0F44F7FD5492264BB9D0
wget https://pkg.devkitpro.org/devkitpro-keyring.pkg.tar.xz
pacman -U devkitpro-keyring.pkg.tar.xz
echo "[dkp-libs]" >> /etc/pacman.conf
echo "Server = https://pkg.devkitpro.org/packages" >> /etc/pacman.conf
echo "[dkp-windows]" >> /etc/pacman.conf
echo "Server = https://pkg.devkitpro.org/packages/windows/$arch/" >> /etc/pacman.conf
pacman -Syu
pacman -S switch-dev
```

### Install extra dependencies for sys-con
Open MSYS2 console from devkitA64 and type below commands:
```
pacman -S make
pacman -S git
pacman -S switch-libjpeg-turbo
pacman -S zip
pacman -S diffutils
make -C lib/libnx install
```

### Build the project with Visual Studio Code

You need to select msys as default terminal in VSCode:
```
CTRL+SHIFT+P
Select "Terminal: Select default profile"
Select "msys2"
```

Then build:
```
CTRL+SHIFT+P
Select "Tasks: Run tasks"
Select "Build Release"
```

### Build the project directly from MSYS
Open MSYS console, move to the project root directory and use one of the following commands:
- `make -j8`: Build the project
- `make clean`: Cleans the project files (but not the dependencies).
- `make mrproper`: Cleans the project files and the dependencies.
- `syscon.sh build`: Build and package sys-con (Similar to github release packages)

Output folder will be there: `out/`
For an in-depth explanation of how sys-con works, see [here](source).

### Debug the application
In order to debug the applicaiton, you can directly refer to the logs available there: `/config/sys-con/log.txt`.

## Credits
**Texita** For his contribution in controllers testings and mappings

## Support
If you want to support this work 

[![ko-fi](https://www.ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/o0Zzz)
