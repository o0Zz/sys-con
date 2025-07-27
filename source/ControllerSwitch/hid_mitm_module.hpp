#pragma once
#include <stratosphere.hpp>

namespace ams::syscon::hid::mitm
{

    class HidMitmModule
    {
    public:
        static constexpr size_t StackSize = 0x8000;
        static constexpr int ThreadPriority = 43;

    public:
        static void ThreadFunction(void *arg);
    };

    void InitializeHidMitm();
    void FinalizeHidMitm();

} // namespace ams::syscon::hid::mitm
