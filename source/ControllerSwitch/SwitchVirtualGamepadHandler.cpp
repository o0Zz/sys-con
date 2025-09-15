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
    Exit();
}

Result SwitchVirtualGamepadHandler::Initialize()
{
    syscon::logger::LogDebug("SwitchVirtualGamepadHandler[%04x-%04x] Initializing ...", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct());

    Result rc = m_controller->Initialize();
    if (R_FAILED(rc))
        return rc;

    return 0;
}

void SwitchVirtualGamepadHandler::Exit()
{
    syscon::logger::LogDebug("SwitchVirtualGamepadHandler[%04x-%04x] Exiting ...", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct());

    ExitThread();

    m_controller->Exit();

    for (int i = 0; i < m_controller->GetInputCount(); i++)
        DetachController(i);

    syscon::logger::LogInfo("SwitchVirtualGamepadHandler[%04x-%04x] Uninitialized !", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct());
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

Result SwitchVirtualGamepadHandler::UpdateInput(uint32_t timeout_us)
{
    uint16_t input_idx = 0;
    bool reattach_controller = false;
    NormalizedButtonData buttonData = {0};
    u64 buttons = 0;
    HidAnalogStickState analog_stick_l;
    HidAnalogStickState analog_stick_r;

    Result read_rc = m_controller->ReadInput(&buttonData, &input_idx, timeout_us);

    /*
        Note: We must not return here if readInput fail, because it might have change the ControllerConnected state.
        So, we must check if the controller is connected and detach it if it's not.
        This case happen with wireless Xbox 360 controllers
    */

    if (m_controllerData[input_idx].m_is_connected != m_controller->IsControllerConnected(input_idx)) // State changed ?
    {
        syscon::logger::LogDebug("SwitchVirtualGamepadHandler[%04x-%04x] Controller connection state changed on idx: %d !", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), input_idx);

        m_controllerData[input_idx].m_is_connected = m_controller->IsControllerConnected(input_idx);
        reattach_controller = m_controllerData[input_idx].m_is_connected; // If state change to connected, we need to re-attach the controller
        if (!m_controllerData[input_idx].m_is_connected)
        {
            syscon::logger::LogDebug("SwitchVirtualGamepadHandler[%04x-%04x] Detaching controller on idx: %d !", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), input_idx);
            DetachController(input_idx);
        }
    }

    if (R_FAILED(read_rc))
        return read_rc;

    auto startTimer = std::chrono::steady_clock::now();

    if (buttonData.buttons[ControllerButton::X])
        buttons |= HidNpadButton_X;
    if (buttonData.buttons[ControllerButton::A])
        buttons |= HidNpadButton_A;
    if (buttonData.buttons[ControllerButton::B])
        buttons |= HidNpadButton_B;
    if (buttonData.buttons[ControllerButton::Y])
        buttons |= HidNpadButton_Y;
    if (buttonData.buttons[ControllerButton::LSTICK_CLICK])
        buttons |= HidNpadButton_StickL;
    if (buttonData.buttons[ControllerButton::RSTICK_CLICK])
        buttons |= HidNpadButton_StickR;
    if (buttonData.buttons[ControllerButton::L])
        buttons |= HidNpadButton_L;
    if (buttonData.buttons[ControllerButton::R])
        buttons |= HidNpadButton_R;
    if (buttonData.buttons[ControllerButton::ZL])
        buttons |= HidNpadButton_ZL;
    if (buttonData.buttons[ControllerButton::ZR])
        buttons |= HidNpadButton_ZR;
    if (buttonData.buttons[ControllerButton::MINUS])
        buttons |= HidNpadButton_Minus;
    if (buttonData.buttons[ControllerButton::PLUS])
        buttons |= HidNpadButton_Plus;
    if (buttonData.buttons[ControllerButton::DPAD_UP])
        buttons |= HidNpadButton_Up;
    if (buttonData.buttons[ControllerButton::DPAD_RIGHT])
        buttons |= HidNpadButton_Right;
    if (buttonData.buttons[ControllerButton::DPAD_DOWN])
        buttons |= HidNpadButton_Down;
    if (buttonData.buttons[ControllerButton::DPAD_LEFT])
        buttons |= HidNpadButton_Left;
    if (buttonData.buttons[ControllerButton::CAPTURE])
        buttons |= HiddbgNpadButton_Capture;
    if (buttonData.buttons[ControllerButton::HOME])
        buttons |= HiddbgNpadButton_Home;

    ConvertAxisToSwitchAxis(buttonData.sticks[0].axis_x, buttonData.sticks[0].axis_y, &analog_stick_l.x, &analog_stick_l.y);
    ConvertAxisToSwitchAxis(buttonData.sticks[1].axis_x, buttonData.sticks[1].axis_y, &analog_stick_r.x, &analog_stick_r.y);

    if (!reattach_controller)
        reattach_controller = (buttons & HidNpadButton_L) && (buttons & HidNpadButton_R); // L+R on the switch allow to re-attach the controller

    if (m_controllerData[input_idx].m_is_connected && reattach_controller)
    {
        syscon::logger::LogDebug("SwitchVirtualGamepadHandler[%04x-%04x] Re-attaching controller on idx: %d !", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), input_idx);
        AttachController(input_idx);
    }

    // We get the button inputs from the input packet and update the state of our controller
    syscon::logger::LogDebug("SwitchVirtualGamepadHandler[%04x-%04x] Updating controller state on idx: %d !", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), input_idx);
    Result res = UpdateControllerState(buttons, analog_stick_l, analog_stick_r, input_idx);

    s64 execution_time_us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - startTimer).count();
    syscon::logger::LogPerf("SwitchVirtualGamepadHandler[%04x-%04x] UpdateInput took: %d us for idx: %d !", m_controller->GetDevice()->GetVendor(), m_controller->GetDevice()->GetProduct(), execution_time_us, input_idx);

    return res;
}

Result SwitchVirtualGamepadHandler::UpdateOutput()
{
    // Vibrations are not supported with HDL
    return 0;
}

void SwitchVirtualGamepadHandler::ConvertAxisToSwitchAxis(float x, float y, int32_t *x_out, int32_t *y_out)
{
    float floatRange = 2.0f;
    float newRange = (JOYSTICK_MAX - JOYSTICK_MIN);

    *x_out = (((x + 1.0f) * newRange) / floatRange) + JOYSTICK_MIN;
    *y_out = -((((y + 1.0f) * newRange) / floatRange) + JOYSTICK_MIN);
}

u8 SwitchVirtualGamepadHandler::ControllerTypeToDeviceType(ControllerType type)
{
    if (type == ControllerType_ProWithBattery)
        return HidDeviceType_FullKey3;
    else if (type == ControllerType_Tarragon)
        return HidDeviceType_FullKey6;
    else if (type == ControllerType_Snes)
        return HidDeviceType_Lucia;
    else if (type == ControllerType_PokeballPlus)
        return HidDeviceType_Palma;
    else if (type == ControllerType_Gamecube)
        return HidDeviceType_FullKey13;
    else if (type == ControllerType_Pro)
        return HidDeviceType_FullKey15;
    else if (type == ControllerType_3rdPartyPro)
        return HidDeviceType_System19;
    else if (type == ControllerType_N64)
        return HidDeviceType_Lagon;
    else if (type == ControllerType_Sega)
        return HidDeviceType_Lager;
    else if (type == ControllerType_Nes)
        return HidDeviceType_LarkNesLeft;
    else if (type == ControllerType_Famicom)
        return HidDeviceType_LarkHvcLeft;

    return HidDeviceType_FullKey15;
}