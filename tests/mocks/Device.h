#pragma once
#include "IUSBDevice.h"
#include "USBInterface.h"

class MockDevice : public controllerlib::IUSBDevice
{
public:
    MockDevice(uint16_t vendorID = 0x000, uint16_t productID = 0x000, std::unique_ptr<controllerlib::IUSBInterface> &&interface = nullptr)
        : IUSBDevice()
    {
        m_vendorID = vendorID;
        m_productID = productID;
        m_interfaces.push_back(std::move(interface));
    }

    controllerlib::Status Open() { return controllerlib::Status::Success; }
    void Close() {}
};
