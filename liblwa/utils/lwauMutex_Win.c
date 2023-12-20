// Windows Mutexes
// ---------------

#include <stdlib.h>
#include <stddef.h>

#include <windows.h>

#include "../lwa_types.h"
#include "lwauMutex.h"

//typedef struct _lwau_mutex LWAU_MUTEX;
struct _lwau_mutex
{
	HANDLE hMutex;
};

uint8_t LWA_API lwauMutex_Init(LWAU_MUTEX** retMutex, uint8_t initLocked)
{
	LWAU_MUTEX* mtx;
	
	mtx = (LWAU_MUTEX*)calloc(1, sizeof(LWAU_MUTEX));
	if (mtx == NULL)
		return LWAU_ERR_MEM_ERR;
	
	mtx->hMutex = CreateMutex(NULL, initLocked ? TRUE : FALSE, NULL);
	if (mtx->hMutex == NULL)
	{
		free(mtx);
		return LWAU_ERR_API_ERR;
	}
	
	*retMutex = mtx;
	return LWAU_ERR_OK;
}

void LWA_API lwauMutex_Deinit(LWAU_MUTEX* mtx)
{
	CloseHandle(mtx->hMutex);
	free(mtx);
	
	return;
}

uint8_t LWA_API lwauMutex_Lock(LWAU_MUTEX* mtx)
{
	DWORD retVal = WaitForSingleObject(mtx->hMutex, INFINITE);
	if (retVal == WAIT_OBJECT_0)
		return LWAU_ERR_OK;
	else
		return LWAU_ERR_API_ERR;
}

uint8_t LWA_API lwauMutex_TryLock(LWAU_MUTEX* mtx)
{
	DWORD retVal = WaitForSingleObject(mtx->hMutex, 0);
	if (retVal == WAIT_OBJECT_0)
		return LWAU_ERR_OK;
	else if (retVal == WAIT_TIMEOUT)
		return LWAU_ERR_MTX_LOCKED;
	else
		return LWAU_ERR_API_ERR;
}

uint8_t LWA_API lwauMutex_Unlock(LWAU_MUTEX* mtx)
{
	BOOL retVal = ReleaseMutex(mtx->hMutex);
	if (retVal)
		return LWAU_ERR_OK;
	else
		return LWAU_ERR_API_ERR;
}
