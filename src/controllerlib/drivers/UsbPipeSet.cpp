#include "drivers/UsbPipeSet.h"

#include <algorithm>

namespace controllerlib
{
    namespace
    {
        constexpr uint8_t MaxEndpointsPerDirection = 15;
    }

    Status UsbPipeSet::OpenPipes(IUSBDevice *device, const ControllerConfig &config, ILogger *logger)
    {
        logger->Log(LogLevel::Debug, "Device[%04x-%04x] Opening interfaces ...", device->GetVendor(), device->GetProduct());

        Status result = device->Open();
        if (result != Status::Success)
        {
            logger->Log(LogLevel::Error, "Device[%04x-%04x] Failed to open device !", device->GetVendor(), device->GetProduct());
            return result;
        }

        std::vector<std::unique_ptr<IUSBInterface>> &interfaces = device->GetInterfaces();
        for (auto &&interface : interfaces)
        {
            logger->Log(LogLevel::Debug, "Device[%04x-%04x] Opening interface %d/%d ...", device->GetVendor(), device->GetProduct(), m_interfaces.size() + 1, interfaces.size());

            Status interfaceResult = interface->Open();
            if (interfaceResult != Status::Success)
            {
                logger->Log(LogLevel::Error, "Device[%04x-%04x] Failed to open interface !", device->GetVendor(), device->GetProduct());
                return interfaceResult;
            }

            for (uint8_t idx = 0; idx < MaxEndpointsPerDirection; idx++)
            {
                IUSBEndpoint *inEndpoint = interface->GetEndpoint(IUSBEndpoint::USB_ENDPOINT_IN, idx);
                if (inEndpoint == NULL)
                    continue;

                Status endpointResult = inEndpoint->Open(config.inputMaxPacketSize);
                if (endpointResult != Status::Success)
                {
                    logger->Log(LogLevel::Error, "Device[%04x-%04x] Failed to open input endpoint idx: %d !", device->GetVendor(), device->GetProduct(), idx);
                    return endpointResult;
                }

                m_inPipe.push_back(inEndpoint);
            }

            for (uint8_t idx = 0; idx < MaxEndpointsPerDirection; idx++)
            {
                IUSBEndpoint *outEndpoint = interface->GetEndpoint(IUSBEndpoint::USB_ENDPOINT_OUT, idx);
                if (outEndpoint == NULL)
                    continue;

                Status endpointResult = outEndpoint->Open(config.outputMaxPacketSize);
                if (endpointResult != Status::Success)
                {
                    logger->Log(LogLevel::Error, "Device[%04x-%04x] Failed to open output endpoint idx: %d !", device->GetVendor(), device->GetProduct(), idx);
                    return endpointResult;
                }

                m_outPipe.push_back(outEndpoint);
            }

            m_interfaces.push_back(interface.get());
        }

        if (m_inPipe.empty())
        {
            logger->Log(LogLevel::Error, "Device[%04x-%04x] Not input endpoint found !", device->GetVendor(), device->GetProduct());
            return Status::InvalidEndpoint;
        }

        logger->Log(LogLevel::Debug, "Device[%04x-%04x] successfully opened !", device->GetVendor(), device->GetProduct());
        return Status::Success;
    }

    void UsbPipeSet::ClosePipes(IUSBDevice *device)
    {
        device->Close();

        /*
            m_inPipe/m_outPipe/m_interfaces are non-owning pointers into the device we just
            closed. Drop them so a later read or write can't dereference endpoints that no
            longer exist; both now see empty pipe lists and fail cleanly instead.
        */
        m_inPipe.clear();
        m_outPipe.clear();
        m_interfaces.clear();
    }

    Status UsbPipeSet::ReadEndpointOnce(uint16_t endpoint_idx, uint8_t *buffer, size_t *size, uint32_t timeout_us)
    {
        *size = std::min((size_t)m_inPipe[endpoint_idx]->GetDescriptor()->wMaxPacketSize, *size);

        Status result = m_inPipe[endpoint_idx]->Read(buffer, size, timeout_us);
        if (result != Status::Success)
            return result;

        if (*size == 0)
            return Status::NothingTodo;

        return Status::Success;
    }
} // namespace controllerlib
