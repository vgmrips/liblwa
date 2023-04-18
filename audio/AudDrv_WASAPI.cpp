// Audio Stream - Windows Audio Session API
// Required libraries:
//	- ole32.lib (COM stuff)
#define _CRTDBG_MAP_ALLOC
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <audiopolicy.h>
#include <FunctionDiscoveryKeys_devpkey.h>	// for PKEY_*
#include <MMSystem.h>	// for WAVEFORMATEX

#ifdef _MSC_VER
#define strdup	_strdup
#define	wcsdup	_wcsdup
#endif

#include "../stdtype.h"

#include "AudioStream.h"
#include "../utils/lwauThread.h"
#include "../utils/lwauSignal.h"
#include "../utils/lwauMutex.h"

#define EXT_C	extern "C"


typedef struct _wasapi_driver
{
	void* audDrvPtr;
	volatile UINT8 devState;	// 0 - not running, 1 - running, 2 - terminating
	UINT16 dummy;	// [for alignment purposes]
	
	WAVEFORMATEX waveFmt;
	UINT32 bufSmpls;
	UINT32 bufSize;
	UINT32 bufCount;
	
	IMMDeviceEnumerator* devEnum;
	IMMDevice* audDev;
	IAudioClient* audClnt;
	IAudioRenderClient* rendClnt;
	
	LWAU_THREAD* hThread;
	LWAU_SIGNAL* hSignal;
	LWAU_MUTEX* hMutex;
	void* userParam;
	LWAOFUNC_FILLBUF FillBuffer;
	
	UINT32 bufFrmCount;
} DRV_WASAPI;


EXT_C UINT8 lwaodWASAPI_IsAvailable(void);
EXT_C UINT8 lwaodWASAPI_Init(void);
EXT_C UINT8 lwaodWASAPI_Deinit(void);
EXT_C const LWAO_DEV_LIST* lwaodWASAPI_GetDeviceList(void);
EXT_C LWAO_OPTS* lwaodWASAPI_GetDefaultOpts(void);

EXT_C UINT8 lwaodWASAPI_Create(void** retDrvObj);
EXT_C UINT8 lwaodWASAPI_Destroy(void* drvObj);
EXT_C UINT8 lwaodWASAPI_Start(void* drvObj, UINT32 deviceID, LWAO_OPTS* options, void* audDrvParam);
EXT_C UINT8 lwaodWASAPI_Stop(void* drvObj);
EXT_C UINT8 lwaodWASAPI_Pause(void* drvObj);
EXT_C UINT8 lwaodWASAPI_Resume(void* drvObj);

EXT_C UINT8 lwaodWASAPI_SetCallback(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam);
EXT_C UINT32 lwaodWASAPI_GetBufferSize(void* drvObj);
static UINT32 GetFreeSamples(DRV_WASAPI* drv);
EXT_C UINT8 lwaodWASAPI_IsBusy(void* drvObj);
EXT_C UINT8 lwaodWASAPI_WriteData(void* drvObj, UINT32 dataSize, void* data);

EXT_C UINT32 lwaodWASAPI_GetLatency(void* drvObj);
static void LWA_API WasapiThread(void* Arg);


extern "C"
{
LWAO_DRIVER lwaoDrv_WASAPI =
{
	{LWAO_DTYPE_OUT, LWAO_DSIG_WASAPI, "WASAPI"},
	
	lwaodWASAPI_IsAvailable,
	lwaodWASAPI_Init, lwaodWASAPI_Deinit,
	lwaodWASAPI_GetDeviceList, lwaodWASAPI_GetDefaultOpts,
	
	lwaodWASAPI_Create, lwaodWASAPI_Destroy,
	lwaodWASAPI_Start, lwaodWASAPI_Stop,
	lwaodWASAPI_Pause, lwaodWASAPI_Resume,
	
	lwaodWASAPI_SetCallback, lwaodWASAPI_GetBufferSize,
	lwaodWASAPI_IsBusy, lwaodWASAPI_WriteData,
	
	lwaodWASAPI_GetLatency,
};
}	// extern "C"


