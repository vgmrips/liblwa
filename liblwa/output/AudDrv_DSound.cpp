// Audio Stream - DirectSound
// Required libraries:
//	- dsound.lib
//	- uuid.lib (for GUID_NULL)
#define _CRTDBG_MAP_ALLOC
#define _WIN32_DCOM	// for CoInitializeEx() / COINIT_MULTITHREADED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIRECTSOUND_VERSION	0x0600
#include <dsound.h>
#include <mmsystem.h>	// for WAVEFORMATEX

#include "../lwa_types.h"

#include "lwao.h"
#include "../utils/lwauThread.h"
#include "../utils/lwauSignal.h"
#include "../utils/lwauMutex.h"

#define EXT_C	extern "C"


#ifdef _MSC_VER
#define	strdup	_strdup
#endif

typedef struct _dsound_device
{
	GUID devGUID;
	char* devName;
	char* devModule;
} DSND_DEVICE;

typedef struct _directsound_driver
{
	void* audDrvPtr;
	volatile uint8_t devState;	// 0 - not running, 1 - running, 2 - terminating
	uint16_t dummy;	// [for alignment purposes]
	
	WAVEFORMATEX waveFmt;
	uint32_t bufSmpls;
	uint32_t bufSize;
	uint32_t bufSegSize;
	uint32_t bufSegCount;
	uint8_t* bufSpace;
	
	HWND hWnd;
	LPDIRECTSOUND dSndIntf;
	LPDIRECTSOUNDBUFFER dSndBuf;
	LWAU_THREAD* hThread;
	LWAU_SIGNAL* hSignal;
	LWAU_MUTEX* hMutex;
	void* userParam;
	LWAOFUNC_FILLBUF FillBuffer;
	
	uint32_t writePos;
} DRV_DSND;


EXT_C uint8_t lwaodDSound_IsAvailable(void);
EXT_C uint8_t lwaodDSound_Init(void);
EXT_C uint8_t lwaodDSound_Deinit(void);
EXT_C const LWAO_DEV_LIST* lwaodDSound_GetDeviceList(void);
EXT_C LWAO_OPTS* lwaodDSound_GetDefaultOpts(void);

EXT_C uint8_t lwaodDSound_Create(void** retDrvObj);
EXT_C uint8_t lwaodDSound_Destroy(void* drvObj);
EXT_C LWAO_EXPORT uint8_t lwaodDSound_SetHWnd(void* drvObj, HWND hWnd);
EXT_C uint8_t lwaodDSound_Start(void* drvObj, uint32_t deviceID, LWAO_OPTS* options, void* audDrvParam);
EXT_C uint8_t lwaodDSound_Stop(void* drvObj);
EXT_C uint8_t lwaodDSound_Pause(void* drvObj);
EXT_C uint8_t lwaodDSound_Resume(void* drvObj);

EXT_C uint8_t lwaodDSound_SetCallback(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam);
EXT_C uint32_t lwaodDSound_GetBufferSize(void* drvObj);
static uint32_t GetFreeBytes(DRV_DSND* drv);
EXT_C uint8_t lwaodDSound_IsBusy(void* drvObj);
EXT_C uint8_t lwaodDSound_WriteData(void* drvObj, uint32_t dataSize, void* data);

EXT_C uint32_t lwaodDSound_GetLatency(void* drvObj);
static void LWA_API DirectSoundThread(void* Arg);
static uint8_t WriteBuffer(DRV_DSND* drv, uint32_t dataSize, void* data);
static uint8_t ClearBuffer(DRV_DSND* drv);


// Notes:
//	- MS Visual C++ (6.0/2010) needs 'extern "C"' for variables
//	- GCC doesn't need it for variables and complains about it
//	  if written in the form 'extern "C" LWAO_DRIVER ...'
//	- MS Visual C++ 6.0 complains if the prototype of a static function has
//	  'extern "C"', but the definition lacks it.
extern "C"
{
LWAO_DRIVER lwaoDrv_DSound =
{
	{LWAO_DTYPE_OUT, LWAO_DSIG_DSOUND, "DirectSound"},
	
	lwaodDSound_IsAvailable,
	lwaodDSound_Init, lwaodDSound_Deinit,
	lwaodDSound_GetDeviceList, lwaodDSound_GetDefaultOpts,
	
	lwaodDSound_Create, lwaodDSound_Destroy,
	lwaodDSound_Start, lwaodDSound_Stop,
	lwaodDSound_Pause, lwaodDSound_Resume,
	
	lwaodDSound_SetCallback, lwaodDSound_GetBufferSize,
	lwaodDSound_IsBusy, lwaodDSound_WriteData,
	
	lwaodDSound_GetLatency,
};
}	// extern "C"


