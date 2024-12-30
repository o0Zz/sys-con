#include "SwitchVirtualGamepadHandler.h"
#include "SwitchLogger.h"
#include <chrono>

SwitchVirtualGamepadHandler::SwitchVirtualGamepadHandler(std::unique_ptr<IController> &&controller, s32 polling_frequency_ms, s8 thread_priority)
    : m_controller(std::move(controller)),
      m_polling_frequency_ms(std::max(1, polling_frequency_ms)),
      m_polling_thread_priority(thread_priority)
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

    m_read_input_timeout_us = (m_polling_frequency_ms * 1000) / m_controller->GetInputCount();

    return 0;
}

void SwitchVirtualGamepadHandler::Exit()
{
    ExitThread();

    m_controller->Exit();
}

void SwitchVirtualGamepadHandler::onRun()
{
    ::syscon::logger::LogDebug("SwitchVirtualGamepadHandler InputThread running ...");

    do
    {
        auto startTimer = std::chrono::steady_clock::now();

        UpdateInput(m_read_input_timeout_us);
        UpdateOutput();

        s64 execution_time_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - startTimer).count();

        if ((execution_time_us - m_read_input_timeout_us) > 10000) // 10ms
            ::syscon::logger::LogWarning("SwitchVirtualGamepadHandler UpdateInputOutput took: %d ms !", execution_time_us / 1000);

        if (execution_time_us < m_read_input_timeout_us)
            svcSleepThread((m_read_input_timeout_us - execution_time_us) * 1000);

    } while (m_ThreadIsRunning);

    ::syscon::logger::LogDebug("SwitchVirtualGamepadHandler InputThread stopped !");
}

void SwitchVirtualGamepadHandlerThreadFunc(void *handler)
{
    static_cast<SwitchVirtualGamepadHandler *>(handler)->onRun();
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

void SwitchVirtualGamepadHandler::ConvertAxisToSwitchAxis(float x, float y, s32 *x_out, s32 *y_out)
{
    float floatRange = 2.0f;
    float newRange = (JOYSTICK_MAX - JOYSTICK_MIN);

    *x_out = (((x + 1.0f) * newRange) / floatRange) + JOYSTICK_MIN;
    *y_out = -((((y + 1.0f) * newRange) / floatRange) + JOYSTICK_MIN);
}