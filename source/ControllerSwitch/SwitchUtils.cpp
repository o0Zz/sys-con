#include "SwitchUtils.h"
#include <time.h>

u64 SwitchUtils::GetCurrentTimeMs()
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);                 // CLOCK_REALTIME gives wall clock time
    return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000); // Convert to milliseconds
}

u64 SwitchUtils::GetMonotonicTimeUs()
{
    u64 ticks = svcGetSystemTick();
    u64 tick_frequency = armGetSystemTickFreq();
    return (ticks * 1000) / tick_frequency;
}
