#pragma once
#include <stratosphere.hpp>

namespace ams::syscon::hid::mitm
{

    class HidMitmModule
    {

    public:
        static void ThreadFunction(void *arg);
    };

    void InitializeHidMitm();
    void FinalizeHidMitm();

} // namespace ams::syscon::hid::mitm
