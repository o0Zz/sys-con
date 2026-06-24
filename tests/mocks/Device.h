#pragma once
#include "IUSBDevice.h"
#include "USBInterface.h"

class MockDevice : public IUSBDevice
{
public:
    MockDevice(uint16_t vendorID = 0x000, uint16_t productID = 0x000, std::unique_ptr<IUSBInterface> &&interface = nullptr)
        : IUSBDevice()
    {
        m_vendorID = vendorID;
        m_productID = productID;
        m_interfaces.push_back(std::move(interface));
    }

    ControllerResult Open() { return CONTROLLER_STATUS_SUCCESS; }
    void Close() {}
    void Reset() {}
};
