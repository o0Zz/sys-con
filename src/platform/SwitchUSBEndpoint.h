#pragma once
#include <switch.h>
#include "IUSBEndpoint.h"
#include <memory>

class SwitchUSBEndpoint : public controllerlib::IUSBEndpoint
{
private:
    UsbHsClientEpSession m_epSession{};
    UsbHsClientIfSession *m_ifSession;
    usb_endpoint_descriptor *m_descriptor;
    u32 m_xferIdRead = 0;
    u32 m_readSize = 0;
    alignas(0x1000) u8 m_usb_buffer_in[512];
    alignas(0x1000) u8 m_usb_buffer_out[512];

    controllerlib::Status PostRead();

public:
    // Pass the necessary information to be able to open the endpoint
    SwitchUSBEndpoint(UsbHsClientIfSession &if_session, usb_endpoint_descriptor &desc);
    ~SwitchUSBEndpoint();

    // Open and close the endpoint
    virtual controllerlib::Status Open(int maxPacketSize = 0) override;
    virtual void Close() override;

    // buffer should point to the data array, and only the specified size will be read.
    virtual controllerlib::Status Write(const uint8_t *inBuffer, size_t bufferSize) override;

    // The data received will be put in the outBuffer array for the length of the specified size.
    virtual controllerlib::Status Read(uint8_t *outBuffer, size_t *bufferSizeInOut, u64 aTimeoutUs) override;

    // Gets the direction of this endpoint (IN or OUT)
    virtual controllerlib::IUSBEndpoint::Direction GetDirection() override;

    // get the endpoint descriptor
    virtual controllerlib::IUSBEndpoint::EndpointDescriptor *GetDescriptor() override;
};