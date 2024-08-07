#include "gmock/gmock.h"
#include "Controllers/BaseController.h"

/* --------------------------- Test setup --------------------------- */

class MockDevice : public IUSBDevice
{
public:
    MOCK_METHOD(ControllerResult, Open, (), (override));
    MOCK_METHOD(void, Close, (), (override));
    MOCK_METHOD(void, Reset, (), (override));
};

class MockLogger : public ILogger
{
public:
    void Log(LogLevel aLogLevel, const char *format, ::std::va_list vl) override {}
    void LogBuffer(LogLevel aLogLevel, const uint8_t *buffer, size_t size) override {}
};

class MockBaseController : public BaseController
{
public:
    MockBaseController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger) : BaseController(std::move(device), config, std::move(logger)) {}
    virtual ~MockBaseController() override {}

    MOCK_METHOD(ControllerResult, ReadRawInput, (RawInputData * rawData, uint16_t *input_idx, uint32_t timeout_us), (override));
};

NormalizedButtonData TestReadInput(ControllerConfig config, RawInputData inputData)
{
    uint16_t input_idx;
    NormalizedButtonData normalizedData = {0};

    MockBaseController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());

    EXPECT_CALL(controller, ReadRawInput(::testing::_, ::testing::_, ::testing::_))
        .WillOnce(::testing::DoAll(
            ::testing::SetArgPointee<0>(inputData), // Set the first argument (rawData) to expectedData
            ::testing::SetArgPointee<1>(0),         // Set the second argument (input_idx) to 0
            ::testing::Return(ControllerResult::CONTROLLER_STATUS_SUCCESS)));

    controller.ReadInput(&normalizedData, &input_idx, 0);

    return normalizedData;
}

/* --------------------------- Tests --------------------------- */

TEST(BaseController, test_input_binding_basis)
{
    ControllerConfig config;
    config.buttons_pin[ControllerButton::X][0] = 1;
    config.buttons_pin[ControllerButton::Y][0] = 2;
    config.buttons_pin[ControllerButton::A][0] = 3;
    config.buttons_pin[ControllerButton::B][0] = 4;
    config.stickConfig[0].X.bind = ControllerAnalogBinding_X;
    config.stickConfig[0].X.sign = 1.0f;
    config.stickConfig[0].Y.bind = ControllerAnalogBinding_Y;
    config.stickConfig[0].Y.sign = 1.0f;

    RawInputData inputData;
    inputData.buttons[1] = true;
    inputData.buttons[3] = true;
    inputData.X = 0.5f;
    inputData.Y = -0.5f;

    NormalizedButtonData normalizedData = TestReadInput(config, inputData);

    EXPECT_TRUE(normalizedData.buttons[ControllerButton::X]);
    EXPECT_FALSE(normalizedData.buttons[ControllerButton::Y]);
    EXPECT_TRUE(normalizedData.buttons[ControllerButton::A]);
    EXPECT_FALSE(normalizedData.buttons[ControllerButton::B]);
    EXPECT_FLOAT_EQ(normalizedData.sticks[0].axis_x, 0.5f);
    EXPECT_FLOAT_EQ(normalizedData.sticks[0].axis_y, -0.5f);
}

TEST(BaseController, test_input_deadzone)
{
    ControllerConfig config;
    config.stickDeadzonePercent[0] = 10;
    config.stickDeadzonePercent[1] = 5;

    RawInputData inputData;
    inputData.X = 0.1f;
    inputData.Y = 0.1f;

    NormalizedButtonData normalizedData = TestReadInput(config, inputData);

    EXPECT_FLOAT_EQ(normalizedData.sticks[0].axis_x, 0.0f);
    EXPECT_FLOAT_EQ(normalizedData.sticks[0].axis_y, 0.0f);
}

TEST(BaseController, test_input_simulate_buttons)
{
    ControllerConfig config;
    config.buttons_pin[ControllerButton::X][0] = 1;
    config.buttons_pin[ControllerButton::Y][0] = 2;
    config.buttons_pin[ControllerButton::A][0] = 3;
    config.buttons_pin[ControllerButton::B][0] = 4;
    config.simulateHome[0] = ControllerButton::X;
    config.simulateHome[1] = ControllerButton::Y;
    config.simulateCapture[0] = ControllerButton::A;
    config.simulateCapture[1] = ControllerButton::B;

    RawInputData inputData;
    inputData.buttons[1] = true;
    inputData.buttons[2] = true;
    inputData.buttons[3] = true;
    inputData.buttons[4] = true;

    NormalizedButtonData normalizedData = TestReadInput(config, inputData);

    EXPECT_FALSE(normalizedData.buttons[ControllerButton::X]);
    EXPECT_FALSE(normalizedData.buttons[ControllerButton::A]);
    EXPECT_FALSE(normalizedData.buttons[ControllerButton::Y]);
    EXPECT_FALSE(normalizedData.buttons[ControllerButton::B]);
    EXPECT_TRUE(normalizedData.buttons[ControllerButton::HOME]);
    EXPECT_TRUE(normalizedData.buttons[ControllerButton::CAPTURE]);
}

