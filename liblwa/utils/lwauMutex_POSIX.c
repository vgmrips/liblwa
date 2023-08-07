// POSIX Mutexes
// -------------

#include <stdlib.h>
#include <stddef.h>

#include <pthread.h>
#include <errno.h>

#include "../lwa_types.h"
#include "lwauMutex.h"

//typedef struct _lwau_mutex LWAU_MUTEX;
struct _lwau_mutex
{
	pthread_mutex_t hMutex;
};

uint8_t LWA_API lwauMutex_Init(LWAU_MUTEX** retMutex, uint8_t initLocked)
{
	LWAU_MUTEX* mtx;
	int retVal;
	
	mtx = (LWAU_MUTEX*)calloc(1, sizeof(LWAU_MUTEX));
	if (mtx == NULL)
		return 0xFF;
	
	retVal = pthread_mutex_init(&mtx->hMutex, NULL);
	if (retVal)
	{
		free(mtx);
		return 0x80;
	}
	if (initLocked)
		lwauMutex_Lock(mtx);
	
	*retMutex = mtx;
	return 0x00;
}

void LWA_API lwauMutex_Deinit(LWAU_MUTEX* mtx)
{
	pthread_mutex_destroy(&mtx->hMutex);
	free(mtx);
	
	return;
}

uint8_t LWA_API lwauMutex_Lock(LWAU_MUTEX* mtx)
{
	int retVal;
	
	retVal = pthread_mutex_lock(&mtx->hMutex);
	if (! retVal)
		return 0x00;
	else
		return 0xFF;
}

uint8_t LWA_API lwauMutex_TryLock(LWAU_MUTEX* mtx)
{
	int retVal;
	
	retVal = pthread_mutex_trylock(&mtx->hMutex);
	if (! retVal)
		return 0x00;
	else if (retVal == EBUSY)
		return 0x01;
	else
		return 0xFF;
}

uint8_t LWA_API lwauMutex_Unlock(LWAU_MUTEX* mtx)
{
	int retVal;
	
	retVal = pthread_mutex_unlock(&mtx->hMutex);
	return retVal ? 0xFF : 0x00;
}
