#pragma once
#include "IUSBInterface.h"
#include <gmock/gmock.h>

class MockUSBInterface : public controllerlib::IUSBInterface
{
public:
    MockUSBInterface(std::unique_ptr<controllerlib::IUSBEndpoint> &&input, std::unique_ptr<controllerlib::IUSBEndpoint> &&output) : IUSBInterface()
    {
        m_inEndpoint = std::move(input);
        m_outEndpoint = std::move(output);
    }
    ~MockUSBInterface() override {}

    MOCK_METHOD(controllerlib::Status, Open, (), (override));
    MOCK_METHOD(void, Close, (), (override));
    MOCK_METHOD(controllerlib::Status, ControlTransferInput, (uint8_t bmRequestType, uint8_t bmRequest, uint16_t wValue, uint16_t wIndex, void *buffer, uint16_t *wLength), (override));
    MOCK_METHOD(controllerlib::Status, ControlTransferOutput, (uint8_t bmRequestType, uint8_t bmRequest, uint16_t wValue, uint16_t wIndex, const void *buffer, uint16_t wLength), (override));

    controllerlib::IUSBEndpoint *GetEndpoint(controllerlib::IUSBEndpoint::Direction direction, uint8_t index) override
    {
        if (direction == controllerlib::IUSBEndpoint::USB_ENDPOINT_IN && index == 0)
        {
            return m_inEndpoint.get();
        }
        else if (direction == controllerlib::IUSBEndpoint::USB_ENDPOINT_OUT && index == 0)
        {
            return m_outEndpoint.get();
        }
        return nullptr;
    }
    MOCK_METHOD(InterfaceDescriptor *, GetDescriptor, (), (override));
    MOCK_METHOD(controllerlib::Status, Reset, (), (override));

private:
    std::unique_ptr<controllerlib::IUSBEndpoint> m_inEndpoint;
    std::unique_ptr<controllerlib::IUSBEndpoint> m_outEndpoint;
};
