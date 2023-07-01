#ifndef LWAUTHREAD_H
#define LWAUTHREAD_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "../stdtype.h"
#include "lwau_export.h"
#include "../lwa_api.h"

typedef struct _lwau_thread LWAU_THREAD;
typedef void (LWA_API *LWAU_THR_FUNC)(void* args);

LWAU_EXPORT UINT8 LWA_API lwauThread_Init(LWAU_THREAD** retThread, LWAU_THR_FUNC threadFunc, void* args);
LWAU_EXPORT void LWA_API lwauThread_Deinit(LWAU_THREAD* thr);
LWAU_EXPORT void LWA_API lwauThread_Join(LWAU_THREAD* thr);
LWAU_EXPORT void LWA_API lwauThread_Cancel(LWAU_THREAD* thr);
LWAU_EXPORT UINT64 LWA_API lwauThread_GetID(const LWAU_THREAD* thr);
LWAU_EXPORT void* LWA_API lwauThread_GetHandle(LWAU_THREAD* thr);	// return a reference to the actual handle

#ifdef __cplusplus
}
#endif

#endif	// LWAUTHREAD_H
