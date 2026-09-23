# ControllerLib

A portable USB game-controller driver library. Given a USB device, it identifies the
controller, speaks its wire protocol, and hands back normalized button and stick values.

It is **standalone**: no libnx, no Atmosphere, no Horizon, no sys-con. It includes no
platform header and links no platform library. Everything it needs from the outside world
arrives through four pure interfaces that the host implements.

Everything it declares lives in `namespace controllerlib`, so embedding it adds exactly one
name to the host's global namespace. That matters for its error type in particular: `Status`
is a name plenty of platforms also want, and a host that has its own (Horizon's `Result`,
say) keeps the two visibly apart at every use.

## Building

On its own:

```sh
cmake -S src/controllerlib -B build-controllerlib
cmake --build build-controllerlib
```

No flags: the library finds its one dependency on its own (see below). CI runs exactly this
on every push, which is what stops it quietly acquiring a dependency on its host.

Embedded in another project:

```cmake
add_subdirectory(path/to/controllerlib)
target_link_libraries(MyApp PRIVATE ControllerLib)
```

That is the whole contract. The target carries its own include path and its own C++20
requirement, and its warning flags are `PRIVATE`, so a host keeps its own warning policy.
Nothing in `CMakeLists.txt` reads a variable, macro or target defined by a parent project —
if that ever stops being true, the standalone configure above breaks and says so.

The one dependency is
[HIDDataInterpreter](https://github.com/o0Zz/HIDDataInterpreter) (HID report descriptor
parsing, used by `GenericHIDController`). The library adds it itself, searching conventional
locations near its own directory — a vendored copy underneath it, a sibling checkout, or an
`external/`, `third_party/`, `lib/` or `deps/` directory up to two levels up — and taking the
innermost match. Two ways to override, in priority order:

- pass `-DCONTROLLERLIB_HIDDATAINTERPRETER_DIR=<path>`, or set that variable before
  `add_subdirectory()` if you are embedding;
- define a `HIDDataInterpreterLib` target yourself beforehand, and the library uses it and
  skips the search entirely.

If the search fails, the error lists every location it tried.

## What the host provides

| Interface | The host implements it with |
|---|---|
| `IUSBDevice` | its USB stack's device handle |
| `IUSBInterface` | a claimed interface on that device |
| `IUSBEndpoint` | a bulk/interrupt endpoint, read and write |
| `ILogger` | wherever its logs go |

Nothing else crosses the boundary. This is what lets the drivers be unit-tested on a PC
against gmock fakes, with no hardware and no emulator.

## What the host gets

```cpp
#include "Controllers.h"

controllerlib::ControllerConfig config;   // pin/axis mapping, deadzones, combos
auto controller = std::make_unique<controllerlib::Xbox360Controller>(
    std::move(device), config, std::move(logger));

if (controllerlib::Failed(controller->Initialize()))
    return;

controllerlib::NormalizedButtonData data;
uint16_t inputIdx = 0;
controllerlib::Status status = controller->ReadInput(&data, &inputIdx, timeoutUs);
```

A translation unit that uses the library heavily can open it with
`using namespace controllerlib;` after its includes — that is what sys-con's own `.cpp`
files do. Headers should qualify instead, so including one does not re-export the library
into everything downstream.

`NormalizedButtonData::buttons` is indexed by `GamepadButton`; sticks are floats in
`[-1, 1]`. Every call returns `Status`, which is `[[nodiscard]]` and does not convert to
`int` — a host that has its own error type converts explicitly at its own boundary.

## Layout

```
ControllerLib/
  Status.h/.cpp         the library's only error type
  InputDeviceBase.h     what every input device has: device, config, logger, lifecycle
  IGamepad.h            the gamepad interface + normalized output
  IKeyboard.h/IMouse.h  the keyboard and mouse interfaces
  InputState.h          keyboard and mouse state, host-independent
  IUSBDevice/Interface/Endpoint.h, ILogger.h    what the host implements
  ControllerConfig.h    mapping, deadzones, combos
  PinId.h               physical input pin (what a driver reports)
  GamepadButton.h       normalized button (what the host consumes)
  AnalogAxis.h          normalized axis
  EnumArray.h           array indexable only by one enum type
  Controllers.h         convenience header: every driver
  drivers/              UsbPipeSet, BaseController + one file per controller family
```

## Adding a driver

Add the new code inside `namespace controllerlib`, like everything else here. Subclass
`BaseController`, implement `ParseData()` to decode the device's report into
`RawInputData` (buttons by **pin**, analog by **axis**), and add the `.cpp` to `SRC_FILES`
in `CMakeLists.txt` and the header to `Controllers.h`. `BaseController` does the rest:
endpoint selection, draining to the freshest report, deadzone, scaling, pin-to-button
mapping and combo simulation.
