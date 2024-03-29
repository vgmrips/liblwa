// POSIX Threads
// -------------

#include <stdlib.h>
#include <stddef.h>

#include <pthread.h>
#ifdef __HAIKU__
#include <kernel/OS.h>
#endif

#include "../lwa_types.h"
#include "lwauThread.h"

//typedef struct _lwau_thread LWAU_THREAD;
struct _lwau_thread
{
	pthread_t id;
	LWAU_THR_FUNC func;
	void* args;
};

static void* lwauThread_Main(void* param);

uint8_t LWA_API lwauThread_Init(LWAU_THREAD** retThread, LWAU_THR_FUNC threadFunc, void* args)
{
	LWAU_THREAD* thr;
	int retVal;
	
	thr = (LWAU_THREAD*)calloc(1, sizeof(LWAU_THREAD));
	if (thr == NULL)
		return LWAU_ERR_MEM_ERR;
	
	thr->func = threadFunc;
	thr->args = args;
	
	retVal = pthread_create(&thr->id, NULL, &lwauThread_Main, thr);
	if (retVal)
	{
		free(thr);
		return LWAU_ERR_API_ERR;
	}
	
	*retThread = thr;
	return LWAU_ERR_OK;
}

static void* lwauThread_Main(void* param)
{
	LWAU_THREAD* thr = (LWAU_THREAD*)param;
	thr->func(thr->args);
	return 0;
}

void LWA_API lwauThread_Deinit(LWAU_THREAD* thr)
{
	if (thr->id)
		pthread_detach(thr->id);	// release handle
	free(thr);
	
	return;
}

void LWA_API lwauThread_Join(LWAU_THREAD* thr)
{
	if (! thr->id)
		return;
	
	pthread_join(thr->id, NULL);
	thr->id = 0;	// mark as "joined"
	
	return;
}

void LWA_API lwauThread_Cancel(LWAU_THREAD* thr)
{
	int retVal;
	
	if (! thr->id)
		return;
	
	retVal = pthread_cancel(thr->id);
	if (! retVal)
		thr->id = 0;	// stopped successfully
	
	return;
}

uint64_t LWA_API lwauThread_GetID(const LWAU_THREAD* thr)
{
#ifdef __APPLE__
	uint64_t idNum;
	pthread_threadid_np (thr->id, &idNum);
	return idNum;
#elif defined (__HAIKU__)
	return get_pthread_thread_id (thr->id);
#else
	return (uint64_t)thr->id;
#endif
}

void* LWA_API lwauThread_GetHandle(LWAU_THREAD* thr)
{
	return &thr->id;
}
