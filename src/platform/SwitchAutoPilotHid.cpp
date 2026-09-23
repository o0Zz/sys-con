#include "SwitchAutoPilotHid.h"
#include "SwitchVirtualHid.h"
#include "SwitchLogger.h"

#include <mutex>

namespace syscon::hid::autopilot
{
    namespace
    {
        // The rate hid itself samples at.
        constexpr u64 PublishPeriodNs = 5000000;

        constexpr int ThreadPriority = 38;
        constexpr int ThreadCore = 3;

        std::mutex g_mutex;
        unsigned g_keyboard_refs = 0;
        unsigned g_mouse_refs = 0;

        bool g_running = false;
        ::Thread g_thread;
        alignas(0x1000) u8 g_thread_stack[0x2000];

        void Publish()
        {
            HidKeyboardState keyboard{};
            if (VirtualKeyboard::Get().Compose(&keyboard))
            {
                HiddbgKeyboardAutoPilotState state{};
                state.modifiers = keyboard.modifiers;
                for (size_t i = 0; i < 4; i++)
                    state.keys[i] = keyboard.keys[i];

                ::hiddbgSetKeyboardAutoPilotState(&state);
            }

            HidMouseState mouse{};
            if (VirtualMouse::Get().Drain(&mouse))
            {
                HiddbgMouseAutoPilotState state{};
                state.x = mouse.x;
                state.y = mouse.y;
                state.delta_x = mouse.delta_x;
                state.delta_y = mouse.delta_y;
                // The auto-pilot state has one wheel field; horizontal wheel is not
                // representable on this channel.
                state.wheel_delta = mouse.wheel_delta_y;
                state.buttons = mouse.buttons;
                state.attributes = mouse.attributes;

                ::hiddbgSetMouseAutoPilotState(&state);
            }
        }

        void ThreadFunc(void *)
        {
            ::syscon::logger::LogDebug("HidAutoPilot publisher running ...");

            while (g_running)
            {
                Publish();
                svcSleepThread(PublishPeriodNs);
            }

            ::syscon::logger::LogDebug("HidAutoPilot publisher stopped !");
        }

        Result StartIfNeeded()
        {
            if (g_running)
                return 0;

            g_running = true;

            Result rc = threadCreate(&g_thread, &ThreadFunc, nullptr, g_thread_stack, sizeof(g_thread_stack), ThreadPriority, ThreadCore);
            if (R_FAILED(rc))
            {
                g_running = false;
                return rc;
            }

            rc = threadStart(&g_thread);
            if (R_FAILED(rc))
            {
                g_running = false;
                threadClose(&g_thread);
                return rc;
            }

            return 0;
        }

        void StopIfIdle()
        {
            if (g_keyboard_refs != 0 || g_mouse_refs != 0 || !g_running)
                return;

            g_running = false;
            svcCancelSynchronization(g_thread.handle);
            threadWaitForExit(&g_thread);
            threadClose(&g_thread);
        }
    } // namespace

    Result AcquireKeyboard()
    {
        std::lock_guard<std::mutex> lock(g_mutex);

        Result rc = StartIfNeeded();
        if (R_FAILED(rc))
            return rc;

        g_keyboard_refs++;
        return 0;
    }

    void ReleaseKeyboard()
    {
        std::lock_guard<std::mutex> lock(g_mutex);

        g_keyboard_refs--;
        if (g_keyboard_refs == 0)
            ::hiddbgUnsetKeyboardAutoPilotState();

        StopIfIdle();
    }

    Result AcquireMouse()
    {
        std::lock_guard<std::mutex> lock(g_mutex);

        Result rc = StartIfNeeded();
        if (R_FAILED(rc))
            return rc;

        g_mouse_refs++;
        return 0;
    }

    void ReleaseMouse()
    {
        std::lock_guard<std::mutex> lock(g_mutex);

        g_mouse_refs--;
        if (g_mouse_refs == 0)
            ::hiddbgUnsetMouseAutoPilotState();

        StopIfIdle();
    }
} // namespace syscon::hid::autopilot
