#pragma once

#include <switch.h>

#include "IController.h"

#include <vector>
#include <memory>
#include <mutex>

/* ------------------------------------------------ */

class HidSharedMemoryEntry
{
    friend class HidSharedMemoryManager;

public:
    HidSharedMemoryEntry(::Service *hid_service, u64 processId, u64 programId);
    ~HidSharedMemoryEntry();

    const ::SharedMemory &GetSharedMemoryHandle() const;

    inline ::HidSharedMemory *GetRealAddr();

    inline ::HidSharedMemory *GetFakeAddr();

    inline u64 GetProcessId() const;
    inline u64 GetProgramId() const;

protected:
    void Copy();

private:
    u64 m_process_id;
    u64 m_program_id;
    ::Result m_status;

    ::Service m_appletresource;

    ::SharedMemory m_real_shared_memory;
    ::SharedMemory m_fake_shared_memory;
};

/* ------------------------------------------------ */

class HidSharedMemoryController
{
public:
    class ControllerInfo
    {
    public:
        u8 deviceType;          ///< \ref HidDeviceType
        u8 npadInterfaceType;   ///< \ref HidNpadInterfaceType. Additional type field used with the above type field (only applies to ::HidDeviceType_JoyRight1, ::HidDeviceType_JoyLeft2, ::HidDeviceType_FullKey3, and ::HidDeviceType_System19), if the value doesn't match one of the following a default is used. ::HidDeviceType_FullKey3: ::HidNpadInterfaceType_USB indicates that the controller is connected via USB. :::HidDeviceType_System19: ::HidNpadInterfaceType_USB = unknown. When value is ::HidNpadInterfaceType_Rail, state is merged with an existing controller (with ::HidDeviceType_JoyRight1 / ::HidDeviceType_JoyLeft2). Otherwise, it's a dedicated controller.
        u32 singleColorBody;    ///< RGBA Single Body Color.
        u32 singleColorButtons; ///< RGBA Single Buttons Color.
        u32 colorLeftGrip;      ///< [9.0.0+] RGBA Left Grip Color.
        u32 colorRightGrip;     ///< [9.0.0+] RGBA Right Grip Color.
    };

    HidSharedMemoryController(const HidSharedMemoryController::ControllerInfo &info);
    ~HidSharedMemoryController();

    void Update(const NormalizedButtonData &data);
    const HidSharedMemoryController::ControllerInfo &GetInfo() const;

private:
    HidSharedMemoryController::ControllerInfo m_info;
};

/* ------------------------------------------------ */

class HidSharedMemoryManager
{
    friend void HidSharedMemoryManagerThreadFunc(void *arg);

public:
    HidSharedMemoryManager();
    ~HidSharedMemoryManager();

    static HidSharedMemoryManager &GetHidSharedMemoryManager();

    std::shared_ptr<HidSharedMemoryController> AttachController(const HidSharedMemoryController::ControllerInfo &info);
    void DetachController(std::shared_ptr<HidSharedMemoryController> controller);

    std::shared_ptr<HidSharedMemoryEntry> CreateIfNotExists(::Service *hid_service, u64 processId, u64 programId);
    std::shared_ptr<HidSharedMemoryEntry> Get(u64 processId, u64 programId);

    int Add(const std::shared_ptr<HidSharedMemoryEntry> &entry);

    int Start();
    void Stop();

private:
    void OnRun();

    void RunGarbageCollector();
    void DumpProcessesAndMemoryAddr();
    void DumpHidSharedMemory();

    alignas(0x1000) u8 m_thread_stack[0x4000];

    bool m_running;
    ::Thread m_thread;

    std::recursive_mutex m_mutex;

    std::vector<std::shared_ptr<HidSharedMemoryEntry>> m_sharedmemory_entry_list;
};
