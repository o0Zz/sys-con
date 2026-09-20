#include "SwitchUSBInterface.h"
#include "SwitchUSBEndpoint.h"
#include "SwitchUSBLock.h"
#include "SwitchLogger.h"
#include <malloc.h>
#include <cstring>

// ControllerLib lives in namespace controllerlib. Pulled in here rather than at
// namespace scope in a header, so including a sys-con header does not drag the
// library into the global namespace of everything downstream.
using namespace controllerlib;


SwitchUSBInterface::SwitchUSBInterface(UsbHsInterface &interface)
    : m_interface(interface)
{
}

SwitchUSBInterface::~SwitchUSBInterface()
{
}

Status SwitchUSBInterface::Open()
{
    SwitchUSBLock usbLock;

    ::syscon::logger::LogDebug("SwitchUSBInterface[%04x-%04x] Openning ...", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct);

    Result rc = usbHsAcquireUsbIf(&m_session, &m_interface);
    if (R_FAILED(rc))
    {
        ::syscon::logger::LogError("SwitchUSBInterface[%04x-%04x] Failed to acquire USB interface - Error: 0x%X (Module: 0x%X, Desc: 0x%X) !", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct, rc, R_MODULE(rc), R_DESCRIPTION(rc));
        return Status::UsbInterfaceAcquire;
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

    return Status::Success;
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

Status SwitchUSBInterface::ControlTransferInput(u8 bmRequestType, u8 bmRequest, u16 wValue, u16 wIndex, void *buffer, u16 *wLength)
{
    ::syscon::logger::LogDebug("SwitchUSBInterface[%04x-%04x] ControlTransferInput (bmRequestType=0x%02X, bmRequest=0x%02X, wValue=0x%04X, wIndex=0x%04X, wLength=%d)...", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct, bmRequestType, bmRequest, wValue, wIndex, *wLength);

    if (!(bmRequestType & USB_ENDPOINT_IN))
    {
        ::syscon::logger::LogError("SwitchUSBInterface[%04x-%04x] ControlTransferInput: Trying to output data !", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct);
        return Status::InvalidArgument;
    }

    u32 transferredSize = 0;
    Result rc;
    bool copied = false;

    {
        SwitchUSBLock usbLock;

        rc = usbHsIfCtrlXfer(&m_session, bmRequestType, bmRequest, wValue, wIndex, *wLength, m_usb_buffer, &transferredSize);
        if (R_SUCCEEDED(rc) && buffer != NULL && *wLength >= transferredSize)
        {
            memcpy(buffer, m_usb_buffer, transferredSize);
            copied = true;
        }
    }

    if (R_FAILED(rc))
    {
        ::syscon::logger::LogError("SwitchUSBInterface[%04x-%04x] ControlTransferInput: Failed to read data !", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct);
        return Status::UnknownError;
    }

    if (!copied)
    {
        ::syscon::logger::LogError("SwitchUSBInterface[%04x-%04x] ControlTransferInput: Invalid buffer size !", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct);
        return Status::InvalidArgument;
    }

    *wLength = transferredSize;

    return Status::Success;
}

Status SwitchUSBInterface::ControlTransferOutput(u8 bmRequestType, u8 bmRequest, u16 wValue, u16 wIndex, const void *buffer, u16 wLength)
{
    ::syscon::logger::LogDebug("SwitchUSBInterface[%04x-%04x] ControlTransferOutput (bmRequestType=0x%02X, bmRequest=0x%02X, wValue=0x%04X, wIndex=0x%04X, wLength=%d)...", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct, bmRequestType, bmRequest, wValue, wIndex, wLength);

    u32 transferredSize = 0;

    if (bmRequestType & USB_ENDPOINT_IN)
    {
        ::syscon::logger::LogError("SwitchUSBInterface[%04x-%04x] ControlTransferOutput Trying to read data !", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct);
        return Status::InvalidArgument;
    }

    Result rc;

    {
        SwitchUSBLock usbLock;

        if (buffer != NULL && wLength > 0)
            memcpy(m_usb_buffer, buffer, wLength);

        rc = usbHsIfCtrlXfer(&m_session, bmRequestType, bmRequest, wValue, wIndex, wLength, m_usb_buffer, &transferredSize);
    }

    if (R_FAILED(rc))
    {
        ::syscon::logger::LogError("SwitchUSBInterface[%04x-%04x] ControlTransferOutput: Failed to send data !", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct);
        return Status::UnknownError;
    }

    return Status::Success;
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

Status SwitchUSBInterface::Reset()
{
    ::syscon::logger::LogDebug("SwitchUSBInterface[%04x-%04x] Reset...", m_interface.device_desc.idVendor, m_interface.device_desc.idProduct);

    SwitchUSBLock usbLock;
    usbHsIfResetDevice(&m_session);

    return Status::Success;
}