static LWAO_OPTS defOptions;
static LWAO_DEV_LIST deviceList;

static uint8_t isInit = 0;
static uint32_t activeDrivers;

static uint32_t devLstAlloc;
static uint32_t devLstCount;
static DSND_DEVICE* devList;

uint8_t lwaodDSound_IsAvailable(void)
{
	return 1;
}

static BOOL CALLBACK DSEnumCallback(LPGUID lpGuid, LPCSTR lpcstrDescription,
									LPCSTR lpcstrModule, LPVOID lpContext)
{
	DSND_DEVICE* tempDev;
	
	if (devLstCount >= devLstAlloc)
	{
		devLstAlloc += 0x10;
		devList = (DSND_DEVICE*)realloc(devList, devLstAlloc * sizeof(DSND_DEVICE));
	}
	
	tempDev = &devList[devLstCount];
	if (lpGuid == NULL)
		memcpy(&tempDev->devGUID, &GUID_NULL, sizeof(GUID));
	else
		memcpy(&tempDev->devGUID, lpGuid, sizeof(GUID));
	tempDev->devName = strdup(lpcstrDescription);
	tempDev->devModule = strdup(lpcstrModule);
	devLstCount ++;
	
	return TRUE;
}

uint8_t lwaodDSound_Init(void)
{
	HRESULT retVal;
	uint32_t curDev;
	
	if (isInit)
		return LWAO_ERR_WASDONE;
	
	devLstAlloc = 0;
	devLstCount = 0;
	devList = NULL;
	retVal = DirectSoundEnumerateA(&DSEnumCallback, NULL);
	if (retVal != DS_OK)
		return LWAO_ERR_API_ERR;
	
	deviceList.devNames = (char**)malloc(sizeof(char*) * devLstCount);
	for (curDev = 0; curDev < devLstCount; curDev ++)
		deviceList.devNames[curDev] = devList[curDev].devName;
	deviceList.devCount = curDev;
	
	
	memset(&defOptions, 0x00, sizeof(LWAO_OPTS));
	defOptions.sampleRate = 44100;
	defOptions.numChannels = 2;
	defOptions.numBitsPerSmpl = 16;
	defOptions.usecPerBuf = 10000;	// 10 ms per buffer
	defOptions.numBuffers = 10;	// 100 ms latency
	
	
	activeDrivers = 0;
	isInit = 1;
	
	return LWAO_ERR_OK;
}

uint8_t lwaodDSound_Deinit(void)
{
	uint32_t curDev;
	DSND_DEVICE* tempDev;
	
	if (! isInit)
		return LWAO_ERR_WASDONE;
	
	deviceList.devCount = 0;
	free(deviceList.devNames);	deviceList.devNames = NULL;
	
	for (curDev = 0; curDev < devLstCount; curDev ++)
	{
		tempDev = &devList[curDev];
		free(tempDev->devName);
		free(tempDev->devModule);
	}
	devLstCount = 0;
	devLstAlloc = 0;
	free(devList);	devList = NULL;
	
	isInit = 0;
	
	return LWAO_ERR_OK;
}

const LWAO_DEV_LIST* lwaodDSound_GetDeviceList(void)
{
	return &deviceList;
}

LWAO_OPTS* lwaodDSound_GetDefaultOpts(void)
{
	return &defOptions;
}


uint8_t lwaodDSound_Create(void** retDrvObj)
{
	DRV_DSND* drv;
	uint8_t retVal8;
	
	drv = (DRV_DSND*)malloc(sizeof(DRV_DSND));
	drv->devState = 0;
	drv->hWnd = NULL;
	drv->dSndIntf = NULL;
	drv->dSndBuf = NULL;
	drv->hThread = NULL;
	drv->hSignal = NULL;
	drv->hMutex = NULL;
	drv->userParam = NULL;
	drv->FillBuffer = NULL;
	
	activeDrivers ++;
	retVal8  = lwauSignal_Init(&drv->hSignal, 0);
	retVal8 |= lwauMutex_Init(&drv->hMutex, 0);
	if (retVal8)
	{
		lwaodDSound_Destroy(drv);
		*retDrvObj = NULL;
		return LWAO_ERR_API_ERR;
	}
	*retDrvObj = drv;
	
	return LWAO_ERR_OK;
}

