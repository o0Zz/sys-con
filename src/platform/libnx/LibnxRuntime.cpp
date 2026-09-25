/*
 * libnx (ATMOSPHERE=0) runtime overhead: the libnx application hooks and the entry point.
 * The flavour-agnostic program lives in src/app/main.cpp; this file only does the SM/FS
 * bring-up that libnx does differently, then hands control to syscon::RunApp().
 *
 * NOTE: this is the libnx build only. The Atmosphere build must NOT define __appInit / main;
 * libstratosphere provides those and calls ams::Main() instead (see AmsRuntime.cpp).
 */
#include <switch.h>
#include "main.h"
#include "StdFileManager.h"
#include <cstdlib>
#include <new>

// Static, so it is charged to the ~7 MB of system memory Atmosphere leaves for every homebrew
// sysmodule on 21.0.0+. main.cpp logs what is actually used; size it from that.
#define INNER_HEAP_SIZE 0x40000 // 256 KiB

#define R_ABORT_UNLESS(rc)             \
    {                                  \
        if (R_FAILED(rc)) [[unlikely]] \
        {                              \
            diagAbortWithResult(rc);   \
        }                              \
    }

extern "C"
{
    u32 __nx_applet_type = AppletType_None;
    u32 __nx_fs_num_sessions = 1;

    void __libnx_initheap(void)
    {
        static u8 inner_heap[INNER_HEAP_SIZE];
        extern void *fake_heap_start;
        extern void *fake_heap_end;

        fake_heap_start = inner_heap;
        fake_heap_end = inner_heap + sizeof(inner_heap);
    }

    void __appInit(void)
    {
        R_ABORT_UNLESS(smInitialize());

        syscon::InitializeModules(); // usbHs, pscm, firmware version
        R_ABORT_UNLESS(fsInitialize());

        // sm stays open on purpose: RunApp opens hid:dbg only once it has read the config
        // and knows the run is in hiddbg mode, which is long after this point.

        R_ABORT_UNLESS(fsdevMountSdmc());
    }

    void __appExit(void)
    {
        syscon::FinalizeModules();
        fsdevUnmountAll();
        fsExit();
    }
}

// Targets of the --wrap list in src/app/Makefile; the two lists must match.
extern "C"
{
#define WRAP_ABORT_FUNC(func) [[noreturn]] void __wrap_##func(void) { diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_ShouldNotHappen)); }
    WRAP_ABORT_FUNC(__cxa_throw)
    WRAP_ABORT_FUNC(__cxa_rethrow)
    WRAP_ABORT_FUNC(__cxa_allocate_exception)
    WRAP_ABORT_FUNC(__cxa_free_exception)
    WRAP_ABORT_FUNC(__cxa_begin_catch)
    WRAP_ABORT_FUNC(__cxa_end_catch)
    WRAP_ABORT_FUNC(__cxa_call_unexpected)
    WRAP_ABORT_FUNC(__cxa_call_terminate)
    WRAP_ABORT_FUNC(__gxx_personality_v0)
    WRAP_ABORT_FUNC(_Unwind_Resume)
    WRAP_ABORT_FUNC(_ZSt17__throw_bad_allocv)
    WRAP_ABORT_FUNC(_ZSt28__throw_bad_array_new_lengthv)
    WRAP_ABORT_FUNC(_ZSt19__throw_logic_errorPKc)
    WRAP_ABORT_FUNC(_ZSt20__throw_length_errorPKc)
    WRAP_ABORT_FUNC(_ZSt20__throw_out_of_rangePKc)
    WRAP_ABORT_FUNC(_ZSt24__throw_out_of_range_fmtPKcz)
    WRAP_ABORT_FUNC(_ZSt24__throw_invalid_argumentPKc)
    WRAP_ABORT_FUNC(_ZSt25__throw_bad_function_callv)
    WRAP_ABORT_FUNC(_ZSt20__throw_system_errori)
    WRAP_ABORT_FUNC(_ZNSt11logic_errorC2EPKc)
#undef WRAP_ABORT_FUNC
}

namespace
{
    void *AllocateOrAbort(void *p)
    {
        if (p == nullptr) [[unlikely]]
            diagAbortWithResult(MAKERESULT(Module_Libnx, LibnxError_OutOfMemory));
        return p;
    }
} // namespace

void *operator new(size_t size) { return AllocateOrAbort(malloc(size)); }
void *operator new[](size_t size) { return AllocateOrAbort(malloc(size)); }
void *operator new(size_t size, const std::nothrow_t &) noexcept { return malloc(size); }
void *operator new[](size_t size, const std::nothrow_t &) noexcept { return malloc(size); }
void *operator new(size_t size, std::align_val_t align) { return AllocateOrAbort(aligned_alloc(static_cast<size_t>(align), size)); }
void *operator new[](size_t size, std::align_val_t align) { return AllocateOrAbort(aligned_alloc(static_cast<size_t>(align), size)); }
void operator delete(void *p) noexcept { free(p); }
void operator delete[](void *p) noexcept { free(p); }
void operator delete(void *p, size_t) noexcept { free(p); }
void operator delete[](void *p, size_t) noexcept { free(p); }
void operator delete(void *p, std::align_val_t) noexcept { free(p); }
void operator delete[](void *p, std::align_val_t) noexcept { free(p); }
void operator delete(void *p, size_t, std::align_val_t) noexcept { free(p); }
void operator delete[](void *p, size_t, std::align_val_t) noexcept { free(p); }

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    syscon::StdFileManager fileManager;
    syscon::RunApp(fileManager);
    return 0;
}
