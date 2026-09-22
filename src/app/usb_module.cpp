#include <switch.h>
#include "usb_module.h"
#include "controller_handler.h"
#include "Controllers.h"

#include "SwitchUSBDevice.h"
#include "SwitchUSBLock.h"
#include "HorizonResult.h"
#include "logger.h"
#include <string.h>
#include <algorithm>

#define MS_TO_NS(x) (x * 1000000ul)

using namespace controllerlib;

namespace syscon::usb
{
    namespace
    {
        constexpr size_t MaxUsbHsInterfacesSize = 8;
        constexpr size_t MaxUsbEvents = 3; // MaxUsbEvents is limited by usbHsCreateInterfaceAvailableEvent, we can have only up to 3 events

        // Thread that waits on generic usb event
        void UsbEventThreadFunc(void *arg);
        // Thread that waits on any disconnected usb devices
        void UsbInterfaceChangeThreadFunc(void *arg);

        alignas(0x1000) u8 usb_event_thread_stack[0x4000];
        alignas(0x1000) u8 usb_interface_change_thread_stack[0x4000];

        Thread g_usb_event_thread;
        Thread g_usb_interface_change_thread;

        bool is_usb_event_thread_running = false;
        bool is_usb_interface_change_thread_running = false;
        bool g_auto_add_controller = false;

        Event g_usbEvent[MaxUsbEvents] = {};
        Waiter g_usbWaiters[MaxUsbEvents] = {};
        size_t g_usbEventCount = 0;

        s32 QueryAcquiredInterfaces(UsbHsInterface *interfaces, size_t interfaces_maxsize);
        s32 QueryAvailableInterfaces(UsbHsInterface *interfaces, size_t interfaces_maxsize, const UsbHsInterfaceFilter &filter);

        constexpr u16 InterfaceClassSubClassProtocol = UsbHsInterfaceFilterFlags_bInterfaceClass | UsbHsInterfaceFilterFlags_bInterfaceSubClass | UsbHsInterfaceFilterFlags_bInterfaceProtocol;

        // Probed in order; the first match decides the profile the config falls back to.
        constexpr struct
        {
            UsbHsInterfaceFilter filter;
            const char *default_profile;
        } ProbedInterfaces[] = {
            {{.Flags = InterfaceClassSubClassProtocol, .bInterfaceClass = USB_CLASS_VENDOR_SPEC, .bInterfaceSubClass = 0x5D, .bInterfaceProtocol = 0x01}, "xbox360"},
            {{.Flags = InterfaceClassSubClassProtocol, .bInterfaceClass = USB_CLASS_VENDOR_SPEC, .bInterfaceSubClass = 0x5D, .bInterfaceProtocol = 0x81}, "xbox360w"},
            {{.Flags = InterfaceClassSubClassProtocol, .bInterfaceClass = USB_CLASS_VENDOR_SPEC, .bInterfaceSubClass = 0x47, .bInterfaceProtocol = 0xD0}, "xboxone"},
            {{.Flags = InterfaceClassSubClassProtocol, .bInterfaceClass = 0x58, .bInterfaceSubClass = 0x42, .bInterfaceProtocol = 0x00}, "xbox"},
            {{.Flags = UsbHsInterfaceFilterFlags_bInterfaceClass, .bInterfaceClass = USB_CLASS_HID}, ""},
        };

        template <typename T>
        std::unique_ptr<IController> MakeController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config)
        {
            return std::make_unique<T>(std::move(device), config, std::make_unique<syscon::logger::Logger>());
        }

        constexpr struct
        {
            const char *driver;
            const char *name;
            std::unique_ptr<IController> (*Make)(std::unique_ptr<IUSBDevice> &&, const ControllerConfig &);
        } Drivers[] = {
            {"dualshock3", "Dualshock 3", MakeController<Dualshock3Controller>},
            {"xbox360w", "Xbox 360 Wireless", MakeController<Xbox360WirelessController>},
            {"xbox360", "Xbox 360", MakeController<Xbox360Controller>},
            /* One XboxOne controller exposes 2 interfaces, thus we have to take all of them */
            {"xboxone", "Xbox One", MakeController<XboxOneController>},
            {"xbox", "Xbox 1st gen", MakeController<XboxController>},
            {"switch", "Switch", MakeController<SwitchController>},
            {"wii", "Wii", MakeController<WiiController>},
            {"sinput", "SInput", MakeController<SInputController>},
            {"steam2026", "Steam Controller 2026", MakeController<SteamController2026>},
        };

        Result AddEvent(UsbHsInterfaceFilter *filter, const std::string &name);

