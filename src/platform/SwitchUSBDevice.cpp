#include "SwitchUSBDevice.h"
#include "SwitchLogger.h"
#include <cstring> //for memset

// ControllerLib lives in namespace controllerlib. Pulled in here rather than at
// namespace scope in a header, so including a sys-con header does not drag the
// library into the global namespace of everything downstream.
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

SwitchUSBDevice::SwitchUSBDevice()
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

void SwitchUSBDevice::Reset()
{
    // I'm expecting all interfaces to point to one device decsriptor
    //  as such resetting on any of them should do the trick
    // TODO: needs testing
    if (m_interfaces.size() != 0)
        m_interfaces[0]->Reset();
}
