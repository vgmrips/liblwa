#ifndef LWAO_DRVFUNCS_H
#define LWAO_DRVFUNCS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "../stdtype.h"
#include "lwao_export.h"
#include "../lwa_api.h"

#ifdef LWAO_DRIVER_DSOUND
#include <windows.h>	// for HWND
#endif


#ifdef LWAO_DRIVER_WAVEWRITE
/**
 * @brief Sets the file name for the WAV file to be written.
 * This function *MUST* be called before calling lwaodStart().
 *
 * @param drvObj audio driver data pointer
 * @param fileName file path and name to be used by the WAV writer
 * @return error code. 0 = success
 */
LWAO_EXPORT UINT8 lwaodWavWrt_SetFileName(void* drvObj, const char* fileName);
/**
 * @brief Gets the file name for the WAV file to be written.
 *
 * @param drvObj audio driver data pointer
 * @return file path and name used by the WAV writer
 */
LWAO_EXPORT const char* lwaodWavWrt_GetFileName(void* drvObj);
#endif

#ifdef LWAO_DRIVER_DSOUND
/**
 * @brief Sets the application window handle for the DirectSound instance
 * This function *MUST* be called before calling lwaodStart() or there will be no sound.
 *
 * @param drvObj audio driver data pointer
 * @param fileName file path and name to be used by the WAV writer
 * @return error code. 0 = success
 */
LWAO_EXPORT UINT8 lwaodDSound_SetHWnd(void* drvObj, HWND hWnd);
#endif

#ifdef LWAO_DRIVER_PULSE
/**
 * @brief Sets the stream description of the PulseAudio instance.
 *
 * @param drvObj audio driver data pointer
 * @param fileName stream description
 * @return error code. 0 = success, LWAO_ERR_WASDONE on error
 */
LWAO_EXPORT UINT8 lwaodPulse_SetStreamDesc(void* drvObj, const char* streamDesc);
/**
 * @brief Gets the stream description of the PulseAudio instance.
 *
 * @param drvObj audio driver data pointer
 * @return stream description
 */
LWAO_EXPORT const char* lwaodPulse_GetStreamDesc(void* drvObj);
#endif

#ifdef __cplusplus
}
#endif

#endif	// LWAO_DRVFUNCS_H
