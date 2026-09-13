/*
 * HidMitmServer - a hand-written, libnx-native HID MITM server.
 *
 * This is the ATMOSPHERE=0 counterpart of src/platform/ams/HidMitm{Service,Module}, which
 * rely on libstratosphere's sf::hipc::ServerManager. libstratosphere is not available in the
 * libnx build, so this reimplements exactly what sys-con needs on plain libnx primitives:
 *
 *   - install a MITM on the "hid" service via Atmosphere's sm tipc extensions (sm_mitm.*),
 *   - run a single-thread svcReplyAndReceive loop over: the mitm port, the ShouldMitm query
 *     handle, and every accepted session (+ the IAppletResource sub-sessions we hand out),
 *   - hook `hid` cmd 0 CreateAppletResource (return our own IAppletResource sub-session) and
 *     IAppletResource cmd 0 GetSharedMemoryHandle (return the fake HID shared memory),
 *   - forward every other command to the real service, tagging the PID like Atmosphere does.
 *
 * The fake shared-memory data plane (HidSharedMemoryManager) is shared unchanged with the
 * ams build. Reference: libstratosphere sf_hipc_server_session_manager.cpp (ForwardRequest /
 * PreProcessCommandBufferForMitm), sf_hipc_mitm_query_api.cpp (ShouldMitm), and
 * src/platform/ams/HidMitmService.cpp.
 */
#include "HidMitm.h"
#include "sm_mitm.h"
#include "SwitchMITMManager.h"
#include "SwitchLogger.h"

#include <vector>
#include <memory>
#include <cstring>

namespace syscon::hid::mitm
{
    namespace
    {
        // Atmosphere tags a forwarded MITM request's PID so the real service still sees the
        // original client. (sf_hipc_server_session_manager.cpp PreProcessCommandBufferForMitm)
        constexpr u64 MitmProcessIdTag = 0xFFFE000000000000ul;
        constexpr u64 ProcessIdMask = 0x0000FFFFFFFFFFFFul;

        // System program-id range (ncm). The three Atmosphere program ids (Mitm/LogManager/
        // Memlet = 0x..10/0x..420/0x..421) all fall inside this range, so it alone suffices.
        constexpr u64 SystemProgramIdStart = 0x0100000000000000ul;
        constexpr u64 SystemProgramIdEnd = 0x01000000000007FFul;

        constexpr size_t MaxHandles = 0x40; // port + query + sessions/sub-sessions
        constexpr int ThreadPriority = 0x20;
        constexpr int ThreadCpuId = 3;

        // Pointer buffer used when forwarding a request that carries recv-statics (see
        // ForwardAndReply). A single buffer is safe: forwarding is synchronous on this one
        // server thread.
        alignas(0x10) u8 g_pointer_buffer[0x1000];

        bool IsSystemProgramId(u64 program_id)
        {
            return program_id >= SystemProgramIdStart && program_id <= SystemProgramIdEnd;
        }

        // Port of HidMitmService::ShouldMitm (src/platform/ams/HidMitmService.cpp:54).
        bool ShouldMitm(const SysconMitmProcessInfo &info)
        {
            static const u64 boot_pid_list[] = {
                0x420000000000000Eul, // sys-ftpd - ignore at boot to avoid an early system crash
            };

            for (u64 boot_pid : boot_pid_list)
            {
                if (info.program_id == boot_pid)
                {
                    ::syscon::logger::LogDebug("HidMitm ShouldMitm: 0x%016lX (Boot) ? (no)", info.program_id);
                    return false;
                }
            }

            if (IsSystemProgramId(info.program_id))
            {
                ::syscon::logger::LogDebug("HidMitm ShouldMitm: 0x%016lX (System) ? (no)", info.program_id);
                return false;
            }

            ::syscon::logger::LogDebug("HidMitm ShouldMitm: 0x%016lX ? (yes)", info.program_id);
            return true;
        }

        /* -------- TLS / CMIF helpers -------- */

        const CmifInHeader *GetInHeader(const HipcParsedRequest &r)
        {
            return static_cast<const CmifInHeader *>(cmifGetAlignedDataStart(r.data.data_words, armGetTls()));
        }

        // Lay out a CMIF reply in TLS: aligned CmifOutHeader (result 0) + out_data + handles.
        // Returns a pointer to the out_data region (right after the header).
        void *BuildCmifReply(u32 out_data_size, u32 num_copy, const Handle *copy, u32 num_move, const Handle *move)
        {
            void *base = armGetTls();
            const u32 raw_size = 16 + static_cast<u32>(sizeof(CmifOutHeader)) + out_data_size;

            HipcRequest req = hipcMakeRequestInline(base,
                                                    .type = CmifCommandType_Invalid,
                                                    .num_data_words = (raw_size + 3) / 4,
                                                    .num_copy_handles = num_copy,
                                                    .num_move_handles = num_move, );

            for (u32 i = 0; i < num_copy; i++)
                req.copy_handles[i] = copy[i];
            for (u32 i = 0; i < num_move; i++)
                req.move_handles[i] = move[i];

            CmifOutHeader *hdr = static_cast<CmifOutHeader *>(cmifGetAlignedDataStart(req.data_words, base));
            hdr->magic = CMIF_OUT_HEADER_MAGIC;
            hdr->version = 0;
            hdr->result = 0;
            hdr->token = 0;
            return static_cast<void *>(hdr + 1);
        }

