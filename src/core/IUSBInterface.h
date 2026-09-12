#pragma once
#include "IUSBEndpoint.h"

class IUSBInterface
{
protected:
public:
    struct InterfaceDescriptor
    {
        uint8_t bLength;
        uint8_t bDescriptorType;   ///< Must match USB_DT_INTERFACE.
        uint8_t bInterfaceNumber;  ///< See also USBDS_DEFAULT_InterfaceNumber.
        uint8_t bAlternateSetting; ///< Must match 0.
        uint8_t bNumEndpoints;
        uint8_t bInterfaceClass;
        uint8_t bInterfaceSubClass;
        uint8_t bInterfaceProtocol;
        uint8_t iInterface; ///< Ignored.
    };
    virtual ~IUSBInterface() = default;

    virtual ControllerResult Open() = 0;
    virtual void Close() = 0;

    virtual ControllerResult ControlTransferInput(uint8_t bmRequestType, uint8_t bmRequest, uint16_t wValue, uint16_t wIndex, void *buffer, uint16_t *wLength) = 0;
    virtual ControllerResult ControlTransferOutput(uint8_t bmRequestType, uint8_t bmRequest, uint16_t wValue, uint16_t wIndex, const void *buffer, uint16_t wLength) = 0;

    virtual IUSBEndpoint *GetEndpoint(IUSBEndpoint::Direction direction, uint8_t index) = 0;
    virtual InterfaceDescriptor *GetDescriptor() = 0;

    virtual ControllerResult Reset() = 0;
};