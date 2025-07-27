#pragma once
#include "ControllerResult.h"
#include <cstdint>
#include <cstddef>

class IUSBEndpoint
{
public:
    enum Direction : uint8_t
    {
        USB_ENDPOINT_IN = 0x80,
        USB_ENDPOINT_OUT = 0x00,
    };

    struct EndpointDescriptor
    {
        uint8_t bLength;
        uint8_t bDescriptorType;
        uint8_t bEndpointAddress;
        uint8_t bmAttributes;
        uint16_t wMaxPacketSize;
        uint8_t bInterval;
    };

    virtual ~IUSBEndpoint() = default;

    // Open and close the endpoint. if maxPacketSize is not set, it uses wMaxPacketSize from the descriptor.
    virtual ControllerResult Open(int maxPacketSize = 0) = 0;
    virtual void Close() = 0;

    // This will read from the inBuffer pointer for the specified size and write it to the endpoint.
    virtual ControllerResult Write(const uint8_t *inBuffer, size_t bufferSize) = 0;

    // This will read from the endpoint and put the data in the outBuffer pointer for the specified size.
    virtual ControllerResult Read(uint8_t *outBuffer, size_t *bufferSizeInOut, uint64_t aTimeoutUs) = 0;

    // Get endpoint's direction. (IN or OUT)
    virtual IUSBEndpoint::Direction GetDirection() = 0;
    // Get the endpoint descriptor
    virtual EndpointDescriptor *GetDescriptor() = 0;
};