static const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
static const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
static const IID IID_IAudioClient = __uuidof(IAudioClient);
static const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);


static LWAO_OPTS defOptions;
static LWAO_DEV_LIST deviceList;
static wchar_t** devListIDs;

static UINT8 isInit = 0;
static UINT32 activeDrivers;

UINT8 lwaodWASAPI_IsAvailable(void)
{
	HRESULT retVal;
	IMMDeviceEnumerator* devEnum;
	UINT8 resVal;
	
	resVal = 0;
	retVal = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (! (retVal == S_OK || retVal == S_FALSE))
		return 0;
	
	retVal = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
								IID_IMMDeviceEnumerator, (void**)&devEnum);
	if (retVal == S_OK)
	{
		resVal = 1;
		devEnum->Release();	devEnum = NULL;
	}
	
	CoUninitialize();
	return resVal;
}

UINT8 lwaodWASAPI_Init(void)
{
	HRESULT retVal;
	UINT devCount;
	UINT32 curDev;
	UINT32 devLstID;
	size_t devNameSize;
	IMMDeviceEnumerator* devEnum;
	IMMDeviceCollection* devList;
	IMMDevice* audDev;
	IPropertyStore* propSt;
	LPWSTR devIdStr;
	PROPVARIANT propName;
	IAudioClient* audClnt;
	WAVEFORMATEX* mixFmt;
	WAVEFORMATEXTENSIBLE* mixFmtX;
	
	if (isInit)
		return LWAO_ERR_WASDONE;
	
	deviceList.devCount = 0;
	deviceList.devNames = NULL;
	devListIDs = NULL;
	
	retVal = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (! (retVal == S_OK || retVal == S_FALSE))
		return LWAO_ERR_API_ERR;
	
	retVal = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
								IID_IMMDeviceEnumerator, (void**)&devEnum);
	if (retVal != S_OK)
		return LWAO_ERR_API_ERR;
	retVal = devEnum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &devList);
	if (retVal != S_OK)
		return LWAO_ERR_API_ERR;
	
	retVal = devList->GetCount(&devCount);
	if (retVal != S_OK)
		return LWAO_ERR_API_ERR;
	
	deviceList.devCount = 1 + devCount;
	deviceList.devNames = (char**)malloc(deviceList.devCount * sizeof(char*));
	devListIDs = (wchar_t**)malloc(deviceList.devCount * sizeof(wchar_t*));
	devLstID = 0;
	
	devListIDs[devLstID] = NULL;	// default device
	deviceList.devNames[devLstID] = strdup("default device");
	devLstID ++;
	
	PropVariantInit(&propName);
	for (curDev = 0; curDev < devCount; curDev ++)
	{
		retVal = devList->Item(curDev, &audDev);
		if (retVal == S_OK)
		{
			retVal = audDev->GetId(&devIdStr);
			if (retVal == S_OK)
			{
				retVal = audDev->OpenPropertyStore(STGM_READ, &propSt);
				if (retVal == S_OK)
				{
					retVal = propSt->GetValue(PKEY_DeviceInterface_FriendlyName, &propName);
					if (retVal == S_OK)
					{
						devListIDs[devLstID] = wcsdup(devIdStr);
						
						devNameSize = wcslen(propName.pwszVal) + 1;
						deviceList.devNames[devLstID] = (char*)malloc(devNameSize);
						wcstombs(deviceList.devNames[devLstID], propName.pwszVal, devNameSize);
						devLstID ++;
						PropVariantClear(&propName);
					}
					propSt->Release();		propSt = NULL;
				}
				CoTaskMemFree(devIdStr);	devIdStr = NULL;
			}
			audDev->Release();	audDev = NULL;
		}
	}
	deviceList.devCount = devLstID;
	devList->Release();	devList = NULL;
	
	memset(&defOptions, 0x00, sizeof(LWAO_OPTS));
	defOptions.sampleRate = 44100;
	defOptions.numChannels = 2;
	defOptions.numBitsPerSmpl = 16;
	defOptions.usecPerBuf = 10000;	// 10 ms per buffer
	defOptions.numBuffers = 10;	// 100 ms latency
	
	retVal = devEnum->GetDefaultAudioEndpoint(eRender, eConsole, &audDev);
	if (retVal == S_OK)
	{
		retVal = audDev->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&audClnt);
		if (retVal == S_OK)
		{
			retVal = audClnt->GetMixFormat(&mixFmt);
			if (retVal == S_OK)
			{
				defOptions.sampleRate = mixFmt->nSamplesPerSec;
				defOptions.numChannels = (UINT8)mixFmt->nChannels;
				if (mixFmt->wFormatTag == WAVE_FORMAT_PCM)
				{
					defOptions.numBitsPerSmpl = (UINT8)mixFmt->wBitsPerSample;
				}
				else if (mixFmt->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
				{
					mixFmtX = (WAVEFORMATEXTENSIBLE*)mixFmt;
					if (mixFmtX->SubFormat == KSDATAFORMAT_SUBTYPE_PCM)
						defOptions.numBitsPerSmpl = (UINT8)mixFmt->wBitsPerSample;
				}
				CoTaskMemFree(mixFmt);	mixFmt = NULL;
			}
			audClnt->Release();	audClnt = NULL;
		}
		audDev->Release();	audDev = NULL;
	}
	devEnum->Release();	devEnum = NULL;
	
	
	activeDrivers = 0;
	isInit = 1;
	
	return LWAO_ERR_OK;
}

