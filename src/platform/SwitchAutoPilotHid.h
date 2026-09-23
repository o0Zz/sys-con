#pragma once

#include <switch.h>

/*
    Publishes the merged keyboard/mouse state through hiddbg's auto-pilot commands
    (mode=hiddbg).

    Those commands are process-global singletons -- one keyboard, one mouse, no per-device
    handle, unlike HDLS -- so only this publisher ever calls them. Handlers feed the
    aggregators in SwitchVirtualHid.h and never touch hiddbg themselves; that is what lets
    two mice coexist without either unsetting the other.

    It runs its own thread because the mouse state is sticky: a non-zero delta left in place
    keeps moving the pointer on every hid tick forever, so the drained (zero when idle) state
    has to be re-pushed at a steady rate regardless of how often the device reports.
*/
namespace syscon::hid::autopilot
{
    Result AcquireKeyboard();
    void ReleaseKeyboard();

    Result AcquireMouse();
    void ReleaseMouse();
} // namespace syscon::hid::autopilot
