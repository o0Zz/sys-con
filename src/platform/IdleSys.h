#pragma once

#include <switch.h>

#ifdef __cplusplus
extern "C" {
#endif

// Minimal idle:sys client. The system libnx shipped with devkitPro predates
// idle:sys, so the two commands we need are implemented here directly.
Result idlesysInitialize(void);
void   idlesysExit(void);
Result idlesysReportUserIsActive(void);

#ifdef __cplusplus
}
#endif
