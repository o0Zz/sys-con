#include <switch.h>
#include <stdio.h>
#include <string>
#include <thread>
#include <algorithm>

std::string buttonToStr(u64 ButtonMask)
{
    std::string buttonStr;

    if (ButtonMask & HidNpadButton_A)
        buttonStr += "A ";
    if (ButtonMask & HidNpadButton_B)
        buttonStr += "B ";
    if (ButtonMask & HidNpadButton_X)
        buttonStr += "X ";
    if (ButtonMask & HidNpadButton_Y)
        buttonStr += "Y ";
    if (ButtonMask & HidNpadButton_StickL)
        buttonStr += "StickL ";
    if (ButtonMask & HidNpadButton_StickR)
        buttonStr += "StickR ";
    if (ButtonMask & HidNpadButton_L)
        buttonStr += "L ";
    if (ButtonMask & HidNpadButton_R)
        buttonStr += "R ";
    if (ButtonMask & HidNpadButton_ZL)
        buttonStr += "ZL ";
    if (ButtonMask & HidNpadButton_ZR)
        buttonStr += "ZR ";
    if (ButtonMask & HidNpadButton_Plus)
        buttonStr += "+ ";
    if (ButtonMask & HidNpadButton_Minus)
        buttonStr += "- ";
    if (ButtonMask & HidNpadButton_Left)
        buttonStr += "Left ";
    if (ButtonMask & HidNpadButton_Up)
        buttonStr += "Up ";
    if (ButtonMask & HidNpadButton_Right)
        buttonStr += "Right ";
    if (ButtonMask & HidNpadButton_Down)
        buttonStr += "Down ";

    buttonStr.erase(buttonStr.find_last_not_of(' ') + 1);

    return buttonStr;
}

std::string mouseButtonToStr(u32 ButtonMask)
{
    std::string buttonStr;

    if (ButtonMask & HidMouseButton_Left)
        buttonStr += "Left ";
    if (ButtonMask & HidMouseButton_Right)
        buttonStr += "Right ";
    if (ButtonMask & HidMouseButton_Middle)
        buttonStr += "Middle ";
    if (ButtonMask & HidMouseButton_Back)
        buttonStr += "Back ";
    if (ButtonMask & HidMouseButton_Forward)
        buttonStr += "Forward ";

    buttonStr.erase(buttonStr.find_last_not_of(' ') + 1);

    return buttonStr;
}

std::string heldKeysToStr(const HidKeyboardState &state)
{
    std::string keysStr;
    char usage[8];

    for (int key = 0; key < 256; key++)
    {
        if (hidKeyboardStateGetKey(&state, static_cast<HidKeyboardKey>(key)))
        {
            snprintf(usage, sizeof(usage), "%02X ", key);
            keysStr += usage;
        }
    }

    keysStr.erase(keysStr.find_last_not_of(' ') + 1);

    return keysStr;
}

// Key-down edges since launch: a key pressed twice must count twice.
int countKeyDowns(const HidKeyboardState &previous, const HidKeyboardState &current)
{
    int downs = 0;
    for (int i = 0; i < 4; i++)
        downs += __builtin_popcountll(current.keys[i] & ~previous.keys[i]);
    return downs;
}

