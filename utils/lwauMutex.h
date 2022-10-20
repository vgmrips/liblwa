#ifndef LWAUMUTEX_H
#define LWAUMUTEX_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "../stdtype.h"

typedef struct _lwau_mutex LWAU_MUTEX;

UINT8 lwauMutex_Init(LWAU_MUTEX** retMutex, UINT8 initLocked);
void lwauMutex_Deinit(LWAU_MUTEX* mtx);
UINT8 lwauMutex_Lock(LWAU_MUTEX* mtx);
UINT8 lwauMutex_TryLock(LWAU_MUTEX* mtx);
UINT8 lwauMutex_Unlock(LWAU_MUTEX* mtx);

#ifdef __cplusplus
}
#endif

#endif	// LWAUMUTEX_H
