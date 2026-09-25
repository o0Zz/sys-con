#pragma once
#include "IUSBEndpoint.h"

namespace controllerlib
{
    class IUSBInterface
    {
    protected:
    public:
        struct InterfaceDescriptor
        {
            uint8_t bLength;
            uint8_t bDescriptorType;
            uint8_t bInterfaceNumber;
            uint8_t bAlternateSetting;
            uint8_t bNumEndpoints;
            uint8_t bInterfaceClass;
            uint8_t bInterfaceSubClass;
            uint8_t bInterfaceProtocol;
            uint8_t iInterface;
        };
        virtual ~IUSBInterface() = default;

        virtual Status Open() = 0;
        virtual void Close() = 0;

        virtual Status ControlTransferInput(uint8_t bmRequestType, uint8_t bmRequest, uint16_t wValue, uint16_t wIndex, void *buffer, uint16_t *wLength) = 0;
        virtual Status ControlTransferOutput(uint8_t bmRequestType, uint8_t bmRequest, uint16_t wValue, uint16_t wIndex, const void *buffer, uint16_t wLength) = 0;

        virtual IUSBEndpoint *GetEndpoint(IUSBEndpoint::Direction direction, uint8_t index) = 0;
        virtual InterfaceDescriptor *GetDescriptor() = 0;
    };
} // namespace controllerlib
