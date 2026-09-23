#include "SwitchVirtualDeviceHandler.h"
#include "SwitchLogger.h"
#include <chrono>
#include <cassert>

using namespace controllerlib;

SwitchVirtualDeviceHandler::SwitchVirtualDeviceHandler(int32_t polling_timeout_ms, int8_t thread_priority)
    : m_polling_thread_priority(thread_priority),
      m_polling_timeout_ms(polling_timeout_ms)
{
}

SwitchVirtualDeviceHandler::~SwitchVirtualDeviceHandler()
{
    // Do NOT call Exit() here: calling virtuals in destructor is unsafe
    // User must call Exit() manually before destruction in the derived class.
    assert(!m_ThreadIsRunning);
}

void SwitchVirtualDeviceHandler::OnRun()
{
    Status rc = Status::Success;
    ::syscon::logger::LogDebug("SwitchVirtualDeviceHandler InputThread running ...");

    /*
     Read timeout depends on the polling frequency and number of interfaces
      - On XBOX360 controllers, we have 4 interfaces, so the read timeout is divided by 4 to make sure we read all interfaces in time.
      - On most of other controllers, we have 1 interface, so the read timeout is equal to the polling frequency but
        it don't have a big impact on the performance - It's even better to set a bigger timeout to avoid the thread to be too busy.
    */
    uint32_t polling_timeout_us = (m_polling_timeout_ms * 1000) / GetInterfaceCount();

    do
    {
        auto startTimer = std::chrono::steady_clock::now();

        rc = UpdateInput(polling_timeout_us);
        (void)UpdateOutput();

        s64 execution_time_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - startTimer).count();

        if ((rc != Status::Timeout) && (execution_time_us > 30000)) // 30ms
            ::syscon::logger::LogWarning("SwitchVirtualDeviceHandler UpdateInputOutput took: %d ms !", execution_time_us / 1000);

        if (Failed(rc) && rc != Status::Timeout && rc != Status::NothingTodo)
        {
            /*
            This case is a "normal case" and happen when the controller is disconnected
            Sleep for 100ms second before retrying, provide time to other threads to detect the controller disconnection
            Otherwise, the thread will be too busy and will not let the other threads to run and the nintendo switch will freeze
            */
            svcSleepThread(100000000);
        }

    } while (m_ThreadIsRunning);

    ::syscon::logger::LogDebug("SwitchVirtualDeviceHandler InputThread stopped !");
}

void SwitchVirtualDeviceHandlerThreadFunc(void *handler)
{
    static_cast<SwitchVirtualDeviceHandler *>(handler)->OnRun();
}

Result SwitchVirtualDeviceHandler::InitThread()
{
    m_ThreadIsRunning = true;
    Result rc = threadCreate(&m_Thread, &SwitchVirtualDeviceHandlerThreadFunc, this, thread_stack, sizeof(thread_stack), m_polling_thread_priority, 3 /* On CPU 3 responsible for input */);
    if (R_FAILED(rc))
        return rc;

    rc = threadStart(&m_Thread);
    if (R_FAILED(rc))
        return rc;

    return 0;
}

void SwitchVirtualDeviceHandler::ExitThread()
{
    if (!m_ThreadIsRunning)
        return;

    m_ThreadIsRunning = false;
    svcCancelSynchronization(m_Thread.handle);
    threadWaitForExit(&m_Thread);
    threadClose(&m_Thread);
}
