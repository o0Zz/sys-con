#pragma once
#include "Status.h"
#include <cstdint>
#include <cstddef>

namespace controllerlib
{
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
        virtual Status Open(int maxPacketSize = 0) = 0;
        virtual void Close() = 0;

        /* Post a pending IN transfer so the endpoint captures the device's very next report.
           Called after the driver's init writes complete: if we armed at Open, the pad's LED
           ack/first input can complete in the window between arming and the next OUT write and
           wedge that OUT for tens of seconds. Default no-op for OUT endpoints and hosts that
           don't need explicit arming. */
        virtual Status ArmForRead() { return Status::Success; }

        // This will read from the inBuffer pointer for the specified size and write it to the endpoint.
        virtual Status Write(const uint8_t *inBuffer, size_t bufferSize) = 0;

        // This will read from the endpoint and put the data in the outBuffer pointer for the specified size.
        virtual Status Read(uint8_t *outBuffer, size_t *bufferSizeInOut, uint64_t aTimeoutUs) = 0;

        // Get endpoint's direction. (IN or OUT)
        virtual IUSBEndpoint::Direction GetDirection() = 0;
        // Get the endpoint descriptor
        virtual EndpointDescriptor *GetDescriptor() = 0;
    };
} // namespace controllerlib