UINT8 lwaodWASAPI_Deinit(void)
{
	UINT32 curDev;
	
	if (! isInit)
		return LWAO_ERR_WASDONE;
	
	for (curDev = 0; curDev < deviceList.devCount; curDev ++)
	{
		free(deviceList.devNames[curDev]);
		free(devListIDs[curDev]);
	}
	deviceList.devCount = 0;
	free(deviceList.devNames);	deviceList.devNames = NULL;
	free(devListIDs);			devListIDs = NULL;
	
	CoUninitialize();
	isInit = 0;
	
	return LWAO_ERR_OK;
}

const LWAO_DEV_LIST* lwaodWASAPI_GetDeviceList(void)
{
	return &deviceList;
}

LWAO_OPTS* lwaodWASAPI_GetDefaultOpts(void)
{
	return &defOptions;
}


UINT8 lwaodWASAPI_Create(void** retDrvObj)
{
	DRV_WASAPI* drv;
	UINT8 retVal8;
	
	drv = (DRV_WASAPI*)malloc(sizeof(DRV_WASAPI));
	drv->devState = 0;
	
	drv->devEnum = NULL;
	drv->audDev = NULL;
	drv->audClnt = NULL;
	drv->rendClnt = NULL;
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
		lwaodWASAPI_Destroy(drv);
		*retDrvObj = NULL;
		return LWAO_ERR_API_ERR;
	}
	*retDrvObj = drv;
	
	return LWAO_ERR_OK;
}

