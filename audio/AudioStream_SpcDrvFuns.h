#ifndef __ASTRM_SPCDRVFUNS_H__
#define __ASTRM_SPCDRVFUNS_H__

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
LWAO_EXPORT UINT8 lwaodWavWrt_SetFileName(void* drvObj, const char* fileName);
LWAO_EXPORT const char* lwaodWavWrt_GetFileName(void* drvObj);
#endif

#ifdef LWAO_DRIVER_DSOUND
LWAO_EXPORT UINT8 lwaodDSound_SetHWnd(void* drvObj, HWND hWnd);
#endif

#ifdef LWAO_DRIVER_PULSE
LWAO_EXPORT UINT8 lwaodPulse_SetStreamDesc(void* drvObj, const char* fileName);
LWAO_EXPORT const char* lwaodPulse_GetStreamDesc(void* drvObj);
#endif

#ifdef __cplusplus
}
#endif

#endif	// __ASTRM_SPCDRVFUNS_H__
