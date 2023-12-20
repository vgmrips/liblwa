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
		return LWAU_ERR_MEM_ERR;
	
	retVal = pthread_mutex_init(&mtx->hMutex, NULL);
	if (retVal)
	{
		free(mtx);
		return LWAU_ERR_API_ERR;
	}
	if (initLocked)
		lwauMutex_Lock(mtx);
	
	*retMutex = mtx;
	return LWAU_ERR_OK;
}

void LWA_API lwauMutex_Deinit(LWAU_MUTEX* mtx)
{
	pthread_mutex_destroy(&mtx->hMutex);
	free(mtx);
	
	return;
}

uint8_t LWA_API lwauMutex_Lock(LWAU_MUTEX* mtx)
{
	int retVal = pthread_mutex_lock(&mtx->hMutex);
	if (! retVal)
		return LWAU_ERR_OK;
	else
		return LWAU_ERR_API_ERR;
}

uint8_t LWA_API lwauMutex_TryLock(LWAU_MUTEX* mtx)
{
	int retVal = pthread_mutex_trylock(&mtx->hMutex);
	if (! retVal)
		return LWAU_ERR_OK;
	else if (retVal == EBUSY)
		return LWAU_ERR_MTX_LOCKED;
	else
		return LWAU_ERR_API_ERR;
}

uint8_t LWA_API lwauMutex_Unlock(LWAU_MUTEX* mtx)
{
	int retVal = pthread_mutex_unlock(&mtx->hMutex);
	if (! retVal)
		return LWAU_ERR_OK;
	else
		return LWAU_ERR_API_ERR;
}