UINT8 lwaodWASAPI_Destroy(void* drvObj)
{
	DRV_WASAPI* drv = (DRV_WASAPI*)drvObj;
	
	if (drv->devState != 0)
		lwaodWASAPI_Stop(drvObj);
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

UINT8 lwaodWASAPI_Start(void* drvObj, UINT32 deviceID, LWAO_OPTS* options, void* audDrvParam)
{
	DRV_WASAPI* drv = (DRV_WASAPI*)drvObj;
	UINT64 tempInt64;
	REFERENCE_TIME bufTime;
	UINT8 errVal;
	HRESULT retVal;
	UINT8 retVal8;
#ifdef NDEBUG
	HANDLE hWinThr;
	BOOL retValB;
#endif
	
	if (drv->devState != 0)
		return 0xD0;	// already running
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
	
	tempInt64 = (UINT64)options->sampleRate * options->usecPerBuf;
	drv->bufSmpls = (UINT32)((tempInt64 + 500000) / 1000000);
	drv->bufSize = drv->waveFmt.nBlockAlign * drv->bufSmpls;
	drv->bufCount = options->numBuffers ? options->numBuffers : 10;
	// REFERENCE_TIME uses 100-ns steps
	bufTime = (REFERENCE_TIME)10000000 * drv->bufSmpls * drv->bufCount;
	bufTime = (bufTime + options->sampleRate / 2) / options->sampleRate;
	
	retVal = CoInitializeEx(NULL, COINIT_MULTITHREADED);	// call again, in case Init() was called by another thread
	if (! (retVal == S_OK || retVal == S_FALSE))
		return LWAO_ERR_API_ERR;
	
	retVal = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
								IID_IMMDeviceEnumerator, (void**)&drv->devEnum);
	if (retVal != S_OK)
		return LWAO_ERR_API_ERR;
	
	errVal = LWAO_ERR_API_ERR;
	if (devListIDs[deviceID] == NULL)
		retVal = drv->devEnum->GetDefaultAudioEndpoint(eRender, eConsole, &drv->audDev);
	else
		retVal = drv->devEnum->GetDevice(devListIDs[deviceID], &drv->audDev);
	if (retVal != S_OK)
		goto StartErr_DevEnum;
	
	retVal = drv->audDev->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&drv->audClnt);
	if (retVal != S_OK)
		goto StartErr_HasDev;
	
	retVal = drv->audClnt->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, bufTime, 0, &drv->waveFmt, NULL);
	if (retVal != S_OK)
		goto StartErr_HasAudClient;
	
	retVal = drv->audClnt->GetBufferSize(&drv->bufFrmCount);
	if (retVal != S_OK)
		goto StartErr_HasAudClient;
	
	retVal = drv->audClnt->GetService(IID_IAudioRenderClient, (void**)&drv->rendClnt);
	if (retVal != S_OK)
		goto StartErr_HasAudClient;
	
	lwauSignal_Reset(drv->hSignal);
	retVal8 = lwauThread_Init(&drv->hThread, &WasapiThread, drv);
	if (retVal8)
	{
		errVal = 0xC8;	// CreateThread failed
		goto StartErr_HasRendClient;
	}
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
	
	retVal = drv->audClnt->Start();
	
	drv->devState = 1;
	lwauSignal_Signal(drv->hSignal);
	
	return LWAO_ERR_OK;

StartErr_HasRendClient:
	drv->rendClnt->Release();	drv->rendClnt = NULL;
StartErr_HasAudClient:
	drv->audClnt->Release();	drv->audClnt = NULL;
StartErr_HasDev:
	drv->audDev->Release();		drv->audDev = NULL;
StartErr_DevEnum:
	drv->devEnum->Release();	drv->devEnum = NULL;
	CoUninitialize();
	return errVal;
}

UINT8 lwaodWASAPI_Stop(void* drvObj)
{
	DRV_WASAPI* drv = (DRV_WASAPI*)drvObj;
	HRESULT retVal;
	
	if (drv->devState != 1)
		return 0xD8;	// is already stopped (or stopping)
	
	drv->devState = 2;
	if (drv->audClnt != NULL)
		retVal = drv->audClnt->Stop();
	
	lwauThread_Join(drv->hThread);
	lwauThread_Deinit(drv->hThread);	drv->hThread = NULL;
	
	drv->rendClnt->Release();	drv->rendClnt = NULL;
	drv->audClnt->Release();	drv->audClnt = NULL;
	drv->audDev->Release();		drv->audDev = NULL;
	drv->devEnum->Release();	drv->devEnum = NULL;
	
	CoUninitialize();
	drv->devState = 0;
	
	return LWAO_ERR_OK;
}

UINT8 lwaodWASAPI_Pause(void* drvObj)
{
	DRV_WASAPI* drv = (DRV_WASAPI*)drvObj;
	HRESULT retVal;
	
	if (drv->devState != 1)
		return 0xFF;
	
	retVal = drv->audClnt->Stop();
	return (retVal == S_OK || retVal == S_FALSE) ? LWAO_ERR_OK : 0xFF;
}

UINT8 lwaodWASAPI_Resume(void* drvObj)
{
	DRV_WASAPI* drv = (DRV_WASAPI*)drvObj;
	HRESULT retVal;
	
	if (drv->devState != 1)
		return 0xFF;
	
	retVal = drv->audClnt->Start();
	return (retVal == S_OK || retVal == S_FALSE) ? LWAO_ERR_OK : 0xFF;
}


