#pragma once

#include "BaseController.h"

enum Dualshock3FeatureValue : uint16_t
{
    Ds3FeatureUnknown1 = 0x0201,
    Ds3FeatureUnknown2 = 0x0301,
    Ds3FeatureDeviceAddress = 0x03F2,
    Ds3FeatureStartDevice = 0x03F4,
    Ds3FeatureHostAddress = 0x03F5,
    Ds3FeatureUnknown3 = 0x03F7,
    Ds3FeatureUnknown4 = 0x03EF,
    Ds3FeatureUnknown5 = 0x03F8,
};

enum Dualshock3InputPacketType : uint8_t
{
    Ds3InputPacket_Button = 0x01,
};

_PACKED(struct Dualshock3ButtonData {
    // byte0
    uint8_t type;
    // byte1
    uint8_t pad0;

    // byte2
    bool button9 : 1;  // back
    bool button10 : 1; // stick_left_click
    bool button11 : 1; // stick_right_click
    bool button12 : 1; // start

    bool dpad_up : 1;
    bool dpad_right : 1;
    bool dpad_down : 1;
    bool dpad_left : 1;

    // byte3
    bool button5 : 1; // trigger_left
    bool button6 : 1; // trigger_right
    bool button7 : 1; // bumper_left
    bool button8 : 1; // bumper_right

    bool button1 : 1; // triangle
    bool button2 : 1; // circle
    bool button3 : 1; // cross
    bool button4 : 1; // square

    // byte4
    bool button13 : 1;
    uint8_t pad1 : 7;
    // byte5
    uint8_t pad2;

    // byte6-7
    uint8_t X;
    uint8_t Y;

    // byte8-9
    uint8_t Z;
    uint8_t Rz;

    // byte10-13
    uint8_t pad3[4];

    // byte14-17 These respond for how hard the corresponding button has been pressed. 0xFF completely, 0x00 not at all
    uint8_t dpad_up_pressure;
    uint8_t dpad_right_pressure;
    uint8_t dpad_down_pressure;
    uint8_t dpad_left_pressure;

    // byte18-19
    uint8_t Rx;
    uint8_t Ry;

    // byte20-21
    uint8_t bumper_left_pressure;
    uint8_t bumper_right_pressure;

    // byte22-25
    uint8_t triangle_pressure;
    uint8_t circle_pressure;
    uint8_t cross_pressure;
    uint8_t square_pressure;

    uint8_t pad4[15];

    // byte41-48
    uint16_t accelerometer_x;
    uint16_t accelerometer_y;
    uint16_t accelerometer_z;
    uint16_t gyroscope;
});

enum Dualshock3LEDValue : uint8_t
{
    DS3LED_1 = 0x01,
    DS3LED_2 = 0x02,
    DS3LED_3 = 0x04,
    DS3LED_4 = 0x08,
    DS3LED_5 = 0x09,
    DS3LED_6 = 0x0A,
    DS3LED_7 = 0x0C,
    DS3LED_8 = 0x0D,
    DS3LED_9 = 0x0E,
    DS3LED_10 = 0x0F,
};

class Dualshock3Controller : public BaseController
{
private:
    ControllerResult SendCommand(Dualshock3FeatureValue feature, const void *buffer, uint16_t size);
    ControllerResult SetLED(Dualshock3LEDValue value);

public:
    Dualshock3Controller(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger);
    virtual ~Dualshock3Controller() override;

    virtual ControllerResult Initialize() override;
    virtual ControllerResult OpenInterfaces() override;

    virtual ControllerResult ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx) override;
};