// Audio Stream - Windows Multimedia API WinMM
//	- winmm.lib
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <mmsystem.h>

#include "../lwa_types.h"

#include "lwao.h"
#include "../utils/lwauThread.h"
#include "../utils/lwauSignal.h"
#include "../utils/lwauMutex.h"


#ifdef _MSC_VER
#define	strdup	_strdup
#endif

#ifdef NEED_WAVEFMT
#pragma pack(1)
typedef struct
{
	uint16_t wFormatTag;
	uint16_t nChannels;
	uint32_t nSamplesPerSec;
	uint32_t nAvgBytesPerSec;
	uint16_t nBlockAlign;
	uint16_t wBitsPerSample;
	uint16_t cbSize;
} WAVEFORMATEX;	// from MSDN Help
#pragma pack()

#define WAVE_FORMAT_PCM	0x0001
#endif


typedef struct _winmm_driver
{
	void* audDrvPtr;
	volatile uint8_t devState;	// 0 - not running, 1 - running, 2 - terminating
	uint16_t dummy;	// [for alignment purposes]
	
	WAVEFORMATEX waveFmt;
	uint32_t bufSmpls;
	uint32_t bufSize;
	uint32_t bufCount;
	uint8_t* bufSpace;
	
	LWAU_THREAD* hThread;
	LWAU_SIGNAL* hSignal;
	LWAU_MUTEX* hMutex;
	HWAVEOUT hWaveOut;
	WAVEHDR* waveHdrs;
	void* userParam;
	LWAOFUNC_FILLBUF FillBuffer;
	
	uint32_t BlocksSent;
	uint32_t BlocksPlayed;
} DRV_WINMM;


uint8_t lwaodWinMM_IsAvailable(void);
uint8_t lwaodWinMM_Init(void);
uint8_t lwaodWinMM_Deinit(void);
const LWAO_DEV_LIST* lwaodWinMM_GetDeviceList(void);
LWAO_OPTS* lwaodWinMM_GetDefaultOpts(void);

uint8_t lwaodWinMM_Create(void** retDrvObj);
uint8_t lwaodWinMM_Destroy(void* drvObj);
uint8_t lwaodWinMM_Start(void* drvObj, uint32_t deviceID, LWAO_OPTS* options, void* audDrvParam);
uint8_t lwaodWinMM_Stop(void* drvObj);
uint8_t lwaodWinMM_Pause(void* drvObj);
uint8_t lwaodWinMM_Resume(void* drvObj);

uint8_t lwaodWinMM_SetCallback(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam);
uint32_t lwaodWinMM_GetBufferSize(void* drvObj);
uint8_t lwaodWinMM_IsBusy(void* drvObj);
uint8_t lwaodWinMM_WriteData(void* drvObj, uint32_t dataSize, void* data);

uint32_t lwaodWinMM_GetLatency(void* drvObj);
static void LWA_API WaveOutThread(void* Arg);
static void WriteBuffer(DRV_WINMM* drv, WAVEHDR* wHdr);
static void BufCheck(DRV_WINMM* drv);


LWAO_DRIVER lwaoDrv_WinMM =
{
	{LWAO_DTYPE_OUT, LWAO_DSIG_WINMM, "WinMM"},
	
	lwaodWinMM_IsAvailable,
	lwaodWinMM_Init, lwaodWinMM_Deinit,
	lwaodWinMM_GetDeviceList, lwaodWinMM_GetDefaultOpts,
	
	lwaodWinMM_Create, lwaodWinMM_Destroy,
	lwaodWinMM_Start, lwaodWinMM_Stop,
	lwaodWinMM_Pause, lwaodWinMM_Resume,
	
	lwaodWinMM_SetCallback, lwaodWinMM_GetBufferSize,
	lwaodWinMM_IsBusy, lwaodWinMM_WriteData,
	
	lwaodWinMM_GetLatency,
};


static LWAO_OPTS defOptions;
static LWAO_DEV_LIST deviceList;
static UINT* devListIDs;

