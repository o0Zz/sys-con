#pragma once

#include <cstdint>

namespace controllerlib
{
    /*
        The result of any ControllerLib operation. This is the library's only error type.

        Scoped and [[nodiscard]] on purpose. As an unscoped enum it converted silently to int,
        and so to whatever integer error type the host happened to use -- an unrelated error
        domain that shares nothing with this one but an underlying type. Code of the shape

            HostResult rc = controller->Initialize();   // returns a Status
            return SomeStatusValue;                     // from a function returning HostResult

        compiled fine and handed the host a number it decoded as one of its own error codes.
        Conversion is now the host's job and has to be written out, which keeps it to one
        function at the host's own boundary instead of happening implicitly anywhere.

        The numeric values are sparse (0, then 100+) and several are never produced; collapsing
        the four near-synonyms for "no input this tick" (NothingTodo, BufferEmpty,
        NoDataAvailable, Timeout) is worth doing but is a behavioural change for every consumer,
        so it is deliberately not done here.
    */
    enum class [[nodiscard]] Status : uint8_t
    {
        Success = 0,
        InvalidEndpoint = 100,
        BufferEmpty = 101,
        NothingTodo = 102,
        NotImplemented = 103,
        UnexpectedData = 104,
        InvalidArgument = 105,
        InvalidReportDescriptor = 106,
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
