#include "gmock/gmock.h"
#include "drivers/BaseController.h"
#include "mocks/Device.h"
#include "mocks/Logger.h"

using namespace controllerlib;

/* --------------------------- Test setup --------------------------- */

class MockBaseController : public BaseController
{
public:
    MockBaseController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger) : BaseController(std::move(device), config, std::move(logger)) {}
    Status ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx) override { return Status::Success; }
    using BaseController::MapRawInputToNormalized; // Move protected method to public for testing
};

/* --------------------------- Tests --------------------------- */

TEST(BaseController, test_input_binding_basis)
{
    NormalizedButtonData normalizedData{};

    ControllerConfig config;
    config.buttonsPin[GamepadButton::X][0] = 1;
    config.buttonsPin[GamepadButton::Y][0] = 2;
    config.buttonsPin[GamepadButton::A][0] = 3;
    config.buttonsPin[GamepadButton::B][0] = 4;
    config.buttonsPin[GamepadButton::RSTICK_CLICK][0] = 15;
    config.buttonsAnalog[GamepadButton::LSTICK_LEFT].bind = AnalogAxis::X;
    config.buttonsAnalog[GamepadButton::LSTICK_LEFT].sign = -1.0f;
    config.buttonsAnalog[GamepadButton::LSTICK_RIGHT].bind = AnalogAxis::X;
    config.buttonsAnalog[GamepadButton::LSTICK_RIGHT].sign = +1.0f;
    config.buttonsAnalog[GamepadButton::LSTICK_UP].bind = AnalogAxis::Y;
    config.buttonsAnalog[GamepadButton::LSTICK_UP].sign = +1.0f;
    config.buttonsAnalog[GamepadButton::LSTICK_DOWN].bind = AnalogAxis::Y;
    config.buttonsAnalog[GamepadButton::LSTICK_DOWN].sign = -1.0f;

    RawInputData inputData;
    inputData.buttons[1] = true;
    inputData.buttons[3] = true;
    inputData.buttons[15] = true;
    inputData.analog[AnalogAxis::X] = 0.5f;  // Right
    inputData.analog[AnalogAxis::Y] = -0.5f; // Down

    MockBaseController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());
    controller.MapRawInputToNormalized(inputData, &normalizedData);

    EXPECT_TRUE(normalizedData.buttons[GamepadButton::X]);
    EXPECT_FALSE(normalizedData.buttons[GamepadButton::Y]);
    EXPECT_TRUE(normalizedData.buttons[GamepadButton::A]);
    EXPECT_FALSE(normalizedData.buttons[GamepadButton::B]);
    EXPECT_FLOAT_EQ(normalizedData.sticks[0].axis_x, 0.5f);
    EXPECT_FLOAT_EQ(normalizedData.sticks[0].axis_y, -0.5f);
    EXPECT_TRUE(normalizedData.buttons[GamepadButton::RSTICK_CLICK]);
}

TEST(BaseController, test_input_deadzone)
{
    NormalizedButtonData normalizedData{};

    ControllerConfig config;
    config.buttonsAnalog[GamepadButton::LSTICK_LEFT].bind = AnalogAxis::X;
    config.buttonsAnalog[GamepadButton::LSTICK_LEFT].sign = -1.0f;
    config.buttonsAnalog[GamepadButton::LSTICK_RIGHT].bind = AnalogAxis::X;
    config.buttonsAnalog[GamepadButton::LSTICK_RIGHT].sign = +1.0f;
    config.buttonsAnalog[GamepadButton::LSTICK_UP].bind = AnalogAxis::Y;
    config.buttonsAnalog[GamepadButton::LSTICK_UP].sign = +1.0f;
    config.buttonsAnalog[GamepadButton::LSTICK_DOWN].bind = AnalogAxis::Y;
    config.buttonsAnalog[GamepadButton::LSTICK_DOWN].sign = -1.0f;

    config.analogDeadzonePercent[AnalogAxis::X] = 10;
    config.analogDeadzonePercent[AnalogAxis::Y] = 10;

    RawInputData inputData;
    inputData.analog[AnalogAxis::X] = 0.1f;
    inputData.analog[AnalogAxis::Y] = 0.2f;

    MockBaseController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());
    controller.MapRawInputToNormalized(inputData, &normalizedData);

    EXPECT_FLOAT_EQ(normalizedData.sticks[0].axis_x, 0.0f);
    EXPECT_NEAR(normalizedData.sticks[0].axis_y, 0.11f, 0.01f);
}

