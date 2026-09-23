#pragma once

#include <switch.h>
#include "InputDeviceBase.h"

/*
    The polling thread and lifecycle shared by every kind of input device.

    One thread per device, on core 3 -- the core Horizon reserves for input. What the thread
    does with a report is the derived class's business: a gamepad publishes an npad, a
    keyboard a key set, a mouse a delta.
*/
class SwitchVirtualDeviceHandler
{
    friend void SwitchVirtualDeviceHandlerThreadFunc(void *arg);

protected:
    int32_t m_polling_thread_priority;
    int32_t m_polling_timeout_ms;

    alignas(0x1000) u8 thread_stack[0x2000];
    Thread m_Thread{};
    bool m_ThreadIsRunning = false;
    bool m_removable = true; // see SetRemovable()

    void OnRun();

    // Number of USB interfaces the read timeout is divided between.
    virtual size_t GetInterfaceCount() = 0;

public:
    // thread_priority (0x00~0x3F); 0x2C is the usual priority of the main thread, 0x3B is a special priority on cores 0..2 that enables preemptive multithreading (0x3F on core 3).
    SwitchVirtualDeviceHandler(int32_t polling_timeout_ms, int8_t thread_priority);
    virtual ~SwitchVirtualDeviceHandler();

    virtual Result Initialize() = 0;
    virtual void Exit() = 0;

    // Separately init the input-reading thread
    Result InitThread();
    // Separately close the input-reading thread
    void ExitThread();

    // The function to call indefinitely by the input thread
    virtual controllerlib::Status UpdateInput(uint32_t timeout_us) = 0;
    // The function to call indefinitely by the output thread
    virtual Result UpdateOutput() = 0;

    // The USB device behind this handler, for unplug detection.
    virtual controllerlib::IUSBDevice *GetDevice() = 0;

    inline void SetRemovable(bool removable) { m_removable = removable; }
    inline bool IsRemovable() const { return m_removable; }
};