UINT8 lwaodWASAPI_SetCallback(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam)
{
	DRV_WASAPI* drv = (DRV_WASAPI*)drvObj;
	
	lwauMutex_Lock(drv->hMutex);
	drv->userParam = userParam;
	drv->FillBuffer = FillBufCallback;
	lwauMutex_Unlock(drv->hMutex);
	
	return LWAO_ERR_OK;
}

UINT32 lwaodWASAPI_GetBufferSize(void* drvObj)
{
	DRV_WASAPI* drv = (DRV_WASAPI*)drvObj;
	
	return drv->bufSize;
}

static UINT32 GetFreeSamples(DRV_WASAPI* drv)
{
	HRESULT retVal;
	UINT paddSmpls;
	
	retVal = drv->audClnt->GetCurrentPadding(&paddSmpls);
	if (retVal != S_OK)
		return 0;
	
	return drv->bufFrmCount - paddSmpls;
}

UINT8 lwaodWASAPI_IsBusy(void* drvObj)
{
	DRV_WASAPI* drv = (DRV_WASAPI*)drvObj;
	UINT32 freeSmpls;
	
	if (drv->FillBuffer != NULL)
		return LWAO_ERR_BAD_MODE;
	
	freeSmpls = GetFreeSamples(drv);
	return (freeSmpls >= drv->bufSmpls) ? LWAO_ERR_OK : LWAO_ERR_BUSY;
}

UINT8 lwaodWASAPI_WriteData(void* drvObj, UINT32 dataSize, void* data)
{
	DRV_WASAPI* drv = (DRV_WASAPI*)drvObj;
	UINT32 dataSmpls;
	HRESULT retVal;
	BYTE* bufData;
	
	if (dataSize > drv->bufSize)
		return LWAO_ERR_TOO_MUCH_DATA;
	
	dataSmpls = dataSize / drv->waveFmt.nBlockAlign;
	while(GetFreeSamples(drv) < dataSmpls)
		Sleep(1);
	
	retVal = drv->rendClnt->GetBuffer(dataSmpls, &bufData);
	if (retVal != S_OK)
		return LWAO_ERR_API_ERR;
	
	memcpy(bufData, data, dataSize);
	
	retVal = drv->rendClnt->ReleaseBuffer(dataSmpls, 0x00);
	
	return LWAO_ERR_OK;
}


UINT32 lwaodWASAPI_GetLatency(void* drvObj)
{
	DRV_WASAPI* drv = (DRV_WASAPI*)drvObj;
	REFERENCE_TIME latencyTime;
	HRESULT retVal;
	
	retVal = drv->audClnt->GetStreamLatency(&latencyTime);
	if (retVal != S_OK)
		return 0;
	
	return (UINT32)((latencyTime + 5000) / 10000);	// 100 ns steps -> 1 ms steps
}

static void LWA_API WasapiThread(void* Arg)
{
	DRV_WASAPI* drv = (DRV_WASAPI*)Arg;
	UINT32 didBuffers;	// number of processed buffers
	UINT32 wrtBytes;
	UINT32 wrtSmpls;
	HRESULT retVal;
	BYTE* bufData;
	
	lwauSignal_Wait(drv->hSignal);	// wait until the initialization is done
	
	while(drv->devState == 1)
	{
		didBuffers = 0;
		
		lwauMutex_Lock(drv->hMutex);
		while(GetFreeSamples(drv) >= drv->bufSmpls && drv->FillBuffer != NULL)
		{
			retVal = drv->rendClnt->GetBuffer(drv->bufSmpls, &bufData);
			if (retVal == S_OK)
			{
				wrtBytes = drv->FillBuffer(drv->audDrvPtr, drv->userParam, drv->bufSize, (void*)bufData);
				wrtSmpls = wrtBytes / drv->waveFmt.nBlockAlign;
				
				retVal = drv->rendClnt->ReleaseBuffer(wrtSmpls, 0x00);
				didBuffers ++;
			}
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
