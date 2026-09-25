#pragma once
#include <switch.h>
#include "IUSBDevice.h"
#include "SwitchUSBInterface.h"

class SwitchUSBDevice : public controllerlib::IUSBDevice
{
public:
    ~SwitchUSBDevice();

    SwitchUSBDevice(UsbHsInterface interfaces[], int length);

    // There are no devices to open on the switch, so instead this returns success if there are any interfaces
    virtual controllerlib::Status Open() override;
    virtual void Close() override;
};