// POSIX Signals
// -------------

#include <stdlib.h>
#include <stddef.h>

#include <pthread.h>
#include <errno.h>

#include "../lwa_types.h"
#include "lwauSignal.h"

//typedef struct _lwau_signal LWAU_SIGNAL;
struct _lwau_signal
{
	pthread_mutex_t hMutex;
	pthread_cond_t hCond;	// condition variable
	uint8_t state;	// signal state
};

uint8_t LWA_API lwauSignal_Init(LWAU_SIGNAL** retSignal, uint8_t initState)
{
	LWAU_SIGNAL* sig;
	int retVal;
	
	sig = (LWAU_SIGNAL*)calloc(1, sizeof(LWAU_SIGNAL));
	if (sig == NULL)
		return LWAU_ERR_MEM_ERR;
	
	retVal = pthread_mutex_init(&sig->hMutex, NULL);
	if (retVal)
	{
		free(sig);
		return LWAU_ERR_API_ERR;
	}
	retVal = pthread_cond_init(&sig->hCond, NULL);
	if (retVal)
	{
		pthread_mutex_destroy(&sig->hMutex);
		free(sig);
		return LWAU_ERR_API_ERR;
	}
	sig->state = initState;
	
	*retSignal = sig;
	return LWAU_ERR_OK;
}

void LWA_API lwauSignal_Deinit(LWAU_SIGNAL* sig)
{
	pthread_mutex_destroy(&sig->hMutex);
	pthread_cond_destroy(&sig->hCond);
	
	free(sig);
	
	return;
}

uint8_t LWA_API lwauSignal_Signal(LWAU_SIGNAL* sig)
{
	int retVal = pthread_mutex_lock(&sig->hMutex);
	if (retVal)
		return LWAU_ERR_API_ERR;
	
	sig->state = 1;
	pthread_cond_signal(&sig->hCond);
	
	retVal = pthread_mutex_unlock(&sig->hMutex);
	return retVal ? LWAU_ERR_API_ERR : LWAU_ERR_OK;
}

uint8_t LWA_API lwauSignal_Reset(LWAU_SIGNAL* sig)
{
	int retVal = pthread_mutex_lock(&sig->hMutex);
	if (retVal)
		return LWAU_ERR_API_ERR;
	
	sig->state = 0;
	pthread_mutex_unlock(&sig->hMutex);
	
	return LWAU_ERR_OK;
}

uint8_t LWA_API lwauSignal_Wait(LWAU_SIGNAL* sig)
{
	int retVal = pthread_mutex_lock(&sig->hMutex);
	if (retVal)
		return LWAU_ERR_API_ERR;
	
	while(! sig->state && ! retVal)
		retVal = pthread_cond_wait(&sig->hCond, &sig->hMutex);
	sig->state = 0;	// reset signal after catching it successfully
	
	pthread_mutex_unlock(&sig->hMutex);
	return retVal ? LWAU_ERR_API_ERR : LWAU_ERR_OK;
}
