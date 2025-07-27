#pragma once

#ifdef __cplusplus
extern "C"
{
#endif
#include <switch.h>

    void customHidSetup(Service *hid_service, Service *out_iappletresource, SharedMemory *out_real_shmem, SharedMemory *out_fake_shmem);
    void customHidExit(Service *iappletresource, SharedMemory *real_shmem, SharedMemory *fake_shmem);
    void customHidInitialize();

    extern Mutex shmem_mutex;

#ifdef __cplusplus
}
#endif