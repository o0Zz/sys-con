#pragma once

#include <array>
#include <cstddef>

namespace controllerlib
{
    /*
        A fixed array that can only be subscripted with one specific enum type.

        This exists because the codebase had two arrays of the same shape that meant completely
        different things, and plain C arrays let them be indexed interchangeably:

          - RawInputData::analog was *sized* by ControllerAnalogType_Count but *indexed* by
            ControllerAnalogBinding_* in some files and ControllerAnalogType_* in others. Both
            were unscoped enums, so both decayed to int and the mix-up compiled silently.
          - RawInputData::buttons is indexed by physical pin, NormalizedButtonData::buttons by
            logical button, and both used the same size constant.

        Subscripting with a scoped enum makes that class of mistake a compile error instead of a
        silent misread, at zero runtime cost.
    */
    template <typename Index, typename Value, std::size_t N>
    class EnumArray
    {
    public:
        constexpr EnumArray() = default;

        constexpr explicit EnumArray(Value initial) { m_values.fill(initial); }

        constexpr Value &operator[](Index index) { return m_values[static_cast<std::size_t>(index)]; }
        constexpr const Value &operator[](Index index) const { return m_values[static_cast<std::size_t>(index)]; }

        // Iteration yields values, not indices; use the enum's own AllX table to walk indices.
        constexpr auto begin() { return m_values.begin(); }
        constexpr auto end() { return m_values.end(); }
        constexpr auto begin() const { return m_values.begin(); }
        constexpr auto end() const { return m_values.end(); }

    private:
        std::array<Value, N> m_values{};
    };
} // namespace controllerlib
