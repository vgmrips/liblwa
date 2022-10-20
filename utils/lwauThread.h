#ifndef LWAUTHREAD_H
#define LWAUTHREAD_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "../stdtype.h"

typedef struct _lwau_thread LWAU_THREAD;
typedef void (*LWAU_THR_FUNC)(void* args);

UINT8 lwauThread_Init(LWAU_THREAD** retThread, LWAU_THR_FUNC threadFunc, void* args);
void lwauThread_Deinit(LWAU_THREAD* thr);
void lwauThread_Join(LWAU_THREAD* thr);
void lwauThread_Cancel(LWAU_THREAD* thr);
UINT64 lwauThread_GetID(const LWAU_THREAD* thr);
void* lwauThread_GetHandle(LWAU_THREAD* thr);	// return a reference to the actual handle

#ifdef __cplusplus
}
#endif

#endif	// LWAUTHREAD_H