TEST(BaseController, test_input_factor)
{
    NormalizedButtonData normalizedData{};

    ControllerConfig config;
    config.buttonsAnalog[GamepadButton::LSTICK_LEFT].bind = AnalogAxis::X;
    config.buttonsAnalog[GamepadButton::LSTICK_LEFT].sign = -1.0f;
    config.buttonsAnalog[GamepadButton::LSTICK_RIGHT].bind = AnalogAxis::X;
    config.buttonsAnalog[GamepadButton::LSTICK_RIGHT].sign = +1.0f;
    config.buttonsAnalog[GamepadButton::LSTICK_UP].bind = AnalogAxis::Y;
    config.buttonsAnalog[GamepadButton::LSTICK_UP].sign = +1.0f;
    config.buttonsAnalog[GamepadButton::LSTICK_DOWN].bind = AnalogAxis::Y;
    config.buttonsAnalog[GamepadButton::LSTICK_DOWN].sign = -1.0f;

    config.analogFactorPercent[AnalogAxis::X] = 110;
    config.analogFactorPercent[AnalogAxis::Y] = 120;

    RawInputData inputData;
    inputData.analog[AnalogAxis::X] = 0.9f;
    inputData.analog[AnalogAxis::Y] = 0.9f;

    MockBaseController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());
    controller.MapRawInputToNormalized(inputData, &normalizedData);

    EXPECT_NEAR(normalizedData.sticks[0].axis_x, 0.99f, 0.01f);
    EXPECT_FLOAT_EQ(normalizedData.sticks[0].axis_y, 1.0f);
}

TEST(BaseController, test_input_simulate_home_capture)
{
    NormalizedButtonData normalizedData{};

    ControllerConfig config;
    config.buttonsPin[GamepadButton::X][0] = 1;
    config.buttonsPin[GamepadButton::Y][0] = 2;
    config.buttonsPin[GamepadButton::A][0] = 3;
    config.buttonsPin[GamepadButton::B][0] = 4;
    config.simulateCombos[0].buttonSimulated = GamepadButton::HOME;
    config.simulateCombos[0].buttons[0] = GamepadButton::X;
    config.simulateCombos[0].buttons[1] = GamepadButton::Y;
    config.simulateCombos[1].buttonSimulated = GamepadButton::CAPTURE;
    config.simulateCombos[1].buttons[0] = GamepadButton::A;
    config.simulateCombos[1].buttons[1] = GamepadButton::B;

    RawInputData inputData;
    inputData.buttons[1] = true;
    inputData.buttons[2] = true;
    inputData.buttons[3] = true;
    inputData.buttons[4] = true;

    MockBaseController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());
    controller.MapRawInputToNormalized(inputData, &normalizedData);

    EXPECT_FALSE(normalizedData.buttons[GamepadButton::X]);
    EXPECT_FALSE(normalizedData.buttons[GamepadButton::A]);
    EXPECT_FALSE(normalizedData.buttons[GamepadButton::Y]);
    EXPECT_FALSE(normalizedData.buttons[GamepadButton::B]);
    EXPECT_TRUE(normalizedData.buttons[GamepadButton::HOME]);
    EXPECT_TRUE(normalizedData.buttons[GamepadButton::CAPTURE]);
}

TEST(BaseController, test_input_stick_by_buttons)
{
    NormalizedButtonData normalizedData{};

    ControllerConfig config;
    config.buttonsPin[GamepadButton::LSTICK_LEFT][0] = 1;
    config.buttonsPin[GamepadButton::LSTICK_DOWN][0] = 2;
    config.buttonsPin[GamepadButton::RSTICK_RIGHT][0] = 3;
    config.buttonsPin[GamepadButton::RSTICK_UP][0] = 4;

    RawInputData inputData;
    inputData.buttons[1] = true;
    inputData.buttons[2] = true;
    inputData.buttons[3] = true;
    inputData.buttons[4] = true;

    inputData.analog[AnalogAxis::X] = 0.0f;
    inputData.analog[AnalogAxis::Y] = 0.0f;

    MockBaseController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());
    controller.MapRawInputToNormalized(inputData, &normalizedData);

    EXPECT_FLOAT_EQ(normalizedData.sticks[0].axis_x, -1.0f);
    EXPECT_FLOAT_EQ(normalizedData.sticks[0].axis_y, -1.0f);

    EXPECT_FLOAT_EQ(normalizedData.sticks[1].axis_x, 1.0f);
    EXPECT_FLOAT_EQ(normalizedData.sticks[1].axis_y, 1.0f);
}

