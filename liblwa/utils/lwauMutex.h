#ifndef LWAUMUTEX_H
#define LWAUMUTEX_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "../stdtype.h"
#include "lwau_export.h"
#include "../lwa_api.h"

typedef struct _lwau_mutex LWAU_MUTEX;

LWAU_EXPORT UINT8 LWA_API lwauMutex_Init(LWAU_MUTEX** retMutex, UINT8 initLocked);
LWAU_EXPORT void LWA_API lwauMutex_Deinit(LWAU_MUTEX* mtx);
LWAU_EXPORT UINT8 LWA_API lwauMutex_Lock(LWAU_MUTEX* mtx);
LWAU_EXPORT UINT8 LWA_API lwauMutex_TryLock(LWAU_MUTEX* mtx);
LWAU_EXPORT UINT8 LWA_API lwauMutex_Unlock(LWAU_MUTEX* mtx);

#ifdef __cplusplus
}
#endif

#endif	// LWAUMUTEX_H
