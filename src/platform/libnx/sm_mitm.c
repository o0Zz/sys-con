/*
 * See sm_mitm.h. Ported from libstratosphere sm_ams.os.horizon.c.
 *
 * Command IDs (Atmosphère sm user interface):
 *   65000 AtmosphereInstallMitm            -> (OutMoveHandle srv, OutMoveHandle qry, ServiceName)
 *   65001 AtmosphereUninstallMitm          -> (ServiceName)
 *   65003 AtmosphereAcknowledgeMitmSession -> (Out<MitmProcessInfo>, OutMoveHandle fwd, ServiceName)
 *   65007 AtmosphereClearFutureMitm        -> (ServiceName)
 */
#include "sm_mitm.h"

static TipcService g_smMitmSrv;

Result smMitmInitialize(void)
{
    Handle sm_handle;
    Result rc = svcConnectToNamedPort(&sm_handle, "sm:");
    while (R_VALUE(rc) == KERNELRESULT(NotFound))
    {
        svcSleepThread(50000000ul);
        rc = svcConnectToNamedPort(&sm_handle, "sm:");
    }

    if (R_SUCCEEDED(rc))
    {
        tipcCreate(&g_smMitmSrv, sm_handle);
        rc = tipcDispatch(&g_smMitmSrv, 0, .in_send_pid = true); /* RegisterClient */
    }

    return rc;
}

void smMitmExit(void)
{
    /* Best-effort DetachClient (cmd 4) then close. */
    tipcDispatch(&g_smMitmSrv, 4, .in_send_pid = true);
    tipcClose(&g_smMitmSrv);
}

Result smMitmInstall(Handle *out_port, Handle *out_query, SmServiceName name)
{
    Handle tmp_handles[2];
    Result rc = tipcDispatchIn(&g_smMitmSrv, 65000, name,
                               .out_handle_attrs = {SfOutHandleAttr_HipcMove, SfOutHandleAttr_HipcMove},
                               .out_handles = tmp_handles, );

    if (R_SUCCEEDED(rc))
    {
        *out_port = tmp_handles[0];
        *out_query = tmp_handles[1];
    }

    return rc;
}

Result smMitmUninstall(SmServiceName name)
{
    return tipcDispatchIn(&g_smMitmSrv, 65001, name);
}

Result smMitmClearFuture(SmServiceName name)
{
    return tipcDispatchIn(&g_smMitmSrv, 65007, name);
}

Result smMitmAcknowledgeSession(Service *out_forward, SysconMitmProcessInfo *out_info, SmServiceName name)
{
    Handle tmp_handle;
    Result rc = tipcDispatchInOut(&g_smMitmSrv, 65003, name, *out_info,
                                  .out_handle_attrs = {SfOutHandleAttr_HipcMove},
                                  .out_handles = &tmp_handle, );

    if (R_SUCCEEDED(rc))
        serviceCreate(out_forward, tmp_handle);

    return rc;
}