        // A CMIF reply carrying only a (failure) result code and no out-data/handles.
        void BuildCmifReplyResult(Result rc)
        {
            void *base = armGetTls();
            const u32 raw_size = 16 + static_cast<u32>(sizeof(CmifOutHeader));

            HipcRequest req = hipcMakeRequestInline(base,
                                                    .type = CmifCommandType_Invalid,
                                                    .num_data_words = (raw_size + 3) / 4, );

            CmifOutHeader *hdr = static_cast<CmifOutHeader *>(cmifGetAlignedDataStart(req.data_words, base));
            hdr->magic = CMIF_OUT_HEADER_MAGIC;
            hdr->version = 0;
            hdr->result = rc;
            hdr->token = 0;
        }

        /* -------- session table -------- */

        enum class SessionKind
        {
            Hid,
            AppletResource,
        };

        struct Session
        {
            Handle handle;
            SessionKind kind;
            Service forward;                             // valid for Hid sessions
            SysconMitmProcessInfo info;                  // valid for Hid sessions
            std::shared_ptr<HidSharedMemoryEntry> entry; // valid for AppletResource sub-sessions
        };

        class Server
        {
        public:
            void Run();
            void RequestStop() { m_stop = true; }

        private:
            void AcceptNewSession();
            void ProcessQuery();
            bool ProcessSession(s32 idx); // returns true if a reply was staged in TLS
            bool ForwardAndReply(const HipcParsedRequest &r, const Service *fwd);
            bool HookCreateAppletResource(Session &s, const CmifInHeader *in);
            bool HookGetSharedMemoryHandle(Session &s);
            const Service *ForwardServiceFor(const Session &s);
            void AddSession(const Session &s);
            void CloseSessionAt(s32 idx);

            Handle m_port = INVALID_HANDLE;
            Handle m_query = INVALID_HANDLE;
            std::vector<Handle> m_handles;   // [0]=port, [1]=query, [2+]=sessions
            std::vector<Session> m_sessions; // aligned with m_handles[2..]
            // Copy handles from a forwarded reply. The kernel duplicates them to the client
            // when we reply, so we own our copies and must close them once the reply is sent
            // (drained at the top of the loop, right after svcReplyAndReceive).
            std::vector<Handle> m_pending_close;
            bool m_stop = false;
        };

        void Server::AddSession(const Session &s)
        {
            if (m_handles.size() >= MaxHandles)
            {
                ::syscon::logger::LogError("HidMitm: session table full (%zu), dropping session", m_handles.size());
                svcCloseHandle(s.handle);
                return;
            }
            m_handles.push_back(s.handle);
            m_sessions.push_back(s);
        }

        void Server::CloseSessionAt(s32 idx)
        {
            Session &s = m_sessions[idx - 2];
            if (s.kind == SessionKind::Hid && serviceIsActive(&s.forward))
                serviceClose(&s.forward);
            svcCloseHandle(s.handle);
            m_handles.erase(m_handles.begin() + idx);
            m_sessions.erase(m_sessions.begin() + (idx - 2));
        }

        const Service *Server::ForwardServiceFor(const Session &s)
        {
            if (s.kind == SessionKind::AppletResource)
                return s.entry->GetForwardAppletResource();
            return &s.forward;
        }

        void Server::AcceptNewSession()
        {
            Handle session_h;
            Result rc = svcAcceptSession(&session_h, m_port);
            if (R_FAILED(rc))
            {
                ::syscon::logger::LogError("HidMitm: svcAcceptSession failed: 0x%X", rc);
                return;
            }

            Session s = {};
            s.handle = session_h;
            s.kind = SessionKind::Hid;

            rc = smMitmAcknowledgeSession(&s.forward, &s.info, smEncodeName("hid"));
            if (R_FAILED(rc))
            {
                ::syscon::logger::LogError("HidMitm: AcknowledgeSession failed: 0x%X", rc);
                svcCloseHandle(session_h);
                return;
            }

            ::syscon::logger::LogDebug("HidMitm: session accepted for program 0x%016lX", s.info.program_id);
            AddSession(s);
        }

