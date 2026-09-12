#include "SwitchUSBDevice.h"
#include "SwitchLogger.h"
#include <cstring> //for memset

SwitchUSBDevice::SwitchUSBDevice(UsbHsInterface interfaces[], int size)
{
    if (size > 0)
    {
        m_vendorID = interfaces[0].device_desc.idVendor;
        m_productID = interfaces[0].device_desc.idProduct;
    }

    for (int i = 0; i < size; i++)
        m_interfaces.push_back(std::make_unique<SwitchUSBInterface>(interfaces[i]));
}

SwitchUSBDevice::~SwitchUSBDevice()
{
}

SwitchUSBDevice::SwitchUSBDevice()
{
}

ControllerResult SwitchUSBDevice::Open()
{
    if (m_interfaces.size() == 0)
        return CONTROLLER_STATUS_NO_INTERFACES;

    return CONTROLLER_STATUS_SUCCESS;
}

void SwitchUSBDevice::Close()
{
    for (auto &&interface : m_interfaces)
    {
        interface->Close();
    }
}

void SwitchUSBDevice::Reset()
{
    // I'm expecting all interfaces to point to one device decsriptor
    //  as such resetting on any of them should do the trick
    // TODO: needs testing
    if (m_interfaces.size() != 0)
        m_interfaces[0]->Reset();
}
