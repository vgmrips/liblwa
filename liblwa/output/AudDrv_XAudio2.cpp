// Audio Stream - XAudio2
// Required libraries:
//	- ole32.lib (COM stuff)
#define _CRTDBG_MAP_ALLOC
#define _WIN32_DCOM	// for CoInitializeEx() / COINIT_MULTITHREADED
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __GNUC__
// suppress warnings about 'uuid' attribute directive and MSVC pragmas
#pragma GCC diagnostic ignored "-Wattributes"
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#endif

#include <xaudio2.h>
#include <mmsystem.h>	// for WAVEFORMATEX

#include "../lwa_types.h"

#include "lwao.h"
#include "../utils/lwauThread.h"
#include "../utils/lwauSignal.h"
#include "../utils/lwauMutex.h"

#define EXT_C	extern "C"


typedef struct _xaudio2_driver
{
	void* audDrvPtr;
	volatile uint8_t devState;	// 0 - not running, 1 - running, 2 - terminating
	uint16_t dummy;	// [for alignment purposes]
	
	WAVEFORMATEX waveFmt;
	uint32_t bufSmpls;
	uint32_t bufSize;
	uint32_t bufCount;
	uint8_t* bufSpace;
	
	IXAudio2* xAudIntf;
	IXAudio2MasteringVoice* xaMstVoice;
	IXAudio2SourceVoice* xaSrcVoice;
	XAUDIO2_BUFFER* xaBufs;
	LWAU_THREAD* hThread;
	LWAU_SIGNAL* hSignal;
	LWAU_MUTEX* hMutex;
	void* userParam;
	LWAOFUNC_FILLBUF FillBuffer;
	
	uint32_t writeBuf;
} DRV_XAUD2;


EXT_C uint8_t lwaodXAudio2_IsAvailable(void);
EXT_C uint8_t lwaodXAudio2_Init(void);
EXT_C uint8_t lwaodXAudio2_Deinit(void);
EXT_C const LWAO_DEV_LIST* lwaodXAudio2_GetDeviceList(void);
EXT_C LWAO_OPTS* lwaodXAudio2_GetDefaultOpts(void);

EXT_C uint8_t lwaodXAudio2_Create(void** retDrvObj);
EXT_C uint8_t lwaodXAudio2_Destroy(void* drvObj);
EXT_C uint8_t lwaodXAudio2_Start(void* drvObj, uint32_t deviceID, LWAO_OPTS* options, void* audDrvParam);
EXT_C uint8_t lwaodXAudio2_Stop(void* drvObj);
EXT_C uint8_t lwaodXAudio2_Pause(void* drvObj);
EXT_C uint8_t lwaodXAudio2_Resume(void* drvObj);

EXT_C uint8_t lwaodXAudio2_SetCallback(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam);
EXT_C uint32_t lwaodXAudio2_GetBufferSize(void* drvObj);
EXT_C uint8_t lwaodXAudio2_IsBusy(void* drvObj);
EXT_C uint8_t lwaodXAudio2_WriteData(void* drvObj, uint32_t dataSize, void* data);

EXT_C uint32_t lwaodXAudio2_GetLatency(void* drvObj);
static void LWA_API XAudio2Thread(void* Arg);


extern "C"
{
LWAO_DRIVER lwaoDrv_XAudio2 =
{
	{LWAO_DTYPE_OUT, LWAO_DSIG_XAUD2, "XAudio2"},
	
	lwaodXAudio2_IsAvailable,
	lwaodXAudio2_Init, lwaodXAudio2_Deinit,
	lwaodXAudio2_GetDeviceList, lwaodXAudio2_GetDefaultOpts,
	
	lwaodXAudio2_Create, lwaodXAudio2_Destroy,
	lwaodXAudio2_Start, lwaodXAudio2_Stop,
	lwaodXAudio2_Pause, lwaodXAudio2_Resume,
	
	lwaodXAudio2_SetCallback, lwaodXAudio2_GetBufferSize,
	lwaodXAudio2_IsBusy, lwaodXAudio2_WriteData,
	
	lwaodXAudio2_GetLatency,
};
}	// extern "C"


static LWAO_OPTS defOptions;
static LWAO_DEV_LIST deviceList;
static uint32_t* devListIDs;

static uint8_t isInit = 0;
static uint32_t activeDrivers;

uint8_t lwaodXAudio2_IsAvailable(void)
{
	return 1;
}

