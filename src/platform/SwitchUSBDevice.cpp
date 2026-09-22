#include "SwitchUSBDevice.h"
#include "SwitchLogger.h"
#include <cstring> //for memset

using namespace controllerlib;

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

Status SwitchUSBDevice::Open()
{
    if (m_interfaces.size() == 0)
        return Status::NoInterfaces;

    return Status::Success;
}

void SwitchUSBDevice::Close()
{
    for (auto &&interface : m_interfaces)
    {
        interface->Close();
    }
}
