#include "Status.h"

namespace controllerlib
{
    const char *ToString(Status status)
    {
        switch (status)
        {
            case Status::Success:
                return "Success";
            case Status::InvalidEndpoint:
                return "InvalidEndpoint";
            case Status::NothingTodo:
                return "NothingTodo";
            case Status::NotImplemented:
                return "NotImplemented";
            case Status::UnexpectedData:
                return "UnexpectedData";
            case Status::InvalidArgument:
                return "InvalidArgument";
            case Status::HidIsNotJoystick:
                return "HidIsNotJoystick";
            case Status::HidIsNotKeyboard:
                return "HidIsNotKeyboard";
            case Status::HidIsNotMouse:
                return "HidIsNotMouse";
            case Status::HidProtocolFailed:
                return "HidProtocolFailed";
            case Status::NoInterfaces:
                return "NoInterfaces";
            case Status::NoDataAvailable:
                return "NoDataAvailable";
            case Status::OutOfMemory:
                return "OutOfMemory";
            case Status::UsbInterfaceAcquire:
                return "UsbInterfaceAcquire";
            case Status::OpenFailed:
                return "OpenFailed";
            case Status::WriteFailed:
                return "WriteFailed";
            case Status::ReadFailed:
                return "ReadFailed";
            case Status::Timeout:
                return "Timeout";
            case Status::UsbEndpointOpen:
                return "UsbEndpointOpen";
            case Status::InvalidIndex:
                return "InvalidIndex";
            case Status::UnknownError:
                return "UnknownError";
        }

        return "Status(?)";
    }
} // namespace controllerlib