uint8_t lwaodXAudio2_Init(void)
{
	HRESULT retVal;
	uint32_t curDev;
	uint32_t devLstID;
	size_t devNameSize;
	IXAudio2* xAudIntf;
	XAUDIO2_DEVICE_DETAILS xDevData;
	
	if (isInit)
		return LWAO_ERR_WASDONE;
	
	deviceList.devCount = 0;
	deviceList.devNames = NULL;
	devListIDs = NULL;
	
	retVal = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (! (retVal == S_OK || retVal == S_FALSE))
		return LWAO_ERR_API_ERR;
	
	retVal = XAudio2Create(&xAudIntf, 0x00, XAUDIO2_DEFAULT_PROCESSOR);
	if (retVal != S_OK)
	{
		CoUninitialize();
		return LWAO_ERR_API_ERR;
	}
	
	retVal = xAudIntf->GetDeviceCount(&deviceList.devCount);
	if (retVal != S_OK)
	{
		CoUninitialize();
		return LWAO_ERR_API_ERR;
	}
	deviceList.devNames = (char**)malloc(deviceList.devCount * sizeof(char*));
	devListIDs = (uint32_t*)malloc(deviceList.devCount * sizeof(uint32_t));
	devLstID = 0;
	for (curDev = 0; curDev < deviceList.devCount; curDev ++)
	{
		retVal = xAudIntf->GetDeviceDetails(curDev, &xDevData);
		if (retVal == S_OK)
		{
			devListIDs[devLstID] = curDev;
			devNameSize = wcslen(xDevData.DisplayName) + 1;
			deviceList.devNames[devLstID] = (char*)malloc(devNameSize);
			wcstombs(deviceList.devNames[devLstID], xDevData.DisplayName, devNameSize);
			devLstID ++;
		}
	}
	deviceList.devCount = devLstID;
	xAudIntf->Release();	xAudIntf = NULL;
	
	
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

uint8_t lwaodXAudio2_Deinit(void)
{
	uint32_t curDev;
	
	if (! isInit)
		return LWAO_ERR_WASDONE;
	
	for (curDev = 0; curDev < deviceList.devCount; curDev ++)
		free(deviceList.devNames[curDev]);
	deviceList.devCount = 0;
	free(deviceList.devNames);	deviceList.devNames = NULL;
	free(devListIDs);			devListIDs = NULL;
	
	CoUninitialize();
	isInit = 0;
	
	return LWAO_ERR_OK;
}

const LWAO_DEV_LIST* lwaodXAudio2_GetDeviceList(void)
{
	return &deviceList;
}

LWAO_OPTS* lwaodXAudio2_GetDefaultOpts(void)
{
	return &defOptions;
}


uint8_t lwaodXAudio2_Create(void** retDrvObj)
{
	DRV_XAUD2* drv;
	uint8_t retVal8;
	
	drv = (DRV_XAUD2*)malloc(sizeof(DRV_XAUD2));
	drv->devState = 0;
	drv->xAudIntf = NULL;
	drv->xaMstVoice = NULL;
	drv->xaSrcVoice = NULL;
	drv->xaBufs = NULL;
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
		lwaodXAudio2_Destroy(drv);
		*retDrvObj = NULL;
		return LWAO_ERR_API_ERR;
	}
	*retDrvObj = drv;
	
	return LWAO_ERR_OK;
}