static uint8_t isInit = 0;
static uint32_t activeDrivers;

uint8_t lwaodWinMM_IsAvailable(void)
{
	UINT numDevs;
	
	numDevs = waveOutGetNumDevs();
	return numDevs ? 1 : 0;
}

uint8_t lwaodWinMM_Init(void)
{
	UINT numDevs;
	WAVEOUTCAPSA woCaps;
	uint32_t curDev;
	uint32_t devLstID;
	MMRESULT retValMM;
	
	if (isInit)
		return LWAO_ERR_WASDONE;
	
	numDevs = waveOutGetNumDevs();
	deviceList.devCount = 1 + numDevs;
	deviceList.devNames = (char**)malloc(deviceList.devCount * sizeof(char*));
	devListIDs = (UINT*)malloc(deviceList.devCount * sizeof(UINT));
	devLstID = 0;
	
	retValMM = waveOutGetDevCapsA(WAVE_MAPPER, &woCaps, sizeof(WAVEOUTCAPSA));
	if (retValMM == MMSYSERR_NOERROR)
	{
		devListIDs[devLstID] = WAVE_MAPPER;
		deviceList.devNames[devLstID] = strdup(woCaps.szPname);
		devLstID ++;
	}
	for (curDev = 0; curDev < numDevs; curDev ++)
	{
		retValMM = waveOutGetDevCapsA(curDev, &woCaps, sizeof(WAVEOUTCAPSA));
		if (retValMM == MMSYSERR_NOERROR)
		{
			devListIDs[devLstID] = curDev;
			deviceList.devNames[devLstID] = strdup(woCaps.szPname);
			devLstID ++;
		}
	}
	deviceList.devCount = devLstID;
	
	
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

uint8_t lwaodWinMM_Deinit(void)
{
	uint32_t curDev;
	
	if (! isInit)
		return LWAO_ERR_WASDONE;
	
	for (curDev = 0; curDev < deviceList.devCount; curDev ++)
		free(deviceList.devNames[curDev]);
	deviceList.devCount = 0;
	free(deviceList.devNames);	deviceList.devNames = NULL;
	free(devListIDs);			devListIDs = NULL;
	
	isInit = 0;
	
	return LWAO_ERR_OK;
}

const LWAO_DEV_LIST* lwaodWinMM_GetDeviceList(void)
{
	return &deviceList;
}

LWAO_OPTS* lwaodWinMM_GetDefaultOpts(void)
{
	return &defOptions;
}


uint8_t lwaodWinMM_Create(void** retDrvObj)
{
	DRV_WINMM* drv;
	uint8_t retVal8;
	
	drv = (DRV_WINMM*)malloc(sizeof(DRV_WINMM));
	drv->devState = 0;
	drv->hWaveOut = NULL;
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
		lwaodWinMM_Destroy(drv);
		*retDrvObj = NULL;
		return LWAO_ERR_API_ERR;
	}
	*retDrvObj = drv;
	
	return LWAO_ERR_OK;
}

uint8_t lwaodWinMM_Destroy(void* drvObj)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	
	if (drv->devState != 0)
		lwaodWinMM_Stop(drvObj);
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

