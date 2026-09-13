/*
 * Atmosphere (ATMOSPHERE=1) runtime overhead. libstratosphere owns the process entry
 * (__appInit/__appExit/__libnx_initheap/__nx_applet_type/main are defined in its
 * init_libnx_shim); a module must instead provide ams::Main() and the ams::init::* hooks,
 * which libstratosphere calls. This file provides those, plus the expheap allocator behind
 * operator new/delete, and hands control to syscon::RunApp().
 *
 * Do NOT define __appInit / int main here - that would collide with libstratosphere.
 */
#include <switch.h>
#include <stratosphere.hpp>
#include <memory>

#include "main.h"
#include "logger.h"
#include "AMSFileManager.h"

namespace ams
{
    namespace syscon
    {
        namespace
        {
            alignas(0x40) constinit u8 g_heap_memory[512_KB];
            constinit lmem::HeapHandle g_heap_handle;
            constinit bool g_heap_initialized;
            constinit os::SdkMutex g_heap_init_mutex;

            lmem::HeapHandle GetHeapHandle()
            {
                if (AMS_UNLIKELY(!g_heap_initialized))
                {
                    std::scoped_lock lk(g_heap_init_mutex);

                    if (AMS_LIKELY(!g_heap_initialized))
                    {
                        g_heap_handle = lmem::CreateExpHeap(g_heap_memory, sizeof(g_heap_memory), lmem::CreateOption_ThreadSafe);
                        g_heap_initialized = true;
                    }
                }

                return g_heap_handle;
            }

            void *Allocate(size_t size)
            {
                return lmem::AllocateFromExpHeap(GetHeapHandle(), size);
            }

            void *AllocateWithAlign(size_t size, size_t align)
            {
                return lmem::AllocateFromExpHeap(GetHeapHandle(), size, align);
            }

            void Deallocate(void *p, size_t size)
            {
                AMS_UNUSED(size);
                return lmem::FreeToExpHeap(GetHeapHandle(), p);
            }

        } // namespace

    } // namespace syscon

    namespace init
    {
        void InitializeSystemModuleBeforeConstructors(void)
        {
            R_ABORT_UNLESS(sm::Initialize());

            fs::InitializeForSystem();
            fs::SetAllocator(ams::syscon::Allocate, ams::syscon::Deallocate);
            fs::SetEnabledAutoAbort(false);

            ::syscon::InitializeModules(); // hiddbg, usbHs, pscm

            R_ABORT_UNLESS(fs::MountSdCard("sdmc"));
        }

        void FinalizeSystemModule(void)
        {
            ::syscon::FinalizeModules();
        }

        void Startup(void)
        {
            /* ... */
        }

    } // namespace init

    namespace
    {
        std::unique_ptr<::syscon::IFileManager> MakeFileManager()
        {
            return std::make_unique<::syscon::AMSFileManager>();
        }

        // ams-only banner line: the Atmosphere version and the HOS version it targets. The
        // common banner (name, build, OS version) is logged by RunApp; these macros only
        // exist here.
        void LogAmsBanner()
        {
            ::syscon::logger::LogInfo("Atmosphere: %d.%d.%d (Max supported HOS: %d.%d.%d)",
                                      ATMOSPHERE_RELEASE_VERSION,
                                      ATMOSPHERE_SUPPORTED_HOS_VERSION_MAJOR, ATMOSPHERE_SUPPORTED_HOS_VERSION_MINOR, ATMOSPHERE_SUPPORTED_HOS_VERSION_MICRO);
        }
    } // namespace

    void Main()
    {
        ::syscon::RunApp(&MakeFileManager, &LogAmsBanner);
    }
} // namespace ams

void *operator new(size_t size)
{
    return ams::syscon::Allocate(size);
}

void *operator new(size_t size, const std::nothrow_t &)
{
    return ams::syscon::Allocate(size);
}

void operator delete(void *p)
{
    return ams::syscon::Deallocate(p, 0);
}

void operator delete(void *p, size_t size)
{
    return ams::syscon::Deallocate(p, size);
}

void *operator new[](size_t size)
{
    return ams::syscon::Allocate(size);
}

void *operator new[](size_t size, const std::nothrow_t &)
{
    return ams::syscon::Allocate(size);
}

void operator delete[](void *p)
{
    return ams::syscon::Deallocate(p, 0);
}

void operator delete[](void *p, size_t size)
{
    return ams::syscon::Deallocate(p, size);
}

void *operator new(size_t size, std::align_val_t align)
{
    return ams::syscon::AllocateWithAlign(size, static_cast<size_t>(align));
}

void operator delete(void *p, std::align_val_t align)
{
    AMS_UNUSED(align);
    return ams::syscon::Deallocate(p, 0);
}