int main()
{
    char outputBuffer[256];
    bool isVibrationPermitted = false;
    HidVibrationDeviceHandle vibrationDeviceHandle;
    HidVibrationValue vibrationValue;
    HidVibrationValue vibrationValueRead;
    HidSixAxisSensorHandle sixAxisHandle;
    PadState pad;

    PrintConsole *console = consoleInit(NULL);

    padConfigureInput(8, HidNpadStyleSet_NpadStandard);

    padInitializeAny(&pad);

    hidSetNpadHandheldActivationMode(HidNpadHandheldActivationMode_Single);

    hidInitializeMouse();
    hidInitializeKeyboard();

    HidKeyboardState previousKeyboard{};
    int keyDowns = 0;
    int pointerRow = 0;
    int pointerColumn = 0;

    if (R_FAILED(hidInitializeVibrationDevices(&vibrationDeviceHandle, 1, HidNpadIdType_No1, HidNpadStyleTag_NpadFullKey)))
        printf("ERR: hidInitializeVibrationDevices failed !\n");

    if (R_FAILED(hidGetSixAxisSensorHandles(&sixAxisHandle, 1, HidNpadIdType_No1, HidNpadStyleTag_NpadFullKey)) || R_FAILED(hidStartSixAxisSensor(sixAxisHandle)))
        printf("ERR: six-axis sensor unavailable !\n");

    float current_vibration = 0.0;

    vibrationValue.amp_low = 0.0;   // Max 1.0
    vibrationValue.freq_low = 220;  // hz
    vibrationValue.amp_high = 0.0;  // Max 1.0
    vibrationValue.freq_high = 220; // hz

    printf("Welcome to sys-con debug application\n");
    printf("Press + to increase vibration (On controller No1)\n");
    printf("Press - to decrease vibration (On controller No1)\n");
    printf("Press + and - to exit\n");
    printf("USB mouse: X is the pointer below (# while a button is held)\n");
    printf("\n");

    while (appletMainLoop())
    {

        padUpdate(&pad);
        u64 buttonPressed = padGetButtons(&pad);
        u64 buttonDown = padGetButtonsDown(&pad);
        HidAnalogStickState stick1 = padGetStickPos(&pad, 0);
        HidAnalogStickState stick2 = padGetStickPos(&pad, 1);

        hidIsVibrationDeviceMounted(vibrationDeviceHandle, &isVibrationPermitted);
        hidGetActualVibrationValue(vibrationDeviceHandle, &vibrationValueRead);

        // Print pad information
        snprintf(outputBuffer, sizeof(outputBuffer), "Button: [%s] Stick1 [%06d, %06d] Stick2 [%06d, %06d] Vib [%d/%d (%s)]                                                       ",
                 buttonToStr(buttonPressed).c_str(),
                 stick1.x, stick1.y,
                 stick2.x, stick2.y,
                 (uint8_t)(current_vibration * 100),
                 (uint8_t)(vibrationValueRead.amp_low * 100),
                 isVibrationPermitted ? "Permitted" : "Not Permitted");

        outputBuffer[console->consoleWidth] = '\0';
        printf("\x1b[7;1H%s", outputBuffer);

        HidSixAxisSensorState sixAxis{};
        hidGetSixAxisSensorStates(sixAxisHandle, &sixAxis, 1);
        snprintf(outputBuffer, sizeof(outputBuffer), "Acc [%+.2f %+.2f %+.2f] Gyro [%+.2f %+.2f %+.2f] Angle [%+.2f %+.2f %+.2f]                              ",
                 sixAxis.acceleration.x, sixAxis.acceleration.y, sixAxis.acceleration.z,
                 sixAxis.angular_velocity.x, sixAxis.angular_velocity.y, sixAxis.angular_velocity.z,
                 sixAxis.angle.x, sixAxis.angle.y, sixAxis.angle.z);
        outputBuffer[console->consoleWidth] = '\0';
        printf("\x1b[8;1H%s", outputBuffer);

        HidMouseState mouse{};
        hidGetMouseStates(&mouse, 1);
        snprintf(outputBuffer, sizeof(outputBuffer), "Mouse: %s Pos [%04d, %04d] Delta [%+04d, %+04d] Wheel [%+04d] Button: [%s]                              ",
                 (mouse.attributes & HidMouseAttribute_IsConnected) ? "Connected" : "Disconnected",
                 mouse.x, mouse.y,
                 mouse.delta_x, mouse.delta_y,
                 mouse.wheel_delta_x,
                 mouseButtonToStr(mouse.buttons).c_str());
        outputBuffer[console->consoleWidth] = '\0';
        printf("\x1b[10;1H%s", outputBuffer);

        HidKeyboardState keyboard{};
        hidGetKeyboardStates(&keyboard, 1);
        keyDowns += countKeyDowns(previousKeyboard, keyboard);
        previousKeyboard = keyboard;
        // The upper half of the modifiers word is hid's keyboard attribute; bit 0 is IsConnected.
        snprintf(outputBuffer, sizeof(outputBuffer), "Keyboard: %s Modifiers [%04X] Key downs [%d] Held: [%s]                              ",
                 ((keyboard.modifiers >> 32) & 1) ? "Connected" : "Disconnected",
                 (u32)keyboard.modifiers,
                 keyDowns,
                 heldKeysToStr(keyboard).c_str());
        outputBuffer[console->consoleWidth] = '\0';
        printf("\x1b[11;1H%s", outputBuffer);

        constexpr int PointerFirstRow = 13;
        const int row = PointerFirstRow + std::clamp(mouse.y, 0, 719) * (console->consoleHeight - PointerFirstRow) / 720;
        const int column = 1 + std::clamp(mouse.x, 0, 1279) * console->consoleWidth / 1280;
        if (pointerRow != 0 && (row != pointerRow || column != pointerColumn))
            printf("\x1b[%d;%dH ", pointerRow, pointerColumn);
        printf("\x1b[%d;%dH%c", row, column, mouse.buttons ? '#' : 'X');
        pointerRow = row;
        pointerColumn = column;

        if (buttonDown & HidNpadButton_Plus)
            current_vibration = std::min(current_vibration + 0.1, 1.0);
        if (buttonDown & HidNpadButton_Minus)
            current_vibration = std::max(current_vibration - 0.1, 0.0);

        if ((buttonPressed & HidNpadButton_Plus) && (buttonPressed & HidNpadButton_Minus))
            break;

        vibrationValue.amp_low = current_vibration;
        vibrationValue.amp_high = current_vibration;
        hidSendVibrationValue(vibrationDeviceHandle, &vibrationValue);

        consoleUpdate(console);

        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Avoid 100% CPU usage
    }

    hidStopSixAxisSensor(sixAxisHandle);
    consoleExit(console);
    return 0;
}