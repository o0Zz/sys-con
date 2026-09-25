#pragma once

#include <cstdint>

namespace controllerlib
{
    /*
        The result of any ControllerLib operation. This is the library's only error type.

        Scoped and [[nodiscard]] so it never converts silently into the host's own integer
        error type, which would decode it as an unrelated error code. The host converts it
        explicitly, at its own boundary.
    */
    enum class [[nodiscard]] Status : uint8_t
    {
        Success = 0,
        InvalidEndpoint = 100,
        NothingTodo = 102,
        NotImplemented = 103,
        UnexpectedData = 104,
        InvalidArgument = 105,
        HidIsNotJoystick = 107,
        NoInterfaces = 108,
        NoDataAvailable = 109,
        OutOfMemory = 110,
        UsbInterfaceAcquire = 111,
        OpenFailed = 112,
        WriteFailed = 113,
        ReadFailed = 114,
        Timeout = 115,
        UsbEndpointOpen = 116,
        InvalidIndex = 117,
        UnknownError = 255,
    };

    constexpr bool Succeeded(Status status) { return status == Status::Success; }
    constexpr bool Failed(Status status) { return status != Status::Success; }

    /// Human-readable name, so failures stop being logged as bare integers.
    const char *ToString(Status status);
} // namespace controllerlib
