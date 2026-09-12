#pragma once
#include "SwitchUSBEndpoint.h"
#include "IUSBInterface.h"
#include <memory>

class IUSBEndpoint;

#define SWITCH_USB_MAX_ENDPOINTS 15

class SwitchUSBInterface : public IUSBInterface
{
private:
    UsbHsClientIfSession m_session;
    UsbHsInterface m_interface;
    std::unique_ptr<IUSBEndpoint> m_inEndpoints[SWITCH_USB_MAX_ENDPOINTS];
    std::unique_ptr<IUSBEndpoint> m_outEndpoints[SWITCH_USB_MAX_ENDPOINTS];
    alignas(0x1000) u8 m_usb_buffer[0x1000];

public:
    // Pass the specified interface to allow for opening the session
    SwitchUSBInterface(UsbHsInterface &interface);
    ~SwitchUSBInterface();

    // Open and close the interface
    virtual ControllerResult Open() override;
    virtual void Close() override;

    virtual ControllerResult ControlTransferInput(u8 bmRequestType, u8 bmRequest, u16 wValue, u16 wIndex, void *buffer, u16 *wLength) override;
    virtual ControllerResult ControlTransferOutput(u8 bmRequestType, u8 bmRequest, u16 wValue, u16 wIndex, const void *buffer, u16 wLength) override;

    // There are a total of 15 endpoints on a switch interface for each direction, get them by passing the desired parameters
    virtual IUSBEndpoint *GetEndpoint(IUSBEndpoint::Direction direction, uint8_t index) override;

    // Reset the device
    virtual ControllerResult Reset() override;

    // Get the unique session ID for this interface
    inline s32 GetID() { return m_session.ID; }
    // Get the raw interface
    inline UsbHsInterface &GetInterface() { return m_interface; }
    // Get the raw session
    inline UsbHsClientIfSession &GetSession() { return m_session; }

    virtual InterfaceDescriptor *GetDescriptor() override { return reinterpret_cast<InterfaceDescriptor *>(&m_interface.inf.interface_desc); }
};