uint8_t lwaodWinMM_Start(void* drvObj, uint32_t deviceID, LWAO_OPTS* options, void* audDrvParam)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	uint64_t tempInt64;
	UINT woDevID;
	uint32_t curBuf;
	WAVEHDR* tempWavHdr;
	MMRESULT retValMM;
	uint8_t retVal8;
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
	
	tempInt64 = (uint64_t)options->sampleRate * options->usecPerBuf;
	drv->bufSmpls = (uint32_t)((tempInt64 + 500000) / 1000000);
	drv->bufSize = drv->waveFmt.nBlockAlign * drv->bufSmpls;
	drv->bufCount = options->numBuffers ? options->numBuffers : 10;
	
	woDevID = devListIDs[deviceID];
	retValMM = waveOutOpen(&drv->hWaveOut, woDevID, &drv->waveFmt, 0x00, 0x00, CALLBACK_NULL);
	if (retValMM != MMSYSERR_NOERROR)
		return 0xC0;		// waveOutOpen failed
	
	lwauSignal_Reset(drv->hSignal);
	retVal8 = lwauThread_Init(&drv->hThread, &WaveOutThread, drv);
	if (retVal8)
	{
		retValMM = waveOutClose(drv->hWaveOut);
		drv->hWaveOut = NULL;
		return 0xC8;	// CreateThread failed
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
	
	drv->waveHdrs = (WAVEHDR*)malloc(drv->bufCount * sizeof(WAVEHDR));
	drv->bufSpace = (uint8_t*)malloc(drv->bufCount * drv->bufSize);
	for (curBuf = 0; curBuf < drv->bufCount; curBuf ++)
	{
		tempWavHdr = &drv->waveHdrs[curBuf];
		tempWavHdr->lpData = (LPSTR)&drv->bufSpace[drv->bufSize * curBuf];
		tempWavHdr->dwBufferLength = drv->bufSize;
		tempWavHdr->dwBytesRecorded = 0x00;
		tempWavHdr->dwUser = 0x00;
		tempWavHdr->dwFlags = 0x00;
		tempWavHdr->dwLoops = 0x00;
		tempWavHdr->lpNext = NULL;
		tempWavHdr->reserved = 0x00;
		retValMM = waveOutPrepareHeader(drv->hWaveOut, tempWavHdr, sizeof(WAVEHDR));
		tempWavHdr->dwFlags |= WHDR_DONE;
	}
	drv->BlocksSent = 0;
	drv->BlocksPlayed = 0;
	
	drv->devState = 1;
	lwauSignal_Signal(drv->hSignal);
	
	return LWAO_ERR_OK;
}

uint8_t lwaodWinMM_Stop(void* drvObj)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	uint32_t curBuf;
	MMRESULT retValMM;
	
	if (drv->devState != 1)
		return 0xD8;	// is already stopped (or stopping)
	
	drv->devState = 2;
	
	lwauThread_Join(drv->hThread);
	lwauThread_Deinit(drv->hThread);	drv->hThread = NULL;
	
	retValMM = waveOutReset(drv->hWaveOut);
	for (curBuf = 0; curBuf < drv->bufCount; curBuf ++)
		retValMM = waveOutUnprepareHeader(drv->hWaveOut, &drv->waveHdrs[curBuf], sizeof(WAVEHDR));
	free(drv->bufSpace);	drv->bufSpace = NULL;
	free(drv->waveHdrs);	drv->waveHdrs = NULL;
	
	retValMM = waveOutClose(drv->hWaveOut);
	if (retValMM != MMSYSERR_NOERROR)
		return 0xC4;		// waveOutClose failed -- but why ???
	drv->hWaveOut = NULL;
	drv->devState = 0;
	
	return LWAO_ERR_OK;
}

uint8_t lwaodWinMM_Pause(void* drvObj)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	MMRESULT retValMM;
	
	if (drv->hWaveOut == NULL)
		return 0xFF;
	
	retValMM = waveOutPause(drv->hWaveOut);
	return (retValMM == MMSYSERR_NOERROR) ? LWAO_ERR_OK : 0xFF;
}

uint8_t lwaodWinMM_Resume(void* drvObj)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	MMRESULT retValMM;
	
	if (drv->hWaveOut == NULL)
		return 0xFF;
	
	retValMM = waveOutRestart(drv->hWaveOut);
	return (retValMM == MMSYSERR_NOERROR) ? LWAO_ERR_OK : 0xFF;
}


uint8_t lwaodWinMM_SetCallback(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	
	lwauMutex_Lock(drv->hMutex);
	drv->userParam = userParam;
	drv->FillBuffer = FillBufCallback;
	lwauMutex_Unlock(drv->hMutex);
	
	return LWAO_ERR_OK;
}

