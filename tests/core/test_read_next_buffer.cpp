#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "drivers/BaseController.h"
#include "mocks/Device.h"
#include "mocks/Logger.h"
#include "mocks/USBEndpoint.h"

#include <cstring>
#include <vector>

using testing::_;
using testing::Invoke;
using testing::Return;

/* --------------------------- Test setup --------------------------- */

// Exposes the protected read path and lets a test inject mock IN endpoints into m_inPipe.
class ReadTestController : public BaseController
{
public:
    ReadTestController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger)
        : BaseController(std::move(device), config, std::move(logger)) {}

    ControllerResult ParseData(uint8_t *, size_t, RawInputData *, uint16_t *) override { return CONTROLLER_STATUS_SUCCESS; }

    void AddEndpoint(IUSBEndpoint *endpoint) { m_inPipe.push_back(endpoint); }
    uint8_t GetCurrentIdx() const { return m_current_controller_idx; }

    using BaseController::ReadNextBuffer;
};

// gmock action that fills the out-buffer with a report and returns success (simulates one read).
static auto ReturnReport(std::vector<uint8_t> data)
{
    return Invoke([data](uint8_t *out, size_t *sz, uint64_t)
                  {
        memcpy(out, data.data(), data.size());
        *sz = data.size();
        return CONTROLLER_STATUS_SUCCESS; });
}

static IUSBEndpoint::EndpointDescriptor MakeDescriptor()
{
    IUSBEndpoint::EndpointDescriptor desc = {};
    desc.bEndpointAddress = 0x81;
    desc.wMaxPacketSize = 64;
    return desc;
}

/* --------------------------- Tests --------------------------- */

// A burst of queued reports must collapse to the freshest one, not replay stale frames.
TEST(ReadNextBuffer, test_read_drains_to_latest_report)
{
    ControllerConfig config;
    IUSBEndpoint::EndpointDescriptor desc = MakeDescriptor();

    MockUSBEndpoint endpoint(IUSBEndpoint::USB_ENDPOINT_IN);
    EXPECT_CALL(endpoint, GetDescriptor()).WillRepeatedly(Return(&desc));
    EXPECT_CALL(endpoint, Read(_, _, _))
        .WillOnce(ReturnReport({0x42, 0x01}))               // first report
        .WillOnce(ReturnReport({0x42, 0x02}))               // queued behind it
        .WillOnce(ReturnReport({0x42, 0x03}))               // newest queued
        .WillRepeatedly(Return(CONTROLLER_STATUS_TIMEOUT)); // queue empty -> stop draining

    ReadTestController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());
    controller.AddEndpoint(&endpoint);

    uint8_t buffer[CONTROLLER_INPUT_BUFFER_SIZE] = {};
    size_t size = sizeof(buffer);
    uint16_t input_idx = 0xFFFF;

    EXPECT_EQ(controller.ReadNextBuffer(buffer, &size, &input_idx, 1000), CONTROLLER_STATUS_SUCCESS);
    EXPECT_EQ(input_idx, 0);
    ASSERT_EQ(size, 2u);
    EXPECT_EQ(buffer[1], 0x03); // freshest report wins
}

// With multiple endpoints, an idle endpoint must not stop us from servicing one that has data,
// and the round-robin cursor must advance past the serviced endpoint (fairness).
TEST(ReadNextBuffer, test_read_skips_idle_endpoint)
{
    ControllerConfig config;
    IUSBEndpoint::EndpointDescriptor desc = MakeDescriptor();

    MockUSBEndpoint idleEndpoint(IUSBEndpoint::USB_ENDPOINT_IN);
    MockUSBEndpoint activeEndpoint(IUSBEndpoint::USB_ENDPOINT_IN);
    MockUSBEndpoint unusedEndpoint(IUSBEndpoint::USB_ENDPOINT_IN);

    EXPECT_CALL(idleEndpoint, GetDescriptor()).WillRepeatedly(Return(&desc));
    EXPECT_CALL(activeEndpoint, GetDescriptor()).WillRepeatedly(Return(&desc));
    EXPECT_CALL(unusedEndpoint, GetDescriptor()).WillRepeatedly(Return(&desc));

    EXPECT_CALL(idleEndpoint, Read(_, _, _)).WillRepeatedly(Return(CONTROLLER_STATUS_TIMEOUT));
    EXPECT_CALL(activeEndpoint, Read(_, _, _))
        .WillOnce(ReturnReport({0x42, 0x55}))
        .WillRepeatedly(Return(CONTROLLER_STATUS_TIMEOUT));
    // unusedEndpoint (index 2) must never be read: we return after servicing index 1.
    EXPECT_CALL(unusedEndpoint, Read(_, _, _)).Times(0);

    ReadTestController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());
    controller.AddEndpoint(&idleEndpoint);   // idx 0
    controller.AddEndpoint(&activeEndpoint); // idx 1
    controller.AddEndpoint(&unusedEndpoint); // idx 2

    uint8_t buffer[CONTROLLER_INPUT_BUFFER_SIZE] = {};
    size_t size = sizeof(buffer);
    uint16_t input_idx = 0xFFFF;

    EXPECT_EQ(controller.ReadNextBuffer(buffer, &size, &input_idx, 1000), CONTROLLER_STATUS_SUCCESS);
    EXPECT_EQ(input_idx, 1);
    EXPECT_EQ(buffer[1], 0x55);
    EXPECT_EQ(controller.GetCurrentIdx(), 2); // cursor advanced past the serviced endpoint
}

// When every endpoint is idle, the call must return without success (and without hanging),
// letting the input thread pace on the blocking wait.
TEST(ReadNextBuffer, test_read_all_idle)
{
    ControllerConfig config;
    IUSBEndpoint::EndpointDescriptor desc = MakeDescriptor();

    MockUSBEndpoint endpoint0(IUSBEndpoint::USB_ENDPOINT_IN);
    MockUSBEndpoint endpoint1(IUSBEndpoint::USB_ENDPOINT_IN);

    EXPECT_CALL(endpoint0, GetDescriptor()).WillRepeatedly(Return(&desc));
    EXPECT_CALL(endpoint1, GetDescriptor()).WillRepeatedly(Return(&desc));
    EXPECT_CALL(endpoint0, Read(_, _, _)).WillRepeatedly(Return(CONTROLLER_STATUS_TIMEOUT));
    EXPECT_CALL(endpoint1, Read(_, _, _)).WillRepeatedly(Return(CONTROLLER_STATUS_TIMEOUT));

    ReadTestController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());
    controller.AddEndpoint(&endpoint0);
    controller.AddEndpoint(&endpoint1);

    uint8_t buffer[CONTROLLER_INPUT_BUFFER_SIZE] = {};
    size_t size = sizeof(buffer);
    uint16_t input_idx = 0xFFFF;

    EXPECT_NE(controller.ReadNextBuffer(buffer, &size, &input_idx, 1000), CONTROLLER_STATUS_SUCCESS);
}
