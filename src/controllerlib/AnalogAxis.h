#pragma once

#include "EnumArray.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace controllerlib
{
    /*
        The analog axes a controller can report, named after the HID usages they come from.
        Note Rz precedes Rx/Ry. Hosts bind axes by name, so the numeric values are not persisted.
    */
    enum class AnalogAxis : uint8_t
    {
        Unknown = 0,
        X,
        Y,
        Z,
        Rz,
        Rx,
        Ry,
        Slider,
        Dial,
        Brake,
        Accelerator,

        Count
    };

    inline constexpr std::size_t AnalogAxisCount = static_cast<std::size_t>(AnalogAxis::Count);

    /// Every real axis, i.e. all of them except Unknown. Use this to iterate rather than
    /// hand-unrolling one line per axis.
    inline constexpr std::array<AnalogAxis, AnalogAxisCount - 1> AllAnalogAxes{
        AnalogAxis::X,
        AnalogAxis::Y,
        AnalogAxis::Z,
        AnalogAxis::Rz,
        AnalogAxis::Rx,
        AnalogAxis::Ry,
        AnalogAxis::Slider,
        AnalogAxis::Dial,
        AnalogAxis::Brake,
        AnalogAxis::Accelerator,
    };

    /// Analog values in the range -1.0 .. +1.0, one per axis.
    using AnalogValues = EnumArray<AnalogAxis, float, AnalogAxisCount>;

    /// Per-axis percentages (deadzone, scaling factor), 0..100.
    using AnalogPercentages = EnumArray<AnalogAxis, uint8_t, AnalogAxisCount>;

    static_assert(AnalogAxisCount == 11, "AnalogAxis gained or lost a value; check AllAnalogAxes too");
    static_assert(AllAnalogAxes.size() == AnalogAxisCount - 1, "AllAnalogAxes should cover every axis except Unknown");
} // namespace controllerlib
