#include "SwitchVirtualGamepadHandler.h"
#include "SwitchLogger.h"
#include <chrono>

SwitchVirtualGamepadHandler::SwitchVirtualGamepadHandler(std::unique_ptr<IController> &&controller, int32_t polling_timeout_ms, int8_t thread_priority)
    : m_controller(std::move(controller)),
      m_polling_thread_priority(thread_priority),
      m_polling_timeout_ms(polling_timeout_ms)
{
}

SwitchVirtualGamepadHandler::~SwitchVirtualGamepadHandler()
{
}

Result SwitchVirtualGamepadHandler::Initialize()
{
    Result rc = m_controller->Initialize();
    if (R_FAILED(rc))
        return rc;

    return 0;
}

void SwitchVirtualGamepadHandler::Exit()
{
    ExitThread();

    m_controller->Exit();
}

void SwitchVirtualGamepadHandler::OnRun()
{
    Result rc;
    ::syscon::logger::LogDebug("SwitchVirtualGamepadHandler InputThread running ...");

    /*
     Read timeout depends on the polling frequency and number of interfaces
      - On XBOX360 controllers, we have 4 interfaces, so the read timeout is divided by 4 to make sure we read all interfaces in time.
      - On most of other controllers, we have 1 interface, so the read timeout is equal to the polling frequency but
        it don't have a big impact on the performance - It's even better to set a bigger timeout to avoid the thread to be too busy.
    */
    uint32_t polling_timeout_us = (m_polling_timeout_ms * 1000) / m_controller->GetDevice()->GetInterfaces().size();

    do
    {
        auto startTimer = std::chrono::steady_clock::now();

        rc = UpdateInput(polling_timeout_us);
        (void)UpdateOutput();

        s64 execution_time_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - startTimer).count();

        if ((rc != CONTROLLER_STATUS_TIMEOUT) && (execution_time_us > 30000)) // 30ms
            ::syscon::logger::LogWarning("SwitchVirtualGamepadHandler UpdateInputOutput took: %d ms !", execution_time_us / 1000);

        if (R_FAILED(rc) && rc != CONTROLLER_STATUS_TIMEOUT)
        {
            /*
            This case is a "normal case" and happen when the controller is disconnected
            Sleep for 100ms second before retrying, provide time to other threads to detect the controller disconnection
            Otherwise, the thread will be too busy and will not let the other threads to run and the nintendo switch will freeze
            */

            //::syscon::logger::LogError("SwitchVirtualGamepadHandler UpdateInputOutput failed with error: 0x%08X !", rc);
            svcSleepThread(100000);
        }

    } while (m_ThreadIsRunning);

    ::syscon::logger::LogDebug("SwitchVirtualGamepadHandler InputThread stopped !");
}

void SwitchVirtualGamepadHandlerThreadFunc(void *handler)
{
    static_cast<SwitchVirtualGamepadHandler *>(handler)->OnRun();
}

Result SwitchVirtualGamepadHandler::InitThread()
{
    m_ThreadIsRunning = true;
    Result rc = threadCreate(&m_Thread, &SwitchVirtualGamepadHandlerThreadFunc, this, thread_stack, sizeof(thread_stack), m_polling_thread_priority, -2);
    if (R_FAILED(rc))
        return rc;

    rc = threadStart(&m_Thread);
    if (R_FAILED(rc))
        return rc;

    return 0;
}

void SwitchVirtualGamepadHandler::ExitThread()
{
    m_ThreadIsRunning = false;
    svcCancelSynchronization(m_Thread.handle);
    threadWaitForExit(&m_Thread);
    threadClose(&m_Thread);
}

void SwitchVirtualGamepadHandler::ConvertAxisToSwitchAxis(float x, float y, int32_t *x_out, int32_t *y_out)
{
    float floatRange = 2.0f;
    float newRange = (JOYSTICK_MAX - JOYSTICK_MIN);

    *x_out = (((x + 1.0f) * newRange) / floatRange) + JOYSTICK_MIN;
    *y_out = -((((y + 1.0f) * newRange) / floatRange) + JOYSTICK_MIN);
}