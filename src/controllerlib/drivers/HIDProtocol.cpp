#include "drivers/HIDProtocol.h"

namespace controllerlib::hid
{
    namespace
    {
        constexpr uint8_t RequestTypeClassInterface = 0x20 | 0x01;

        constexpr uint8_t RequestGetDescriptor = 0x06;
        constexpr uint8_t RequestSetIdle = 0x0A;
        constexpr uint8_t RequestSetProtocol = 0x0B;
        constexpr uint8_t RequestSetReport = 0x09;

        constexpr uint16_t DescriptorReport = 0x22;
        constexpr uint16_t ReportTypeOutput = 0x02;

        inline uint16_t InterfaceNumber(IUSBInterface *interface)
        {
            return interface->GetDescriptor()->bInterfaceNumber;
        }
    } // namespace

    Status SetIdle(IUSBInterface *interface, uint8_t duration, uint8_t reportId)
    {
        return interface->ControlTransferOutput((uint8_t)IUSBEndpoint::USB_ENDPOINT_OUT | RequestTypeClassInterface,
                                                RequestSetIdle,
                                                (uint16_t)((duration << 8) | reportId),
                                                InterfaceNumber(interface),
                                                nullptr, 0);
    }

    Status SetProtocol(IUSBInterface *interface, Protocol protocol)
    {
        return interface->ControlTransferOutput((uint8_t)IUSBEndpoint::USB_ENDPOINT_OUT | RequestTypeClassInterface,
                                                RequestSetProtocol,
                                                (uint16_t)protocol,
                                                InterfaceNumber(interface),
                                                nullptr, 0);
    }

    Status GetReportDescriptor(IUSBInterface *interface, uint8_t *buffer, uint16_t *size)
    {
        return interface->ControlTransferInput((uint8_t)IUSBEndpoint::USB_ENDPOINT_IN | 0x01,
                                               RequestGetDescriptor,
                                               (uint16_t)(DescriptorReport << 8),
                                               InterfaceNumber(interface),
                                               buffer, size);
    }

    Status SetOutputReport(IUSBInterface *interface, uint8_t reportId, const uint8_t *data, uint16_t size)
    {
        return interface->ControlTransferOutput((uint8_t)IUSBEndpoint::USB_ENDPOINT_OUT | RequestTypeClassInterface,
                                                RequestSetReport,
                                                (uint16_t)((ReportTypeOutput << 8) | reportId),
                                                InterfaceNumber(interface),
                                                data, size);
    }
} // namespace controllerlib::hid
