#pragma once

#include "EnumArray.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace controllerlib
{
    /*
        The library's normalized gamepad layout, i.e. the output side of input mapping. The
        names follow the Nintendo button layout because that is the superset this library
        normalizes to; a host maps them onto whatever its own output device expects.

        This is deliberately distinct from a physical pin (see PinId): a driver reports pins, a
        host's configuration maps pins onto these, and only these leave the library. The two
        used to be plain bool arrays of identical size, so nothing stopped one being indexed
        with the other's values.

        Scoping the enum also stops it exporting NONE, X, A, B, Y, L, R, ZL, ZR, MINUS, PLUS,
        HOME, CAPTURE and COUNT into the global namespace of every translation unit that
        includes IController.h -- which matters more for a library than for an application.
    */
    enum class GamepadButton : uint8_t
    {
        NONE = 0,
        X,
        A,
        B,
        Y,
        LSTICK_CLICK,
        LSTICK_LEFT,
        LSTICK_RIGHT,
        LSTICK_UP,
        LSTICK_DOWN,
        RSTICK_CLICK,
        RSTICK_LEFT,
        RSTICK_RIGHT,
        RSTICK_UP,
        RSTICK_DOWN,
        L,
        R,
        ZL,
        ZR,
        MINUS,
        PLUS,
        DPAD_UP,
        DPAD_RIGHT,
        DPAD_DOWN,
        DPAD_LEFT,
        CAPTURE,
        HOME,

        COUNT
    };

    inline constexpr std::size_t GamepadButtonCount = static_cast<std::size_t>(GamepadButton::COUNT);

    /*
        The buttons that are mapped straight from a pin, i.e. everything except NONE and the
        eight stick directions (those are derived from analog axes instead, see
        InputNormalizer/MapRawInputToNormalized).

        BaseController.cpp used to carry this as a hand-written 18-entry array that had to be
        kept in step with the enum by eye.
    */
    inline constexpr std::array<GamepadButton, 18> AllDigitalButtons{
        GamepadButton::X,
        GamepadButton::A,
        GamepadButton::B,
        GamepadButton::Y,
        GamepadButton::LSTICK_CLICK,
        GamepadButton::RSTICK_CLICK,
        GamepadButton::L,
        GamepadButton::R,
        GamepadButton::ZL,
        GamepadButton::ZR,
        GamepadButton::MINUS,
        GamepadButton::PLUS,
        GamepadButton::CAPTURE,
        GamepadButton::HOME,
        GamepadButton::DPAD_UP,
        GamepadButton::DPAD_DOWN,
        GamepadButton::DPAD_RIGHT,
        GamepadButton::DPAD_LEFT,
    };

    /// One flag per normalized button. 27 entries; the array this replaced was sized 36,
    /// because it shared MAX_CONTROLLER_BUTTONS with the physical-pin array.
    using GamepadButtonStates = EnumArray<GamepadButton, bool, GamepadButtonCount>;

    static_assert(GamepadButtonCount == 27, "GamepadButton gained or lost a value; check AllDigitalButtons too");
    static_assert(sizeof(GamepadButtonStates) == 27, "GamepadButtonStates should be exactly one bool per button");
    static_assert(AllDigitalButtons.size() == GamepadButtonCount - 1 - 8,
                  "AllDigitalButtons should cover every button except NONE and the 8 stick directions");
} // namespace controllerlib
