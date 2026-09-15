#pragma once

#include "BaseController.h"

namespace controllerlib
{
    /*
        A controller whose reports are produced by software rather than by hardware, used to
        script input for testing.

        There is no wire format to reverse-engineer here: this one is defined by sys-con, and it
        is deliberately the simplest thing that can express a gamepad. A sender fills in
        NetworkPadReport and the transport delivers it to an IUSBEndpoint; on the console that
        transport is a UDP socket (see src/platform/UdpDevice.h), but nothing in this file
        knows that, which is what keeps it testable on a PC.

        Unlike every other driver, the pin numbers this reports are not dictated by a device --
        so they are the identity: bit N of NetworkPadReport::buttons lands on pin N, and N is a
        GamepadButton value. The shipped [network] profile writes that mapping out in full
        (a=2, b=3, ... home=26) rather than leaving it implicit, so it still passes through the
        normal pin -> button mapping, deadzones and combos like any other controller.

        Note this puts the d-pad on pins 21..24 (the GamepadButton::DPAD_* values) rather than
        the 32..35 pseudo-pins a HID hat reports through. Both are legal -- a pin number is only
        ever what a driver chose to write -- and the identity mapping is worth more here than
        matching a convention that exists for hardware reasons this device does not have.
    */
    _PACKED(struct NetworkPadReport {
        uint32_t magic;     ///< NetworkPadReportMagic. Anything else is dropped.
        uint8_t version;    ///< NetworkPadReportVersion.
        uint8_t pad_index;  ///< Reserved for multiple network pads; only 0 is honoured today.
        uint8_t connected;  ///< 0 reports the pad as unplugged, 1 as plugged.
        uint8_t reserved;   ///< Must be 0.
        uint32_t buttons;   ///< Bit N set means GamepadButton N is pressed. Bit 0 is unused.
        int16_t stick_left_x;   ///< -32768..32767. Positive is right.
        int16_t stick_left_y;   ///< -32768..32767. Positive is *up* (see [default]'s lstick_up=+Y).
        int16_t stick_right_x;
        int16_t stick_right_y;
    });

    /// 'SCNP', little-endian. Guards against a stray datagram on the port being read as input.
    inline constexpr uint32_t NetworkPadReportMagic = 0x504E4353;
    inline constexpr uint8_t NetworkPadReportVersion = 1;

    class NetworkController : public BaseController
    {
    public:
        NetworkController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger);
        virtual ~NetworkController() override;

        virtual Status ParseData(uint8_t *buffer, size_t size, RawInputData *rawData, uint16_t *input_idx) override;

        bool Support(ControllerFeature feature) override;
        Status SetRumble(uint16_t input_idx, float amp_high, float amp_low) override;

        /*
            False until the first valid report arrives, then whatever that report said.

            Starting disconnected is what makes the pad appear on the console exactly when a
            test starts driving it, rather than as a phantom pad at boot. It costs nothing in
            latency: SwitchVirtualGamepadHandler::UpdateInput runs its connect-edge detection
            after ReadInput returns, so the first report attaches the pad and applies its state
            in the same tick.
        */
        bool IsControllerConnected(uint16_t input_idx) override;

        // The report is fixed-size, so there is no reason to offer the endpoint a 256-byte
        // buffer and then ignore most of it.
        size_t GetMaxInputBufferSize() override;

    private:
        bool m_connected = false;
    };
} // namespace controllerlib
