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

#pragma once
#include "hid_mitm_iappletresource.hpp"

#include <memory>
#include <stratosphere.hpp>

class HidMitmService : public ams::sf::IMitmServiceObject {
private:
    enum class CommandId {
        CreateAppletResource = 0,
        ReloadConfig = 65000,
        ClearConfig = 65001,
    };
public:
    HidMitmService(std::shared_ptr<Service> &&s, const ams::sm::MitmProcessInfo &c) : IMitmServiceObject(std::move(s), c) {
        /* ... */
    }

    static bool ShouldMitm(const ams::sm::MitmProcessInfo &client_info)
    {
        // We want to be loaded into as few as possible processess to save ram+cpu-time

        if(ams::ncm::IsApplicationProgramId(client_info.program_id))
        {
            return true;
        }

        if(ams::ncm::IsAppletProgramId(client_info.program_id))
        {
            return client_info.program_id.value != 0x010000000000100Cul;
        }

        return false;
    }

  protected:
    /* Overridden commands. */
    virtual ams::Result CreateAppletResource(ams::sf::Out<std::shared_ptr<IAppletResourceMitmService>> out, ams::sf::ClientAppletResourceUserId arid) final;
    virtual ams::Result ReloadConfig() final;
    virtual ams::Result ClearConfig() final;

  public:
    DEFINE_SERVICE_DISPATCH_TABLE{
        MAKE_SERVICE_COMMAND_META(CreateAppletResource),
        MAKE_SERVICE_COMMAND_META(ReloadConfig),
        MAKE_SERVICE_COMMAND_META(ClearConfig),
    };
};
