#ifndef LWAUSIGNAL_H
#define LWAUSIGNAL_H

/**
 * @file
 * @brief liblwa utilities: signal functions
 */

#ifdef __cplusplus
extern "C"
{
#endif

#include "../stdtype.h"
#include "lwau_export.h"
#include "../lwa_api.h"

/** @brief liblwa utility signal object */
typedef struct _lwau_signal LWAU_SIGNAL;

/**
 * @brief Creates a signal object.
 *
 * @param retSignal address of the signal object pointer to be stored in
 * @param initState Initial state of the signal. 0 = reset, 1 = signaled
 * @return error code. 0 = success, other values = error
 */
LWAU_EXPORT UINT8 LWA_API lwauSignal_Init(LWAU_SIGNAL** retSignal, UINT8 initState);
/**
 * @brief Destroys a signal object.
 *
 * @param sig signal object pointer
 */
LWAU_EXPORT void LWA_API lwauSignal_Deinit(LWAU_SIGNAL* sig);
/**
 * @brief Set signal object to "signaled" state.
 *
 * @param sig signal object pointer
 * @return error code. 0 = success, 0xFF = error
 */
LWAU_EXPORT UINT8 LWA_API lwauSignal_Signal(LWAU_SIGNAL* sig);
/**
 * @brief Set signal object to "non-signaled" state.
 *
 * @param sig signal object pointer
 * @return error code. 0 = success, 0xFF = error
 */
LWAU_EXPORT UINT8 LWA_API lwauSignal_Reset(LWAU_SIGNAL* sig);
/**
 * @brief Wait for signal object to be set to "signaled" state.
 *
 * @param sig signal object pointer
 * @return error code. 0 = success, 0xFF = error
 */
LWAU_EXPORT UINT8 LWA_API lwauSignal_Wait(LWAU_SIGNAL* sig);

#ifdef __cplusplus
}
#endif

#endif	// LWAUSIGNAL_H
