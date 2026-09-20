#include "SwitchUSBEndpoint.h"
#include "SwitchUSBLock.h"
#include "SwitchLogger.h"
#include <cstring>
#include <malloc.h>

// ControllerLib lives in namespace controllerlib. Pulled in here rather than at
// namespace scope in a header, so including a sys-con header does not drag the
// library into the global namespace of everything downstream.
using namespace controllerlib;


SwitchUSBEndpoint::SwitchUSBEndpoint(UsbHsClientIfSession &if_session, usb_endpoint_descriptor &desc)
    : m_ifSession(&if_session),
      m_descriptor(&desc)
{
}

SwitchUSBEndpoint::~SwitchUSBEndpoint()
{
}

Status SwitchUSBEndpoint::Open(int maxPacketSize)
{
    maxPacketSize = maxPacketSize != 0 ? maxPacketSize : m_descriptor->wMaxPacketSize;

    ::syscon::logger::LogDebug("SwitchUSBEndpoint[0x%02X] Opening (Pkt size: %d)...", m_descriptor->bEndpointAddress, maxPacketSize);

    Result rc;
    {
        SwitchUSBLock usbLock;
        rc = usbHsIfOpenUsbEp(m_ifSession, &m_epSession, 1, maxPacketSize, m_descriptor);
    }

    if (R_FAILED(rc))
    {
        ::syscon::logger::LogError("SwitchUSBEndpoint[0x%02X] Failed to open: 0x%08X (Module: 0x%X, Desc: 0x%X)", m_descriptor->bEndpointAddress, rc, R_MODULE(rc), R_DESCRIPTION(rc));
        return Status::UsbEndpointOpen;
    }

    ::syscon::logger::LogDebug("SwitchUSBEndpoint[0x%02X] Successfully opened !", m_descriptor->bEndpointAddress);

    return Status::Success;
}

void SwitchUSBEndpoint::Close()
{
    SwitchUSBLock usbLock;

    usbHsEpClose(&m_epSession);
}

Status SwitchUSBEndpoint::Write(const uint8_t *inBuffer, size_t bufferSize)
{
    if (GetDirection() == USB_ENDPOINT_IN)
        ::syscon::logger::LogError("SwitchUSBEndpoint[0x%02X] Trying to write an INPUT endpoint!", m_descriptor->bEndpointAddress);

    ::syscon::logger::LogTrace("SwitchUSBEndpoint[0x%02X] Write %d bytes", m_descriptor->bEndpointAddress, bufferSize);
    ::syscon::logger::LogBuffer(LogLevel::Trace, inBuffer, bufferSize);

    Result rc;
    {
        SwitchUSBLock usbLock;
        u32 transferredSize = 0;

        memcpy(m_usb_buffer_out, inBuffer, bufferSize);
        rc = usbHsEpPostBuffer(&m_epSession, m_usb_buffer_out, bufferSize, &transferredSize);
    }

    if (R_FAILED(rc))
    {
        ::syscon::logger::LogError("SwitchUSBEndpoint[0x%02X] Write failed: 0x%08X (Module: 0x%X, Desc: 0x%X)", m_descriptor->bEndpointAddress, rc, R_MODULE(rc), R_DESCRIPTION(rc));
        return Status::WriteFailed;
    }

    /* bInterval pacing belongs to this endpoint: sleeping under the global USB lock stalls every
       other controller's polling thread for the whole interval. */
    svcSleepThread(m_descriptor->bInterval * 1000000);

    return Status::Success;
}

Status SwitchUSBEndpoint::Read(uint8_t *outBuffer, size_t *bufferSizeInOut, u64 aTimeoutUs)
{
    if (GetDirection() == USB_ENDPOINT_OUT)
        ::syscon::logger::LogError("SwitchUSBEndpoint[0x%02X] Trying to read an OUTPUT endpoint!", m_descriptor->bEndpointAddress);

    if (aTimeoutUs == UINT64_MAX)
        return ReadSync(outBuffer, bufferSizeInOut);

    return ReadAsync(outBuffer, bufferSizeInOut, aTimeoutUs);
}

