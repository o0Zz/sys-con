#include "IdleSys.h"

namespace
{
    Service g_idlesys_srv;
}

extern "C" Result idlesysInitialize(void)
{
    return smGetService(&g_idlesys_srv, "idle:sys");
}

extern "C" void idlesysExit(void)
{
    serviceClose(&g_idlesys_srv);
}

extern "C" Result idlesysReportUserIsActive(void)
{
    return serviceDispatch(&g_idlesys_srv, 5);
}
