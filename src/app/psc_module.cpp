#include "psc_module.h"
#include "usb_module.h"
#include "config_handler.h"
#include "controller_handler.h"
#include "network_module.h"
#include "logger.h"

namespace syscon::psc
{
    namespace
    {
        PscPmModule pscModule;
        Waiter pscModuleWaiter;
        const uint32_t dependencies[] = {PscPmModuleId_Fs};

        // Thread to check for psc:pm state change (console waking up/going to sleep)
        void PscThreadFunc(void *arg);

        // Must be large enough for controllers::Clear()'s teardown on sleep/shutdown: destructor
        // chains through every connected controller plus several Log*() calls, each driving
        // newlib's stack-heavy vsnprintf internals. 0x1000 was observed to overflow (crash report
        // showed SP sitting exactly at the stack region's floor) - sized to match the USB threads.
        alignas(0x1000) u8 psc_thread_stack[0x4000];
        Thread g_psc_thread{};

        bool is_psc_thread_running = false;
        bool is_psc_module_open = false;

        void PscThreadFunc(void *arg)
        {
            (void)arg;
            do
            {
                if (R_SUCCEEDED(waitSingle(pscModuleWaiter, UINT64_MAX)))
                {
                    PscPmState pscState;
                    u32 out_flags;
                    if (R_SUCCEEDED(pscPmModuleGetRequest(&pscModule, &pscState, &out_flags)))
                    {
                        switch (pscState)
                        {
                            case PscPmState_Awake:
                            case PscPmState_ReadyAwaken:
                                ::syscon::logger::LogDebug("Power management: Awake");
                                if (pscState == PscPmState_Awake)
                                    ::syscon::networkpad::OnWake();
                                break;
                            case PscPmState_ReadyShutdown:
                            case PscPmState_ReadySleep:
                                ::syscon::logger::LogDebug(pscState == PscPmState_ReadyShutdown ? "Power management: Shutdown" : "Power management: Sleep");
                                controllers::Clear();
                                break;
                            default:
                                break;
                        }
                        pscPmModuleAcknowledge(&pscModule, pscState);

                        // Only after the acknowledge: the main loop closes the module as soon as
                        // this flag drops.
                        if (pscState == PscPmState_ReadyShutdown)
                            is_psc_thread_running = false;
                    }
                }
            } while (is_psc_thread_running);
        }
    } // namespace

    int Initialize()
    {
        Result rc = pscmGetPmModule(&pscModule, PscPmModuleId(0x7E), dependencies, sizeof(dependencies) / sizeof(uint32_t), true);
        if (R_FAILED(rc))
            return rc;

        is_psc_module_open = true;
        pscModuleWaiter = waiterForEvent(&pscModule.event);
        rc = threadCreate(&g_psc_thread, &PscThreadFunc, nullptr, psc_thread_stack, sizeof(psc_thread_stack), 0x2C, -2);
        if (R_FAILED(rc))
            return rc;

        is_psc_thread_running = true;
        rc = threadStart(&g_psc_thread);
        if (R_FAILED(rc))
        {
            is_psc_thread_running = false;
            threadClose(&g_psc_thread);
            g_psc_thread = Thread{};
            return rc;
        }

        return 0;
    }

    bool IsRunning()
    {
        return is_psc_thread_running;
    }

    void Exit()
    {
        is_psc_thread_running = false;

        if (g_psc_thread.handle != INVALID_HANDLE)
        {
            svcCancelSynchronization(g_psc_thread.handle);
            threadWaitForExit(&g_psc_thread);
            threadClose(&g_psc_thread);
            g_psc_thread = Thread{};
        }

        if (is_psc_module_open)
        {
            pscPmModuleFinalize(&pscModule);
            pscPmModuleClose(&pscModule);
            eventClose(&pscModule.event);
            is_psc_module_open = false;
        }
    }
}; // namespace syscon::psc