uint8_t lwaodDSound_Destroy(void* drvObj)
{
	DRV_DSND* drv = (DRV_DSND*)drvObj;
	
	if (drv->devState != 0)
		lwaodDSound_Stop(drvObj);
	if (drv->hThread != NULL)
	{
		lwauThread_Cancel(drv->hThread);
		lwauThread_Deinit(drv->hThread);
	}
	if (drv->hSignal != NULL)
		lwauSignal_Deinit(drv->hSignal);
	if (drv->hMutex != NULL)
		lwauMutex_Deinit(drv->hMutex);
	
	free(drv);
	activeDrivers --;
	
	return LWAO_ERR_OK;
}

uint8_t lwaodDSound_SetHWnd(void* drvObj, HWND hWnd)
{
	DRV_DSND* drv = (DRV_DSND*)drvObj;
	
	drv->hWnd = hWnd;
	
	return LWAO_ERR_OK;
}

uint8_t lwaodDSound_Start(void* drvObj, uint32_t deviceID, LWAO_OPTS* options, void* audDrvParam)
{
	DRV_DSND* drv = (DRV_DSND*)drvObj;
	GUID* devGUID;
	uint64_t tempInt64;
	DSBUFFERDESC bufDesc;
	HRESULT retVal;
	uint8_t retVal8;
#ifdef NDEBUG
	HANDLE hWinThr;
	BOOL retValB;
#endif
	
	if (drv->devState != 0)
		return 0xD0;	// already running
	if (deviceID >= devLstCount)
		return LWAO_ERR_INVALID_DEV;
	if (drv->hWnd == NULL)
		return LWAO_ERR_CALL_SPC_FUNC;
	
	drv->audDrvPtr = audDrvParam;
	if (options == NULL)
		options = &defOptions;
	drv->waveFmt.wFormatTag = WAVE_FORMAT_PCM;
	drv->waveFmt.nChannels = options->numChannels;
	drv->waveFmt.nSamplesPerSec = options->sampleRate;
	drv->waveFmt.wBitsPerSample = options->numBitsPerSmpl;
	drv->waveFmt.nBlockAlign = drv->waveFmt.wBitsPerSample * drv->waveFmt.nChannels / 8;
	drv->waveFmt.nAvgBytesPerSec = drv->waveFmt.nSamplesPerSec * drv->waveFmt.nBlockAlign;
	drv->waveFmt.cbSize = 0;
	
	tempInt64 = (uint64_t)options->sampleRate * options->usecPerBuf;
	drv->bufSmpls = (uint32_t)((tempInt64 + 500000) / 1000000);
	drv->bufSegSize = drv->waveFmt.nBlockAlign * drv->bufSmpls;
	drv->bufSegCount = options->numBuffers ? options->numBuffers : 10;
	drv->bufSize = drv->bufSegSize * drv->bufSegCount;
	
	retVal = CoInitializeEx(NULL, COINIT_MULTITHREADED);	// call again, in case Init() was called by another thread
	if (! (retVal == S_OK || retVal == S_FALSE))
		return LWAO_ERR_API_ERR;
	
	if (! memcmp(&devList[deviceID].devGUID, &GUID_NULL, sizeof(GUID)))
		devGUID = NULL;	// default device
	else
		devGUID = &devList[deviceID].devGUID;
	retVal = DirectSoundCreate(devGUID, &drv->dSndIntf, NULL);
	if (retVal != DS_OK)
		return LWAO_ERR_API_ERR;
	
	retVal = drv->dSndIntf->SetCooperativeLevel(drv->hWnd, DSSCL_PRIORITY);
	if (retVal == DSERR_INVALIDPARAM)
		return LWAO_ERR_CALL_SPC_FUNC;
	if (retVal != DS_OK)
		return LWAO_ERR_API_ERR;
	
	// Create Secondary Sound Buffer
	bufDesc.dwSize = sizeof(DSBUFFERDESC);
	bufDesc.dwFlags = DSBCAPS_GLOBALFOCUS;	// Global Focus -> don't mute sound when losing focus
	bufDesc.dwBufferBytes = drv->bufSize;
	bufDesc.dwReserved = 0;
	bufDesc.lpwfxFormat = &drv->waveFmt;
	retVal = drv->dSndIntf->CreateSoundBuffer(&bufDesc, &drv->dSndBuf, NULL);
	if (retVal != DS_OK)
		return LWAO_ERR_API_ERR;
	
	lwauSignal_Reset(drv->hSignal);
	retVal8 = lwauThread_Init(&drv->hThread, &DirectSoundThread, drv);
	if (retVal8)
		return 0xC8;	// CreateThread failed
#ifdef NDEBUG
	hWinThr = *(HANDLE*)lwauThread_GetHandle(drv->hThread);
	retValB = SetThreadPriority(hWinThr, THREAD_PRIORITY_TIME_CRITICAL);
	if (! retValB)
	{
		// Error setting priority
		// Try a lower priority, because too low priorities cause sound stuttering.
		retValB = SetThreadPriority(hWinThr, THREAD_PRIORITY_HIGHEST);
	}
#endif
	
	drv->bufSpace = (uint8_t*)malloc(drv->bufSegSize);
	
	drv->writePos = 0;
	ClearBuffer(drv);
	retVal = drv->dSndBuf->Play(0, 0, DSBPLAY_LOOPING);
	
	drv->devState = 1;
	lwauSignal_Signal(drv->hSignal);
	
	return LWAO_ERR_OK;
}

