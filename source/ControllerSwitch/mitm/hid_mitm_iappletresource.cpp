#include "hid_mitm_iappletresource.hpp"

#include "hid_custom.h"
#include "ini.h"
#include "udp_input.h"

#include <arpa/inet.h>
#include <map>
#include <math.h>
#include <mutex>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>

//static SharedMemory fake_shmem = {0};
//static HidSharedMemory *fake_shmem_mem;
//static HidSharedMemory *real_shmem_mem;

// TODO: Currently assumes one sharedmem per pid.
static std::unordered_map<u64, std::pair<HidSharedMemory *, HidSharedMemory *>> sharedmems;

static HidSharedMemory tmp_shmem_mem;

static Thread shmem_patch_thread, network_thread;

//static std::unordered_map<u64, u64> rebind_config;
static std::vector<std::pair<u64, u64>> rebind_config;
// VALUE is the key that we want to get when we click KEY

static Mutex configMutex, pkgMutex;
static struct input_msg cur_fakegamepad_state = {0};

void add_shmem(u64 pid, SharedMemory *real_shmem, SharedMemory *fake_shmem)
{
    mutexLock(&shmem_mutex);
    HidSharedMemory *real_mapped = (HidSharedMemory *)shmemGetAddr(real_shmem);
    HidSharedMemory *fake_mapped = (HidSharedMemory *)shmemGetAddr(fake_shmem);
    sharedmems[pid] = std::pair<HidSharedMemory *, HidSharedMemory *>(real_mapped, fake_mapped);
    mutexUnlock(&shmem_mutex);
}

void del_shmem(u64 pid)
{
    mutexLock(&shmem_mutex);
    if (sharedmems.find(pid) != sharedmems.end())
    {
        sharedmems.erase(pid);
    }
    mutexUnlock(&shmem_mutex);
}

std::string key_names[] = {"KEY_A", "KEY_B", "KEY_X", "KEY_Y", "KEY_LSTICK", "KEY_RSTICK", "KEY_L", "KEY_R", "KEY_ZL", "KEY_ZR", "KEY_PLUS", "KEY_MINUS", "KEY_DLEFT", "KEY_DUP", "KEY_DRIGHT", "KEY_DDOWN"};

s64 get_key_ind(std::string str)
{
    for (u64 i = 0; i < sizeof(key_names) / sizeof(char *); i++)
    {
        if (str == key_names[i])
        {
            return BIT(i);
        }
    }
    return -1;
}

/* sample config:
; Gamepad-rebind config. Currently always rebinds for all players, no individual config.
; VALUE is the button that gets registered when KEY is held down
[player1]
deadzone=0
KEY_A=KEY_A
KEY_B=KEY_B
KEY_X=KEY_X
KEY_Y=KEY_Y
KEY_LSTICK=KEY_LSTICK
KEY_RSTICK=KEY_RSTICK
KEY_L=KEY_L
KEY_R=KEY_R
KEY_ZL=KEY_ZL
KEY_ZR=KEY_ZR
KEY_PLUS=KEY_PLUS
KEY_MINUS=KEY_MINUS
KEY_DLEFT=KEY_DLEFT
KEY_DUP=KEY_DUP
KEY_DRIGHT=KEY_DRIGHT
KEY_DDOWN=KEY_DDOWN
[network]
enabled=1
*/

int networking_enabled = 0;

s16 stick_deadzone = 0;

static int handler(void *dummy, const char *section, const char *name,
                   const char *value)
{
    if (!strcmp(name, "enabled"))
    {
        networking_enabled = atoi(value);
    }
    else if (!strcmp(name, "deadzone"))
    {
        stick_deadzone = atoi(value);
    }

    s64 key = get_key_ind(name);
    s64 val = get_key_ind(value);

    if (key < 0 || val < 0)
    {
        return -1;
    }
    rebind_config.push_back(std::pair<u64, u64>(key, val));
    return 0;
}

void loadConfig()
{
    mutexLock(&configMutex);
    rebind_config.clear();
    if (ini_parse("/config/hid_mitm/config.ini", handler, NULL) < 0)
    {
        //fatalSimple(MAKERESULT(321, 1)); // 2321-0001 bad config
    }
    mutexUnlock(&configMutex);
}