Status SwitchUSBEndpoint::ReadSync(uint8_t *outBuffer, size_t *bufferSizeInOut)
{
    Result rc;
    u32 transferredSize = 0;

    {
        SwitchUSBLock usbLock;

        rc = usbHsEpPostBuffer(&m_epSession, m_usb_buffer_in, *bufferSizeInOut, &transferredSize);
        if (R_SUCCEEDED(rc))
            memcpy(outBuffer, m_usb_buffer_in, transferredSize);
    }

    if (R_FAILED(rc))
    {
        ::syscon::logger::LogError("SwitchUSBEndpoint[0x%02X] ReadSync post failed: 0x%08X", m_descriptor->bEndpointAddress, rc);
        return Status::ReadFailed;
    }

    *bufferSizeInOut = transferredSize;

    if (transferredSize == 0)
    {
        ::syscon::logger::LogDebug("SwitchUSBEndpoint[0x%02X] ReadSync returned no data !", m_descriptor->bEndpointAddress);
        return Status::NoDataAvailable;
    }

    ::syscon::logger::LogTrace("SwitchUSBEndpoint[0x%02X] ReadSync %d bytes", m_descriptor->bEndpointAddress, *bufferSizeInOut);
    ::syscon::logger::LogBuffer(LogLevel::Trace, outBuffer, *bufferSizeInOut);

    return Status::Success;
}

Status SwitchUSBEndpoint::ReadAsync(uint8_t *outBuffer, size_t *bufferSizeInOut, u64 aTimeoutUs)
{
    u32 count = 0;
    UsbHsXferReport report;
    u32 tmpXcferId = 0;
    Result rc;

    if (m_xferIdRead == 0)
    {
        {
            SwitchUSBLock usbLock;
            rc = usbHsEpPostBufferAsync(&m_epSession, m_usb_buffer_in, *bufferSizeInOut, 0, &m_xferIdRead);
        }

        if (R_FAILED(rc))
        {
            ::syscon::logger::LogError("SwitchUSBEndpoint[0x%02X] ReadAsync post failed: 0x%08X", m_descriptor->bEndpointAddress, rc);
            return Status::ReadFailed;
        }
    }

    /* eventWait/eventClear are syscalls on the endpoint's own event, not usbHs IPC. Holding the
       global USB lock across the wait would pin it for the whole polling timeout and starve every
       other controller and the discovery thread. */
    if (R_FAILED(eventWait(usbHsEpGetXferEvent(&m_epSession), aTimeoutUs * 1000)))
        return Status::Timeout;

    eventClear(usbHsEpGetXferEvent(&m_epSession));

    tmpXcferId = m_xferIdRead;
    m_xferIdRead = 0;

    memset(&report, 0, sizeof(report));
    {
        SwitchUSBLock usbLock;
        rc = usbHsEpGetXferReport(&m_epSession, &report, 1, &count);
        if (R_SUCCEEDED(rc) && (count > 0) && (tmpXcferId == report.xferId))
            memcpy(outBuffer, m_usb_buffer_in, report.transferredSize);
    }

    if (R_FAILED(rc))
    {
        ::syscon::logger::LogError("SwitchUSBEndpoint[0x%02X] ReadAsync report failed: 0x%08X", m_descriptor->bEndpointAddress, rc);
        return Status::ReadFailed;
    }

    if ((count <= 0) || (tmpXcferId != report.xferId))
    {
        ::syscon::logger::LogTrace("SwitchUSBEndpoint[0x%02X] ReadAsync no report (Count: %d, xferId %d/%d)", m_descriptor->bEndpointAddress, count, tmpXcferId, report.xferId);
        return Status::NoDataAvailable;
    }

    *bufferSizeInOut = report.transferredSize;

    if (report.transferredSize == 0)
        return Status::NoDataAvailable;

    ::syscon::logger::LogTrace("SwitchUSBEndpoint[0x%02X] ReadAsync %d bytes", m_descriptor->bEndpointAddress, *bufferSizeInOut);
    ::syscon::logger::LogBuffer(LogLevel::Trace, outBuffer, *bufferSizeInOut);

    if (R_FAILED(report.res))
    {
        ::syscon::logger::LogError("SwitchUSBEndpoint[0x%02X] ReadAsync transfer failed: 0x%08X", m_descriptor->bEndpointAddress, report.res);
        return Status::ReadFailed;
    }

    return Status::Success;
}

IUSBEndpoint::Direction SwitchUSBEndpoint::GetDirection()
{
    return ((m_descriptor->bEndpointAddress & USB_ENDPOINT_IN) ? USB_ENDPOINT_IN : USB_ENDPOINT_OUT);
}

IUSBEndpoint::EndpointDescriptor *SwitchUSBEndpoint::GetDescriptor()
{
    return reinterpret_cast<IUSBEndpoint::EndpointDescriptor *>(m_descriptor);
}