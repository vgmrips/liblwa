#ifndef __ASTRM_SPCDRVFUNS_H__
#define __ASTRM_SPCDRVFUNS_H__

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef LWAO_DRIVER_DSOUND
#include <windows.h>	// for HWND
#endif


#ifdef LWAO_DRIVER_WAVEWRITE
UINT8 lwaodWavWrt_SetFileName(void* drvObj, const char* fileName);
const char* lwaodWavWrt_GetFileName(void* drvObj);
#endif

#ifdef LWAO_DRIVER_DSOUND
UINT8 lwaodDSound_SetHWnd(void* drvObj, HWND hWnd);
#endif

#ifdef LWAO_DRIVER_PULSE
UINT8 lwaodPulse_SetStreamDesc(void* drvObj, const char* fileName);
const char* lwaodPulse_GetStreamDesc(void* drvObj);
#endif

#ifdef __cplusplus
}
#endif

#endif	// __ASTRM_SPCDRVFUNS_H__
