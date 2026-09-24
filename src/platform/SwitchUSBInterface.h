#pragma once
#include "SwitchUSBEndpoint.h"
#include "IUSBInterface.h"
#include <memory>

#define SWITCH_USB_MAX_ENDPOINTS 15

class SwitchUSBInterface : public controllerlib::IUSBInterface
{
private:
    UsbHsClientIfSession m_session;
    UsbHsInterface m_interface;
    std::unique_ptr<controllerlib::IUSBEndpoint> m_inEndpoints[SWITCH_USB_MAX_ENDPOINTS];
    std::unique_ptr<controllerlib::IUSBEndpoint> m_outEndpoints[SWITCH_USB_MAX_ENDPOINTS];
    alignas(0x1000) u8 m_usb_buffer[0x1000];

public:
    // Pass the specified interface to allow for opening the session
    SwitchUSBInterface(UsbHsInterface &interface);
    ~SwitchUSBInterface();

    // Open and close the interface
    virtual controllerlib::Status Open() override;
    virtual void Close() override;

    virtual controllerlib::Status ControlTransferInput(u8 bmRequestType, u8 bmRequest, u16 wValue, u16 wIndex, void *buffer, u16 *wLength) override;
    virtual controllerlib::Status ControlTransferOutput(u8 bmRequestType, u8 bmRequest, u16 wValue, u16 wIndex, const void *buffer, u16 wLength) override;

    // There are a total of 15 endpoints on a switch interface for each direction, get them by passing the desired parameters
    virtual controllerlib::IUSBEndpoint *GetEndpoint(controllerlib::IUSBEndpoint::Direction direction, uint8_t index) override;

    // Get the unique session ID for this interface
    inline s32 GetID() { return m_session.ID; }
    virtual controllerlib::IUSBInterface::InterfaceDescriptor *GetDescriptor() override { return reinterpret_cast<controllerlib::IUSBInterface::InterfaceDescriptor *>(&m_interface.inf.interface_desc); }
};