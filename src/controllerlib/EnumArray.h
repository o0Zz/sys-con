#pragma once

#include <array>
#include <cstddef>

namespace controllerlib
{
    /*
        A fixed array that can only be subscripted with one specific enum type, so indexing a
        physical-pin array with a logical button (or one axis enum with another) is a compile
        error instead of a silent misread.
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
