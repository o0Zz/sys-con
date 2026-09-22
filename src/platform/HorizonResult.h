#pragma once

#include "Status.h"

#include <switch.h>

namespace syscon
{
    /*
        sys-con's module number for Horizon Result codes.

        Horizon packs a Result as MAKERESULT(module, description), so a ControllerLib Status
        handed to Horizon raw would decode as a module sys-con does not own with a description
        of zero, losing the actual error. Convert through ToHorizonResult() instead.

        The number below is arbitrary. It sits in the range Atmosphere leaves to homebrew
        and is not claimed by libnx or Atmosphere, but nothing enforces that, so treat it as
        "a value we picked" rather than "a value reserved for us".
    */
    constexpr u32 ModuleSysCon = 420;

    /// The single explicit bridge from ControllerLib's error domain into Horizon's.
    /// Everything else should stay in one domain or the other.
    inline Result ToHorizonResult(controllerlib::Status status)
    {
        if (controllerlib::Succeeded(status))
            return 0;

        return MAKERESULT(ModuleSysCon, static_cast<u32>(status));
    }
} // namespace syscon