// Clears config until next load
void clearConfig()
{
    mutexLock(&configMutex);
    rebind_config.clear();
    mutexUnlock(&configMutex);
}

void rebind_keys(int gamepad_ind)
{
    if (tmp_shmem_mem.controllers[gamepad_ind].misc.deviceType == 0)
    {
        return;
    }
    mutexLock(&configMutex);
    for (int layout = 0; layout < LAYOUT_DEFAULT + 1; layout++)
    {
        HidControllerLayout *currentTmpLayout = &tmp_shmem_mem.controllers[gamepad_ind].layouts[layout];

        int cur_tmp_ent_ind = currentTmpLayout->header.latestEntry;

        HidControllerInputEntry *curTmpEnt = &currentTmpLayout->entries[cur_tmp_ent_ind];
        if (curTmpEnt->connectionState == 0)
            continue;

        if (rebind_config.size() > 0)
        {
            u64 buttons = 0;

            for (auto it = rebind_config.begin(); it != rebind_config.end(); it++)
            {
                if (curTmpEnt->buttons & it->first)
                {
                    buttons |= it->second;
                }
            }

            // lstick rstick stuff
            for (int i = 0; i < JOYSTICK_NUM_STICKS; i++)
            {
                if (pow(curTmpEnt->joysticks[i].dx, 2) + pow(curTmpEnt->joysticks[i].dy, 2) <= pow(stick_deadzone, 2))
                {
                    curTmpEnt->joysticks[i].dx = 0;
                    curTmpEnt->joysticks[i].dy = 0;

                    // Stick digital stuff
                    for (int j = 16; j <= 19; j++)
                    {
                        buttons &= ~BIT(j + i * 4); // Unset each bit
                    }
                }
                else
                {
                    // Stick digital stuff
                    for (int j = 16; j <= 19; j++)
                    {
                        buttons |= curTmpEnt->buttons & BIT(j + i * 4); // Set each bit if it was originally set
                    }
                }
            }

            // sl sr stuff
            for (int i = 24; i <= 27; i++)
            {
                buttons |= curTmpEnt->buttons & BIT(i);
            }
            curTmpEnt->buttons = buttons;
        }
    }
    mutexUnlock(&configMutex);
}

struct hid_unk_section
{
    u64 type;
    u64 unk;
};

int apply_fake_gamepad(struct input_msg msg)
{
    if(msg.magic != INPUT_MSG_MAGIC)
        return -1;

    int gamepad;
    for (gamepad = CONTROLLER_PLAYER_1; gamepad <= CONTROLLER_PLAYER_8; gamepad++)
    {
        if (tmp_shmem_mem.controllers[gamepad].misc.deviceType == 0)
            break;
    }

    memset(&tmp_shmem_mem.controllers[gamepad].misc, 0, 0x40); // can probably be sizeof(misc)
    memset(&tmp_shmem_mem.controllers[gamepad].header, 0, sizeof(HidControllerHeader));

    // Pro controller magic
    tmp_shmem_mem.controllers[gamepad].misc.deviceType = 0x01;

    tmp_shmem_mem.controllers[gamepad].header.singleColorBody = 0;
    tmp_shmem_mem.controllers[gamepad].header.singleColorButtons = 0xFFFFFFFF;

    //Joycon: 0x20000004
    //Pro controller: 0x20000001
    tmp_shmem_mem.controllers[gamepad].header.type = 0x20000001;

    for (int layout = 0; layout < LAYOUT_DEFAULT + 1; layout++)
    {
        HidControllerLayout *currentTmpLayout = &tmp_shmem_mem.controllers[gamepad].layouts[layout];

        int ent = currentTmpLayout->header.latestEntry;

        HidControllerInputEntry *curTmpEnt = &currentTmpLayout->entries[ent];
        curTmpEnt->connectionState = 1;
        curTmpEnt->buttons = msg.keys;
        curTmpEnt->joysticks[0].dx = msg.joy_l_x;
        curTmpEnt->joysticks[0].dy = msg.joy_l_y;
        curTmpEnt->joysticks[1].dx = msg.joy_r_x;
        curTmpEnt->joysticks[1].dy = msg.joy_r_y;
    }
    return gamepad;
}

