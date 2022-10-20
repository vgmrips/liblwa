// Windows Threads
// ---------------

#include <stdlib.h>
#include <stddef.h>

#include <windows.h>

#include "../stdtype.h"
#include "lwauThread.h"

//typedef struct _lwau_thread LWAU_THREAD;
struct _lwau_thread
{
	DWORD id;
	HANDLE hThread;
	LWAU_THR_FUNC func;
	void* args;
};

static DWORD WINAPI lwauThread_Main(LPVOID lpParam);

UINT8 lwauThread_Init(LWAU_THREAD** retThread, LWAU_THR_FUNC threadFunc, void* args)
{
	LWAU_THREAD* thr;
	
	thr = (LWAU_THREAD*)calloc(1, sizeof(LWAU_THREAD));
	if (thr == NULL)
		return 0xFF;
	
	thr->func = threadFunc;
	thr->args = args;
	
	thr->hThread = CreateThread(NULL, 0, &lwauThread_Main, thr, 0x00, &thr->id);
	if (thr->hThread == NULL)
	{
		free(thr);
		return 0x80;
	}
	
	*retThread = thr;
	return 0x00;
}

static DWORD WINAPI lwauThread_Main(LPVOID lpParam)
{
	LWAU_THREAD* thr = (LWAU_THREAD*)lpParam;
	thr->func(thr->args);
	return 0;
}

void lwauThread_Deinit(LWAU_THREAD* thr)
{
	CloseHandle(thr->hThread);
	free(thr);
	
	return;
}

void lwauThread_Join(LWAU_THREAD* thr)
{
	if (! thr->id)
		return;
	
	WaitForSingleObject(thr->hThread, INFINITE);
	thr->id = 0;	// mark as "joined"
	
	return;
}

void lwauThread_Cancel(LWAU_THREAD* thr)
{
	BOOL retVal;
	
	if (! thr->id)
		return;
	
	retVal = TerminateThread(thr->hThread, 1);
	if (retVal)
		thr->id = 0;	// stopped successfully
	
	return;
}

UINT64 lwauThread_GetID(const LWAU_THREAD* thr)
{
	return thr->id;
}

void* lwauThread_GetHandle(LWAU_THREAD* thr)
{
	return &thr->hThread;
}
