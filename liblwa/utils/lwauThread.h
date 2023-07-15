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

/**
 * @brief Creates a thread object.
 *
 * @param retThread address of the thread object pointer to be stored in
 * @param threadFunc function that the thread executes
 * @param args parameter pointer passes to the thread function
 * @return error code. 0 = success, other values = error
 */
LWAU_EXPORT UINT8 LWA_API lwauThread_Init(LWAU_THREAD** retThread, LWAU_THR_FUNC threadFunc, void* args);
/**
 * @brief Destroys a thread object.
 *
 * @param thr thread object pointer
 */
LWAU_EXPORT void LWA_API lwauThread_Deinit(LWAU_THREAD* thr);
/**
 * @brief Wait for a thread to terminate.
 *
 * @param thr thread object pointer
 */
LWAU_EXPORT void LWA_API lwauThread_Join(LWAU_THREAD* thr);
/**
 * @brief Forcibly terminate a thread.
 *
 * @param thr thread object pointer
 */
LWAU_EXPORT void LWA_API lwauThread_Cancel(LWAU_THREAD* thr);
/**
 * @brief Get thread ID of the thread object.
 *
 * @param thr thread object pointer
 * @return thread ID
 */
LWAU_EXPORT UINT64 LWA_API lwauThread_GetID(const LWAU_THREAD* thr);
/**
 * @brief Get pointer to OS-specific thread handle.
 *
 * @param thr thread object pointer
 * @return reference to the actual handle
 */
LWAU_EXPORT void* LWA_API lwauThread_GetHandle(LWAU_THREAD* thr);

#ifdef __cplusplus
}
#endif

#endif	// LWAUTHREAD_H
