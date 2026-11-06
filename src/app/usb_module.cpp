#include <switch.h>
#include "usb_module.h"
#include "controller_handler.h"
#include "Controllers.h"
#include "HidKeyboardMouse.h"

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
        IFileManager *g_file_manager = nullptr;

        /*
            Interfaces whose controller failed to start. A failed start releases the interface, so
            without this it is available again on the next sweep and retried every millisecond.
            They are forgotten once they are no longer present.
        */
        std::vector<s32> g_rejected_interface_ids;

        Event g_usbEvent[MaxUsbEvents] = {};
        Waiter g_usbWaiters[MaxUsbEvents] = {};
        size_t g_usbEventCount = 0;

        s32 QueryAcquiredInterfaces(UsbHsInterface *interfaces, size_t interfaces_maxsize);
        s32 QueryAvailableInterfaces(UsbHsInterface *interfaces, size_t interfaces_maxsize, const UsbHsInterfaceFilter &filter);

        constexpr u16 InterfaceClassSubClassProtocol = UsbHsInterfaceFilterFlags_bInterfaceClass | UsbHsInterfaceFilterFlags_bInterfaceSubClass | UsbHsInterfaceFilterFlags_bInterfaceProtocol;

        constexpr u16 InterfaceVidPid = UsbHsInterfaceFilterFlags_idVendor | UsbHsInterfaceFilterFlags_idProduct;

        enum class DeviceKind
        {
            Gamepad,
            Keyboard,
            Mouse,
        };

        struct Probe
        {
            UsbHsInterfaceFilter filter;
            DeviceKind kind;
            const char *default_profile;
            bool is_xbox; // Still probed with discovery_mode=1, which keeps Xbox controllers.
        };

        // Probed in order; the first match decides the profile the config falls back to.
        // Keyboards and mice come before the HID catch-all so it never turns one into a pad.
        constexpr Probe BuiltInProbes[] = {
            {{.Flags = InterfaceVidPid, .idVendor = 0x045e, .idProduct = 0x02e6}, DeviceKind::Gamepad, "xboxadapter", true},
            {{.Flags = InterfaceVidPid, .idVendor = 0x045e, .idProduct = 0x02fe}, DeviceKind::Gamepad, "xboxadapter", true},
            {{.Flags = InterfaceClassSubClassProtocol, .bInterfaceClass = USB_CLASS_VENDOR_SPEC, .bInterfaceSubClass = 0x5D, .bInterfaceProtocol = 0x01}, DeviceKind::Gamepad, "xbox360", true},
            {{.Flags = InterfaceClassSubClassProtocol, .bInterfaceClass = USB_CLASS_VENDOR_SPEC, .bInterfaceSubClass = 0x5D, .bInterfaceProtocol = 0x81}, DeviceKind::Gamepad, "xbox360w", true},
            {{.Flags = InterfaceClassSubClassProtocol, .bInterfaceClass = USB_CLASS_VENDOR_SPEC, .bInterfaceSubClass = 0x47, .bInterfaceProtocol = 0xD0}, DeviceKind::Gamepad, "xboxone", true},
            {{.Flags = InterfaceClassSubClassProtocol, .bInterfaceClass = 0x58, .bInterfaceSubClass = 0x42, .bInterfaceProtocol = 0x00}, DeviceKind::Gamepad, "xbox", true},
            {{.Flags = InterfaceClassSubClassProtocol, .bInterfaceClass = USB_CLASS_HID, .bInterfaceSubClass = HidSubclassBoot, .bInterfaceProtocol = HidProtocolKeyboard}, DeviceKind::Keyboard, "", false},
            {{.Flags = InterfaceClassSubClassProtocol, .bInterfaceClass = USB_CLASS_HID, .bInterfaceSubClass = HidSubclassBoot, .bInterfaceProtocol = HidProtocolMouse}, DeviceKind::Mouse, "", false},
            {{.Flags = UsbHsInterfaceFilterFlags_bInterfaceClass, .bInterfaceClass = USB_CLASS_HID}, DeviceKind::Gamepad, "", false},
        };

        // The built-in probes discovery_mode allows, then one per discovery_vidpid entry.
        std::vector<Probe> g_probes;

        // A pad with a dedicated driver may expose boot keyboard/mouse interfaces of its own
        // (Steam controllers do); those stay with the pad.
        bool BelongsToDedicatedDriver(const UsbHsInterface &interface)
        {
            return !::syscon::config::FindControllerDriver(CONFIG_FULLPATH, interface.device_desc.idVendor, interface.device_desc.idProduct).empty();
        }

        template <typename T>
        std::unique_ptr<IController> MakeController(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config)
        {
            return std::make_unique<T>(std::move(device), config, std::make_unique<syscon::logger::Logger>());
        }

        // Microsoft's own name, so the file goes on the SD card exactly as extracted from the .cab.
        constexpr const char *XboxAdapterFirmwarePath = CONFIG_PATH "FW_ACC_00U.bin";

        // A missing file reads as an empty image; the driver reports it.
        class FileFirmwareSource final : public IFirmwareSource
        {
        public:
            explicit FileFirmwareSource(const char *path) : m_path(path), m_file(g_file_manager->open(path, OpenFlags_Read)) {}

            size_t Read(uint8_t *buffer, size_t size) override { return m_file ? m_file->read(buffer, size) : 0; }
            const char *GetName() const override { return m_path; }

        private:
            const char *m_path;
            std::unique_ptr<IFile> m_file;
        };

        std::unique_ptr<IController> MakeXboxWirelessAdapter(std::unique_ptr<IUSBDevice> &&device, const ControllerConfig &config)
        {
            return std::make_unique<XboxWirelessAdapter>(std::move(device), config, std::make_unique<syscon::logger::Logger>(), std::make_unique<FileFirmwareSource>(XboxAdapterFirmwarePath));
        }

        bool IsRejected(const UsbHsInterface &interface)
        {
            return std::find(g_rejected_interface_ids.begin(), g_rejected_interface_ids.end(), interface.inf.ID) != g_rejected_interface_ids.end();
        }

        void Reject(const UsbHsInterface *interfaces, s32 count)
        {
            syscon::logger::LogError("USB device [%04x-%04x] failed to start - ignoring it until it is replugged", interfaces[0].device_desc.idVendor, interfaces[0].device_desc.idProduct);
            for (s32 i = 0; i < count; i++)
                g_rejected_interface_ids.push_back(interfaces[i].inf.ID);
        }

        void ForgetUnpluggedRejections()
        {
            if (g_rejected_interface_ids.empty())
                return;

            static UsbHsInterface present[MaxUsbHsInterfacesSize];
            const UsbHsInterfaceFilter any{.Flags = UsbHsInterfaceFilterFlags_bcdDevice_Min, .bcdDevice_Min = 0x0000};
            const s32 count = QueryAvailableInterfaces(present, sizeof(present), any);

            std::erase_if(g_rejected_interface_ids, [count](s32 id)
                          { return std::none_of(present, present + count, [id](const UsbHsInterface &interface)
                                                { return interface.inf.ID == id; }); });
        }

        /*
            usbHsQueryAvailableInterfaces matches interfaces, not devices: two identical pads give
            one list. Only the interfaces of the first usable device are kept, so each device
            becomes its own controller.
        */
        s32 SelectDevice(const Probe &probe, UsbHsInterface *interfaces)
        {
            static UsbHsInterface found[MaxUsbHsInterfacesSize];
            const s32 total = QueryAvailableInterfaces(found, sizeof(found), probe.filter);

            s32 count = 0;
            for (s32 i = 0; i < total; i++)
            {
                if (IsRejected(found[i]))
                    continue;

                if (count == 0)
                {
                    if (probe.kind != DeviceKind::Gamepad && BelongsToDedicatedDriver(found[i]))
                        continue;
                }
                else if (found[i].busID != interfaces[0].busID || found[i].deviceID != interfaces[0].deviceID)
                    continue;

                interfaces[count++] = found[i];
            }

            return count;
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
            {"xboxadapter", "Xbox Wireless Adapter", MakeXboxWirelessAdapter},
            {"xbox", "Xbox 1st gen", MakeController<XboxController>},
            {"switch", "Switch", MakeController<SwitchController>},
            {"wii", "Wii", MakeController<WiiController>},
            {"sinput", "SInput", MakeController<SInputController>},
            {"steam2026", "Steam Controller 2026", MakeController<SteamController2026>},
        };

        // A start that fails rejects the interfaces it was given until they are replugged.
        void StartDevice(DeviceKind kind, const std::string &default_profile, UsbHsInterface *interfaces, s32 count)
        {
            const UsbHsInterface *interface = &interfaces[0];

            syscon::logger::LogInfo("Trying to initialize USB device: [%04x-%04x] (Class: 0x%02X, SubClass: 0x%02X, Protocol: 0x%02X, bcd: 0x%04X)...",
                                    interface->device_desc.idVendor,
                                    interface->device_desc.idProduct,
                                    interface->device_desc.bDeviceClass,
                                    interface->device_desc.bDeviceSubClass,
                                    interface->device_desc.bDeviceProtocol,
                                    interface->device_desc.bcdDevice);

            if (kind == DeviceKind::Keyboard)
            {
                syscon::logger::LogInfo("Initializing keyboard ...");
                if (R_FAILED(controllers::InsertKeyboard(std::make_unique<SwitchUSBDevice>(interfaces, 1))))
                    Reject(interfaces, 1);
                return;
            }

            if (kind == DeviceKind::Mouse)
            {
                syscon::logger::LogInfo("Initializing mouse ...");
                if (R_FAILED(controllers::InsertMouse(std::make_unique<SwitchUSBDevice>(interfaces, 1))))
                    Reject(interfaces, 1);
                return;
            }

            ControllerConfig config;
            ::syscon::config::LoadControllerConfig(CONFIG_FULLPATH, &config, interface->device_desc.idVendor, interface->device_desc.idProduct, g_auto_add_controller, default_profile);

            const auto *driver = std::find_if(std::begin(Drivers), std::end(Drivers), [&config](const auto &candidate)
                                              { return config.driver == candidate.driver; });

            if (driver != std::end(Drivers))
            {
                syscon::logger::LogInfo("Initializing %s controller (Interface count: %d) ...", driver->name, count);
                if (R_FAILED(controllers::Insert(driver->Make(std::make_unique<SwitchUSBDevice>(interfaces, count), config))))
                    Reject(interfaces, count);
                return;
            }

            // A generic HID device is driven through its first interface only; the others are
            // probed on their own.
            syscon::logger::LogInfo("Initializing Generic controller ...");
            if (R_FAILED(controllers::Insert(std::make_unique<GenericHIDController>(std::make_unique<SwitchUSBDevice>(interfaces, 1), config, std::make_unique<syscon::logger::Logger>()))))
                Reject(interfaces, 1);
        }

        Result AddEvent(UsbHsInterfaceFilter *filter, const std::string &name);

        void UsbEventThreadFunc(void *arg)
        {
            static UsbHsInterface interfaces[MaxUsbHsInterfacesSize];
            u64 timeoutNs = MS_TO_NS(1);
            bool limit_logged = false;
            (void)arg;

            do
            {
                /*
                    Weird issue with this function, This function will never return in some cases (XBOX Serie for example #14)
                    So, we need to use the timeout=1ms to avoid being stuck in this function the first call, and then we can use the UINT64_MAX
                */
                s32 idx_out = 0;
                Result rc = waitObjects(&idx_out, g_usbWaiters, g_usbEventCount, timeoutNs);
                if (R_FAILED(rc) && R_VALUE(rc) != KERNELRESULT(TimedOut))
                {
                    if (is_usb_event_thread_running)
                    {
                        syscon::logger::LogError("Waiting for USB devices failed (Error: 0x%08X)", rc);
                        svcSleepThread(MS_TO_NS(1000));
                    }
                    continue;
                }

                syscon::logger::LogDebug("New USB device detected (Or polling timeout), checking for controllers ...");

                /*
                    For unknown reason we have to keep this lock in order to lock the usb stacks during the controller initialization
                    If we don't do that, we will have some issue with the USB stack when we have multiple controllers connected at boot time
                    (Example: Not being able to setLed to the device - On XBOX360 wired controller)
                */
                SwitchUSBLock usbLock;

                ForgetUnpluggedRejections();

                s32 count = 0;
                const Probe *matched = nullptr;
                for (const Probe &probe : g_probes)
                {
                    if ((count = SelectDevice(probe, interfaces)) > 0)
                    {
                        matched = &probe;
                        break;
                    }
                }

                if (matched == nullptr)
                {
                    syscon::logger::LogDebug("No HID or XBOX interfaces found !");
                    timeoutNs = UINT64_MAX; // As soon as no controller is found, we wait for the next event
                    continue;
                }

                /*
                    The device stays available, so it is seen again on every sweep: one log, and a
                    slow retry that picks it up once another controller is unplugged.
                */
                if (controllers::IsAtControllerLimit())
                {
                    if (!limit_logged)
                        syscon::logger::LogError("Reach controller limit - Can't add anymore controller !");
                    limit_logged = true;
                    timeoutNs = MS_TO_NS(1000);
                    continue;
                }

                limit_logged = false;
                timeoutNs = MS_TO_NS(1); // Everytime we find a controller we reset the timeout to loop again on next controllers

                StartDevice(matched->kind, matched->default_profile, interfaces, count);
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
            if (R_FAILED(ret))
            {
                syscon::logger::LogError("Unable to create the USB event for filter %s (Error: 0x%08X)", name.c_str(), ret);
                return ret;
            }

            g_usbWaiters[g_usbEventCount] = waiterForEvent(&g_usbEvent[g_usbEventCount]);
            g_usbEventCount++;
            return ret;
        }

    } // namespace

    int Initialize(IFileManager &fileManager, syscon::config::DiscoveryMode discovery_mode, std::vector<syscon::config::ControllerVidPid> &discovery_vidpid, bool auto_add_controller)
    {
        g_file_manager = &fileManager;
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

        if (g_usbEventCount == 0)
            syscon::logger::LogError("No USB event could be created (discovery_mode=%d, %d discovery_vidpid entries): only devices plugged at boot will be detected", discovery_mode, static_cast<int>(discovery_vidpid.size()));

        // The events only decide when to look; these decide what is taken.
        for (const Probe &probe : BuiltInProbes)
        {
            if (discovery_mode == syscon::config::DiscoveryMode::HID_AND_XBOX ||
                (discovery_mode == syscon::config::DiscoveryMode::VIDPID_AND_XBOX && probe.is_xbox))
                g_probes.push_back(probe);
        }

        // A listed device is still told apart by its interfaces: a keyboard or mouse it exposes
        // is not taken for a pad.
        if (discovery_mode != syscon::config::DiscoveryMode::HID_AND_XBOX)
        {
            for (const syscon::config::ControllerVidPid &vidpid : discovery_vidpid)
            {
                UsbHsInterfaceFilter device{.Flags = UsbHsInterfaceFilterFlags_idVendor, .idVendor = vidpid.vid};
                if (vidpid.pid != 0)
                {
                    device.Flags |= UsbHsInterfaceFilterFlags_idProduct;
                    device.idProduct = vidpid.pid;
                }

                UsbHsInterfaceFilter keyboard = device;
                keyboard.Flags |= InterfaceClassSubClassProtocol;
                keyboard.bInterfaceClass = USB_CLASS_HID;
                keyboard.bInterfaceSubClass = HidSubclassBoot;
                keyboard.bInterfaceProtocol = HidProtocolKeyboard;

                UsbHsInterfaceFilter mouse = keyboard;
                mouse.bInterfaceProtocol = HidProtocolMouse;

                g_probes.push_back({keyboard, DeviceKind::Keyboard, "", false});
                g_probes.push_back({mouse, DeviceKind::Mouse, "", false});
                g_probes.push_back({device, DeviceKind::Gamepad, "", false});
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