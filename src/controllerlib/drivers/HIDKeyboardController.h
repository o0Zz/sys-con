#pragma once

#include "IKeyboard.h"
#include "drivers/UsbPipeSet.h"

#include <memory>

// HIDKeyboard belongs to HIDDataInterpreter and lives in the global namespace. The forward
// declaration has to stay out here: inside `controllerlib` it would declare a second,
// unrelated type that shadows the real one at every use.
class HIDKeyboard;
class HIDReportDescriptor;

namespace controllerlib
{
    class HIDKeyboardController : public IKeyboard, protected UsbPipeSet
    {
    public:
        HIDKeyboardController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger);
        ~HIDKeyboardController() override;

        Status Initialize() override;
        void Exit() override;

        Status ReadInput(KeyboardState *state, uint32_t timeout_us) override;

    private:
        std::shared_ptr<HIDReportDescriptor> m_descriptor;
        std::shared_ptr<HIDKeyboard> m_keyboard;

        // The lock keys are host state, not device state: a USB keyboard reports Caps Lock as
        // an ordinary press and waits for the host to tell it which LEDs to light.
        uint8_t m_locks = 0;
        std::array<uint8_t, KeyboardMaxKeys> m_previous_keys{};

        void UpdateLocks(const KeyboardState &state);
        bool WasHeld(uint8_t key) const;
    };
} // namespace controllerlib
