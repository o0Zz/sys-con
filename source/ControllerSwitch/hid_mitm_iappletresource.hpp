#pragma once
#include <vapours.hpp>
#include <stratosphere.hpp>
#include <switch.h>
/*
HOW MITM works:

    We deploy a mitm server that expose a "CreateAppletResource" method.
    This method returns a HidMitmAppletResource object that contains a shared memory handle.
        -> At this point we will create a fake shared memory that will be returned and we will keep the original shared memory handle.


                                      ┌──────────────────────────────────────┐
                                      │           Service HID (Original)     │
                                      │                                      │
                  ┌─────────────────► │                                      │
                  │ 3 CreateAppletRessource                                  │
                  │                   │                                      │
                  │                   │                                      │
                  │                   │                                      │
                  │                   └──────────────────────────────────────┘
                  │
                  │
                  │
                  │
                  │
    ┌─────────────┼─────────────────────┐           ┌──────────────────────────────────────┐
    │           Applet                  │           │            Service MITM              │
    │                                   │           │                                      │     1
    │                                  ◄┼───────────┼─        CreateAppletResource  ◄──────┼──────────────
    │   4. Save Original Shared         │           │                                      │
    │   5. Create Fake Memory           │2 Create   │                                      │
    │                                  ─┼───────────┼──────────────────────────────────────┼─────►
    │                                   │           │          6  Return Applet            │
    │                                   │           │                                      │
    │                                   │           │                                      │
    │                                   │           │                                      │
    │                                   │           └──────────────────────────────────────┘
    │                                   │
    │                                   │
    │                                   │
    │                                   │
    │                                   │◄──────────────────────────────────────────────────────────
    │                                   │                        7 GetSharedMemory
    │                                   │
    │                                   │
    │                                   ├───────────────────────────────────────────────────────────►
    │                                   │                       8 Return Fake Memory
    │                                   │
    └───────────────────────────────────┘


*/

#define AMS_HID_MITM_APPLET_RESOURCE_INTERFACE_INFO(C, H) \
    AMS_SF_METHOD_INFO(C, H, 0, Result, GetSharedMemoryHandle, (ams::sf::OutCopyHandle out), (out))

AMS_SF_DEFINE_INTERFACE(ams::syscon::hid::mitm, IHidMitmAppletResourceInterface, AMS_HID_MITM_APPLET_RESOURCE_INTERFACE_INFO, 0x48494542)

namespace ams::syscon::hid::mitm
{
    class HidMitmAppletResource : public sf::IServiceObject
    {
    public:
        using Interface = IHidMitmAppletResourceInterface;

    public:
        ::SharedMemory m_original_shared_memory;
        ::SharedMemory m_fake_shared_memory;
        ::Service m_appletresource_handle;
        u64 m_pid;

    public:
        HidMitmAppletResource();
        ~HidMitmAppletResource();

        Result GetSharedMemoryHandle(ams::sf::OutCopyHandle out);
        int apply_fake_gamepad(HidSharedMemory *tmp_shmem_mem);

    private:
        u64 GetCurrentTimestamp();
    };
    static_assert(IsIHidMitmAppletResourceInterface<HidMitmAppletResource>);

} // namespace ams::syscon::hid::mitm
