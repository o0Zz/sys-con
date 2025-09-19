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
    /*if (ButtonMask & HidNpadButton_StickLLeft)
        buttonStr += "StickLLeft ";
    if (ButtonMask & HidNpadButton_StickLUp)
        buttonStr += "StickLUp ";
    if (ButtonMask & HidNpadButton_StickLRight)
        buttonStr += "StickLRight ";
    if (ButtonMask & HidNpadButton_StickLDown)
        buttonStr += "StickLDown ";
    if (ButtonMask & HidNpadButton_StickRLeft)
        buttonStr += "StickRLeft ";
    if (ButtonMask & HidNpadButton_StickRUp)
        buttonStr += "StickRUp ";
    if (ButtonMask & HidNpadButton_StickRRight)
        buttonStr += "StickRRight ";
    if (ButtonMask & HidNpadButton_StickRDown)
        buttonStr += "StickRDown ";
    if (ButtonMask & HidNpadButton_LeftSL)
        buttonStr += "LeftSL ";
    if (ButtonMask & HidNpadButton_LeftSR)
        buttonStr += "LeftSR ";
    if (ButtonMask & HidNpadButton_RightSL)
        buttonStr += "RightSL ";
    if (ButtonMask & HidNpadButton_RightSR)
        buttonStr += "RightSR ";
    */

    buttonStr.erase(buttonStr.find_last_not_of(' ') + 1);

    return buttonStr;
}

int main()
{
    char outputBuffer[256];
    bool isVibrationPermitted = false;
    HidVibrationDeviceHandle vibrationDeviceHandle;
    HidVibrationValue vibrationValue;
    HidVibrationValue vibrationValueRead;
    PadState pad;

    PrintConsole *console = consoleInit(NULL);

    padConfigureInput(8, HidNpadStyleSet_NpadStandard);

    padInitializeAny(&pad);

    hidSetNpadHandheldActivationMode(HidNpadHandheldActivationMode_Single);

    if (R_FAILED(hidInitializeVibrationDevices(&vibrationDeviceHandle, 1, HidNpadIdType_No1, HidNpadStyleTag_NpadFullKey)))
        printf("ERR: hidInitializeVibrationDevices failed !\n");

    float current_vibration = 0.0;

    vibrationValue.amp_low = 0.0;   // Max 1.0
    vibrationValue.freq_low = 220;  // hz
    vibrationValue.amp_high = 0.0;  // Max 1.0
    vibrationValue.freq_high = 220; // hz

    printf("Welcome to sys-con debug application\n");
    printf("Press + to increase vibration (On controller No1)\n");
    printf("Press - to decrease vibration (On controller No1)\n");
    printf("Press + and - to exit\n");
    printf("\n");

    while (appletMainLoop())
    {

        // Get current pad information (Button, stick, ...)
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

        outputBuffer[console->consoleWidth - 1] = '\r';
        outputBuffer[console->consoleWidth] = '\0';
        printf(outputBuffer);

        // Update vibrations
        if (buttonDown & HidNpadButton_Plus)
            current_vibration = std::min(current_vibration + 0.1, 1.0);
        if (buttonDown & HidNpadButton_Minus)
            current_vibration = std::max(current_vibration - 0.1, 0.0);

        if ((buttonPressed & HidNpadButton_Plus) && (buttonPressed & HidNpadButton_Minus))
            break;

        vibrationValue.amp_low = current_vibration;
        vibrationValue.amp_high = current_vibration;
        hidSendVibrationValue(vibrationDeviceHandle, &vibrationValue);

        // Update console
        consoleUpdate(console);

        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Avoid 100% CPU usage
    }

    consoleExit(console);
    return 0;
}