#pragma once

#include "mocks/Device.h"
#include "mocks/USBEndpoint.h"
#include "mocks/USBInterface.h"

#include <cstring>
#include <deque>
#include <memory>
#include <vector>

/*
    A HID device wired up far enough that a driver's Initialize() and ReadInput() run end to
    end: the interface answers GET_DESCRIPTOR with a report descriptor of the test's choosing,
    and the IN endpoint hands back queued reports in order.
*/
class HIDTestRig
{
public:
    HIDTestRig(const uint8_t *report_descriptor, size_t report_descriptor_size, uint8_t subclass, uint8_t protocol)
        : m_report_descriptor(report_descriptor, report_descriptor + report_descriptor_size)
    {
        m_interface_descriptor = {9, 4, 0, 0, 1, 0x03, subclass, protocol, 0};
        m_endpoint_descriptor = {7, 5, 0x81, 0x03, 64, 1};

        auto in = std::make_unique<::testing::NiceMock<MockUSBEndpoint>>(controllerlib::IUSBEndpoint::USB_ENDPOINT_IN);
        m_in = in.get();
        auto interface = std::make_unique<::testing::NiceMock<MockUSBInterface>>(std::move(in), nullptr);
        m_interface = interface.get();
        m_device = std::make_unique<MockDevice>(0x046d, 0xc31c, std::move(interface));

        using ::testing::_;
        using ::testing::Return;

        ON_CALL(*m_interface, Open()).WillByDefault(Return(controllerlib::Status::Success));
        ON_CALL(*m_interface, GetDescriptor()).WillByDefault(Return(&m_interface_descriptor));
        ON_CALL(*m_interface, ControlTransferOutput(_, _, _, _, _, _)).WillByDefault(Return(controllerlib::Status::Success));
        ON_CALL(*m_interface, ControlTransferInput(_, _, _, _, _, _))
            .WillByDefault([this](uint8_t, uint8_t, uint16_t, uint16_t, void *buffer, uint16_t *length) {
                *length = (uint16_t)m_report_descriptor.size();
                memcpy(buffer, m_report_descriptor.data(), m_report_descriptor.size());
                return controllerlib::Status::Success;
            });

        ON_CALL(*m_in, Open(_)).WillByDefault(Return(controllerlib::Status::Success));
        ON_CALL(*m_in, GetDescriptor()).WillByDefault(Return(&m_endpoint_descriptor));
        ON_CALL(*m_in, Read(_, _, _))
            .WillByDefault([this](uint8_t *out, size_t *size, uint64_t) {
                if (m_reports.empty())
                    return controllerlib::Status::Timeout;

                const std::vector<uint8_t> report = m_reports.front();
                m_reports.pop_front();

                *size = report.size();
                memcpy(out, report.data(), report.size());
                return controllerlib::Status::Success;
            });
    }

    void QueueReport(std::initializer_list<uint8_t> bytes) { m_reports.emplace_back(bytes); }

    std::unique_ptr<controllerlib::IUSBDevice> TakeDevice() { return std::move(m_device); }

    MockUSBInterface *Interface() { return m_interface; }

private:
    std::vector<uint8_t> m_report_descriptor;
    controllerlib::IUSBInterface::InterfaceDescriptor m_interface_descriptor;
    controllerlib::IUSBEndpoint::EndpointDescriptor m_endpoint_descriptor;

    std::deque<std::vector<uint8_t>> m_reports;

    ::testing::NiceMock<MockUSBEndpoint> *m_in;
    ::testing::NiceMock<MockUSBInterface> *m_interface;
    std::unique_ptr<MockDevice> m_device;
};

// Standard USB HID boot protocol keyboard: modifier byte, reserved byte, six scan codes.
inline constexpr uint8_t BootKeyboardDescriptor[] = {
    0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7,
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02, 0x95, 0x01,
    0x75, 0x08, 0x81, 0x03, 0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
    0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00, 0xC0,
};

// Standard USB HID boot protocol mouse: button bitfield, then signed X, Y and wheel bytes.
inline constexpr uint8_t BootMouseDescriptor[] = {
    0x05, 0x01, 0x09, 0x02, 0xA1, 0x01, 0x09, 0x01, 0xA1, 0x00, 0x05, 0x09,
    0x19, 0x01, 0x29, 0x05, 0x15, 0x00, 0x25, 0x01, 0x95, 0x05, 0x75, 0x01,
    0x81, 0x02, 0x95, 0x01, 0x75, 0x03, 0x81, 0x03, 0x05, 0x01, 0x09, 0x30,
    0x09, 0x31, 0x09, 0x38, 0x15, 0x81, 0x25, 0x7F, 0x75, 0x08, 0x95, 0x03,
    0x81, 0x06, 0xC0, 0xC0,
};
