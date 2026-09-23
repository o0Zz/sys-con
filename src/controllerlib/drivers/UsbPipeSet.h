#pragma once

#include "IUSBDevice.h"
#include "ILogger.h"
#include "ControllerConfig.h"
#include "Status.h"

#include <vector>

namespace controllerlib
{
    /*
        The endpoints of one opened USB device.

        A base rather than a member so that BaseController, HIDKeyboardController and
        HIDMouseController -- which share no interface -- share this without a diamond, and
        so that the eleven gamepad drivers keep naming m_interfaces/m_inPipe/m_outPipe
        directly. It owns no lifetime: the pointers belong to the IUSBDevice passed to Open().
    */
    class UsbPipeSet
    {
    protected:
        std::vector<IUSBEndpoint *> m_inPipe;
        std::vector<IUSBEndpoint *> m_outPipe;
        std::vector<IUSBInterface *> m_interfaces;

        Status OpenPipes(IUSBDevice *device, const ControllerConfig &config, ILogger *logger);
        void ClosePipes(IUSBDevice *device);

        // One transfer, clamped to the endpoint's wMaxPacketSize. The only endpoint primitive;
        // every read policy above is built out of it.
        Status ReadEndpointOnce(uint16_t endpoint_idx, uint8_t *buffer, size_t *size, uint32_t timeout_us);
    };
} // namespace controllerlib
