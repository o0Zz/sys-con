/*
    Regression tests for the report-bounds and input-index guards in ParseData.

    Every case here reads or writes out of bounds without the guard it exercises, so these
    are memory-safety regressions rather than behavioural ones. They are grouped by the
    defect they cover rather than by driver, because the same two mistakes were copy-pasted
    across several drivers and fixed in one pass.
*/
#include <gtest/gtest.h>

#include "Controllers/SteamController2026.h"
#include "Controllers/WiiController.h"
#include "Controllers/Xbox360WirelessController.h"
#include "Controllers/XboxOneController.h"
#include "mocks/Device.h"
#include "mocks/Logger.h"

namespace {

template <typename TController>
TController MakeController(const ControllerConfig &config)
{
    return TController(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());
}

} // namespace

/* ---------- Truncated reports must be rejected, not read past the end ---------- */

TEST(ReportBounds, test_xbox360w_rejects_report_shorter_than_receiver_header)
{
    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;

    Xbox360WirelessController controller = MakeController<Xbox360WirelessController>(config);

    // The 4-byte receiver header was read unconditionally before being bounds-checked.
    uint8_t buffer[4] = {0x00, 0x01, 0x00, 0xf0};

    for (size_t size = 0; size < XBOX360_WIRELESS_HEADER_SIZE; size++)
        EXPECT_EQ(controller.ParseData(buffer, size, &rawData, &input_idx), CONTROLLER_STATUS_UNEXPECTED_DATA) << "size=" << size;
}

TEST(ReportBounds, test_xbox360w_payload_size_accounts_for_receiver_header)
{
    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;

    Xbox360WirelessController controller = MakeController<Xbox360WirelessController>(config);

    uint8_t buffer[XBOX360_WIRELESS_HEADER_SIZE + sizeof(Xbox360ButtonData)] = {0x00, 0x01, 0x00, 0xf0};

    /*
        The payload starts at buffer + 4, so a full report is 4 + sizeof(Xbox360ButtonData).
        The check used to compare against sizeof(Xbox360ButtonData) alone, so the last four
        bytes of the struct were read out of bounds for any size in this range.
    */
    for (size_t size = XBOX360_WIRELESS_HEADER_SIZE; size < XBOX360_WIRELESS_HEADER_SIZE + sizeof(Xbox360ButtonData); size++)
        EXPECT_EQ(controller.ParseData(buffer, size, &rawData, &input_idx), CONTROLLER_STATUS_UNEXPECTED_DATA) << "size=" << size;
}

TEST(ReportBounds, test_xboxone_rejects_empty_report)
{
    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;

    XboxOneController controller = MakeController<XboxOneController>(config);

    // buttonData->type was dereferenced before the size check below it. The contents do not
    // matter here: at size 0 there is no byte to legitimately read.
    uint8_t buffer[1] = {0x00};

    EXPECT_EQ(controller.ParseData(buffer, 0, &rawData, &input_idx), CONTROLLER_STATUS_UNEXPECTED_DATA);
}

TEST(ReportBounds, test_steam2026_rejects_empty_report)
{
    ControllerConfig config;
    RawInputData rawData;
    uint16_t input_idx = 0;

    SteamController2026 controller = MakeController<SteamController2026>(config);

    // buffer[0] was read before any size validation.
    uint8_t buffer[1] = {0x00};

    EXPECT_EQ(controller.ParseData(buffer, 0, &rawData, &input_idx), CONTROLLER_STATUS_UNEXPECTED_DATA);
}

/* ---------- input_idx must be range-checked before indexing per-slot state ---------- */

TEST(ReportBounds, test_xbox360w_rejects_out_of_range_input_idx)
{
    ControllerConfig config;
    RawInputData rawData;

    Xbox360WirelessController controller = MakeController<Xbox360WirelessController>(config);

    // 0x08 selects the connect/disconnect path, which indexes m_is_connected[*input_idx].
    uint8_t buffer[XBOX360_WIRELESS_HEADER_SIZE + sizeof(Xbox360ButtonData)] = {0x08, 0x80, 0x00, 0x00};

    uint16_t in_range = XBOX360_MAX_INPUTS - 1;
    EXPECT_NE(controller.ParseData(buffer, sizeof(buffer), &rawData, &in_range), CONTROLLER_STATUS_INVALID_INDEX);

    for (uint16_t input_idx = XBOX360_MAX_INPUTS; input_idx < XBOX360_MAX_INPUTS + 4; input_idx++)
        EXPECT_EQ(controller.ParseData(buffer, sizeof(buffer), &rawData, &input_idx), CONTROLLER_STATUS_INVALID_INDEX) << "input_idx=" << input_idx;
}

TEST(ReportBounds, test_wii_rejects_out_of_range_input_idx)
{
    ControllerConfig config;
    RawInputData rawData;

    WiiController controller = MakeController<WiiController>(config);

    // Writes m_rumble_supported[*input_idx] and m_is_connected[*input_idx].
    uint8_t buffer[9] = {0x10, 0x10, 0x00, 0x82, 0x82, 0x7B, 0x81, 0x17, 0x1A};

    uint16_t in_range = WII_MAX_INPUTS - 1;
    EXPECT_NE(controller.ParseData(buffer, sizeof(buffer), &rawData, &in_range), CONTROLLER_STATUS_INVALID_INDEX);

    for (uint16_t input_idx = WII_MAX_INPUTS; input_idx < WII_MAX_INPUTS + 4; input_idx++)
        EXPECT_EQ(controller.ParseData(buffer, sizeof(buffer), &rawData, &input_idx), CONTROLLER_STATUS_INVALID_INDEX) << "input_idx=" << input_idx;
}
