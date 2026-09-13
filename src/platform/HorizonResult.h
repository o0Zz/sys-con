#pragma once

#include "Status.h"

#include <switch.h>

namespace syscon
{
    /*
        sys-con's module number for Horizon Result codes.

        Horizon packs a Result as MAKERESULT(module, description). ControllerLib's Status
        used to be handed to Horizon raw -- `return Status::InvalidIndex;` from a function
        returning Result produced the integer 117, which decodes as "module 117,
        description 0": a module sys-con does not own, and a description of zero, i.e. it
        lost the actual error. The library now keeps its Status inside its own namespace,
        which is the other half of the same fix.

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
