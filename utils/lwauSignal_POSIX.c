// POSIX Signals
// -------------

#include <stdlib.h>
#include <stddef.h>

#include <pthread.h>
#include <errno.h>

#include "../stdtype.h"
#include "lwauSignal.h"

//typedef struct _lwau_signal LWAU_SIGNAL;
struct _lwau_signal
{
	pthread_mutex_t hMutex;
	pthread_cond_t hCond;	// condition variable
	UINT8 state;	// signal state
};

UINT8 lwauSignal_Init(LWAU_SIGNAL** retSignal, UINT8 initState)
{
	LWAU_SIGNAL* sig;
	int retVal;
	
	sig = (LWAU_SIGNAL*)calloc(1, sizeof(LWAU_SIGNAL));
	if (sig == NULL)
		return 0xFF;
	
	retVal = pthread_mutex_init(&sig->hMutex, NULL);
	if (retVal)
	{
		free(sig);
		return 0x80;
	}
	retVal = pthread_cond_init(&sig->hCond, NULL);
	if (retVal)
	{
		pthread_mutex_destroy(&sig->hMutex);
		free(sig);
		return 0x80;
	}
	sig->state = initState;
	
	*retSignal = sig;
	return 0x00;
}

void lwauSignal_Deinit(LWAU_SIGNAL* sig)
{
	pthread_mutex_destroy(&sig->hMutex);
	pthread_cond_destroy(&sig->hCond);
	
	free(sig);
	
	return;
}

UINT8 lwauSignal_Signal(LWAU_SIGNAL* sig)
{
	int retVal;
	
	retVal = pthread_mutex_lock(&sig->hMutex);
	if (retVal)
		return 0xFF;
	
	sig->state = 1;
	pthread_cond_signal(&sig->hCond);
	
	retVal = pthread_mutex_unlock(&sig->hMutex);
	return retVal ? 0xFF : 0x00;
}

UINT8 lwauSignal_Reset(LWAU_SIGNAL* sig)
{
	int retVal;
	
	retVal = pthread_mutex_lock(&sig->hMutex);
	if (retVal)
		return 0xFF;
	
	sig->state = 0;
	pthread_mutex_unlock(&sig->hMutex);
	
	return 0x00;
}

UINT8 lwauSignal_Wait(LWAU_SIGNAL* sig)
{
	int retVal;
	
	retVal = pthread_mutex_lock(&sig->hMutex);
	if (retVal)
		return 0xFF;
	
	while(! sig->state && ! retVal)
		retVal = pthread_cond_wait(&sig->hCond, &sig->hMutex);
	sig->state = 0;	// reset signal after catching it successfully
	
	pthread_mutex_unlock(&sig->hMutex);
	return retVal ? 0xFF : 0x00;
}
