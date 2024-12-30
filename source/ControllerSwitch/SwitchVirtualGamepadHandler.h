#pragma once
#include "switch.h"
#include "IController.h"

// This class is a base class for SwitchHDLHandler and SwitchAbstractedPaadHandler.
class SwitchVirtualGamepadHandler
{
    friend void SwitchVirtualGamepadHandlerThreadFunc(void *arg);

protected:
    std::unique_ptr<IController> m_controller;
    s32 m_polling_frequency_ms;
    s32 m_polling_thread_priority;
    s32 m_read_input_timeout_us;

    alignas(0x1000) u8 thread_stack[0x2000];
    Thread m_Thread;
    bool m_ThreadIsRunning = false;

    void onRun();

public:
    // thread_priority (0x00~0x3F); 0x2C is the usual priority of the main thread, 0x3B is a special priority on cores 0..2 that enables preemptive multithreading (0x3F on core 3).
    SwitchVirtualGamepadHandler(std::unique_ptr<IController> &&controller, s32 polling_frequency_ms, s8 thread_priority = 0x30);
    virtual ~SwitchVirtualGamepadHandler();

    // Override this if you want a custom init procedure
    virtual Result Initialize();
    // Override this if you want a custom exit procedure
    virtual void Exit();

    // Separately init the input-reading thread
    Result InitThread();
    // Separately close the input-reading thread
    void ExitThread();

    // The function to call indefinitely by the input thread
    virtual void UpdateInput(s32 timeout_us) = 0;
    // The function to call indefinitely by the output thread
    virtual void UpdateOutput() = 0;

    void ConvertAxisToSwitchAxis(float x, float y, s32 *x_out, s32 *y_out);

    // Get the raw controller pointer
    inline IController *GetController() { return m_controller.get(); }
};