TEST(BaseController, test_input_stick_by_buttons)
{
    ControllerConfig config;
    config.buttons_pin[ControllerButton::LSTICK_LEFT][0] = 1;
    config.buttons_pin[ControllerButton::LSTICK_DOWN][0] = 2;
    config.buttons_pin[ControllerButton::RSTICK_RIGHT][0] = 3;
    config.buttons_pin[ControllerButton::RSTICK_UP][0] = 4;

    RawInputData inputData;
    inputData.buttons[1] = true;
    inputData.buttons[2] = true;
    inputData.buttons[3] = true;
    inputData.buttons[4] = true;
    inputData.X = 0.0f;
    inputData.Y = 0.0f;

    NormalizedButtonData normalizedData = TestReadInput(config, inputData);

    EXPECT_TRUE(normalizedData.buttons[ControllerButton::LSTICK_LEFT]);
    EXPECT_TRUE(normalizedData.buttons[ControllerButton::LSTICK_DOWN]);
    EXPECT_TRUE(normalizedData.buttons[ControllerButton::RSTICK_RIGHT]);
    EXPECT_TRUE(normalizedData.buttons[ControllerButton::RSTICK_UP]);

    EXPECT_FLOAT_EQ(normalizedData.sticks[0].axis_x, -1.0f);
    EXPECT_FLOAT_EQ(normalizedData.sticks[0].axis_y, 1.0f);

    EXPECT_FLOAT_EQ(normalizedData.sticks[1].axis_x, 1.0f);
    EXPECT_FLOAT_EQ(normalizedData.sticks[1].axis_y, -1.0f);
}

TEST(BaseController, test_input_alias)
{
    ControllerConfig config;
    config.buttons_alias[ControllerButton::X] = ControllerButton::DPAD_UP;

    RawInputData inputData;
    inputData.dpad_up = true;

    NormalizedButtonData normalizedData = TestReadInput(config, inputData);

    EXPECT_TRUE(normalizedData.buttons[ControllerButton::X]);
    EXPECT_FALSE(normalizedData.buttons[ControllerButton::A]);
    EXPECT_FALSE(normalizedData.buttons[ControllerButton::Y]);
    EXPECT_FALSE(normalizedData.buttons[ControllerButton::B]);

    EXPECT_TRUE(normalizedData.buttons[ControllerButton::DPAD_UP]);
    EXPECT_FALSE(normalizedData.buttons[ControllerButton::DPAD_DOWN]);
}

TEST(BaseController, test_input_multiple_pin)
{
    ControllerConfig config;
    config.buttons_pin[ControllerButton::X][0] = 1;
    config.buttons_pin[ControllerButton::X][1] = 2;

    RawInputData inputData;
    inputData.buttons[2] = true;

    NormalizedButtonData normalizedData = TestReadInput(config, inputData);

    EXPECT_TRUE(normalizedData.buttons[ControllerButton::X]);
}

TEST(BaseController, test_input_complex_combination)
{
    ControllerConfig config;
    config.stickActivationThreshold = 10;
    config.buttons_pin[ControllerButton::L][0] = 1;
    config.buttons_pin[ControllerButton::L][1] = 2;
    config.buttons_alias[ControllerButton::A] = ControllerButton::L;
    config.buttons_alias[ControllerButton::X] = ControllerButton::DPAD_UP;
    config.buttons_alias[ControllerButton::Y] = ControllerButton::LSTICK_LEFT;
    config.buttons_alias[ControllerButton::R] = ControllerButton::DPAD_RIGHT;
    config.simulateHome[0] = ControllerButton::Y;
    config.simulateHome[1] = ControllerButton::X;
    config.simulateCapture[0] = ControllerButton::A;
    config.simulateCapture[1] = ControllerButton::R;
    config.stickConfig[0].X.bind = ControllerAnalogBinding_X;
    config.stickConfig[0].X.sign = 1.0f;
    config.stickConfig[0].Y.bind = ControllerAnalogBinding_Y;
    config.stickConfig[0].Y.sign = 1.0f;

    RawInputData inputData;
    inputData.X = -0.5f;
    inputData.buttons[1] = true;
    inputData.dpad_up = true;
    inputData.dpad_right = true;
    // inputData.buttons[2] = true; // Enable L
    // L linked to A
    // R linked to DPAD_RIGHT
    // L+R will simulate CAPTURE

    // DPAD_UP is alias of X
    // axis_x(0.5) will generate a LSTICK_LEFT
    // LSTICK_LEFT will enable Y
    // X+Y will simulate HOME

    NormalizedButtonData normalizedData = TestReadInput(config, inputData);

    EXPECT_FALSE(normalizedData.buttons[ControllerButton::X]);
    EXPECT_FALSE(normalizedData.buttons[ControllerButton::Y]);
    EXPECT_TRUE(normalizedData.buttons[ControllerButton::HOME]);
    EXPECT_TRUE(normalizedData.buttons[ControllerButton::CAPTURE]);
    EXPECT_TRUE(normalizedData.buttons[ControllerButton::DPAD_UP]);
    EXPECT_TRUE(normalizedData.buttons[ControllerButton::DPAD_RIGHT]);
    EXPECT_FLOAT_EQ(normalizedData.sticks[0].axis_x, -0.5f);
}
