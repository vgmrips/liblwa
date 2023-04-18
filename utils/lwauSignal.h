#ifndef LWAUSIGNAL_H
#define LWAUSIGNAL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "../stdtype.h"
#include "lwau_export.h"
#include "../lwa_api.h"

typedef struct _lwau_signal LWAU_SIGNAL;

LWAU_EXPORT UINT8 LWA_API lwauSignal_Init(LWAU_SIGNAL** retSignal, UINT8 initState);
LWAU_EXPORT void LWA_API lwauSignal_Deinit(LWAU_SIGNAL* sig);
LWAU_EXPORT UINT8 LWA_API lwauSignal_Signal(LWAU_SIGNAL* sig);
LWAU_EXPORT UINT8 LWA_API lwauSignal_Reset(LWAU_SIGNAL* sig);
LWAU_EXPORT UINT8 LWA_API lwauSignal_Wait(LWAU_SIGNAL* sig);

#ifdef __cplusplus
}
#endif

#endif	// LWAUSIGNAL_H