uint8_t lwaodDSound_Stop(void* drvObj)
{
	DRV_DSND* drv = (DRV_DSND*)drvObj;
	
	if (drv->devState != 1)
		return 0xD8;	// is already stopped (or stopping)
	
	drv->devState = 2;
	
	if (drv->dSndBuf != NULL)
		drv->dSndBuf->Stop();
	
	lwauThread_Join(drv->hThread);
	lwauThread_Deinit(drv->hThread);	drv->hThread = NULL;
	
	free(drv->bufSpace);
	drv->bufSpace = NULL;
	if (drv->dSndBuf != NULL)
	{
		drv->dSndBuf->Release();
		drv->dSndBuf = NULL;
	}
	drv->dSndIntf->Release();
	drv->dSndIntf = NULL;
	
	CoUninitialize();
	drv->devState = 0;
	
	return LWAO_ERR_OK;
}

uint8_t lwaodDSound_Pause(void* drvObj)
{
	DRV_DSND* drv = (DRV_DSND*)drvObj;
	HRESULT retVal;
	
	if (drv->devState != 1)
		return 0xFF;
	
	retVal = drv->dSndBuf->Stop();
	return (retVal == DS_OK) ? LWAO_ERR_OK : 0xFF;
}

uint8_t lwaodDSound_Resume(void* drvObj)
{
	DRV_DSND* drv = (DRV_DSND*)drvObj;
	HRESULT retVal;
	
	if (drv->devState != 1)
		return 0xFF;
	
	retVal = drv->dSndBuf->Play(0, 0, DSBPLAY_LOOPING);
	return (retVal == DS_OK) ? LWAO_ERR_OK : 0xFF;
}


uint8_t lwaodDSound_SetCallback(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam)
{
	DRV_DSND* drv = (DRV_DSND*)drvObj;
	
	lwauMutex_Lock(drv->hMutex);
	drv->userParam = userParam;
	drv->FillBuffer = FillBufCallback;
	lwauMutex_Unlock(drv->hMutex);
	
	return LWAO_ERR_OK;
}

uint32_t lwaodDSound_GetBufferSize(void* drvObj)
{
	DRV_DSND* drv = (DRV_DSND*)drvObj;
	
	return drv->bufSegSize;
}

static uint32_t GetFreeBytes(DRV_DSND* drv)
{
	HRESULT retVal;
	DWORD writeEndPos;
	
	retVal = drv->dSndBuf->GetCurrentPosition(NULL, &writeEndPos);
	if (retVal != DS_OK)
		return 0;
	
	if (drv->writePos <= writeEndPos)
		return writeEndPos - drv->writePos;
	else
		return (drv->bufSize - drv->writePos) + writeEndPos;
}

uint8_t lwaodDSound_IsBusy(void* drvObj)
{
	DRV_DSND* drv = (DRV_DSND*)drvObj;
	uint32_t freeBytes;
	
	if (drv->FillBuffer != NULL)
		return LWAO_ERR_BAD_MODE;
	
	freeBytes = GetFreeBytes(drv);
	return (freeBytes >= drv->bufSegSize) ? LWAO_ERR_OK : LWAO_ERR_BUSY;
}

