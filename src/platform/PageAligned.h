#pragma once
#include <cstddef>
#include <cstdint>

/*
 * usb:hs transfer buffers and thread stacks must be 0x1000-aligned and sized. As a class member
 * the alignment rounds the whole object up to page multiples, padding included, so heap-allocate
 * the page on its own instead.
 */
template <size_t Size>
struct alignas(0x1000) PageAligned
{
    static_assert(Size % 0x1000 == 0);
    uint8_t buffer[Size];
};
