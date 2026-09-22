#pragma once

#include "EnumArray.h"

#include <cstddef>
#include <cstdint>

namespace controllerlib
{
    /*
        A physical input "pin", i.e. the input side of mapping.

        A pin number is the HID button usage id a driver reports, and it is the right-hand side
        of whatever mapping a host exposes to its users (a host config that reads `a=2` means
        "the A button comes from pin 2"). Every driver uses the same convention:

          0        unmapped. No driver writes it, so reading it always yields false, which is
                   what makes an unmapped button read as released.
          1..31    HID button usage ids, as reported by the device.
          32..35   the d-pad/hat directions, which have no button usage of their own.

        This is a distinct type from GamepadButton on purpose: both index a bool array, and
        confusing the two index spaces is now a compile error. The constructor is deliberately
        implicit, because drivers legitimately write `rawData->buttons[3]` and requiring
        PinId{3} everywhere would add noise without catching anything.
    */
    class PinId
    {
    public:
        static constexpr uint8_t Unmapped = 0;

        constexpr PinId() = default;
        constexpr PinId(uint8_t value)
            : m_value(value)
        {
        }

        constexpr uint8_t Raw() const { return m_value; }

        constexpr explicit operator std::size_t() const { return m_value; }

        constexpr bool operator==(const PinId &other) const = default;

    private:
        uint8_t m_value{Unmapped};
    };

    /// Button usage ids a HID device can report (HIDDataInterpreter caps these at 32).
    inline constexpr std::size_t MaxHidPins = 32;

    /*
        The d-pad occupies four pseudo-pins above the HID button range, because a hat switch
        reports a direction rather than four button usages. These values are user-facing: the
        README documents 32-35 and configs in the wild use them, so do not renumber.
    */
    inline constexpr PinId DPAD_UP_BUTTON_ID{static_cast<uint8_t>(MaxHidPins + 0)};
    inline constexpr PinId DPAD_DOWN_BUTTON_ID{static_cast<uint8_t>(MaxHidPins + 1)};
    inline constexpr PinId DPAD_LEFT_BUTTON_ID{static_cast<uint8_t>(MaxHidPins + 2)};
    inline constexpr PinId DPAD_RIGHT_BUTTON_ID{static_cast<uint8_t>(MaxHidPins + 3)};

    /// Total addressable pins: the HID button range plus the four d-pad pseudo-pins.
    inline constexpr std::size_t MaxPinCount = MaxHidPins + 4;

    /// One flag per pin, as decoded from a report by a driver's ParseData().
    using RawButtonStates = EnumArray<PinId, bool, MaxPinCount>;

    static_assert(MaxPinCount == 36, "Pin numbering is public API; hosts document 1-31 and 32-35 to users");
    static_assert(DPAD_RIGHT_BUTTON_ID.Raw() == 35, "d-pad pseudo-pins must stay at 32..35");
} // namespace controllerlib
