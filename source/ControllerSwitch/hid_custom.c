
#include "hid_custom.h"

#include <stdio.h>

Mutex shmem_mutex;

static Result _customHidCreateAppletResource(Service *srv, Service *out_iappletresource)
{
    u64 AppletResourceUserId = appletGetAppletResourceUserId();

    return serviceDispatchIn(srv, 0, AppletResourceUserId,
                             .in_send_pid = true,
                             .out_num_objects = 1,
                             .out_objects = out_iappletresource, );
}

static Result _customHidGetSharedMemoryHandle(Service *srv, Handle *handle_out)
{
    return serviceDispatch(srv, 0,
                           .out_handle_attrs = {SfOutHandleAttr_HipcCopy},
                           .out_handles = handle_out, );
}

void customHidSetup(Service *hid_service, Service *out_iappletresource, SharedMemory *out_real_shmem, SharedMemory *out_fake_shmem)
{
    Result rc;
    rc = _customHidCreateAppletResource(hid_service, out_iappletresource); // Executes the original ipc
    if (R_FAILED(rc))
        fatalThrow(rc);

    Handle sharedMemHandle;

    rc = _customHidGetSharedMemoryHandle(out_iappletresource, &sharedMemHandle);
    if (R_FAILED(rc))
        fatalThrow(rc);

    shmemLoadRemote(out_real_shmem, sharedMemHandle, 0x40000, Perm_R);
    rc = shmemMap(out_real_shmem);
    if (R_FAILED(rc))
        fatalThrow(rc);

    shmemCreate(out_fake_shmem, 0x40000, Perm_Rw, Perm_R); // sizeof(HidSharedMemory)
    shmemMap(out_fake_shmem);
}

void customHidExit(Service *iappletresource, SharedMemory *real_shmem, SharedMemory *fake_shmem)
{
    mutexLock(&shmem_mutex);
    shmemUnmap(real_shmem);
    shmemClose(real_shmem);

    shmemUnmap(fake_shmem);
    shmemClose(fake_shmem);
    mutexUnlock(&shmem_mutex);

    serviceClose(iappletresource);
}

void customHidInitialize()
{
    mutexInit(&shmem_mutex);
}