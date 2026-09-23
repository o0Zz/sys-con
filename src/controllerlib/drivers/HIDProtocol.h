#pragma once

#include "IUSBInterface.h"
#include "Status.h"

#include <cstdint>

// https://www.usb.org/sites/default/files/documents/hid1_11.pdf
namespace controllerlib::hid
{
    enum Protocol : uint8_t
    {
        PROTOCOL_BOOT = 0,
        PROTOCOL_REPORT = 1,
    };

    enum LedFlag : uint8_t
    {
        LED_NUM_LOCK = 0x01,
        LED_CAPS_LOCK = 0x02,
        LED_SCROLL_LOCK = 0x04,
    };

    Status SetIdle(IUSBInterface *interface, uint8_t duration, uint8_t reportId);

    // Only defined on a boot-subclass interface (bInterfaceSubClass == 1); a device that does
    // not claim boot protocol is free to STALL it.
    Status SetProtocol(IUSBInterface *interface, Protocol protocol);

    Status GetReportDescriptor(IUSBInterface *interface, uint8_t *buffer, uint16_t *size);

    Status SetOutputReport(IUSBInterface *interface, uint8_t reportId, const uint8_t *data, uint16_t size);
} // namespace controllerlib::hid