uint32_t lwaodWinMM_GetBufferSize(void* drvObj)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	
	return drv->bufSize;
}

uint8_t lwaodWinMM_IsBusy(void* drvObj)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	uint32_t curBuf;
	
	if (drv->FillBuffer != NULL)
		return LWAO_ERR_BAD_MODE;
	
	for (curBuf = 0; curBuf < drv->bufCount; curBuf ++)
	{
		if (drv->waveHdrs[curBuf].dwFlags & WHDR_DONE)
			return LWAO_ERR_OK;
	}
	return LWAO_ERR_BUSY;
}

uint8_t lwaodWinMM_WriteData(void* drvObj, uint32_t dataSize, void* data)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	uint32_t curBuf;
	WAVEHDR* tempWavHdr;
	
	if (dataSize > drv->bufSize)
		return LWAO_ERR_TOO_MUCH_DATA;
	
	lwauMutex_Lock(drv->hMutex);
	while(1)
	{
		for (curBuf = 0; curBuf < drv->bufCount; curBuf ++)
		{
			tempWavHdr = &drv->waveHdrs[curBuf];
			if (tempWavHdr->dwFlags & WHDR_DONE)
			{
				memcpy(tempWavHdr->lpData, data, dataSize);
				tempWavHdr->dwBufferLength = dataSize;
				
				WriteBuffer(drv, tempWavHdr);
				lwauMutex_Unlock(drv->hMutex);
				return LWAO_ERR_OK;
			}
		}
	}
	lwauMutex_Unlock(drv->hMutex);
	return LWAO_ERR_BUSY;
}


uint32_t lwaodWinMM_GetLatency(void* drvObj)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	uint32_t bufBehind;
	uint32_t smplsBehind;
	
	BufCheck(drv);
	bufBehind = drv->BlocksSent - drv->BlocksPlayed;
	smplsBehind = drv->bufSmpls * bufBehind;
	return smplsBehind * 1000 / drv->waveFmt.nSamplesPerSec;
}

static void LWA_API WaveOutThread(void* Arg)
{
	DRV_WINMM* drv = (DRV_WINMM*)Arg;
	uint32_t curBuf;
	uint32_t didBuffers;	// number of processed buffers
	WAVEHDR* tempWavHdr;
	
	lwauSignal_Wait(drv->hSignal);	// wait until the initialization is done
	
	while(drv->devState == 1)
	{
		didBuffers = 0;
		lwauMutex_Lock(drv->hMutex);
		for (curBuf = 0; curBuf < drv->bufCount; curBuf ++)
		{
			tempWavHdr = &drv->waveHdrs[curBuf];
			if (tempWavHdr->dwFlags & WHDR_DONE)
			{
				if (drv->FillBuffer == NULL)
					break;
				tempWavHdr->dwBufferLength = drv->FillBuffer(drv->audDrvPtr, drv->userParam,
															drv->bufSize, tempWavHdr->lpData);
				WriteBuffer(drv, tempWavHdr);
				didBuffers ++;
			}
			if (drv->devState > 1)
				break;
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

static void WriteBuffer(DRV_WINMM* drv, WAVEHDR* wHdr)
{
	if (wHdr->dwUser & 0x01)
		drv->BlocksPlayed ++;
	else
		wHdr->dwUser |= 0x01;
	
	waveOutWrite(drv->hWaveOut, wHdr, sizeof(WAVEHDR));
	drv->BlocksSent ++;
	
	return;
}


static void BufCheck(DRV_WINMM* drv)
{
	uint32_t curBuf;
	
	for (curBuf = 0; curBuf < drv->bufCount; curBuf ++)
	{
		if (drv->waveHdrs[curBuf].dwFlags & WHDR_DONE)
		{
			if (drv->waveHdrs[curBuf].dwUser & 0x01)
			{
				drv->waveHdrs[curBuf].dwUser &= ~0x01;
				drv->BlocksPlayed ++;
			}
		}
	}
	
	return;
}
