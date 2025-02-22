/*
 * Copyright (c) 2018 Atmosphère-NX
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
 
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <malloc.h>
#include <optional>
#include <mutex>

#include <switch.h>
#include <stratosphere.hpp>

#include "hid_mitm_service.hpp"
#include "hid_mitm_iappletresource.hpp"
#include "hid_custom.h"

extern "C" {
    extern u32 __start__;

    u32 __nx_applet_type = AppletType_None;

    #define INNER_HEAP_SIZE 0x60000
    size_t nx_inner_heap_size = INNER_HEAP_SIZE;
    char   nx_inner_heap[INNER_HEAP_SIZE];
    
    void __libnx_init_time(void);
    void __libnx_initheap(void);
    void __appInit(void);
    void __appExit(void);
}


void __libnx_initheap(void) {
	void*  addr = nx_inner_heap;
	size_t size = nx_inner_heap_size;

	/* Newlib */
	extern char* fake_heap_start;
	extern char* fake_heap_end;

	fake_heap_start = (char*)addr;
	fake_heap_end   = (char*)addr + size;
}



static const SocketInitConfig sockInitConf = {
    .bsdsockets_version = 1,

    .tcp_tx_buf_size        = 0x200,
    .tcp_rx_buf_size        = 0x400,
    .tcp_tx_buf_max_size    = 0x400,
    .tcp_rx_buf_max_size    = 0x800,
    // We're not using tcp anyways

    .udp_tx_buf_size = 0x2400,
    .udp_rx_buf_size = 0xA500,

    .sb_efficiency = 2,

    .num_bsd_sessions = 3,
    .bsd_service_type = BsdServiceType_User
};


void __appInit(void) {
    Result rc;

    rc = smInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);
    rc = fsInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);
    rc = fsdevMountSdmc();
    if (R_FAILED(rc))
        fatalThrow(rc);
    rc = timeInitialize();
    if (R_FAILED(rc))
        fatalThrow(rc);
    __libnx_init_time();

    //rc = hidInitialize();
    //if (R_FAILED(rc))
    //    fatalThrow(rc);
    
    
    rc = socketInitialize(&sockInitConf);
    if (R_FAILED(rc))
        fatalThrow(rc);
    
}

void __appExit(void) {
    /* Cleanup services. */
    fsdevUnmountAll();
    fsExit();
    smExit();
    timeExit();
}

struct HidManagerOptions {
    static const size_t PointerBufferSize = 0x200;
    static const size_t MaxDomains = 4;
    static const size_t MaxDomainObjects = 0x100;
};


constexpr size_t MaxServers = 1;
using HidMitmManager = ams::sf::hipc::ServerManager<MaxServers, HidManagerOptions>;

constexpr ams::sm::ServiceName MitmServiceName = ams::sm::ServiceName::Encode("hid");

namespace ams::result {
    bool CallFatalOnResultAssertion = false;
}

int main(int argc, char **argv)
{
    consoleDebugInit(debugDevice_SVC);
    customHidInitialize();
    copyThreadInitialize();

    HidMitmManager server_manager;
    R_ASSERT(server_manager.RegisterMitmServer<HidMitmService>(MitmServiceName));
    server_manager.LoopProcess();
    
    return 0;
}

