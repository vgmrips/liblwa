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
		return 0xFF;
	
	sig->hEvent = CreateEvent(NULL, FALSE, initState ? TRUE : FALSE, NULL);
	if (sig->hEvent == NULL)
	{
		free(sig);
		return 0x80;
	}
	
	*retSignal = sig;
	return 0x00;
}

void LWA_API lwauSignal_Deinit(LWAU_SIGNAL* sig)
{
	CloseHandle(sig->hEvent);
	free(sig);
	
	return;
}

uint8_t LWA_API lwauSignal_Signal(LWAU_SIGNAL* sig)
{
	BOOL retVal;
	
	retVal = SetEvent(sig->hEvent);
	return retVal ? 0x00 : 0xFF;
}

uint8_t LWA_API lwauSignal_Reset(LWAU_SIGNAL* sig)
{
	BOOL retVal;
	
	retVal = ResetEvent(sig->hEvent);
	return retVal ? 0x00 : 0xFF;
}

uint8_t LWA_API lwauSignal_Wait(LWAU_SIGNAL* sig)
{
	DWORD retVal;
	
	retVal = WaitForSingleObject(sig->hEvent, INFINITE);
	if (retVal == WAIT_OBJECT_0)
		return 0x00;
	else
		return 0xFF;
}
