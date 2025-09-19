#include "psc_module.h"
#include "usb_module.h"
#include "config_handler.h"
#include "controller_handler.h"
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

        alignas(0x1000) u8 psc_thread_stack[0x1000];
        Thread g_psc_thread;

        bool is_psc_thread_running = false;

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
                                break;
                            case PscPmState_ReadyShutdown:
                                ::syscon::logger::LogDebug("Power management: Shutdown");
                                is_psc_thread_running = false; // Exit thread
                                [[fallthrough]];
                            case PscPmState_ReadySleep:
                                ::syscon::logger::LogDebug("Power management: Sleep");
                                controllers::Clear();
                                break;
                            default:
                                break;
                        }
                        pscPmModuleAcknowledge(&pscModule, pscState);
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

        pscModuleWaiter = waiterForEvent(&pscModule.event);
        is_psc_thread_running = true;
        rc = threadCreate(&g_psc_thread, &PscThreadFunc, nullptr, psc_thread_stack, sizeof(psc_thread_stack), 0x2C, -2);
        if (R_FAILED(rc))
            return rc;

        rc = threadStart(&g_psc_thread);
        if (R_FAILED(rc))
            return rc;

        return 0;
    }

    bool IsRunning()
    {
        return is_psc_thread_running;
    }

    void Exit()
    {
        is_psc_thread_running = false;

        pscPmModuleFinalize(&pscModule);
        pscPmModuleClose(&pscModule);
        eventClose(&pscModule.event);

        svcCancelSynchronization(g_psc_thread.handle);
        threadWaitForExit(&g_psc_thread);
        threadClose(&g_psc_thread);
    }
}; // namespace syscon::psc