        void UsbEventThreadFunc(void *arg)
        {
            UsbHsInterface interfaces[MaxUsbHsInterfacesSize] = {};
            u64 timeoutNs = MS_TO_NS(1);
            (void)arg;

            do
            {

                /*
                    Weird issue with this function, This function will never return in some cases (XBOX Serie for example #14)
                    So, we need to use the timeout=1ms to avoid being stuck in this function the first call, and then we can use the UINT64_MAX
                */
                s32 idx_out = 0;
                Result rc = waitObjects(&idx_out, g_usbWaiters, g_usbEventCount, timeoutNs);
                if (R_SUCCEEDED(rc) || R_VALUE(rc) == KERNELRESULT(TimedOut))
                {
                    syscon::logger::LogDebug("New USB device detected (Or polling timeout), checking for controllers ...");

                    /*
                        For unknown reason we have to keep this lock in order to lock the usb stacks during the controller initialization
                        If we don't do that, we will have some issue with the USB stack when we have multiple controllers connected at boot time
                        (Example: Not being able to setLed to the device - On XBOX360 wired controller)
                    */

                    SwitchUSBLock usbLock;

                    s32 total_entries = 0;
                    std::string default_profile;
                    for (const auto &probe : ProbedInterfaces)
                    {
                        total_entries = QueryAvailableInterfaces(interfaces, sizeof(interfaces), probe.filter);
                        if (total_entries > 0)
                        {
                            default_profile = probe.default_profile;
                            break;
                        }
                    }

                    if (total_entries > 0)
                    {
                        timeoutNs = MS_TO_NS(1); // Everytime we find a controller we reset the timeout to loop again on next controllers

                        if (controllers::IsAtControllerLimit())
                        {
                            syscon::logger::LogError("Reach controller limit - Can't add anymore controller !");
                            continue;
                        }

                        UsbHsInterface *interface = &interfaces[0];

                        syscon::logger::LogInfo("Trying to initialize USB device: [%04x-%04x] (Class: 0x%02X, SubClass: 0x%02X, Protocol: 0x%02X, bcd: 0x%04X)...",
                                                interface->device_desc.idVendor,
                                                interface->device_desc.idProduct,
                                                interface->device_desc.bDeviceClass,
                                                interface->device_desc.bDeviceSubClass,
                                                interface->device_desc.bDeviceProtocol,
                                                interface->device_desc.bcdDevice);

                        ControllerConfig config;
                        ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH, &config, interface->device_desc.idVendor, interface->device_desc.idProduct, g_auto_add_controller, default_profile);

                        const auto *driver = std::find_if(std::begin(Drivers), std::end(Drivers), [&config](const auto &candidate)
                                                          { return config.driver == candidate.driver; });

                        if (driver != std::end(Drivers))
                        {
                            syscon::logger::LogInfo("Initializing %s controller (Interface count: %d) ...", driver->name, total_entries);
                            controllers::Insert(driver->Make(std::make_unique<SwitchUSBDevice>(interfaces, total_entries), config));
                        }
                        else
                        {
                            /* For now if Generic controller expose more than 1 interface, we will create as many GenericHIDController as we have interfaces */
                            syscon::logger::LogInfo("Initializing Generic controller (Interface count: %d) ...", total_entries);
                            controllers::Insert(std::make_unique<GenericHIDController>(std::make_unique<SwitchUSBDevice>(interfaces, 1), config, std::make_unique<syscon::logger::Logger>()));
                        }
                    }
                    else
                    {
                        syscon::logger::LogDebug("No HID or XBOX interfaces found !");
                        timeoutNs = UINT64_MAX; // As soon as no controller is found, we wait for the next event
                    }
                }
            } while (is_usb_event_thread_running);
        }

        void UsbInterfaceChangeThreadFunc(void *arg)
        {
            (void)arg;
            UsbHsInterface interfaces[MaxUsbHsInterfacesSize] = {};

            do
            {
                if (R_SUCCEEDED(eventWait(usbHsGetInterfaceStateChangeEvent(), UINT64_MAX)))
                {
                    eventClear(usbHsGetInterfaceStateChangeEvent());

                    syscon::logger::LogDebug("USBInterface state was changed !");

                    s32 total_entries = QueryAcquiredInterfaces(interfaces, sizeof(interfaces));

                    syscon::logger::LogDebug("USBInterface %d interfaces acquired !", total_entries);

                    std::vector<s32> interfaceIDsPlugged;
                    for (int i = 0; i < total_entries; i++)
                        interfaceIDsPlugged.push_back(interfaces[i].inf.ID);

                    controllers::RemoveAllNonPlugged(interfaceIDsPlugged);
                }

            } while (is_usb_interface_change_thread_running);
        }

        s32 QueryAcquiredInterfaces(UsbHsInterface *interfaces, size_t interfaces_maxsize)
        {
            SwitchUSBLock usbLock;
            s32 out_entries = 0;

            if (R_SUCCEEDED(usbHsQueryAcquiredInterfaces(interfaces, interfaces_maxsize, &out_entries)))
                return out_entries;

            return 0;
        }

        s32 QueryAvailableInterfaces(UsbHsInterface *interfaces, size_t interfaces_maxsize, const UsbHsInterfaceFilter &filter)
        {
            SwitchUSBLock usbLock;

            s32 out_entries = 0;
            memset(interfaces, 0, interfaces_maxsize);

            if (R_SUCCEEDED(usbHsQueryAvailableInterfaces(&filter, interfaces, interfaces_maxsize, &out_entries)))
                return out_entries;

            return 0;
        }

        inline Result AddEvent(UsbHsInterfaceFilter *filter, const std::string &name)
        {
            SwitchUSBLock usbLock;

            if (g_usbEventCount >= MaxUsbEvents)
            {
                static bool isMaxEventLogged = false;
                if (!isMaxEventLogged)
                {
                    syscon::logger::LogError("Unable to add future events ! (Max USB events reached !)");
                    isMaxEventLogged = true;
                }
                return syscon::ToHorizonResult(Status::OutOfMemory);
            }

            syscon::logger::LogDebug("Adding event with filter: %s (%d/%d)...", name.c_str(), g_usbEventCount + 1, MaxUsbEvents);
            Result ret = usbHsCreateInterfaceAvailableEvent(&g_usbEvent[g_usbEventCount], true, g_usbEventCount, filter);
            g_usbWaiters[g_usbEventCount] = waiterForEvent(&g_usbEvent[g_usbEventCount]);

            g_usbEventCount++;
            return ret;
        }

    } // namespace

    int Initialize(syscon::config::DiscoveryMode discovery_mode, std::vector<syscon::config::ControllerVidPid> &discovery_vidpid, bool auto_add_controller)
    {
        g_auto_add_controller = auto_add_controller;

        syscon::logger::LogInfo("USB configuration: Discovery mode(%d), Auto add controller(%s)", discovery_mode, auto_add_controller ? "true" : "false");

        if (discovery_mode == syscon::config::DiscoveryMode::HID_AND_XBOX || discovery_mode == syscon::config::DiscoveryMode::VIDPID_AND_XBOX)
        {
            // Filter use to detect XBOX controllers
            UsbHsInterfaceFilter filterAllDevices1{
                .Flags = UsbHsInterfaceFilterFlags_bcdDevice_Min,
                .bcdDevice_Min = 0x0000,
            };
            AddEvent(&filterAllDevices1, "XBOX");
        }

        if (discovery_mode == syscon::config::DiscoveryMode::HID_AND_XBOX)
        {
            // Filter used to detect Generic HID controllers
            // Cause issue with Native Switch Controllers
            UsbHsInterfaceFilter filterAllDevices2{
                .Flags = UsbHsInterfaceFilterFlags_bInterfaceClass | UsbHsInterfaceFilterFlags_bcdDevice_Min,
                .bcdDevice_Min = 0x0000,
                .bInterfaceClass = USB_CLASS_HID,
            };
            AddEvent(&filterAllDevices2, "USB_CLASS_HID");
        }

        if (discovery_mode == syscon::config::DiscoveryMode::VIDPID || discovery_mode == syscon::config::DiscoveryMode::VIDPID_AND_XBOX)
        {
            // Filter known VID/PID
            for (syscon::config::ControllerVidPid &vidpid : discovery_vidpid)
            {
                UsbHsInterfaceFilter filterKnownDevice{
                    .Flags = UsbHsInterfaceFilterFlags_idVendor,
                    .idVendor = vidpid.vid,
                };

                if (vidpid.pid != 0)
                {
                    filterKnownDevice.Flags |= UsbHsInterfaceFilterFlags_idProduct;
                    filterKnownDevice.idProduct = vidpid.pid;
                }

                AddEvent(&filterKnownDevice, std::string(vidpid));
            }
        }

        is_usb_event_thread_running = true;
        Result rc = threadCreate(&g_usb_event_thread, &UsbEventThreadFunc, nullptr, usb_event_thread_stack, sizeof(usb_event_thread_stack), 0x3A, -2);
        if (R_FAILED(rc))
            return rc;
        rc = threadStart(&g_usb_event_thread);
        if (R_FAILED(rc))
            return rc;

        is_usb_interface_change_thread_running = true;
        rc = threadCreate(&g_usb_interface_change_thread, &UsbInterfaceChangeThreadFunc, nullptr, usb_interface_change_thread_stack, sizeof(usb_interface_change_thread_stack), 0x2C, -2);
        if (R_FAILED(rc))
            return rc;
        rc = threadStart(&g_usb_interface_change_thread);
        if (R_FAILED(rc))
            return rc;

        return 0;
    }

    void Exit()
    {
        is_usb_event_thread_running = false;
        is_usb_interface_change_thread_running = false;

        svcCancelSynchronization(g_usb_event_thread.handle);
        threadWaitForExit(&g_usb_event_thread);
        threadClose(&g_usb_event_thread);

        svcCancelSynchronization(g_usb_interface_change_thread.handle);
        threadWaitForExit(&g_usb_interface_change_thread);
        threadClose(&g_usb_interface_change_thread);

        for (size_t i = 0; i < g_usbEventCount; i++)
        {
            SwitchUSBLock usbLock;
            usbHsDestroyInterfaceAvailableEvent(&g_usbEvent[i], i);
        }

        controllers::Clear();
    }

} // namespace syscon::usb