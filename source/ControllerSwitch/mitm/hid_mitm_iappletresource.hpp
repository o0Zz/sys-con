#pragma once
#include <stratosphere.hpp>

void loadConfig();
void clearConfig();
void copyThreadInitialize();
void add_shmem(u64 pid, SharedMemory *real_shmem, SharedMemory *fake_shmem);
void del_shmem(u64 pid);

class IAppletResourceMitmService : public ams::sf::IServiceObject {
private:
    enum class CommandId {
        GetSharedMemoryHandle = 0
    };
public:
    IAppletResourceMitmService(IAppletResourceMitmService *dummy) {
        /* ... */
    }

    ~IAppletResourceMitmService();

protected:
    /* Overridden commands. */
    virtual ams::Result GetSharedMemoryHandle(ams::sf::OutCopyHandle shmem_hand) final;

public:
    DEFINE_SERVICE_DISPATCH_TABLE{
        MAKE_SERVICE_COMMAND_META(GetSharedMemoryHandle),
    };

    ams::os::ProcessId pid;
    Service iappletresource_handle;
    SharedMemory real_sharedmem;
    SharedMemory fake_sharedmem;
};
