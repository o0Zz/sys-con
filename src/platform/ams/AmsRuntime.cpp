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

#include "main.h"
#include "AMSFileManager.h"


namespace ams
{
    namespace syscon
    {
        namespace
        {
            // Also serves libnx through __libnx_alloc below, and the socket driver alone
            // takes ~148 KiB of it for its transfer memory. Do not grow this past 512 KiB:
            // it is static storage, so it counts against the memory the kernel reserves for
            // the process, and pm refuses to launch the module at all (LimitReached) beyond it.
            alignas(0x40) constinit u8 g_heap_memory[512_KB];

            // Backs malloc(); libnx and operator new use the heap above. devkitPro's
            // libsysbase allocates file descriptors with malloc, so socket() needs this.
            alignas(0x1000) constinit u8 g_malloc_memory[64_KB];
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
            // libstratosphere replaces newlib's malloc with one that returns nullptr until a
            // region is registered here, so without this every malloc in the process fails.
            init::InitializeAllocator(ams::syscon::g_malloc_memory, sizeof(ams::syscon::g_malloc_memory));

            R_ABORT_UNLESS(sm::Initialize());

            fs::InitializeForSystem();
            fs::SetAllocator(ams::syscon::Allocate, ams::syscon::Deallocate);
            fs::SetEnabledAutoAbort(false);

            ::syscon::InitializeModules(); // usbHs, pscm

            R_ABORT_UNLESS(fs::MountSdCard("sdmc"));
        }

        void FinalizeSystemModule(void)
        {
            ::syscon::FinalizeModules();
        }

        void Startup(void)
        {
        }

    } // namespace init

    void Main()
    {
        ::syscon::AMSFileManager fileManager;
        ::syscon::RunApp(fileManager);
    }
} // namespace ams

/*
    libnx allocates through these rather than newlib (see libnx tmem.c, which the socket
    driver uses for its transfer memory). libstratosphere only supplies weak versions that
    abort, because a stratosphere module is expected to name its own heap here - which is
    what its -Wl,--require-defined,__libnx_alloc flags are about.
*/
extern "C" void *__libnx_alloc(size_t size)
{
    return ams::syscon::Allocate(size);
}

extern "C" void *__libnx_aligned_alloc(size_t align, size_t size)
{
    return ams::syscon::AllocateWithAlign(size, align);
}

extern "C" void __libnx_free(void *p)
{
    return ams::syscon::Deallocate(p, 0);
}

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
