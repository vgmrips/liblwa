#ifndef LWAUSIGNAL_H
#define LWAUSIGNAL_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "../stdtype.h"

typedef struct _lwau_signal LWAU_SIGNAL;

UINT8 lwauSignal_Init(LWAU_SIGNAL** retSignal, UINT8 initState);
void lwauSignal_Deinit(LWAU_SIGNAL* sig);
UINT8 lwauSignal_Signal(LWAU_SIGNAL* sig);
UINT8 lwauSignal_Reset(LWAU_SIGNAL* sig);
UINT8 lwauSignal_Wait(LWAU_SIGNAL* sig);

#ifdef __cplusplus
}
#endif

#endif	// LWAUSIGNAL_H
