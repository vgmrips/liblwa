// Windows Signals
// ---------------

#include <stdlib.h>
#include <stddef.h>

#include <windows.h>

#include "../lwa_types.h"
#include "lwauSignal.h"

//typedef struct _lwau_signal LWAU_SIGNAL;
struct _lwau_signal
{
	HANDLE hEvent;
};

uint8_t LWA_API lwauSignal_Init(LWAU_SIGNAL** retSignal, uint8_t initState)
{
	LWAU_SIGNAL* sig;
	
	sig = (LWAU_SIGNAL*)calloc(1, sizeof(LWAU_SIGNAL));
	if (sig == NULL)
		return LWAU_ERR_MEM_ERR;
	
	sig->hEvent = CreateEvent(NULL, FALSE, initState ? TRUE : FALSE, NULL);
	if (sig->hEvent == NULL)
	{
		free(sig);
		return LWAU_ERR_API_ERR;
	}
	
	*retSignal = sig;
	return LWAU_ERR_OK;
}

void LWA_API lwauSignal_Deinit(LWAU_SIGNAL* sig)
{
	CloseHandle(sig->hEvent);
	free(sig);
	
	return;
}

uint8_t LWA_API lwauSignal_Signal(LWAU_SIGNAL* sig)
{
	BOOL retVal = SetEvent(sig->hEvent);
	return retVal ? LWAU_ERR_OK : LWAU_ERR_API_ERR;
}

uint8_t LWA_API lwauSignal_Reset(LWAU_SIGNAL* sig)
{
	BOOL retVal = ResetEvent(sig->hEvent);
	return retVal ? LWAU_ERR_OK : LWAU_ERR_API_ERR;
}

uint8_t LWA_API lwauSignal_Wait(LWAU_SIGNAL* sig)
{
	DWORD retVal = WaitForSingleObject(sig->hEvent, INFINITE);
	if (retVal == WAIT_OBJECT_0)
		return LWAU_ERR_OK;
	else
		return LWAU_ERR_API_ERR;
}
