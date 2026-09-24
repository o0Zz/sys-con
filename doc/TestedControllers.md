# List of controllers tested with sys-con
| Controller name         | Supported | Rumble | Motion | Comment | Link |
|-------------------------|-----------|--------|--------|---------|------|
| 8BitDo Ultimate 2C | Yes | Yes | No | Might be long to dectect (Up to 2min) - Might don't works with Dock (See [#78](https://github.com/o0Zz/sys-con/issues/78)) | - |
| 8bitdo Ultimate 2C Wireless Dongle | Yes | No | No | - | - |
| 8BitDo Wireless Bluetooth USB Adapter 2 | Yes | No | No | You need to use Dinput mode. See "Manual setting" in https://support.8bitdo.com/Manual/USB-Adapter-2/xbox-switch.html | - |
| 8BitDo Ultimate C 2.4g Wireless Controller  | No | - | - | Not detected: [#21](https://github.com/o0Zz/sys-con/issues/21) | - |
| 8BitDo Ultimate 2.4g Wireless Controller  | Yes | No | No | You need to use Dinput mode. Controller has a physical switch [#91](https://github.com/o0Zz/sys-con/issues/91) | - |
| Dualshock 4 | Yes | Yes | No | - | - |
| Dualshock 3 | Yes | Yes | No | - | - |
| Dualsense (PS5) | Yes | Yes | No | - | - |
| Hori battlepad | Yes | No | No | - | - |
| Hori Real Arcade Pro 4 | Yes | No | No | - | https://www.amazon.com/HORI-Real-Arcade-Pro-PlayStation-4/dp/B00SULMRI4 |
| PDP Wave Afterglow Xbox Series | Yes | Yes | No | - | - |
| PDP Neon White Xbox One | Yes | Yes | No | - | - |
| MSI GC20 | Yes | No | No | - | - |
| MSI GC30 v2 | Yes | 2.4G only | No | - | - |
| Thrustmaster T150 Pro (Wheel) | Yes | No | No | - | - |
| Thrustmaster Dual Analog 4 | Yes | No | No | - | - |
| Xbox Original | Yes | Yes | No | - | - |
| Xbox 360 Controller | Yes | Yes | No | - | - |
| Xbox 360 Wireless adapter (Up to 4 controller) (045e-0291, 045e-0719) | Yes | Yes | No | - | - |
| Xbox One X/S Controller | Yes | Yes | No | - | - |
| Xbox One Phantom White DPD | Yes | Yes | No | - | - |
| Y3 Arcade Fighter Stick (DATA FROG)  | Yes | No | No | - | - |
| Y3 Arcade Fighter Stick with GAME STICK 4K LITE SEGAM M8 V7.0 | Yes | No | No | - | - |
| Y3 Joypad with Receiver no-name | Yes | No | No | - | - |
| Y3 Joypad with GAME STICK 4K LITE SEGAM M8 V4.0 | Yes | No | No | - | - |
| Y3 Joypad (Receiver YC5)| No | - | - | Not detected: [#30](https://github.com/o0Zz/sys-con/issues/30) | - |
| Atomic PSX/PS2 adapter | Yes | No | No | - | - |
| Cosmic Byte Xbox360 | Yes | Yes | No | You need to set `discovery_mode=1` | - |
| Activbb X6-34U Controller | Yes | No | No | - | - |
| Foyu Controller | Yes | No | No | - | - |
| Gamecube adapter (HS-WU025, BX-W201C) - PC Mode | Yes | BX-W201C only | No | - | - |
| Gamecube adapter (HS-WU025, BX-W201C, [057e-0337] ...) - Switch Mode | Yes | Yes | No | - | - |
| Gamecube adapter (WUP-028) | Yes | Yes | No | Might need up to 30s to be detected | - |
| N64 Hyperkin Adapter | Yes | No | No | - | https://www.amazon.com.au/Hyperkin-Controller-Adapter-Compatible-Nintendo/dp/B082N7K8QS |
| N64 MayFlash Controller Adapter| Yes | No | No | - | https://www.amazon.com/dp/B002B9FIUU |
| N64 KIWITATA USB Mini Controller | Yes | No | No | Edit config.ini and replace [0f0d-00c1] by the commented entry | https://www.amazon.ca/dp/B08P8CWBC1 |
| N64 Adaptoid Adpater | Yes | No | No | - | - |
| HS-N6420, [057e-0337] | Yes | Yes | No | Switch mode (Need 30s to receive first input) | https://french.alibaba.com/product-detail/HS-N6420-For-N64-Controller-Converter-1600893705131.html |
| Chinese wired SNES Controller | Yes | No | No | - | - |
| Xinmotek XM-10 (Arcade controller) | Yes | No | No | - | - |
| Logitech Driving Force GT (Wheel) | Yes | No | No | - | - |
| BSP-D9 Mobile Phone Stretch Game Controller | Yes | No | No | - | - |
| Qanba Drone 1 | Yes | No | No | - | https://www.amazon.com/Qanba-Drone-PlayStation-4/dp/B01I0GEDEY |
| PowerA Nintendo Switch Wired Controller | Yes | No | No | - | https://www.amazon.com/PowerA-Wired-Controller-Nintendo-Switch-Black/dp/B07PDJ45BT |
| PS2 Dual Converter | Yes | No | No | - | - |
| Raphnet Classic Controller USB adapter | Yes | No | No | - | - |
| Playstation classic controller | Yes | No | No | Do not works on Dock or with USB hub. Only works with direct USB-C OTG cable | - |
| BipBop 2.4g usb wireless | Yes | No | No | - | - |
| Dazz Double Shock Fighter USB 2.0 | Yes | No | No | DInput mode | - |
| Buffalo iBuffalo Classic USB | Yes | No | No | - | https://www.amazon.com/Buffalo-iBuffalo-Classic-USB-Gamepad/dp/B002B9XB0E |
| L-TEK Dance Pad PRO (DDR pad) | Yes | No | No | - | - |
| Google Stadia Controller | Yes | Yes | No | - | https://www.amazon.fr/Google-Stadia-Premiere-Edition-White/dp/B09N751DP2 |
| Raptor PS4 Wired (0c12-0e16) | Yes | No | No | 	You need to set `discovery_mode=1` & `discovery_vidpid=0c12-0e16` ([#97](https://github.com/o0Zz/sys-con/issues/97)) | - |
| Steam controller 2026 (Wired + Wireless puck) | Yes | Yes | Yes | - | - |
| Retro-bit SEGA Saturn Wireless 8-Button Arcade | Yes | No | No | - | https://www.amazon.fr/Retrobit-Saturn-Manette-boutons-dOrigine/dp/B07Y5M8R41 |
| DragonRise USB gamepad (0079-0011) | Yes | No | No | Do not works on Dock or with USB hub - Only works with direct USB-C OTG cable ([#105](https://github.com/o0Zz/sys-con/issues/105))| - |
| Great Tech Chinese Controller (2563-0575 045e-028e) | Yes | Yes | No | Hold Turbo+Home for 8 seconds to force it in xinput ([#108](https://github.com/o0Zz/sys-con/issues/108)) | https://www.noon.com/oman-en/pc360-pc-version-p3-android-gamepad-macro-definition-programming-dual-vibration-compatible-with-multi-platform/ZD0DFC47CFE4367017D0CZ/p/
| Saulabi 4K Arcade Stick | Yes | No | No | - | - |
xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx | | | |  |  |