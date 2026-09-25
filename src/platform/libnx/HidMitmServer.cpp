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
 *   - answer ConvertCurrentObjectToDomain ourselves so the session and its forward become
 *     domains together and share one object-id space (see HookConvertToDomain),
 *   - forward every other command to the real service, tagging the PID like Atmosphere does.
 *
 * CloneCurrentObject(Ex) is still forwarded verbatim: the client gets a clone of the real
 * session and only loses the MITM on it, which is a hole but never a protocol mismatch.
 *
 * The fake shared-memory data plane (HidSharedMemoryManager) is shared unchanged with the
 * ams build. Reference: libstratosphere sf_hipc_server_session_manager.cpp (ForwardRequest /
 * PreProcessCommandBufferForMitm), sf_hipc_mitm_query_api.cpp (ShouldMitm), and
 * src/platform/ams/HidMitmService.cpp.
 */
#include "HidMitm.h"
#include "sm_mitm.h"
#include "SwitchMITMManager.h"
#include "SwitchMITMVibration.h"
#include "SwitchLogger.h"

#include <vector>
#include <memory>
#include <cstring>
#include <algorithm>

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

        // Everything Nintendo signs - system modules, applets, applications - lives under
        // 0x01. A program id outside it is a homebrew sysmodule (sys-con itself is 0x69...,
        // sys-ftpd and the overlay loader 0x42...): none of them shows a controller, and
        // handing one a fake HID shared memory has taken the console down.
        constexpr u64 NintendoProgramIdStart = 0x0100000000000000ul;
        constexpr u64 NintendoProgramIdEnd = 0x01FFFFFFFFFFFFFFul;

        constexpr size_t MaxHandles = 0x40; // port + query + sessions/sub-sessions
        constexpr int ThreadPriority = 0x20;
        constexpr int ThreadCpuId = 3;

        // Pointer buffer used when forwarding a request that carries recv-statics (see
        // ForwardAndReply). A single buffer is safe: forwarding is synchronous on this one
        // server thread.
        alignas(0x10) u8 g_pointer_buffer[0x1000];

        // Replies are staged here rather than in TLS, for the same reason requests are copied
        // out of it: logging goes through fs, and fs IPC reuses this thread's command buffer.
        // The loop copies this into TLS immediately before svcReplyAndReceive.
        alignas(0x10) u8 g_reply[0x100];

        bool IsSystemProgramId(u64 program_id)
        {
            return program_id >= SystemProgramIdStart && program_id <= SystemProgramIdEnd;
        }

        bool IsHomebrewSysmoduleProgramId(u64 program_id)
        {
            return program_id < NintendoProgramIdStart || program_id > NintendoProgramIdEnd;
        }

        // Port of HidMitmService::ShouldMitm (src/platform/ams/HidMitmService.cpp).
        bool ShouldMitm(const SysconMitmProcessInfo &info)
        {
            if (IsHomebrewSysmoduleProgramId(info.program_id))
            {
                ::syscon::logger::LogDebug("HidMitm ShouldMitm: 0x%016lX (Sysmodule) ? (no)", info.program_id);
                return false;
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

        const CmifInHeader *GetInHeader(const HipcParsedRequest &r, const void *base)
        {
            return static_cast<const CmifInHeader *>(cmifGetAlignedDataStart(r.data.data_words, const_cast<void *>(base)));
        }

        const CmifDomainInHeader *GetDomainInHeader(const HipcParsedRequest &r, const void *base)
        {
            return static_cast<const CmifDomainInHeader *>(cmifGetAlignedDataStart(r.data.data_words, const_cast<void *>(base)));
        }

        // Lay out a CMIF reply in TLS: [CmifDomainOutHeader] + CmifOutHeader + out_data +
        // [object ids], plus the handles. Returns a pointer to the out_data region.
        void *BuildReply(bool domain, Result rc, u32 out_data_size, u32 num_objects, const u32 *objects,
                         u32 num_copy, const Handle *copy, u32 num_move, const Handle *move)
        {
            void *base = g_reply;
            u32 raw_size = 16 + static_cast<u32>(sizeof(CmifOutHeader)) + out_data_size;
            if (domain)
                raw_size += static_cast<u32>(sizeof(CmifDomainOutHeader)) + num_objects * static_cast<u32>(sizeof(u32));

            HipcRequest req = hipcMakeRequestInline(base,
                                                    .type = CmifCommandType_Invalid,
                                                    .num_data_words = (raw_size + 3) / 4,
                                                    .num_copy_handles = num_copy,
                                                    .num_move_handles = num_move, );

            for (u32 i = 0; i < num_copy; i++)
                req.copy_handles[i] = copy[i];
            for (u32 i = 0; i < num_move; i++)
                req.move_handles[i] = move[i];

            u8 *cursor = static_cast<u8 *>(cmifGetAlignedDataStart(req.data_words, base));
            if (domain)
            {
                CmifDomainOutHeader *domain_hdr = reinterpret_cast<CmifDomainOutHeader *>(cursor);
                domain_hdr->num_out_objects = num_objects;
                domain_hdr->padding[0] = domain_hdr->padding[1] = domain_hdr->padding[2] = 0;
                cursor += sizeof(CmifDomainOutHeader);
            }

            CmifOutHeader *hdr = reinterpret_cast<CmifOutHeader *>(cursor);
            hdr->magic = CMIF_OUT_HEADER_MAGIC;
            hdr->version = 0;
            hdr->result = rc;
            hdr->token = 0;

            void *out_data = static_cast<void *>(hdr + 1);
            u32 *out_objects = reinterpret_cast<u32 *>(static_cast<u8 *>(out_data) + out_data_size);
            for (u32 i = 0; i < num_objects; i++)
                out_objects[i] = objects[i];

            return out_data;
        }

        void *BuildCmifReply(u32 out_data_size, u32 num_copy, const Handle *copy, u32 num_move, const Handle *move)
        {
            return BuildReply(false, 0, out_data_size, 0, nullptr, num_copy, copy, num_move, move);
        }

        void *BuildCmifDomainReply(u32 out_data_size, u32 num_objects, const u32 *objects, u32 num_copy, const Handle *copy)
        {
            return BuildReply(true, 0, out_data_size, num_objects, objects, num_copy, copy, 0, nullptr);
        }

        // A CMIF reply carrying only a (failure) result code and no out-data/handles. The
        // reply to a domain request keeps the domain header: that is how the client reads it.
        void BuildCmifReplyResult(Result rc, bool domain = false)
        {
            BuildReply(domain, rc, 0, 0, nullptr, 0, nullptr, 0, nullptr);
        }

        // A successful reply carrying nothing but out-data, whichever shape the session has.
        void *BuildCmifDataReply(u32 out_data_size, bool domain)
        {
            if (domain)
                return BuildCmifDomainReply(out_data_size, 0, nullptr, 0, nullptr);
            return BuildCmifReply(out_data_size, 0, nullptr, 0, nullptr);
        }

        /* -------- hid vibration commands (https://switchbrew.org/wiki/HID_services) -------- */

        constexpr u32 HidCmdSetSupportedNpadStyleSet = 100;
        constexpr u32 HidCmdDisconnectNpad = 107;
        constexpr u32 HidCmdGetVibrationDeviceInfo = 200;
        constexpr u32 HidCmdSendVibrationValue = 201;
        constexpr u32 HidCmdGetActualVibrationValue = 202;
        constexpr u32 HidCmdCreateActiveVibrationDeviceList = 203;
        constexpr u32 HidCmdSendVibrationValues = 206;
        constexpr u32 HidCmdSendVibrationGcErmCommand = 207;
        constexpr u32 HidCmdGetActualVibrationGcErmCommand = 208;
        constexpr u32 HidCmdIsVibrationDeviceMounted = 211;
        constexpr u32 ActiveVibrationDeviceListCmdActivate = 0;

        // Request payloads, as libnx serializes them (external/libnx/nx/source/services/hid.c).
        struct VibrationSendValueIn
        {
            HidVibrationDeviceHandle handle;
            HidVibrationValue value;
            u32 pad;
            u64 aruid;
        };

        struct VibrationHandleIn
        {
            HidVibrationDeviceHandle handle;
            u32 pad;
            u64 aruid;
        };

        struct VibrationGcErmIn
        {
            HidVibrationDeviceHandle handle;
            u32 pad;
            u64 aruid;
            u64 command;
        };

        static_assert(sizeof(VibrationSendValueIn) == 0x20);
        static_assert(sizeof(VibrationHandleIn) == 0x10);
        static_assert(sizeof(VibrationGcErmIn) == 0x18);

        /* -------- session table -------- */


        enum class SessionKind
        {
            Hid,
            AppletResource,
            VibrationDeviceList,
        };

        const char *KindName(SessionKind kind)
        {
            switch (kind)
            {
                case SessionKind::AppletResource:
                    return "appletres";
                case SessionKind::VibrationDeviceList:
                    return "vibrationlist";
                default:
                    return "hid";
            }
        }

        void TraceRequest(const char *what, SessionKind kind, const HipcParsedRequest &r, u32 command_id)
        {
            ::syscon::logger::LogDebug("HidMitm: %s [%s] type=%u cmd=%u pid=%u statics=%u/%u bufs=%u/%u/%u handles=%u/%u words=%u",
                                       what, KindName(kind), r.meta.type, command_id, r.meta.send_pid,
                                       r.meta.num_send_statics, r.meta.num_recv_statics,
                                       r.meta.num_send_buffers, r.meta.num_recv_buffers, r.meta.num_exch_buffers,
                                       r.meta.num_copy_handles, r.meta.num_move_handles, r.meta.num_data_words);
        }

        // An object hosted in a session the client turned into a domain. Its id is the one the
        // real service handed out for the matching real object, which is what lets every
        // domain request we do not hook be forwarded byte for byte.
        struct DomainObject
        {
            u32 object_id;
            SessionKind kind;
            std::shared_ptr<HidSharedMemoryEntry> entry; // AppletResource objects only
        };

        struct Session
        {
            Handle handle;
            SessionKind kind;
            Service forward;                             // valid for Hid sessions
            SysconMitmProcessInfo info;                  // valid for Hid sessions
            std::shared_ptr<HidSharedMemoryEntry> entry; // valid for AppletResource sub-sessions
            Service forward_sub;                         // valid for VibrationDeviceList sub-sessions
            bool is_domain;
            std::vector<DomainObject> objects; // non-empty only once is_domain
        };

        class Server
        {
        public:
            void Run();
            void RequestStop() { m_stop = true; }

        private:
            void AcceptNewSession();
            bool ProcessQuery(); // returns true if a reply was staged in TLS
            void SaveRequest() { std::memcpy(m_request, armGetTls(), sizeof(m_request)); }
            void RestoreRequest() { std::memcpy(armGetTls(), m_request, sizeof(m_request)); }
            bool ProcessSession(s32 idx); // returns true if a reply was staged in TLS
            bool ProcessRequest(s32 idx, const HipcParsedRequest &r);
            bool ProcessDomainRequest(s32 idx, const HipcParsedRequest &r);
            bool ForwardAndReply(const HipcParsedRequest &r, Handle forward_session, bool domain_reply);
            bool HookConvertToDomain(s32 idx);
            bool HookCreateAppletResource(s32 idx, u64 aruid);
            bool HookGetSharedMemoryHandle(const std::shared_ptr<HidSharedMemoryEntry> &entry, bool domain);
            bool HookVibration(const HipcParsedRequest &r, u32 command_id, bool domain);
            bool HookSendVibrationValues(const HipcParsedRequest &r, bool domain);
            bool HookCreateVibrationDeviceList(s32 idx, bool domain);
            bool HookActivateVibrationDevice(const HipcParsedRequest &r, Handle forward_session, bool domain);
            const void *GetInData(const HipcParsedRequest &r, bool domain);
            Handle ForwardSessionFor(const Session &s);
            static DomainObject *FindDomainObject(Session &s, u32 object_id);
            bool AddSession(const Session &s);
            void CloseSessionAt(s32 idx);

            /*
             * A private copy of the incoming message. syscon::logger writes to the SD card,
             * and a file write is IPC, which reuses this thread's TLS command buffer - so a
             * single log line between receiving a request and reading its payload silently
             * replaces the request with an fs reply. Everything below parses this copy, and
             * ForwardAndReply puts it back in TLS just before sending it on.
             */
            alignas(0x10) u8 m_request[0x100];

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

        bool Server::AddSession(const Session &s)
        {
            if (m_handles.size() >= MaxHandles)
            {
                ::syscon::logger::LogError("HidMitm: session table full (%zu), dropping session", m_handles.size());
                svcCloseHandle(s.handle);
                return false;
            }
            m_handles.push_back(s.handle);
            m_sessions.push_back(s);
            return true;
        }

        // The real IAppletResource objects a domain session hosts live in the forward
        // session's domain, so closing that session leaves the entries holding a handle
        // number the kernel is free to hand to somebody else. Make them forget it first.
        void AbandonDomainObjects(Session &s)
        {
            for (DomainObject &object : s.objects)
            {
                if (object.kind == SessionKind::AppletResource && object.entry)
                    object.entry->AbandonForwardAppletResource();
            }
            s.objects.clear();
        }

        void Server::CloseSessionAt(s32 idx)
        {
            Session &s = m_sessions[idx - 2];
            if (s.kind == SessionKind::Hid)
            {
                AbandonDomainObjects(s);
                if (serviceIsActive(&s.forward))
                    serviceClose(&s.forward);
            }
            if (s.kind == SessionKind::VibrationDeviceList && serviceIsActive(&s.forward_sub))
                serviceClose(&s.forward_sub);
            svcCloseHandle(s.handle);
            m_handles.erase(m_handles.begin() + idx);
            m_sessions.erase(m_sessions.begin() + (idx - 2));
        }

        Handle Server::ForwardSessionFor(const Session &s)
        {
            if (s.kind == SessionKind::AppletResource)
                return s.entry->GetForwardAppletResource()->session;
            if (s.kind == SessionKind::VibrationDeviceList)
                return s.forward_sub.session;
            return s.forward.session;
        }

        DomainObject *Server::FindDomainObject(Session &s, u32 object_id)
        {
            for (DomainObject &object : s.objects)
            {
                if (object.object_id == object_id)
                    return &object;
            }
            return nullptr;
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

            ::syscon::logger::LogInfo("HidMitm: session accepted for program 0x%016lX", s.info.program_id);
            AddSession(s);
        }

        /*
         * The peer on this handle is sm itself, asking whether to intercept a process that is
         * acquiring 'hid'. Every service acquisition on the console is blocked behind the
         * answer, so a reply of the wrong shape - or a reply to a message that must not get
         * one, such as a session close - deadlocks the whole system with the kernel still
         * running. Anything not positively recognised is therefore left unanswered rather
         * than answered with an invented CMIF payload.
         */
        bool Server::ProcessQuery()
        {
            HipcParsedRequest r = hipcParseRequest(m_request);
            ::syscon::logger::LogDebug("HidMitm: query type=%u words=%u", r.meta.type, r.meta.num_data_words);

            switch (r.meta.type)
            {
                case CmifCommandType_Request:
                case CmifCommandType_RequestWithContext:
                {
                    const CmifInHeader *in = GetInHeader(r, m_request);
                    if (in->magic != CMIF_IN_HEADER_MAGIC)
                    {
                        ::syscon::logger::LogWarning("HidMitm: query has no CMIF header (magic 0x%08X), not replying", in->magic);
                        return false;
                    }
                    if (in->command_id != 65000) // ShouldMitm
                    {
                        ::syscon::logger::LogWarning("HidMitm: unexpected query command %u, not replying", in->command_id);
                        return false;
                    }

                    const SysconMitmProcessInfo *info = reinterpret_cast<const SysconMitmProcessInfo *>(in + 1);
                    bool *out = static_cast<bool *>(BuildCmifReply(sizeof(bool), 0, nullptr, 0, nullptr));
                    *out = ShouldMitm(*info);
                    return true;
                }

                case CmifCommandType_Control:
                case CmifCommandType_ControlWithContext:
                {
                    const CmifInHeader *in = GetInHeader(r, m_request);
                    if (in->magic == CMIF_IN_HEADER_MAGIC && in->command_id == 3) // QueryPointerBufferSize
                    {
                        u16 *out = static_cast<u16 *>(BuildCmifReply(sizeof(u16), 0, nullptr, 0, nullptr));
                        *out = 0;
                        return true;
                    }
                    ::syscon::logger::LogWarning("HidMitm: unhandled query control message, not replying");
                    return false;
                }

                default:
                    ::syscon::logger::LogWarning("HidMitm: query message type %u left unanswered", r.meta.type);
                    return false;
            }
        }

        bool Server::ForwardAndReply(const HipcParsedRequest &r, Handle forward_session, bool domain_reply)
        {
            u8 *base = m_request;

            // Tag the PID so the real service still attributes the request to the original client.
            if (r.meta.send_pid)
            {
                u64 *pid = reinterpret_cast<u64 *>(base + sizeof(HipcHeader) + sizeof(HipcSpecialHeader));
                *pid = MitmProcessIdTag | (*pid & ProcessIdMask);
            }

            // Redirect any pointer (recv-static) output into our own pointer buffer.
            if (r.meta.num_recv_statics)
            {
                reinterpret_cast<HipcHeader *>(base)->recv_static_mode = 2;
                const uintptr_t off = reinterpret_cast<uintptr_t>(r.data.recv_list) - reinterpret_cast<uintptr_t>(base);
                *reinterpret_cast<HipcRecvListEntry *>(base + off) = hipcMakeRecvStatic(g_pointer_buffer, sizeof(g_pointer_buffer));
            }

            // Put the untouched request back where the kernel expects it. Everything above
            // may have logged, and logging goes through fs, which uses this same buffer.
            RestoreRequest();

            Result rc = svcSendSyncRequest(forward_session);

            // Take the reply out of TLS before anything logs, then it is safe to talk.
            std::memcpy(g_reply, armGetTls(), sizeof(g_reply));

            if (R_FAILED(rc))
            {
                ::syscon::logger::LogError("HidMitm: forward svcSendSyncRequest failed: 0x%X", rc);
                BuildCmifReplyResult(rc, domain_reply);
                return true;
            }

            // Reply it verbatim. Any copy handles in it are duplicated to the client when we
            // reply, so our own copies have to be closed once that has happened.
            HipcResponse resp = hipcParseResponse(g_reply);
            for (u32 i = 0; i < resp.num_copy_handles; i++)
                m_pending_close.push_back(resp.copy_handles[i]);
            return true;
        }

        /*
         * Turn the session into a domain the same way libstratosphere does for a mitm session
         * (HipcManager::ConvertCurrentObjectToDomain): convert the forward service too, and
         * adopt the object id the real service reserved for it instead of allocating our own.
         * Both domains then name the same objects by the same ids, which is what makes every
         * unhooked domain request forwardable verbatim - and forwarding this command instead,
         * leaving our side a plain session, is what used to wedge the console.
         */
        bool Server::HookConvertToDomain(s32 idx)
        {
            Session &s = m_sessions[idx - 2];

            if (s.kind != SessionKind::Hid || s.is_domain)
            {
                ::syscon::logger::LogWarning("HidMitm: ConvertCurrentObjectToDomain refused (kind=%d, is_domain=%d)",
                                             static_cast<int>(s.kind), static_cast<int>(s.is_domain));
                BuildCmifReplyResult(MAKERESULT(11, 403));
                return true;
            }

            Result rc = serviceConvertToDomain(&s.forward);
            if (R_FAILED(rc))
            {
                ::syscon::logger::LogError("HidMitm: serviceConvertToDomain failed: 0x%X", rc);
                BuildCmifReplyResult(rc);
                return true;
            }

            const u32 object_id = s.forward.object_id;
            s.is_domain = true;
            s.objects.push_back(DomainObject{object_id, SessionKind::Hid, nullptr});

            ::syscon::logger::LogInfo("HidMitm: session for program 0x%016lX converted to a domain (object id %u)",
                                      s.info.program_id, object_id);

            u32 *out = static_cast<u32 *>(BuildCmifReply(sizeof(u32), 0, nullptr, 0, nullptr));
            *out = object_id;
            return true;
        }

        bool Server::HookCreateAppletResource(s32 idx, u64 aruid)
        {
            // Copy what we need before AddSession() below, which may reallocate m_sessions
            // and invalidate any reference into it.
            Service forward = m_sessions[idx - 2].forward;
            const u64 program_id = m_sessions[idx - 2].info.program_id;
            const u64 process_id = m_sessions[idx - 2].info.process_id;
            const bool domain = m_sessions[idx - 2].is_domain;

            ::syscon::logger::LogInfo("HidMitm: CreateAppletResource from program 0x%016lX (aruid=0x%lX) ...", program_id, aruid);

            std::shared_ptr<HidSharedMemoryEntry> entry =
                HidSharedMemoryManager::GetHidSharedMemoryManager().CreateIfNotExists(&forward, aruid, process_id, program_id);
            if (!entry)
            {
                ::syscon::logger::LogError("HidMitm: CreateIfNotExists failed (aruid=0x%lX)", aruid);
                BuildCmifReplyResult(MAKERESULT(11, 403), domain);
                return true;
            }

            if (domain)
            {
                // The forward is a domain, so the real IAppletResource came back as a domain
                // object: hand the client that very id and host it on our side.
                const u32 object_id = entry->GetForwardAppletResource()->object_id;
                if (object_id == 0)
                {
                    ::syscon::logger::LogError("HidMitm: forwarded IAppletResource is not a domain object");
                    BuildCmifReplyResult(MAKERESULT(11, 403), true);
                    return true;
                }

                m_sessions[idx - 2].objects.push_back(DomainObject{object_id, SessionKind::AppletResource, entry});

                const u32 out_objects[1] = {object_id};
                BuildCmifDomainReply(0, 1, out_objects, 0, nullptr);
                ::syscon::logger::LogInfo("HidMitm: CreateAppletResource hooked for program 0x%016lX (aruid=0x%lX, object id %u)", program_id, aruid, object_id);
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
            sub.info.program_id = program_id;
            sub.entry = entry;
            if (!AddSession(sub))
            {
                // AddSession already closed the server side, so the client end would be a
                // session nobody answers: fail the command instead of handing it over.
                svcCloseHandle(cli_h);
                BuildCmifReplyResult(MAKERESULT(11, 403));
                return true;
            }

            const Handle move_handles[1] = {cli_h};
            BuildCmifReply(0, 0, nullptr, 1, move_handles);
            ::syscon::logger::LogInfo("HidMitm: CreateAppletResource hooked for program 0x%016lX (aruid=0x%lX)", program_id, aruid);
            return true;
        }

        bool Server::HookGetSharedMemoryHandle(const std::shared_ptr<HidSharedMemoryEntry> &entry, bool domain)
        {
            const Handle copy_handles[1] = {entry->GetSharedMemoryHandle().handle};
            ::syscon::logger::LogInfo("HidMitm: GetSharedMemoryHandle hooked -> fake shmem handle 0x%X (domain=%d)",
                                      copy_handles[0], static_cast<int>(domain));
            if (domain)
                BuildCmifDomainReply(0, 0, nullptr, 1, copy_handles);
            else
                BuildCmifReply(0, 1, copy_handles, 0, nullptr);
            return true;
        }

        // The CMIF in-data of the request, past the domain header a domain session adds.
        const void *Server::GetInData(const HipcParsedRequest &r, bool domain)
        {
            const CmifInHeader *in = domain
                                         ? reinterpret_cast<const CmifInHeader *>(GetDomainInHeader(r, m_request) + 1)
                                         : GetInHeader(r, m_request);
            return in + 1;
        }

        /*
         * The vibration commands a game aims at a pad sys-con owns are answered here rather
         * than forwarded: the real hid has no npad in that slot, so it would reject them and
         * the game would stop rumbling. A handle naming a real controller is left alone -
         * returning false tells the caller to forward the request untouched.
         */
        bool Server::HookVibration(const HipcParsedRequest &r, u32 command_id, bool domain)
        {
            const void *data = GetInData(r, domain);

            switch (command_id)
            {
                case HidCmdGetVibrationDeviceInfo:
                {
                    const HidVibrationDeviceHandle handle = *static_cast<const HidVibrationDeviceHandle *>(data);
                    if (!vibration::IsOwned(handle))
                        return false;

                    const HidVibrationDeviceInfo info = vibration::GetDeviceInfo(handle);
                    *static_cast<HidVibrationDeviceInfo *>(BuildCmifDataReply(sizeof(info), domain)) = info;
                    return true;
                }

                case HidCmdSendVibrationValue:
                {
                    const VibrationSendValueIn *in = static_cast<const VibrationSendValueIn *>(data);
                    if (!vibration::IsOwned(in->handle))
                        return false;

                    vibration::Store(in->handle, in->value);
                    BuildCmifDataReply(0, domain);
                    return true;
                }

                case HidCmdGetActualVibrationValue:
                {
                    const VibrationHandleIn *in = static_cast<const VibrationHandleIn *>(data);
                    HidVibrationValue value;
                    if (!vibration::Load(in->handle, &value))
                        return false;

                    *static_cast<HidVibrationValue *>(BuildCmifDataReply(sizeof(value), domain)) = value;
                    return true;
                }

                case HidCmdIsVibrationDeviceMounted:
                {
                    const VibrationHandleIn *in = static_cast<const VibrationHandleIn *>(data);
                    if (!vibration::IsOwned(in->handle))
                        return false;

                    *static_cast<u8 *>(BuildCmifDataReply(sizeof(u8), domain)) = 1;
                    return true;
                }

                case HidCmdSendVibrationValues:
                    return HookSendVibrationValues(r, domain);

                case HidCmdSendVibrationGcErmCommand:
                {
                    const VibrationGcErmIn *in = static_cast<const VibrationGcErmIn *>(data);
                    if (!vibration::IsOwned(in->handle))
                        return false;

                    vibration::StoreGcErm(in->handle, in->command);
                    BuildCmifDataReply(0, domain);
                    return true;
                }

                case HidCmdGetActualVibrationGcErmCommand:
                {
                    const VibrationHandleIn *in = static_cast<const VibrationHandleIn *>(data);
                    u64 command;
                    if (!vibration::LoadGcErm(in->handle, &command))
                        return false;

                    *static_cast<u64 *>(BuildCmifDataReply(sizeof(command), domain)) = command;
                    return true;
                }

                default:
                    return false;
            }
        }

        /*
         * One command can carry handles for a sys-con pad and for a real controller at once.
         * Ours are picked out here; the request is still forwarded unless every handle in it
         * was ours, so the real pads keep rumbling.
         */
        bool Server::HookSendVibrationValues(const HipcParsedRequest &r, bool domain)
        {
            if (r.meta.num_send_statics < 2)
                return false;

            const HidVibrationDeviceHandle *handles = static_cast<const HidVibrationDeviceHandle *>(hipcGetStaticAddress(&r.data.send_statics[0]));
            const HidVibrationValue *values = static_cast<const HidVibrationValue *>(hipcGetStaticAddress(&r.data.send_statics[1]));
            const size_t count = std::min(hipcGetStaticSize(&r.data.send_statics[0]) / sizeof(HidVibrationDeviceHandle),
                                          hipcGetStaticSize(&r.data.send_statics[1]) / sizeof(HidVibrationValue));

            size_t owned = 0;
            for (size_t i = 0; i < count; i++)
            {
                if (!vibration::IsOwned(handles[i]))
                    continue;

                vibration::Store(handles[i], values[i]);
                owned++;
            }

            if (count == 0 || owned != count)
                return false;

            BuildCmifDataReply(0, domain);
            return true;
        }

        /*
         * A game activates its vibration devices through an IActiveVibrationDeviceList before
         * sending any value, so the list has to accept handles for pads the real hid does not
         * have. The real object is still created and kept, because activating a handle that
         * belongs to a real controller has to reach it.
         */
        bool Server::HookCreateVibrationDeviceList(s32 idx, bool domain)
        {
            // Copy what we need before AddSession() below, which may reallocate m_sessions
            // and invalidate any reference into it.
            Service forward = m_sessions[idx - 2].forward;
            const u64 program_id = m_sessions[idx - 2].info.program_id;

            Service real_list = {};
            Result rc = serviceDispatch(&forward, HidCmdCreateActiveVibrationDeviceList,
                                        .out_num_objects = 1,
                                        .out_objects = &real_list, );
            if (R_FAILED(rc))
            {
                ::syscon::logger::LogError("HidMitm: forwarding CreateActiveVibrationDeviceList failed: 0x%X", rc);
                BuildCmifReplyResult(rc, domain);
                return true;
            }

            if (domain)
            {
                // The real object already has an id in the domain both sides share, so the
                // client gets that one and every unhooked command on it forwards verbatim.
                const u32 object_id = real_list.object_id;
                if (object_id == 0)
                {
                    ::syscon::logger::LogError("HidMitm: forwarded IActiveVibrationDeviceList is not a domain object");
                    BuildCmifReplyResult(MAKERESULT(11, 403), true);
                    return true;
                }

                m_sessions[idx - 2].objects.push_back(DomainObject{object_id, SessionKind::VibrationDeviceList, nullptr});

                const u32 out_objects[1] = {object_id};
                BuildCmifDomainReply(0, 1, out_objects, 0, nullptr);
                ::syscon::logger::LogInfo("HidMitm: CreateActiveVibrationDeviceList hooked for program 0x%016lX (object id %u)", program_id, object_id);
                return true;
            }

            Handle srv_h, cli_h;
            rc = svcCreateSession(&srv_h, &cli_h, 0, 0);
            if (R_FAILED(rc))
            {
                ::syscon::logger::LogError("HidMitm: svcCreateSession failed: 0x%X", rc);
                serviceClose(&real_list);
                BuildCmifReplyResult(rc);
                return true;
            }

            Session sub = {};
            sub.handle = srv_h;
            sub.kind = SessionKind::VibrationDeviceList;
            sub.info.program_id = program_id;
            sub.forward_sub = real_list;
            if (!AddSession(sub))
            {
                // AddSession already closed the server side, so the client end would be a
                // session nobody answers: fail the command instead of handing it over.
                svcCloseHandle(cli_h);
                serviceClose(&real_list);
                BuildCmifReplyResult(MAKERESULT(11, 403));
                return true;
            }

            const Handle move_handles[1] = {cli_h};
            BuildCmifReply(0, 0, nullptr, 1, move_handles);
            ::syscon::logger::LogInfo("HidMitm: CreateActiveVibrationDeviceList hooked for program 0x%016lX", program_id);
            return true;
        }

        bool Server::HookActivateVibrationDevice(const HipcParsedRequest &r, Handle forward_session, bool domain)
        {
            const HidVibrationDeviceHandle handle = *static_cast<const HidVibrationDeviceHandle *>(GetInData(r, domain));

            if (!vibration::IsOwned(handle))
                return ForwardAndReply(r, forward_session, domain);

            BuildCmifDataReply(0, domain);
            return true;
        }

        bool Server::ProcessRequest(s32 idx, const HipcParsedRequest &r)
        {
            Session &s = m_sessions[idx - 2];
            const CmifInHeader *in = GetInHeader(r, m_request);
            const u32 command_id = in->command_id;
            const u64 aruid = *reinterpret_cast<const u64 *>(in + 1);
            TraceRequest("request", s.kind, r, command_id);

            if (s.kind == SessionKind::Hid && command_id == 0)
                return HookCreateAppletResource(idx, aruid);
            if (s.kind == SessionKind::AppletResource && command_id == 0)
                return HookGetSharedMemoryHandle(s.entry, false);
            if (s.kind == SessionKind::VibrationDeviceList && command_id == ActiveVibrationDeviceListCmdActivate)
                return HookActivateVibrationDevice(r, ForwardSessionFor(s), false);
            if (s.kind == SessionKind::Hid && command_id == HidCmdCreateActiveVibrationDeviceList)
                return HookCreateVibrationDeviceList(idx, false);
            if (s.kind == SessionKind::Hid && HookVibration(r, command_id, false))
                return true;
            if (s.kind == SessionKind::Hid && command_id == HidCmdDisconnectNpad)
                HidSharedMemoryManager::GetHidSharedMemoryManager().RetireDisconnectedNpad(*static_cast<const u32 *>(GetInData(r, false)));
            if (s.kind == SessionKind::Hid && command_id == HidCmdSetSupportedNpadStyleSet)
                HidSharedMemoryManager::GetHidSharedMemoryManager().OnSupportedNpadStyleSet(s.info.program_id, *static_cast<const u32 *>(GetInData(r, false)));
            return ForwardAndReply(r, ForwardSessionFor(s), false);
        }

        bool Server::ProcessDomainRequest(s32 idx, const HipcParsedRequest &r)
        {
            Session &s = m_sessions[idx - 2];
            const CmifDomainInHeader *domain_hdr = GetDomainInHeader(r, m_request);
            const CmifInHeader *in = reinterpret_cast<const CmifInHeader *>(domain_hdr + 1);
            const u8 domain_type = domain_hdr->type;
            const u32 command_id = in->command_id;
            const u64 aruid = *reinterpret_cast<const u64 *>(in + 1);
            const Handle forward_session = ForwardSessionFor(s);
            DomainObject *object = FindDomainObject(s, domain_hdr->object_id);

            if (domain_type == CmifDomainRequestType_Close)
            {
                // Our ids are the real service's ids, so the close has to reach it - and the
                // entry must stop owning an object the client just freed.
                if (object != nullptr)
                {
                    if (object->entry)
                        object->entry->AbandonForwardAppletResource();
                    s.objects.erase(s.objects.begin() + (object - s.objects.data()));
                }
                return ForwardAndReply(r, forward_session, true);
            }

            if (domain_type == CmifDomainRequestType_SendMessage && object != nullptr)
            {
                if (object->kind == SessionKind::VibrationDeviceList && command_id == ActiveVibrationDeviceListCmdActivate)
                    return HookActivateVibrationDevice(r, forward_session, true);

                if (object->kind == SessionKind::AppletResource && command_id == 0)
                    return HookGetSharedMemoryHandle(object->entry, true);

                if (object->kind == SessionKind::Hid)
                {
                    if (command_id == 0)
                        return HookCreateAppletResource(idx, aruid);
                    if (command_id == HidCmdCreateActiveVibrationDeviceList)
                        return HookCreateVibrationDeviceList(idx, true);
                    if (HookVibration(r, command_id, true))
                        return true;
                    if (command_id == HidCmdDisconnectNpad)
                        HidSharedMemoryManager::GetHidSharedMemoryManager().RetireDisconnectedNpad(*static_cast<const u32 *>(GetInData(r, true)));
                    if (command_id == HidCmdSetSupportedNpadStyleSet)
                        HidSharedMemoryManager::GetHidSharedMemoryManager().OnSupportedNpadStyleSet(s.info.program_id, *static_cast<const u32 *>(GetInData(r, true)));
                }
            }

            return ForwardAndReply(r, forward_session, true);
        }

        bool Server::ProcessSession(s32 idx)
        {
            HipcParsedRequest r = hipcParseRequest(m_request);

            switch (r.meta.type)
            {
                case CmifCommandType_Close:
                case TipcCommandType_Close:
                    ::syscon::logger::LogDebug("HidMitm: close [%s]", KindName(m_sessions[idx - 2].kind));
                    CloseSessionAt(idx);
                    return false;

                case CmifCommandType_Request:
                case CmifCommandType_RequestWithContext:
                    if (m_sessions[idx - 2].is_domain)
                        return ProcessDomainRequest(idx, r);
                    return ProcessRequest(idx, r);

                case CmifCommandType_Control:
                case CmifCommandType_ControlWithContext:
                    // Control messages are never domain-wrapped, whatever the session is.
                {
                    const u32 control_id = GetInHeader(r, m_request)->command_id;
                    TraceRequest("control", m_sessions[idx - 2].kind, r, control_id);
                    if (control_id == 0) // ConvertCurrentObjectToDomain
                        return HookConvertToDomain(idx);
                    return ForwardAndReply(r, ForwardSessionFor(m_sessions[idx - 2]), false);
                }
                default:
                    TraceRequest("other", m_sessions[idx - 2].kind, r, 0);
                    return ForwardAndReply(r, ForwardSessionFor(m_sessions[idx - 2]), false);
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
                // Reply and receive are two syscalls, as in libstratosphere: the staged reply
                // is copied into TLS and sent on its own, so the receive below always starts
                // from a buffer we control. A zero timeout returns TimedOut once the reply is
                // delivered.
                if (reply_target != INVALID_HANDLE)
                {
                    std::memcpy(armGetTls(), g_reply, sizeof(g_reply));
                    s32 unused = 0;
                    const Result reply_rc = svcReplyAndReceive(&unused, nullptr, 0, reply_target, 0);
                    if (R_FAILED(reply_rc) && R_VALUE(reply_rc) != KERNELRESULT(TimedOut))
                        ::syscon::logger::LogWarning("HidMitm: reply on 0x%X failed: 0x%X", reply_target, reply_rc);
                    reply_target = INVALID_HANDLE;
                }

                // Arm the receive with our pointer buffer, so a request carrying in-pointer
                // data (hid's SetSupportedNpadIdType does) has somewhere to land.
                hipcMakeRequestInline(armGetTls(),
                                      .type = CmifCommandType_Invalid,
                                      .num_recv_statics = HIPC_AUTO_RECV_STATIC, )
                    .recv_list[0] = hipcMakeRecvStatic(g_pointer_buffer, sizeof(g_pointer_buffer));

                rc = svcReplyAndReceive(&idx, m_handles.data(), static_cast<s32>(m_handles.size()), INVALID_HANDLE, UINT64_MAX);

                // Snapshot the message before anything else touches this thread's TLS - and
                // that includes the very next log line, whose fs write goes through the same
                // buffer.
                SaveRequest();

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
                    if (ProcessQuery())
                        reply_target = m_query;
                }
                else
                {
                    Handle h = m_handles[idx];
                    if (ProcessSession(idx))
                        reply_target = h;
                }
            }

            smMitmUninstall(hid_name);
            for (Session &s : m_sessions)
            {
                if (s.kind == SessionKind::Hid)
                {
                    AbandonDomainObjects(s);
                    if (serviceIsActive(&s.forward))
                        serviceClose(&s.forward);
                }
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