        void Server::ProcessQuery()
        {
            HipcParsedRequest r = hipcParseRequest(armGetTls());

            if (r.meta.type == CmifCommandType_Request || r.meta.type == CmifCommandType_RequestWithContext)
            {
                const CmifInHeader *in = GetInHeader(r);
                bool should = false;
                if (in->command_id == 65000) // ShouldMitm
                {
                    const SysconMitmProcessInfo *info = reinterpret_cast<const SysconMitmProcessInfo *>(in + 1);
                    should = ShouldMitm(*info);
                }
                bool *out = static_cast<bool *>(BuildCmifReply(sizeof(bool), 0, nullptr, 0, nullptr));
                *out = should;
            }
            else // Control (e.g. QueryPointerBufferSize when the client wraps our handle)
            {
                const CmifInHeader *in = GetInHeader(r);
                if (in->command_id == 3) // QueryPointerBufferSize
                {
                    u16 *out = static_cast<u16 *>(BuildCmifReply(sizeof(u16), 0, nullptr, 0, nullptr));
                    *out = 0;
                }
                else
                {
                    BuildCmifReplyResult(MAKERESULT(11, 403)); // sf::ResultNotSupported
                }
            }
        }

        bool Server::ForwardAndReply(const HipcParsedRequest &r, const Service *fwd)
        {
            void *base = armGetTls();

            // Tag the PID so the real service still attributes the request to the original client.
            if (r.meta.send_pid)
            {
                u64 *pid = reinterpret_cast<u64 *>(static_cast<u8 *>(base) + sizeof(HipcHeader) + sizeof(HipcSpecialHeader));
                *pid = MitmProcessIdTag | (*pid & ProcessIdMask);
            }

            // Redirect any pointer (recv-static) output into our own pointer buffer.
            if (r.meta.num_recv_statics)
            {
                reinterpret_cast<HipcHeader *>(base)->recv_static_mode = 2;
                const uintptr_t off = reinterpret_cast<uintptr_t>(r.data.recv_list) - reinterpret_cast<uintptr_t>(base);
                *reinterpret_cast<HipcRecvListEntry *>(static_cast<u8 *>(base) + off) =
                    hipcMakeRecvStatic(g_pointer_buffer, sizeof(g_pointer_buffer));
            }

            Result rc = svcSendSyncRequest(fwd->session);
            if (R_FAILED(rc))
            {
                ::syscon::logger::LogError("HidMitm: forward svcSendSyncRequest failed: 0x%X", rc);
                BuildCmifReplyResult(rc);
                return true;
            }

            // The real service's reply now sits in TLS; reply it verbatim. Any copy handles
            // in it are duplicated to the client on our reply, so close our copies afterwards.
            HipcResponse resp = hipcParseResponse(armGetTls());
            for (u32 i = 0; i < resp.num_copy_handles; i++)
                m_pending_close.push_back(resp.copy_handles[i]);
            return true;
        }

        bool Server::HookCreateAppletResource(Session &s, const CmifInHeader *in)
        {
            const u64 aruid = *reinterpret_cast<const u64 *>(in + 1);

            // Copy what we need before AddSession() below, which may reallocate m_sessions
            // and invalidate the reference `s`.
            Service forward = s.forward;
            const u64 program_id = s.info.program_id;

            std::shared_ptr<HidSharedMemoryEntry> entry =
                HidSharedMemoryManager::GetHidSharedMemoryManager().CreateIfNotExists(&forward, aruid, program_id);
            if (!entry)
            {
                ::syscon::logger::LogError("HidMitm: CreateIfNotExists failed (aruid=0x%lX)", aruid);
                BuildCmifReplyResult(MAKERESULT(11, 403));
                return true;
            }

            Handle srv_h, cli_h;
            Result rc = svcCreateSession(&srv_h, &cli_h, 0, 0);
            if (R_FAILED(rc))
            {
                ::syscon::logger::LogError("HidMitm: svcCreateSession failed: 0x%X", rc);
                BuildCmifReplyResult(rc);
                return true;
            }

            Session sub = {};
            sub.handle = srv_h;
            sub.kind = SessionKind::AppletResource;
            sub.entry = entry;
            AddSession(sub);

            const Handle move_handles[1] = {cli_h};
            BuildCmifReply(0, 0, nullptr, 1, move_handles);
            ::syscon::logger::LogDebug("HidMitm: CreateAppletResource hooked (aruid=0x%lX)", aruid);
            return true;
        }

        bool Server::HookGetSharedMemoryHandle(Session &s)
        {
            const Handle copy_handles[1] = {s.entry->GetSharedMemoryHandle().handle};
            BuildCmifReply(0, 1, copy_handles, 0, nullptr);
            return true;
        }

