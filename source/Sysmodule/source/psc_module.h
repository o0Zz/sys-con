#pragma once

namespace syscon::psc
{
    int Initialize();
    void Exit();
    bool IsRunning();
};