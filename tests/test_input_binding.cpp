#include "gmock/gmock.h"
#include "Controllers/BaseController.h"
#include "mocks/Device.h"
#include "mocks/Logger.h"

/* --------------------------- Test setup --------------------------- */

class MockBaseController : public BaseController
{
public:
    MockBaseController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger) : BaseController(std::move(device), config, std::move(logger)) {}
    ControllerResult ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx) override { return CONTROLLER_STATUS_SUCCESS; }
    using BaseController::MapRawInputToNormalized; // Move protected method to public for testing
};

/* --------------------------- Tests --------------------------- */

TEST(BaseController, test_input_binding_basis)
{
    NormalizedButtonData normalizedData = {0};

    ControllerConfig config;
    config.buttonsPin[ControllerButton::X][0] = 1;
    config.buttonsPin[ControllerButton::Y][0] = 2;
    config.buttonsPin[ControllerButton::A][0] = 3;
    config.buttonsPin[ControllerButton::B][0] = 4;
    config.buttonsPin[ControllerButton::RSTICK_CLICK][0] = 15;
    config.buttonsAnalog[ControllerButton::LSTICK_LEFT].bind = ControllerAnalogBinding_X;
    config.buttonsAnalog[ControllerButton::LSTICK_LEFT].sign = -1.0f;
    config.buttonsAnalog[ControllerButton::LSTICK_RIGHT].bind = ControllerAnalogBinding_X;
    config.buttonsAnalog[ControllerButton::LSTICK_RIGHT].sign = +1.0f;
    config.buttonsAnalog[ControllerButton::LSTICK_UP].bind = ControllerAnalogBinding_Y;
    config.buttonsAnalog[ControllerButton::LSTICK_UP].sign = +1.0f;
    config.buttonsAnalog[ControllerButton::LSTICK_DOWN].bind = ControllerAnalogBinding_Y;
    config.buttonsAnalog[ControllerButton::LSTICK_DOWN].sign = -1.0f;

    RawInputData inputData;
    inputData.buttons[1] = true;
    inputData.buttons[3] = true;
    inputData.buttons[15] = true;
    inputData.analog[ControllerAnalogBinding_X] = 0.5f;  // Right
    inputData.analog[ControllerAnalogBinding_Y] = -0.5f; // Down

    MockBaseController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());
    controller.MapRawInputToNormalized(inputData, &normalizedData);

    EXPECT_TRUE(normalizedData.buttons[ControllerButton::X]);
    EXPECT_FALSE(normalizedData.buttons[ControllerButton::Y]);
    EXPECT_TRUE(normalizedData.buttons[ControllerButton::A]);
    EXPECT_FALSE(normalizedData.buttons[ControllerButton::B]);
    EXPECT_FLOAT_EQ(normalizedData.sticks[0].axis_x, 0.5f);
    EXPECT_FLOAT_EQ(normalizedData.sticks[0].axis_y, -0.5f);
    EXPECT_TRUE(normalizedData.buttons[ControllerButton::RSTICK_CLICK]);
}

TEST(BaseController, test_input_deadzone)
{
    NormalizedButtonData normalizedData = {0};

    ControllerConfig config;
    config.buttonsAnalog[ControllerButton::LSTICK_LEFT].bind = ControllerAnalogBinding_X;
    config.buttonsAnalog[ControllerButton::LSTICK_LEFT].sign = -1.0f;
    config.buttonsAnalog[ControllerButton::LSTICK_RIGHT].bind = ControllerAnalogBinding_X;
    config.buttonsAnalog[ControllerButton::LSTICK_RIGHT].sign = +1.0f;
    config.buttonsAnalog[ControllerButton::LSTICK_UP].bind = ControllerAnalogBinding_Y;
    config.buttonsAnalog[ControllerButton::LSTICK_UP].sign = +1.0f;
    config.buttonsAnalog[ControllerButton::LSTICK_DOWN].bind = ControllerAnalogBinding_Y;
    config.buttonsAnalog[ControllerButton::LSTICK_DOWN].sign = -1.0f;

    config.analogDeadzonePercent[ControllerAnalogBinding_X] = 10;
    config.analogDeadzonePercent[ControllerAnalogBinding_Y] = 10;

    RawInputData inputData;
    inputData.analog[ControllerAnalogType_X] = 0.1f;
    inputData.analog[ControllerAnalogType_Y] = 0.2f;

    MockBaseController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());
    controller.MapRawInputToNormalized(inputData, &normalizedData);

    EXPECT_FLOAT_EQ(normalizedData.sticks[0].axis_x, 0.0f);
    EXPECT_NEAR(normalizedData.sticks[0].axis_y, 0.11f, 0.01f);
}