uint8_t lwaodXAudio2_Destroy(void* drvObj)
{
	DRV_XAUD2* drv = (DRV_XAUD2*)drvObj;
	
	if (drv->devState != 0)
		lwaodXAudio2_Stop(drvObj);
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

uint8_t lwaodXAudio2_Start(void* drvObj, uint32_t deviceID, LWAO_OPTS* options, void* audDrvParam)
{
	DRV_XAUD2* drv = (DRV_XAUD2*)drvObj;
	uint64_t tempInt64;
	uint32_t curBuf;
	XAUDIO2_BUFFER* tempXABuf;
	HRESULT retVal;
	uint8_t retVal8;
#ifdef NDEBUG
	HANDLE hWinThr;
	BOOL retValB;
#endif
	
	if (drv->devState != 0)
		return LWAO_ERR_IS_RUNNING;
	if (deviceID >= deviceList.devCount)
		return LWAO_ERR_INVALID_DEV;
	
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
	drv->bufSize = drv->waveFmt.nBlockAlign * drv->bufSmpls;
	drv->bufCount = options->numBuffers ? options->numBuffers : 10;
	
	retVal = CoInitializeEx(NULL, COINIT_MULTITHREADED);	// call again, in case Init() was called by another thread
	if (! (retVal == S_OK || retVal == S_FALSE))
		return LWAO_ERR_API_ERR;
	
	retVal = XAudio2Create(&drv->xAudIntf, 0x00, XAUDIO2_DEFAULT_PROCESSOR);
	if (retVal != S_OK)
		return LWAO_ERR_API_ERR;
	
	retVal = drv->xAudIntf->CreateMasteringVoice(&drv->xaMstVoice,
				XAUDIO2_DEFAULT_CHANNELS, drv->waveFmt.nSamplesPerSec, 0x00,
				devListIDs[deviceID], NULL);
	if (retVal != S_OK)
		return LWAO_ERR_DEV_OPEN_FAIL;
	
	retVal = drv->xAudIntf->CreateSourceVoice(&drv->xaSrcVoice,
				&drv->waveFmt, 0x00, XAUDIO2_DEFAULT_FREQ_RATIO, NULL, NULL, NULL);
	if (retVal != S_OK)
		return LWAO_ERR_BAD_FORMAT;
	
	lwauSignal_Reset(drv->hSignal);
	retVal8 = lwauThread_Init(&drv->hThread, &XAudio2Thread, drv);
	if (retVal8)
		return LWAO_ERR_THREAD_FAIL;
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
	
	drv->xaBufs = (XAUDIO2_BUFFER*)calloc(drv->bufCount, sizeof(XAUDIO2_BUFFER));
	drv->bufSpace = (uint8_t*)malloc(drv->bufCount * drv->bufSize);
	for (curBuf = 0; curBuf < drv->bufCount; curBuf ++)
	{
		tempXABuf = &drv->xaBufs[curBuf];
		tempXABuf->AudioBytes = drv->bufSize;
		tempXABuf->pAudioData = &drv->bufSpace[drv->bufSize * curBuf];
		tempXABuf->pContext = NULL;
		tempXABuf->Flags = 0x00;
	}
	drv->writeBuf = 0;
	
	retVal = drv->xaSrcVoice->Start(0x00, XAUDIO2_COMMIT_NOW);
	
	drv->devState = 1;
	lwauSignal_Signal(drv->hSignal);
	
	return LWAO_ERR_OK;
}

uint8_t lwaodXAudio2_Stop(void* drvObj)
{
	DRV_XAUD2* drv = (DRV_XAUD2*)drvObj;
	HRESULT retVal;
	
	if (drv->devState != 1)
		return LWAO_ERR_NOT_RUNNING;
	
	drv->devState = 2;
	if (drv->xaSrcVoice != NULL)
		retVal = drv->xaSrcVoice->Stop(0x00, XAUDIO2_COMMIT_NOW);
	
	lwauThread_Join(drv->hThread);
	lwauThread_Deinit(drv->hThread);	drv->hThread = NULL;
	
	drv->xaMstVoice->DestroyVoice();	drv->xaMstVoice = NULL;
	drv->xaSrcVoice->DestroyVoice();	drv->xaSrcVoice = NULL;
	drv->xAudIntf->Release();			drv->xAudIntf = NULL;
	
	free(drv->xaBufs);		drv->xaBufs = NULL;
	free(drv->bufSpace);	drv->bufSpace = NULL;
	
	CoUninitialize();
	drv->devState = 0;
	
	return LWAO_ERR_OK;
}

uint8_t lwaodXAudio2_Pause(void* drvObj)
{
	DRV_XAUD2* drv = (DRV_XAUD2*)drvObj;
	HRESULT retVal;
	
	if (drv->devState != 1)
		return LWAO_ERR_NOT_RUNNING;
	
	retVal = drv->xaSrcVoice->Stop(0x00, XAUDIO2_COMMIT_NOW);
	return (retVal == S_OK) ? LWAO_ERR_OK : 0xFF;
	if (retVal != S_OK)
		return LWAO_ERR_API_ERR;
	
	return LWAO_ERR_OK;
}

uint8_t lwaodXAudio2_Resume(void* drvObj)
{
	DRV_XAUD2* drv = (DRV_XAUD2*)drvObj;
	HRESULT retVal;
	
	if (drv->devState != 1)
		return LWAO_ERR_NOT_RUNNING;
	
	retVal = drv->xaSrcVoice->Start(0x00, XAUDIO2_COMMIT_NOW);
	if (retVal != S_OK)
		return LWAO_ERR_API_ERR;
	
	return LWAO_ERR_OK;
}


uint8_t lwaodXAudio2_SetCallback(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam)
{
	DRV_XAUD2* drv = (DRV_XAUD2*)drvObj;
	
	lwauMutex_Lock(drv->hMutex);
	drv->userParam = userParam;
	drv->FillBuffer = FillBufCallback;
	lwauMutex_Unlock(drv->hMutex);
	
	return LWAO_ERR_OK;
}

uint32_t lwaodXAudio2_GetBufferSize(void* drvObj)
{
	DRV_XAUD2* drv = (DRV_XAUD2*)drvObj;
	
	return drv->bufSize;
}

uint8_t lwaodXAudio2_IsBusy(void* drvObj)
{
	DRV_XAUD2* drv = (DRV_XAUD2*)drvObj;
	XAUDIO2_VOICE_STATE xaVocState;
	
	if (drv->FillBuffer != NULL)
		return LWAO_ERR_BAD_MODE;
	
	drv->xaSrcVoice->GetState(&xaVocState);
	return (xaVocState.BuffersQueued < drv->bufCount) ? LWAO_ERR_OK : LWAO_ERR_BUSY;
}

uint8_t lwaodXAudio2_WriteData(void* drvObj, uint32_t dataSize, void* data)
{
	DRV_XAUD2* drv = (DRV_XAUD2*)drvObj;
	XAUDIO2_BUFFER* tempXABuf;
	XAUDIO2_VOICE_STATE xaVocState;
	HRESULT retVal;
	
	if (dataSize > drv->bufSize)
		return LWAO_ERR_TOO_MUCH_DATA;
	
	drv->xaSrcVoice->GetState(&xaVocState);
	while(xaVocState.BuffersQueued >= drv->bufCount)
	{
		Sleep(1);
		drv->xaSrcVoice->GetState(&xaVocState);
	}
	
	tempXABuf = &drv->xaBufs[drv->writeBuf];
	memcpy((void*)tempXABuf->pAudioData, data, dataSize);
	tempXABuf->AudioBytes = dataSize;
	
	retVal = drv->xaSrcVoice->SubmitSourceBuffer(tempXABuf, NULL);
	drv->writeBuf ++;
	if (drv->writeBuf >= drv->bufCount)
		drv->writeBuf -= drv->bufCount;
	return LWAO_ERR_OK;
}


uint32_t lwaodXAudio2_GetLatency(void* drvObj)
{
	DRV_XAUD2* drv = (DRV_XAUD2*)drvObj;
	XAUDIO2_VOICE_STATE xaVocState;
	uint32_t bufBehind;
	uint32_t bytesBehind;
	uint32_t curBuf;
	
	drv->xaSrcVoice->GetState(&xaVocState);
	
	bufBehind = xaVocState.BuffersQueued;
	curBuf = drv->writeBuf;
	bytesBehind = 0;
	while(bufBehind > 0)
	{
		if (curBuf == 0)
			curBuf = drv->bufCount;
		curBuf --;
		bytesBehind += drv->xaBufs[curBuf].AudioBytes;
		bufBehind --;
	}
	return bytesBehind * 1000 / drv->waveFmt.nAvgBytesPerSec;
}

static void LWA_API XAudio2Thread(void* Arg)
{
	DRV_XAUD2* drv = (DRV_XAUD2*)Arg;
	XAUDIO2_VOICE_STATE xaVocState;
	XAUDIO2_BUFFER* tempXABuf;
	uint32_t didBuffers;	// number of processed buffers
	HRESULT retVal;
	
	lwauSignal_Wait(drv->hSignal);	// wait until the initialization is done
	
	while(drv->devState == 1)
	{
		didBuffers = 0;
		
		lwauMutex_Lock(drv->hMutex);
		drv->xaSrcVoice->GetState(&xaVocState);
		while(xaVocState.BuffersQueued < drv->bufCount && drv->FillBuffer != NULL)
		{
			tempXABuf = &drv->xaBufs[drv->writeBuf];
			tempXABuf->AudioBytes = drv->FillBuffer(drv->audDrvPtr, drv->userParam,
										drv->bufSize, (void*)tempXABuf->pAudioData);
			
			retVal = drv->xaSrcVoice->SubmitSourceBuffer(tempXABuf, NULL);
			drv->writeBuf ++;
			if (drv->writeBuf >= drv->bufCount)
				drv->writeBuf -= drv->bufCount;
			didBuffers ++;
			
			drv->xaSrcVoice->GetState(&xaVocState);
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