TEST(BaseController, test_input_multiple_pin)
{
    NormalizedButtonData normalizedData{};

    ControllerConfig config;
    config.buttonsPin[GamepadButton::X][0] = 1;
    config.buttonsPin[GamepadButton::X][1] = 2;

    RawInputData inputData;
    inputData.buttons[2] = true;

    MockBaseController controller(std::make_unique<MockDevice>(), config, std::make_unique<MockLogger>());
    controller.MapRawInputToNormalized(inputData, &normalizedData);

    EXPECT_TRUE(normalizedData.buttons[GamepadButton::X]);
}

TEST(BaseController, test_input_complex_combination)
{
    NormalizedButtonData normalizedData{};

    ControllerConfig config;

    config.buttonsPin[GamepadButton::A][0] = 2;
    config.buttonsPin[GamepadButton::X][0] = DPAD_UP_BUTTON_ID;

    config.buttonsPin[GamepadButton::DPAD_UP][0] = DPAD_UP_BUTTON_ID;
    config.buttonsPin[GamepadButton::DPAD_DOWN][0] = DPAD_DOWN_BUTTON_ID;
    config.buttonsPin[GamepadButton::DPAD_RIGHT][0] = DPAD_RIGHT_BUTTON_ID;
    config.buttonsPin[GamepadButton::DPAD_LEFT][0] = DPAD_LEFT_BUTTON_ID;

    config.buttonsAnalog[GamepadButton::Y].bind = AnalogAxis::X;
    config.buttonsAnalog[GamepadButton::Y].sign = -1.0f;

    config.buttonsPin[GamepadButton::L][0] = 1;
    config.buttonsPin[GamepadButton::L][1] = 2;

    config.buttonsPin[GamepadButton::R][0] = DPAD_RIGHT_BUTTON_ID;

    config.simulateCombos[0].buttonSimulated = GamepadButton::HOME;
    config.simulateCombos[0].buttons[0] = GamepadButton::X;
    config.simulateCombos[0].buttons[1] = GamepadButton::Y;
    config.simulateCombos[1].buttonSimulated = GamepadButton::CAPTURE;
    config.simulateCombos[1].buttons[0] = GamepadButton::L;
    config.simulateCombos[1].buttons[1] = GamepadButton::R;

    config.buttonsAnalogUsed = true;

    config.buttonsAnalog[GamepadButton::LSTICK_LEFT].bind = AnalogAxis::X;
    config.buttonsAnalog[GamepadButton::LSTICK_LEFT].sign = -1.0f;
    config.buttonsAnalog[GamepadButton::LSTICK_RIGHT].bind = AnalogAxis::X;
    config.buttonsAnalog[GamepadButton::LSTICK_RIGHT].sign = +1.0f;

    config.buttonsAnalog[GamepadButton::LSTICK_UP].bind = AnalogAxis::Y;
    config.buttonsAnalog[GamepadButton::LSTICK_UP].sign = +1.0f;
    config.buttonsAnalog[GamepadButton::LSTICK_DOWN].bind = AnalogAxis::Y;
    config.buttonsAnalog[GamepadButton::LSTICK_DOWN].sign = -1.0f;

    RawInputData inputData;
    inputData.analog[AnalogAxis::X] = -0.5f;
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

    EXPECT_FALSE(normalizedData.buttons[GamepadButton::X]);
    EXPECT_FALSE(normalizedData.buttons[GamepadButton::Y]);
    EXPECT_TRUE(normalizedData.buttons[GamepadButton::HOME]);
    EXPECT_TRUE(normalizedData.buttons[GamepadButton::CAPTURE]);
    EXPECT_TRUE(normalizedData.buttons[GamepadButton::DPAD_UP]);
    EXPECT_TRUE(normalizedData.buttons[GamepadButton::DPAD_RIGHT]);
    EXPECT_FLOAT_EQ(normalizedData.sticks[0].axis_x, -0.5f);
}
