#ifndef LWAUMUTEX_H
#define LWAUMUTEX_H

/**
 * @file
 * @brief liblwa utilities: mutex functions
 */

#ifdef __cplusplus
extern "C"
{
#endif

#include "../lwa_types.h"
#include "lwau_export.h"
#include "../lwa_api.h"

/** @brief liblwa utility mutex object */
typedef struct _lwau_mutex LWAU_MUTEX;

/**
 * @brief Creates a mutex object.
 *
 * @param retMutex address of the mutex object pointer to be stored in
 * @param initLocked Initial state of the mutex. 0 = unlocked, 1 = locked
 * @return error code. 0 = success, other values = error
 */
LWAU_EXPORT uint8_t LWA_API lwauMutex_Init(LWAU_MUTEX** retMutex, uint8_t initLocked);
/**
 * @brief Destroys a mutex object.
 *
 * @param mtx mutex object pointer
 */
LWAU_EXPORT void LWA_API lwauMutex_Deinit(LWAU_MUTEX* mtx);
/**
 * @brief Locks a mutex. If the mutex is already locked, the function blocks until it gets available.
 *
 * @param mtx mutex object pointer
 * @return error code. 0 = success, 0xFF = error
 */
LWAU_EXPORT uint8_t LWA_API lwauMutex_Lock(LWAU_MUTEX* mtx);
/**
 * @brief Tries to lock a mutex. Returns immediately if the mutex is already locked.
 *
 * @param mtx mutex object pointer
 * @return error code. 0 = success, 1 = mutex is already locked, 0xFF = error
 */
LWAU_EXPORT uint8_t LWA_API lwauMutex_TryLock(LWAU_MUTEX* mtx);
/**
 * @brief Unlocks a mutex.
 *
 * @param mtx mutex object pointer
 * @return error code. 0 = success, 0xFF = error
 */
LWAU_EXPORT uint8_t LWA_API lwauMutex_Unlock(LWAU_MUTEX* mtx);

#ifdef __cplusplus
}
#endif

#endif	// LWAUMUTEX_H
