#pragma once

#include "IUSBDevice.h"
#include "ILogger.h"
#include "ControllerTypes.h"
#include "ControllerConfig.h"
#include "InputState.h"
#include "Status.h"

#include <memory>

namespace controllerlib
{
    /*
        What every input device has regardless of its shape: the USB device it speaks to, the
        config it was opened with, a logger, and a lifecycle.

        IGamepad, IKeyboard and IMouse each derive from this once and add their own read call.
        They are peers rather than one fat interface because their reads have nothing in
        common: a gamepad reports an absolute stick position, a keyboard a set of held keys,
        a mouse a delta. The host never holds an InputDeviceBase pointer and downcasts it --
        it constructs the concrete driver and hands it to the matching handler -- so there is
        no diamond and no need for RTTI, which the device build does not have.
    */
    class InputDeviceBase
    {
    protected:
        std::unique_ptr<IUSBDevice> m_device;
        ControllerConfig m_config;
        std::unique_ptr<ILogger> m_logger;

    public:
        InputDeviceBase(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config, std::unique_ptr<ILogger> &&logger)
            : m_device(std::move(device)),
              m_config(config),
              m_logger(std::move(logger))
        {
        }

        virtual ~InputDeviceBase() = default;

        virtual InputDeviceKind GetKind() const = 0;

        virtual Status Initialize() = 0;
        virtual void Exit() = 0;

        inline const ControllerConfig &GetConfig() const { return m_config; }

        inline IUSBDevice *GetDevice() { return m_device.get(); }
    };
} // namespace controllerlib
