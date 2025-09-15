#pragma once
#include <switch.h>
#include "IController.h"

class SwitchVirtualGamepadHandlerData
{

public:
    SwitchVirtualGamepadHandlerData()
        : m_is_connected(false)
    {
    }

    bool m_is_connected;
};

// This class is a base class for SwitchHDLHandler and SwitchAbstractedPaadHandler.
class SwitchVirtualGamepadHandler
{
    friend void SwitchVirtualGamepadHandlerThreadFunc(void *arg);

protected:
    SwitchVirtualGamepadHandlerData m_controllerData[CONTROLLER_MAX_INPUTS];

protected:
    std::unique_ptr<IController> m_controller;
    int32_t m_polling_thread_priority;
    int32_t m_polling_timeout_ms;

    alignas(0x1000) u8 thread_stack[0x2000];
    Thread m_Thread;
    bool m_ThreadIsRunning = false;

    // Fills out the HDL state with the specified button data and passes it to HID
    virtual Result UpdateControllerState(u64 buttons, const HidAnalogStickState &analog_stick_l, const HidAnalogStickState &analog_stick_r, uint16_t input_idx) = 0;
    virtual Result AttachController(uint16_t input_idx) = 0;
    virtual Result DetachController(uint16_t input_idx) = 0;

    void OnRun();

public:
    // thread_priority (0x00~0x3F); 0x2C is the usual priority of the main thread, 0x3B is a special priority on cores 0..2 that enables preemptive multithreading (0x3F on core 3).
    SwitchVirtualGamepadHandler(std::unique_ptr<IController> &&controller, int32_t polling_timeout_ms, int8_t thread_priority = 0x30);
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
    virtual Result UpdateInput(uint32_t timeout_us);
    // The function to call indefinitely by the output thread
    virtual Result UpdateOutput();

    static void ConvertAxisToSwitchAxis(float x, float y, int32_t *x_out, int32_t *y_out);
    static u8 ControllerTypeToDeviceType(ControllerType type);

    // Get the raw controller pointer
    inline IController *GetController() { return m_controller.get(); }
};