uint8_t lwaodDSound_WriteData(void* drvObj, uint32_t dataSize, void* data)
{
	DRV_DSND* drv = (DRV_DSND*)drvObj;
	uint32_t freeBytes;
	
	if (dataSize > drv->bufSegSize)
		return LWAO_ERR_TOO_MUCH_DATA;
	
	freeBytes = GetFreeBytes(drv);
	while(freeBytes < dataSize)
		Sleep(1);
	
	WriteBuffer(drv, dataSize, data);
	return LWAO_ERR_OK;
}


uint32_t lwaodDSound_GetLatency(void* drvObj)
{
	DRV_DSND* drv = (DRV_DSND*)drvObj;
	uint32_t bytesBehind;
#if 0
	DWORD readPos;
	HRESULT retVal;
	
	retVal = drv->dSndBuf->GetCurrentPosition(&readPos, NULL);
	if (retVal != DS_OK)
		return 0;
	
	if (drv->writePos >= readPos)
		bytesBehind = drv->writePos - readPos;
	else
		bytesBehind = (drv->bufSize - readPos) + drv->writePos;
#else
	bytesBehind = drv->bufSize - GetFreeBytes(drv);
#endif
	return bytesBehind * 1000 / drv->waveFmt.nAvgBytesPerSec;
}

static void LWA_API DirectSoundThread(void* Arg)
{
	DRV_DSND* drv = (DRV_DSND*)Arg;
	uint32_t wrtBytes;
	uint32_t didBuffers;	// number of processed buffers
	
	lwauSignal_Wait(drv->hSignal);	// wait until the initialization is done
	
	while(drv->devState == 1)
	{
		didBuffers = 0;
		
		lwauMutex_Lock(drv->hMutex);
		while(GetFreeBytes(drv) >= drv->bufSegSize && drv->FillBuffer != NULL)
		{
			wrtBytes = drv->FillBuffer(drv->audDrvPtr, drv->userParam, drv->bufSegSize, drv->bufSpace);
			WriteBuffer(drv, wrtBytes, drv->bufSpace);
			didBuffers ++;
		}
		lwauMutex_Unlock(drv->hMutex);
		if (! didBuffers)
			Sleep(1);
		
		while(drv->FillBuffer == NULL && drv->devState == 1)
			Sleep(1);
		//while(drv->PauseThread && drv->devState == 1)
		//	Sleep(1);
	}
	
	return;
}

static uint8_t WriteBuffer(DRV_DSND* drv, uint32_t dataSize, void* data)
{
	HRESULT retVal;
	DWORD bufSize1;
	void* bufPtr1;
	DWORD bufSize2;
	void* bufPtr2;
	
	retVal = drv->dSndBuf->Lock(drv->writePos, dataSize, &bufPtr1, &bufSize1,
								&bufPtr2, &bufSize2, 0x00);
	if (retVal != DS_OK)
		return 0xFF;
	
	memcpy(bufPtr1, data, bufSize1);
	drv->writePos += bufSize1;
	if (drv->writePos >= drv->bufSize)
		drv->writePos -= drv->bufSize;
	if (bufPtr2 != NULL)
	{
		memcpy(bufPtr2, (char*)data + bufSize1, bufSize2);
		drv->writePos += bufSize2;
	}
	
	retVal = drv->dSndBuf->Unlock(bufPtr1, bufSize1, bufPtr2, bufSize2);
	if (retVal != DS_OK)
		return 0xFF;
	
	return LWAO_ERR_OK;
}

static uint8_t ClearBuffer(DRV_DSND* drv)
{
	HRESULT retVal;
	DWORD bufSize;
	void* bufPtr;
	
	retVal = drv->dSndBuf->Lock(0x00, 0x00, &bufPtr, &bufSize,
								NULL, NULL, DSBLOCK_ENTIREBUFFER);
	if (retVal != DS_OK)
		return 0xFF;
	
	memset(bufPtr, 0x00, bufSize);
	
	retVal = drv->dSndBuf->Unlock(bufPtr, bufSize, NULL, 0);
	if (retVal != DS_OK)
		return 0xFF;
	
	return LWAO_ERR_OK;
}
