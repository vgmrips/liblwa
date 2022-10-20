// Audio Stream - Windows Multimedia API WinMM
//	- winmm.lib
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <mmsystem.h>

#include "../stdtype.h"

#include "AudioStream.h"
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
	UINT16 wFormatTag;
	UINT16 nChannels;
	UINT32 nSamplesPerSec;
	UINT32 nAvgBytesPerSec;
	UINT16 nBlockAlign;
	UINT16 wBitsPerSample;
	UINT16 cbSize;
} WAVEFORMATEX;	// from MSDN Help
#pragma pack()

#define WAVE_FORMAT_PCM	0x0001
#endif


typedef struct _winmm_driver
{
	void* audDrvPtr;
	volatile UINT8 devState;	// 0 - not running, 1 - running, 2 - terminating
	UINT16 dummy;	// [for alignment purposes]
	
	WAVEFORMATEX waveFmt;
	UINT32 bufSmpls;
	UINT32 bufSize;
	UINT32 bufCount;
	UINT8* bufSpace;
	
	LWAU_THREAD* hThread;
	LWAU_SIGNAL* hSignal;
	LWAU_MUTEX* hMutex;
	HWAVEOUT hWaveOut;
	WAVEHDR* waveHdrs;
	void* userParam;
	LWAOFUNC_FILLBUF FillBuffer;
	
	UINT32 BlocksSent;
	UINT32 BlocksPlayed;
} DRV_WINMM;


UINT8 lwaodWinMM_IsAvailable(void);
UINT8 lwaodWinMM_Init(void);
UINT8 lwaodWinMM_Deinit(void);
const LWAO_DEV_LIST* lwaodWinMM_GetDeviceList(void);
LWAO_OPTS* lwaodWinMM_GetDefaultOpts(void);

UINT8 lwaodWinMM_Create(void** retDrvObj);
UINT8 lwaodWinMM_Destroy(void* drvObj);
UINT8 lwaodWinMM_Start(void* drvObj, UINT32 deviceID, LWAO_OPTS* options, void* audDrvParam);
UINT8 lwaodWinMM_Stop(void* drvObj);
UINT8 lwaodWinMM_Pause(void* drvObj);
UINT8 lwaodWinMM_Resume(void* drvObj);

UINT8 lwaodWinMM_SetCallback(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam);
UINT32 lwaodWinMM_GetBufferSize(void* drvObj);
UINT8 lwaodWinMM_IsBusy(void* drvObj);
UINT8 lwaodWinMM_WriteData(void* drvObj, UINT32 dataSize, void* data);

UINT32 lwaodWinMM_GetLatency(void* drvObj);
static void WaveOutThread(void* Arg);
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

static UINT8 isInit = 0;
static UINT32 activeDrivers;

UINT8 lwaodWinMM_IsAvailable(void)
{
	UINT32 numDevs;
	
	numDevs = waveOutGetNumDevs();
	return numDevs ? 1 : 0;
}

UINT8 lwaodWinMM_Init(void)
{
	UINT numDevs;
	WAVEOUTCAPSA woCaps;
	UINT32 curDev;
	UINT32 devLstID;
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

UINT8 lwaodWinMM_Deinit(void)
{
	UINT32 curDev;
	
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


UINT8 lwaodWinMM_Create(void** retDrvObj)
{
	DRV_WINMM* drv;
	UINT8 retVal8;
	
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

UINT8 lwaodWinMM_Destroy(void* drvObj)
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

UINT8 lwaodWinMM_Start(void* drvObj, UINT32 deviceID, LWAO_OPTS* options, void* audDrvParam)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	UINT64 tempInt64;
	UINT32 woDevID;
	UINT32 curBuf;
	WAVEHDR* tempWavHdr;
	MMRESULT retValMM;
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
	drv->bufSpace = (UINT8*)malloc(drv->bufCount * drv->bufSize);
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

UINT8 lwaodWinMM_Stop(void* drvObj)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	UINT32 curBuf;
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

UINT8 lwaodWinMM_Pause(void* drvObj)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	MMRESULT retValMM;
	
	if (drv->hWaveOut == NULL)
		return 0xFF;
	
	retValMM = waveOutPause(drv->hWaveOut);
	return (retValMM == MMSYSERR_NOERROR) ? LWAO_ERR_OK : 0xFF;
}

UINT8 lwaodWinMM_Resume(void* drvObj)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	MMRESULT retValMM;
	
	if (drv->hWaveOut == NULL)
		return 0xFF;
	
	retValMM = waveOutRestart(drv->hWaveOut);
	return (retValMM == MMSYSERR_NOERROR) ? LWAO_ERR_OK : 0xFF;
}


UINT8 lwaodWinMM_SetCallback(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	
	lwauMutex_Lock(drv->hMutex);
	drv->userParam = userParam;
	drv->FillBuffer = FillBufCallback;
	lwauMutex_Unlock(drv->hMutex);
	
	return LWAO_ERR_OK;
}

UINT32 lwaodWinMM_GetBufferSize(void* drvObj)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	
	return drv->bufSize;
}

UINT8 lwaodWinMM_IsBusy(void* drvObj)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	UINT32 curBuf;
	
	if (drv->FillBuffer != NULL)
		return LWAO_ERR_BAD_MODE;
	
	for (curBuf = 0; curBuf < drv->bufCount; curBuf ++)
	{
		if (drv->waveHdrs[curBuf].dwFlags & WHDR_DONE)
			return LWAO_ERR_OK;
	}
	return LWAO_ERR_BUSY;
}

UINT8 lwaodWinMM_WriteData(void* drvObj, UINT32 dataSize, void* data)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	UINT32 curBuf;
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


UINT32 lwaodWinMM_GetLatency(void* drvObj)
{
	DRV_WINMM* drv = (DRV_WINMM*)drvObj;
	UINT32 bufBehind;
	UINT32 smplsBehind;
	
	BufCheck(drv);
	bufBehind = drv->BlocksSent - drv->BlocksPlayed;
	smplsBehind = drv->bufSmpls * bufBehind;
	return smplsBehind * 1000 / drv->waveFmt.nSamplesPerSec;
}

static void WaveOutThread(void* Arg)
{
	DRV_WINMM* drv = (DRV_WINMM*)Arg;
	UINT32 curBuf;
	UINT32 didBuffers;	// number of processed buffers
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
	UINT32 curBuf;
	
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