TEST(BaseController, test_input_factor)
{
    NormalizedButtonData normalizedData = {0};

    ControllerConfig config;
    config.buttonsAnalog[ControllerButton::LSTICK_LEFT].bind = ControllerAnalogBinding_X;
    config.buttonsAnalog[ControllerButton::LSTICK_LEFT].sign = -1.0f;
    config.buttonsAnalog[ControllerButton::LSTICK_RIGHT].bind = ControllerAnalogBinding_X;
    config.buttonsAnalog[ControllerButton::LSTICK_RIGHT].sign = +1.0f;
    config.buttonsAnalog[ControllerButton::LSTICK_UP].bind = ControllerAnalogBinding_Y;
    config.buttonsAnalog[ControllerButton::LSTICK_UP].sign = +1.0f;
    config.buttonsAnalog[ControllerButton::LSTICK_DOWN].bind = ControllerAnalogBinding_Y;
    config.buttonsAnalog[ControllerButton::LSTICK_DOWN].sign = -1.0f;

    config.analogFactorPercent[ControllerAnalogBinding_X] = 110;
    config.analogFactorPercent[ControllerAnalogBinding_Y] = 120;

    RawInputData inputData;
    inputData.analog[ControllerAnalogType_X] = 0.9f;
    inputData.analog[ControllerAnalogType_Y] = 0.9f;

    MockBaseController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());
    controller.MapRawInputToNormalized(inputData, &normalizedData);

    EXPECT_NEAR(normalizedData.sticks[0].axis_x, 0.99f, 0.01f);
    EXPECT_FLOAT_EQ(normalizedData.sticks[0].axis_y, 1.0f);
}

TEST(BaseController, test_input_simulate_home_capture)
{
    NormalizedButtonData normalizedData = {0};

    ControllerConfig config;
    config.buttonsPin[ControllerButton::X][0] = 1;
    config.buttonsPin[ControllerButton::Y][0] = 2;
    config.buttonsPin[ControllerButton::A][0] = 3;
    config.buttonsPin[ControllerButton::B][0] = 4;
    config.simulateCombos[0].buttonSimulated = ControllerButton::HOME;
    config.simulateCombos[0].buttons[0] = ControllerButton::X;
    config.simulateCombos[0].buttons[1] = ControllerButton::Y;
    config.simulateCombos[1].buttonSimulated = ControllerButton::CAPTURE;
    config.simulateCombos[1].buttons[0] = ControllerButton::A;
    config.simulateCombos[1].buttons[1] = ControllerButton::B;

    RawInputData inputData;
    inputData.buttons[1] = true;
    inputData.buttons[2] = true;
    inputData.buttons[3] = true;
    inputData.buttons[4] = true;

    MockBaseController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());
    controller.MapRawInputToNormalized(inputData, &normalizedData);

    EXPECT_FALSE(normalizedData.buttons[ControllerButton::X]);
    EXPECT_FALSE(normalizedData.buttons[ControllerButton::A]);
    EXPECT_FALSE(normalizedData.buttons[ControllerButton::Y]);
    EXPECT_FALSE(normalizedData.buttons[ControllerButton::B]);
    EXPECT_TRUE(normalizedData.buttons[ControllerButton::HOME]);
    EXPECT_TRUE(normalizedData.buttons[ControllerButton::CAPTURE]);
}

TEST(BaseController, test_input_stick_by_buttons)
{
    NormalizedButtonData normalizedData = {0};

    ControllerConfig config;
    config.buttonsPin[ControllerButton::LSTICK_LEFT][0] = 1;
    config.buttonsPin[ControllerButton::LSTICK_DOWN][0] = 2;
    config.buttonsPin[ControllerButton::RSTICK_RIGHT][0] = 3;
    config.buttonsPin[ControllerButton::RSTICK_UP][0] = 4;

    RawInputData inputData;
    inputData.buttons[1] = true;
    inputData.buttons[2] = true;
    inputData.buttons[3] = true;
    inputData.buttons[4] = true;

    inputData.analog[ControllerAnalogType_X] = 0.0f;
    inputData.analog[ControllerAnalogType_Y] = 0.0f;

    MockBaseController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());
    controller.MapRawInputToNormalized(inputData, &normalizedData);

    EXPECT_FLOAT_EQ(normalizedData.sticks[0].axis_x, -1.0f);
    EXPECT_FLOAT_EQ(normalizedData.sticks[0].axis_y, -1.0f);

    EXPECT_FLOAT_EQ(normalizedData.sticks[1].axis_x, 1.0f);
    EXPECT_FLOAT_EQ(normalizedData.sticks[1].axis_y, 1.0f);
}

