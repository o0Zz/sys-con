#pragma once
#include "switch.h"

namespace SwitchUtils
{
    constexpr size_t ThreadStackAlignment = 0x1000;
    constexpr size_t MemoryPageSize = 0x1000;

    u64 GetCurrentTimeMs();
    u64 GetMonotonicTimeUs();

#ifndef R_ABORT_UNLESS
    #define R_ABORT_UNLESS(rc)             \
        {                                  \
            if (R_FAILED(rc)) [[unlikely]] \
            {                              \
                diagAbortWithResult(rc);   \
            }                              \
        }
#endif

#ifndef R_TRY
    #define R_TRY(rc)         \
        {                     \
            if (R_FAILED(rc)) \
            {                 \
                return rc;    \
            }                 \
        }
#endif

} // namespace SwitchUtils