        bool Server::ProcessSession(s32 idx)
        {
            Session &s = m_sessions[idx - 2];
            HipcParsedRequest r = hipcParseRequest(armGetTls());

            switch (r.meta.type)
            {
                case CmifCommandType_Close:
                case TipcCommandType_Close:
                    CloseSessionAt(idx);
                    return false;

                case CmifCommandType_Request:
                case CmifCommandType_RequestWithContext:
                {
                    const CmifInHeader *in = GetInHeader(r);
                    if (s.kind == SessionKind::Hid && in->command_id == 0)
                        return HookCreateAppletResource(s, in);
                    if (s.kind == SessionKind::AppletResource && in->command_id == 0)
                        return HookGetSharedMemoryHandle(s);
                    return ForwardAndReply(r, ForwardServiceFor(s));
                }

                default: // Control / ControlWithContext / anything else -> forward
                    return ForwardAndReply(r, ForwardServiceFor(s));
            }
        }

        void Server::Run()
        {
            const SmServiceName hid_name = smEncodeName("hid");

            Result rc = smMitmInstall(&m_port, &m_query, hid_name);
            if (R_FAILED(rc))
            {
                ::syscon::logger::LogError("HidMitm: smMitmInstall(hid) failed: 0x%X", rc);
                return;
            }
            smMitmClearFuture(hid_name);

            m_handles.clear();
            m_handles.push_back(m_port);
            m_handles.push_back(m_query);

            ::syscon::logger::LogInfo("HidMitm: MITM installed on 'hid', serving requests ...");

            Handle reply_target = INVALID_HANDLE;
            while (!m_stop)
            {
                s32 idx = 0;
                rc = svcReplyAndReceive(&idx, m_handles.data(), static_cast<s32>(m_handles.size()), reply_target, UINT64_MAX);
                reply_target = INVALID_HANDLE;

                // The reply staged last iteration has now been delivered: close any copy
                // handles we forwarded onward.
                for (Handle h : m_pending_close)
                    svcCloseHandle(h);
                m_pending_close.clear();

                if (R_FAILED(rc))
                {
                    if (R_VALUE(rc) == KERNELRESULT(ConnectionClosed))
                    {
                        if (idx >= 2 && static_cast<size_t>(idx) < m_handles.size())
                            CloseSessionAt(idx);
                        else
                            break; // port or query closed -> stop
                        continue;
                    }
                    if (R_VALUE(rc) == KERNELRESULT(Cancelled))
                        break;

                    ::syscon::logger::LogError("HidMitm: svcReplyAndReceive failed: 0x%X", rc);
                    continue;
                }

                if (idx == 0)
                {
                    AcceptNewSession(); // no reply
                }
                else if (idx == 1)
                {
                    ProcessQuery();
                    reply_target = m_query;
                }
                else
                {
                    Handle h = m_handles[idx];
                    if (ProcessSession(idx))
                        reply_target = h;
                }
            }

            // Teardown.
            smMitmUninstall(hid_name);
            for (Session &s : m_sessions)
            {
                if (s.kind == SessionKind::Hid && serviceIsActive(&s.forward))
                    serviceClose(&s.forward);
                svcCloseHandle(s.handle);
            }
            m_sessions.clear();
            if (m_query != INVALID_HANDLE)
                svcCloseHandle(m_query);
            if (m_port != INVALID_HANDLE)
                svcCloseHandle(m_port);
            m_handles.clear();
        }

        Server g_server;
        Thread g_thread;
        alignas(0x1000) u8 g_thread_stack[0x4000];
        bool g_initialized = false;

        void ServerThreadFunc(void *)
        {
            g_server.Run();
        }
    } // namespace

    Result Initialize()
    {
        if (g_initialized)
        {
            ::syscon::logger::LogWarning("HidMitm: already initialized, skipping.");
            return 0;
        }

        Result rc = smMitmInitialize();
        if (R_FAILED(rc))
        {
            ::syscon::logger::LogError("HidMitm: smMitmInitialize failed: 0x%X", rc);
            return rc;
        }

        HidSharedMemoryManager::GetHidSharedMemoryManager().Start();

        rc = threadCreate(&g_thread, ServerThreadFunc, nullptr, g_thread_stack, sizeof(g_thread_stack), ThreadPriority, ThreadCpuId);
        if (R_FAILED(rc))
        {
            ::syscon::logger::LogError("HidMitm: threadCreate failed: 0x%X", rc);
            return rc;
        }

        rc = threadStart(&g_thread);
        if (R_FAILED(rc))
        {
            ::syscon::logger::LogError("HidMitm: threadStart failed: 0x%X", rc);
            return rc;
        }

        g_initialized = true;
        return 0;
    }

    void Finalize()
    {
        if (!g_initialized)
            return;

        g_server.RequestStop();
        svcCancelSynchronization(g_thread.handle);
        threadWaitForExit(&g_thread);
        threadClose(&g_thread);

        HidSharedMemoryManager::GetHidSharedMemoryManager().Stop();
        smMitmExit();

        g_initialized = false;
    }
} // namespace syscon::hid::mitm