TEST(BaseController, test_input_multiple_pin)
{
    NormalizedButtonData normalizedData = {0};

    ControllerConfig config;
    config.buttonsPin[ControllerButton::X][0] = 1;
    config.buttonsPin[ControllerButton::X][1] = 2;

    RawInputData inputData;
    inputData.buttons[2] = true;

    MockBaseController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());
    controller.MapRawInputToNormalized(inputData, &normalizedData);

    EXPECT_TRUE(normalizedData.buttons[ControllerButton::X]);
}

TEST(BaseController, test_input_complex_combination)
{
    NormalizedButtonData normalizedData = {0};

    ControllerConfig config;

    config.buttonsPin[ControllerButton::A][0] = 2;
    config.buttonsPin[ControllerButton::X][0] = DPAD_UP_BUTTON_ID;

    config.buttonsPin[ControllerButton::DPAD_UP][0] = DPAD_UP_BUTTON_ID;
    config.buttonsPin[ControllerButton::DPAD_DOWN][0] = DPAD_DOWN_BUTTON_ID;
    config.buttonsPin[ControllerButton::DPAD_RIGHT][0] = DPAD_RIGHT_BUTTON_ID;
    config.buttonsPin[ControllerButton::DPAD_LEFT][0] = DPAD_LEFT_BUTTON_ID;

    config.buttonsAnalog[ControllerButton::Y].bind = ControllerAnalogBinding_X;
    config.buttonsAnalog[ControllerButton::Y].sign = -1.0f;

    config.buttonsPin[ControllerButton::L][0] = 1;
    config.buttonsPin[ControllerButton::L][1] = 2;

    config.buttonsPin[ControllerButton::R][0] = DPAD_RIGHT_BUTTON_ID;

    config.simulateCombos[0].buttonSimulated = ControllerButton::HOME;
    config.simulateCombos[0].buttons[0] = ControllerButton::X;
    config.simulateCombos[0].buttons[1] = ControllerButton::Y;
    config.simulateCombos[1].buttonSimulated = ControllerButton::CAPTURE;
    config.simulateCombos[1].buttons[0] = ControllerButton::L;
    config.simulateCombos[1].buttons[1] = ControllerButton::R;

    config.buttonsAnalogUsed = true;

    config.buttonsAnalog[ControllerButton::LSTICK_LEFT].bind = ControllerAnalogBinding_X;
    config.buttonsAnalog[ControllerButton::LSTICK_LEFT].sign = -1.0f;
    config.buttonsAnalog[ControllerButton::LSTICK_RIGHT].bind = ControllerAnalogBinding_X;
    config.buttonsAnalog[ControllerButton::LSTICK_RIGHT].sign = +1.0f;

    config.buttonsAnalog[ControllerButton::LSTICK_UP].bind = ControllerAnalogBinding_Y;
    config.buttonsAnalog[ControllerButton::LSTICK_UP].sign = +1.0f;
    config.buttonsAnalog[ControllerButton::LSTICK_DOWN].bind = ControllerAnalogBinding_Y;
    config.buttonsAnalog[ControllerButton::LSTICK_DOWN].sign = -1.0f;

    RawInputData inputData;
    inputData.analog[ControllerAnalogType_X] = -0.5f;
    inputData.buttons[2] = true;
    inputData.buttons[DPAD_UP_BUTTON_ID] = true;
    inputData.buttons[DPAD_RIGHT_BUTTON_ID] = true;
    // inputData.buttons[2] = true;
    // L linked to A
    // R linked to DPAD_RIGHT
    // L+R will simulate CAPTURE

    // DPAD_UP is alias of X
    // axis_x(0.5) will generate a LSTICK_LEFT
    // LSTICK_LEFT will enable Y
    // X+Y will simulate HOME

    MockBaseController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());
    controller.MapRawInputToNormalized(inputData, &normalizedData);

    EXPECT_FALSE(normalizedData.buttons[ControllerButton::X]);
    EXPECT_FALSE(normalizedData.buttons[ControllerButton::Y]);
    EXPECT_TRUE(normalizedData.buttons[ControllerButton::HOME]);
    EXPECT_TRUE(normalizedData.buttons[ControllerButton::CAPTURE]);
    EXPECT_TRUE(normalizedData.buttons[ControllerButton::DPAD_UP]);
    EXPECT_TRUE(normalizedData.buttons[ControllerButton::DPAD_RIGHT]);
    EXPECT_FLOAT_EQ(normalizedData.sticks[0].axis_x, -0.5f);
}