void shmem_copy(HidSharedMemory *source, HidSharedMemory *dest)
{

    // Apparently unused
    //memcpy(dest->controllerSerials, source->controllerSerials, sizeof(dest->controllerSerials));

    memcpy(&dest->touchscreen, &source->touchscreen, sizeof(source->touchscreen) - sizeof(source->touchscreen.padding));
    memcpy(&dest->mouse, &source->mouse, sizeof(source->mouse) - sizeof(source->mouse.padding));
    memcpy(&dest->keyboard, &source->keyboard, sizeof(source->keyboard) - sizeof(source->keyboard.padding));
    for (int i = 0; i < 10; i++)
    {
        // Only copy used gamepads
        if (dest->controllers[i].misc.deviceType != 0 || source->controllers[i].misc.deviceType != 0)
            memcpy(&dest->controllers[i], &source->controllers[i], sizeof(source->controllers[i]) - sizeof(source->controllers[i].unk_2)); // unk_2 is apparently unused and is huge
    }
}

void net_thread(void* _)
{
    struct input_msg tmp_pkg;
    while (true)
    {
        if (networking_enabled)
        {
            int poll_res = poll_udp_input(&tmp_pkg);

            mutexLock(&pkgMutex);
            if (poll_res == 0)
            {
                cur_fakegamepad_state = tmp_pkg;
            }
            else
            {
                cur_fakegamepad_state.magic = 0;
                svcSleepThread(1e+7l);
            }
            mutexUnlock(&pkgMutex);

            svcSleepThread(-1);
        } else {
            svcSleepThread(1e+9l);
        }
    }
}

#define WANT_TIME 96000
// Official software ticks 200 times/second

void copy_thread(void* _)
{
    ams::Result rc;

    /*rc = viInitialize(ViServiceType_System);
    if (R_FAILED(rc))
        fatalSimple(rc);

    ViDisplay disp;
    rc = viOpenDefaultDisplay(&disp);
    if (R_FAILED(rc))
        fatalSimple(rc);

    Event event;
    rc = viGetDisplayVsyncEvent(&disp, &event);
    if (R_FAILED(rc))
        fatalSimple(rc);*/

    loadConfig();

    while (true)
    {
        u64 curTime = svcGetSystemTick();

        mutexLock(&shmem_mutex);
        mutexLock(&pkgMutex);

        for (auto it = sharedmems.begin(); it != sharedmems.end(); it++)
        {
            shmem_copy(it->second.first, &tmp_shmem_mem);
            svcSleepThread(-1);

            if (networking_enabled)
            {
                apply_fake_gamepad(cur_fakegamepad_state);
            }

            for (int i = CONTROLLER_PLAYER_1; i <= CONTROLLER_HANDHELD; i++)
            {
                rebind_keys(i);
            }

            shmem_copy(&tmp_shmem_mem, it->second.second);
        }
        mutexUnlock(&pkgMutex);

        if (sharedmems.empty())
        {
            mutexUnlock(&shmem_mutex);
            svcSleepThread(1e+9L / 60);
            continue;
        }
        mutexUnlock(&shmem_mutex);

        s64 time_rest = WANT_TIME - (svcGetSystemTick() - curTime);
        if (time_rest > 0)
        {
            svcSleepThread((time_rest * 1e+9L) / 19200000);
        }
    }

    /*viCloseDisplay(&disp);
    viExit();*/
}

void copyThreadInitialize()
{
    mutexInit(&configMutex);
    mutexInit(&pkgMutex);
    loadConfig();
    threadCreate(&shmem_patch_thread, copy_thread, NULL, NULL, 0x1000, 0x21, 3);
    threadStart(&shmem_patch_thread);

    threadCreate(&network_thread, net_thread, NULL, NULL, 0x1000, 0x30, 3);
    threadStart(&network_thread);
}

IAppletResourceMitmService::~IAppletResourceMitmService()
{
    del_shmem(this->pid.value);
    customHidExit(&this->iappletresource_handle, &this->real_sharedmem, &this->fake_sharedmem);
}

ams::Result IAppletResourceMitmService::GetSharedMemoryHandle(ams::sf::OutCopyHandle shmem_hand)
{
    shmem_hand.SetValue(this->fake_sharedmem.handle);

    return ams::ResultSuccess();
}

