#include "SwitchUSBInterface.h"
#include "SwitchUSBEndpoint.h"
#include "SwitchUSBLock.h"
#include "SwitchLogger.h"
#include <malloc.h>
#include <cstring>

SwitchUSBInterface::SwitchUSBInterface(UsbHsInterface &interface)
    : m_interface(interface)
{
}

SwitchUSBInterface::~SwitchUSBInterface()
{
}

ControllerResult SwitchUSBInterface::Open()
{
    SwitchUSBLock usbLock;

    ::syscon::logger::LogDebug("SwitchUSBInterface[%04x-%04x] Openning ...", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct);

    Result rc = usbHsAcquireUsbIf(&m_session, &m_interface);
    if (R_FAILED(rc))
    {
        ::syscon::logger::LogError("SwitchUSBInterface[%04x-%04x] Failed to acquire USB interface - Error: 0x%X (Module: 0x%X, Desc: 0x%X) !", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct, rc, R_MODULE(rc), R_DESCRIPTION(rc));
        return CONTROLLER_STATUS_USB_INTERFACE_ACQUIRE;
    }

    for (int i = 0; i < SWITCH_USB_MAX_ENDPOINTS; i++)
    {
        usb_endpoint_descriptor &epdesc = m_session.inf.inf.input_endpoint_descs[i];
        if (epdesc.bLength != 0)
        {
            ::syscon::logger::LogDebug("SwitchUSBInterface[%04x-%04x] Input endpoint found 0x%x (Idx: %d)", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct, epdesc.bEndpointAddress, i);
            m_inEndpoints[i] = std::make_unique<SwitchUSBEndpoint>(m_session, epdesc);
        }
        else
        {
            //::syscon::logger::LogWarning("SwitchUSBInterface[%04x-%04x] Input endpoint %d is null", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct, i);
        }
    }

    for (int i = 0; i < SWITCH_USB_MAX_ENDPOINTS; i++)
    {
        usb_endpoint_descriptor &epdesc = m_session.inf.inf.output_endpoint_descs[i];
        if (epdesc.bLength != 0)
        {
            ::syscon::logger::LogDebug("SwitchUSBInterface[%04x-%04x] Output endpoint found 0x%x (Idx: %d)", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct, epdesc.bEndpointAddress, i);
            m_outEndpoints[i] = std::make_unique<SwitchUSBEndpoint>(m_session, epdesc);
        }
        else
        {
            //::syscon::logger::LogWarning("SwitchUSBInterface[%04x-%04x] Output endpoint %d is null", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct, i);
        }
    }

    return CONTROLLER_STATUS_SUCCESS;
}

void SwitchUSBInterface::Close()
{
    ::syscon::logger::LogDebug("SwitchUSBInterface[%04x-%04x] Closing...", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct);

    SwitchUSBLock usbLock;

    for (int i = 0; i < SWITCH_USB_MAX_ENDPOINTS; i++)
    {
        if (m_inEndpoints[i])
            m_inEndpoints[i]->Close();
        if (m_outEndpoints[i])
            m_outEndpoints[i]->Close();
    }

    usbHsIfClose(&m_session);

    ::syscon::logger::LogDebug("SwitchUSBInterface[%04x-%04x] Closed !", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct);
}

ControllerResult SwitchUSBInterface::ControlTransferInput(u8 bmRequestType, u8 bmRequest, u16 wValue, u16 wIndex, void *buffer, u16 *wLength)
{
    SwitchUSBLock usbLock;

    ::syscon::logger::LogDebug("SwitchUSBInterface[%04x-%04x] ControlTransferInput (bmRequestType=0x%02X, bmRequest=0x%02X, wValue=0x%04X, wIndex=0x%04X, wLength=%d)...", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct, bmRequestType, bmRequest, wValue, wIndex, *wLength);

    if (!(bmRequestType & USB_ENDPOINT_IN))
    {
        ::syscon::logger::LogError("SwitchUSBInterface[%04x-%04x] ControlTransferInput: Trying to output data !", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct);
        return CONTROLLER_STATUS_INVALID_ARGUMENT;
    }

    u32 transferredSize = 0;

    if (R_FAILED(usbHsIfCtrlXfer(&m_session, bmRequestType, bmRequest, wValue, wIndex, *wLength, m_usb_buffer, &transferredSize)))
    {
        ::syscon::logger::LogError("SwitchUSBInterface[%04x-%04x] ControlTransferInput: Failed to read data !", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct);
        return CONTROLLER_STATUS_UNKNOWN_ERROR;
    }

    if (bmRequestType & USB_ENDPOINT_IN)
    {
        if (buffer != NULL && *wLength >= transferredSize)
        {
            memcpy(buffer, m_usb_buffer, transferredSize);
            *wLength = transferredSize;
        }
        else
        {
            ::syscon::logger::LogError("SwitchUSBInterface[%04x-%04x] ControlTransferInput: Invalid buffer size !", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct);
            return CONTROLLER_STATUS_INVALID_ARGUMENT;
        }
    }

    return CONTROLLER_STATUS_SUCCESS;
}

ControllerResult SwitchUSBInterface::ControlTransferOutput(u8 bmRequestType, u8 bmRequest, u16 wValue, u16 wIndex, const void *buffer, u16 wLength)
{
    SwitchUSBLock usbLock;

    ::syscon::logger::LogDebug("SwitchUSBInterface[%04x-%04x] ControlTransferOutput (bmRequestType=0x%02X, bmRequest=0x%02X, wValue=0x%04X, wIndex=0x%04X, wLength=%d)...", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct, bmRequestType, bmRequest, wValue, wIndex, wLength);

    u32 transferredSize = 0;

    if (bmRequestType & USB_ENDPOINT_IN)
    {
        ::syscon::logger::LogError("SwitchUSBInterface[%04x-%04x] ControlTransferOutput Trying to read data !", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct);
        return CONTROLLER_STATUS_INVALID_ARGUMENT;
    }

    if (buffer != NULL && wLength > 0)
        memcpy(m_usb_buffer, buffer, wLength);

    if (R_FAILED(usbHsIfCtrlXfer(&m_session, bmRequestType, bmRequest, wValue, wIndex, wLength, m_usb_buffer, &transferredSize)))
    {
        ::syscon::logger::LogError("SwitchUSBInterface[%04x-%04x] ControlTransferOutput: Failed to send data !", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct);
        return CONTROLLER_STATUS_UNKNOWN_ERROR;
    }

    return CONTROLLER_STATUS_SUCCESS;
}

IUSBEndpoint *SwitchUSBInterface::GetEndpoint(IUSBEndpoint::Direction direction, uint8_t index)
{
    if (index >= SWITCH_USB_MAX_ENDPOINTS)
        return NULL;

    if (direction == IUSBEndpoint::USB_ENDPOINT_IN)
        return m_inEndpoints[index].get();
    else
        return m_outEndpoints[index].get();
}

ControllerResult SwitchUSBInterface::Reset()
{
    SwitchUSBLock usbLock;

    ::syscon::logger::LogDebug("SwitchUSBInterface[%04x-%04x] Reset...", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct);

    usbHsIfResetDevice(&m_session);

    return CONTROLLER_STATUS_SUCCESS;
}