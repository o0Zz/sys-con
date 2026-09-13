/*
 * sm_mitm - libnx-native wrappers for Atmosphère's `sm` MITM tipc extensions.
 *
 * libnx ships no wrappers for Atmosphère's MITM commands (InstallMitm, Acknowledge
 * MitmSession, ...), so this is a faithful C port of the relevant parts of
 * libstratosphere's source/sm/sm_ams.os.horizon.c, written on plain libnx tipc
 * primitives. It lets the ATMOSPHERE=0 build install a MITM without libstratosphere.
 *
 * A single dedicated `sm:` tipc session is opened at Initialize() (RegisterClient via
 * cmd 0 with the PID descriptor) and used from the MITM server thread only.
 */
#pragma once
#include <switch.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Layout-compatible with ams::sm::MitmProcessInfo (ProcessId + ProgramId +
 * cfg::OverrideStatus). Must stay exactly 0x20 bytes: it is the out-payload of
 * AcknowledgeSession (65003) and the in-payload of the ShouldMitm query (65000).
 */
typedef struct
{
    u64 process_id;
    u64 program_id;
    u64 keys_held;
    u64 flags;
} SysconMitmProcessInfo;

Result smMitmInitialize(void);
void   smMitmExit(void);

/* Install a MITM on `name`; returns the mitm port handle and the ShouldMitm query handle. */
Result smMitmInstall(Handle *out_port, Handle *out_query, SmServiceName name);
Result smMitmUninstall(SmServiceName name);
Result smMitmClearFuture(SmServiceName name);

/* Acknowledge a freshly accepted MITM session: yields the forward service + client info. */
Result smMitmAcknowledgeSession(Service *out_forward, SysconMitmProcessInfo *out_info, SmServiceName name);

#ifdef __cplusplus
}
#endif
