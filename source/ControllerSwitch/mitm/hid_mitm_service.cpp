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

#include "hid_mitm_service.hpp"

#include "hid_custom.h"
#include "hid_mitm_iappletresource.hpp"

#include <mutex>
#include <optional>

ams::Result HidMitmService::CreateAppletResource(ams::sf::Out<std::shared_ptr<IAppletResourceMitmService>> out, ams::sf::ClientAppletResourceUserId arid)
{
    Service out_iappletresource;
    SharedMemory real_shmem, fake_shmem;
    // This needs to be the first ipc being done since it relies on stuff that libstrato left for us. TODO: Do this properly
    customHidSetup(this->forward_service.get(), &out_iappletresource, &real_shmem, &fake_shmem);


    std::shared_ptr<IAppletResourceMitmService> intf = nullptr;



    intf = std::make_shared<IAppletResourceMitmService>(new IAppletResourceMitmService(0));

    intf->pid = arid.GetValue();
    intf->fake_sharedmem = fake_shmem;
    intf->real_sharedmem = real_shmem;
    intf->iappletresource_handle = out_iappletresource;
    add_shmem(intf->pid.value, &intf->real_sharedmem, &intf->fake_sharedmem);

    out.SetValue(std::move(intf));
    return 0;
}

ams::Result HidMitmService::ReloadConfig() {
    loadConfig();
    return 0;
}

ams::Result HidMitmService::ClearConfig() {
    clearConfig();
    return 0;
}

