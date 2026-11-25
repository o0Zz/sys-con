#pragma once
#include <switch.h>
#include "IUSBEndpoint.h"
#include "PageAligned.h"
#include <memory>

class SwitchUSBEndpoint : public controllerlib::IUSBEndpoint
{
private:
    UsbHsClientEpSession m_epSession{};
    UsbHsClientIfSession *m_ifSession;
    usb_endpoint_descriptor *m_descriptor;
    u32 m_xferIdRead = 0;
    u32 m_readSize = 0;
    // IN endpoints only: a posted read stays in flight outside the USB lock.
    std::unique_ptr<PageAligned<0x1000>> m_readBuffer;

    controllerlib::Status PostRead();

public:
    SwitchUSBEndpoint(UsbHsClientIfSession &if_session, usb_endpoint_descriptor &desc);
    ~SwitchUSBEndpoint();

    virtual controllerlib::Status Open(int maxPacketSize = 0) override;
    virtual void Close() override;
    virtual controllerlib::Status Write(const uint8_t *inBuffer, size_t bufferSize) override;
    virtual controllerlib::Status Read(uint8_t *outBuffer, size_t *bufferSizeInOut, u64 aTimeoutUs) override;
    virtual controllerlib::IUSBEndpoint::Direction GetDirection() override;
    virtual controllerlib::IUSBEndpoint::EndpointDescriptor *GetDescriptor() override;
};