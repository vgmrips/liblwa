// Audio Stream - Solaris Audio Device Architecture
#define ENABLE_SADA_THREAD

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>	// for open() constants
#include <unistd.h>	// for I/O calls
#include <sys/audioio.h>

#ifdef ENABLE_SADA_THREAD
#include <unistd.h>		// for usleep()
#define	Sleep(msec)	usleep(msec * 1000)
#endif

#include "../stdtype.h"

#include "AudioStream.h"
#include "../utils/lwauThread.h"
#include "../utils/lwauSignal.h"
#include "../utils/lwauMutex.h"


typedef struct
{
	UINT16 wFormatTag;
	UINT16 nChannels;
	UINT32 nSamplesPerSec;
	UINT32 nAvgBytesPerSec;
	UINT16 nBlockAlign;
	UINT16 wBitsPerSample;
} WAVEFORMAT;	// from MSDN Help

#define WAVE_FORMAT_PCM	0x0001


typedef struct _sada_driver
{
	void* audDrvPtr;
	volatile UINT8 devState;	// 0 - not running, 1 - running, 2 - terminating
	
	WAVEFORMAT waveFmt;
	UINT32 bufSmpls;
	UINT32 bufSize;
	UINT32 bufCount;
	UINT8* bufSpace;
	
#ifdef ENABLE_SADA_THREAD
	LWAU_THREAD* hThread;
#endif
	LWAU_SIGNAL* hSignal;
	LWAU_MUTEX* hMutex;
	int hFileDSP;
	volatile UINT8 pauseThread;
	
	void* userParam;
	LWAOFUNC_FILLBUF FillBuffer;
	struct audio_info sadaParams;
} DRV_SADA;


UINT8 lwaodSADA_IsAvailable(void);
UINT8 lwaodSADA_Init(void);
UINT8 lwaodSADA_Deinit(void);
const LWAO_DEV_LIST* lwaodSADA_GetDeviceList(void);
LWAO_OPTS* lwaodSADA_GetDefaultOpts(void);

UINT8 lwaodSADA_Create(void** retDrvObj);
UINT8 lwaodSADA_Destroy(void* drvObj);
UINT8 lwaodSADA_Start(void* drvObj, UINT32 deviceID, LWAO_OPTS* options, void* audDrvParam);
UINT8 lwaodSADA_Stop(void* drvObj);
UINT8 lwaodSADA_Pause(void* drvObj);
UINT8 lwaodSADA_Resume(void* drvObj);

UINT8 lwaodSADA_SetCallback(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam);
UINT32 lwaodSADA_GetBufferSize(void* drvObj);
UINT8 lwaodSADA_IsBusy(void* drvObj);
UINT8 lwaodSADA_WriteData(void* drvObj, UINT32 dataSize, void* data);

UINT32 lwaodSADA_GetLatency(void* drvObj);
static void SadaThread(void* Arg);


LWAO_DRIVER lwaoDrv_SADA =
{
	{LWAO_DTYPE_OUT, LWAO_DSIG_SADA, "SADA"},
	
	lwaodSADA_IsAvailable,
	lwaodSADA_Init, lwaodSADA_Deinit,
	lwaodSADA_GetDeviceList, lwaodSADA_GetDefaultOpts,
	
	lwaodSADA_Create, lwaodSADA_Destroy,
	lwaodSADA_Start, lwaodSADA_Stop,
	lwaodSADA_Pause, lwaodSADA_Resume,
	
	lwaodSADA_SetCallback, lwaodSADA_GetBufferSize,
	lwaodSADA_IsBusy, lwaodSADA_WriteData,
	
	lwaodSADA_GetLatency,
};


static char* ossDevNames[1] = {"/dev/audio"};
static LWAO_OPTS defOptions;
static LWAO_DEV_LIST deviceList;

static UINT8 isInit = 0;
static UINT32 activeDrivers;

UINT8 lwaodSADA_IsAvailable(void)
{
	int hFile;
	
	hFile = open(ossDevNames[0], O_WRONLY);
	if (hFile < 0)
		return 0;
	close(hFile);
	
	return 1;
}

UINT8 lwaodSADA_Init(void)
{
	if (isInit)
		return LWAO_ERR_WASDONE;
	
	deviceList.devCount = 1;
	deviceList.devNames = ossDevNames;
	
	
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

UINT8 lwaodSADA_Deinit(void)
{
	if (! isInit)
		return LWAO_ERR_WASDONE;
	
	deviceList.devCount = 0;
	deviceList.devNames = NULL;
	
	isInit = 0;
	
	return LWAO_ERR_OK;
}

const LWAO_DEV_LIST* lwaodSADA_GetDeviceList(void)
{
	return &deviceList;
}

LWAO_OPTS* lwaodSADA_GetDefaultOpts(void)
{
	return &defOptions;
}


UINT8 lwaodSADA_Create(void** retDrvObj)
{
	DRV_SADA* drv;
	UINT8 retVal8;
	
	drv = (DRV_SADA*)malloc(sizeof(DRV_SADA));
	drv->devState = 0;
	drv->hFileDSP = 0;
#ifdef ENABLE_SADA_THREAD
	drv->hThread = NULL;
#endif
	drv->hSignal = NULL;
	drv->hMutex = NULL;
	drv->userParam = NULL;
	drv->FillBuffer = NULL;
	
	activeDrivers ++;
	retVal8  = lwauSignal_Init(&drv->hSignal, 0);
	retVal8 |= lwauMutex_Init(&drv->hMutex, 0);
	if (retVal8)
	{
		lwaodSADA_Destroy(drv);
		*retDrvObj = NULL;
		return LWAO_ERR_API_ERR;
	}
	*retDrvObj = drv;
	
	return LWAO_ERR_OK;
}

UINT8 lwaodSADA_Destroy(void* drvObj)
{
	DRV_SADA* drv = (DRV_SADA*)drvObj;
	
	if (drv->devState != 0)
		lwaodSADA_Stop(drvObj);
#ifdef ENABLE_SADA_THREAD
	if (drv->hThread != NULL)
	{
		lwauThread_Cancel(drv->hThread);
		lwauThread_Deinit(drv->hThread);
	}
#endif
	if (drv->hSignal != NULL)
		lwauSignal_Deinit(drv->hSignal);
	if (drv->hMutex != NULL)
		lwauMutex_Deinit(drv->hMutex);
	
	free(drv);
	activeDrivers --;
	
	return LWAO_ERR_OK;
}

UINT8 lwaodSADA_Start(void* drvObj, UINT32 deviceID, LWAO_OPTS* options, void* audDrvParam)
{
	DRV_SADA* drv = (DRV_SADA*)drvObj;
	UINT64 tempInt64;
	int retVal;
#ifdef ENABLE_OSS_THREAD
	UINT8 retVal8;
#endif
	
	if (drv->devState != 0)
		return 0xD0;	// already running
	
	drv->audDrvPtr = audDrvParam;
	if (options == NULL)
		options = &defOptions;
	drv->waveFmt.wFormatTag = WAVE_FORMAT_PCM;
	drv->waveFmt.nChannels = options->numChannels;
	drv->waveFmt.nSamplesPerSec = options->sampleRate;
	drv->waveFmt.wBitsPerSample = options->numBitsPerSmpl;
	drv->waveFmt.nBlockAlign = drv->waveFmt.wBitsPerSample * drv->waveFmt.nChannels / 8;
	drv->waveFmt.nAvgBytesPerSec = drv->waveFmt.nSamplesPerSec * drv->waveFmt.nBlockAlign;
	
	tempInt64 = (UINT64)options->sampleRate * options->usecPerBuf;
	drv->bufSmpls = (UINT32)((tempInt64 + 500000) / 1000000);
	drv->bufSize = drv->waveFmt.nBlockAlign * drv->bufSmpls;
	drv->bufCount = options->numBuffers ? options->numBuffers : 10;
	
	LWAO_INITINFO(&drv->sadaParams);
	drv->sadaParams.mode = AUMODE_PLAY;
	drv->sadaParams.play.sample_rate = drv->waveFmt.nSamplesPerSec;
	drv->sadaParams.play.channels = drv->waveFmt.nChannels;
	drv->sadaParams.play.precision = drv->waveFmt.wBitsPerSample;
	if (drv->waveFmt.wBitsPerSample == 8)
		drv->sadaParams.play.encoding = LWAO_ENCODING_ULINEAR;
	else
		drv->sadaParams.play.encoding = LWAO_ENCODING_SLINEAR;
	
	drv->hFileDSP = open(ossDevNames[0], O_WRONLY);
	if (drv->hFileDSP < 0)
	{
		drv->hFileDSP = 0;
		return 0xC0;		// open() failed
	}
	
	retVal = ioctl(drv->hFileDSP, LWAO_SETINFO, &drv->sadaParams);
	if (retVal)
		printf("Error setting audio information!\n");
	
	lwauSignal_Reset(drv->hSignal);
#ifdef ENABLE_SADA_THREAD
	retVal8 = lwauThread_Init(&drv->hThread, &SadaThread, drv);
	if (retVal8)
	{
		retVal = close(drv->hFileDSP);
		drv->hFileDSP = 0;
		return 0xC8;	// CreateThread failed
	}
#endif
	
	drv->bufSpace = (UINT8*)malloc(drv->bufSize);
	
	drv->devState = 1;
	drv->pauseThread = 0x00;
	lwauSignal_Signal(drv->hSignal);
	
	return LWAO_ERR_OK;
}

UINT8 lwaodSADA_Stop(void* drvObj)
{
	DRV_SADA* drv = (DRV_SADA*)drvObj;
	int retVal;
	
	if (drv->devState != 1)
		return 0xD8;	// is already stopped (or stopping)
	
	drv->devState = 2;
	
#ifdef ENABLE_SADA_THREAD
	lwauThread_Join(drv->hThread);
	lwauThread_Deinit(drv->hThread);	drv->hThread = NULL;
#endif
	
	free(drv->bufSpace);	drv->bufSpace = NULL;
	
	retVal = close(drv->hFileDSP);
	if (retVal)
		return 0xC4;		// close failed -- but why ???
	drv->hFileDSP = 0;
	drv->devState = 0;
	
	return LWAO_ERR_OK;
}

UINT8 lwaodSADA_Pause(void* drvObj)
{
	DRV_SADA* drv = (DRV_SADA*)drvObj;
	
	if (! drv->hFileDSP)
		return 0xFF;
	
	drv->pauseThread |= 0x01;
	return LWAO_ERR_OK;
}

UINT8 lwaodSADA_Resume(void* drvObj)
{
	DRV_SADA* drv = (DRV_SADA*)drvObj;
	
	if (! drv->hFileDSP)
		return 0xFF;
	
	drv->pauseThread &= ~0x01;
	return LWAO_ERR_OK;
}


UINT8 lwaodSADA_SetCallback(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam)
{
	DRV_SADA* drv = (DRV_SADA*)drvObj;
	
#ifdef ENABLE_SADA_THREAD
	drv->pauseThread |= 0x02;
	lwauMutex_Lock(drv->hMutex);
	drv->userParam = userParam;
	drv->FillBuffer = FillBufCallback;
	drv->pauseThread &= ~0x02;
	lwauMutex_Unlock(drv->hMutex);
	
	return LWAO_ERR_OK;
#else
	return LWAO_ERR_NO_SUPPORT;
#endif
}

UINT32 lwaodSADA_GetBufferSize(void* drvObj)
{
	DRV_SADA* drv = (DRV_SADA*)drvObj;
	
	return drv->bufSize;
}

UINT8 lwaodSADA_IsBusy(void* drvObj)
{
	DRV_SADA* drv = (DRV_SADA*)drvObj;
	
	if (drv->FillBuffer != NULL)
		return LWAO_ERR_BAD_MODE;
	
	//return LWAO_ERR_BUSY;
	return LWAO_ERR_OK;
}

UINT8 lwaodSADA_WriteData(void* drvObj, UINT32 dataSize, void* data)
{
	DRV_SADA* drv = (DRV_SADA*)drvObj;
	ssize_t wrtBytes;
	
	if (dataSize > drv->bufSize)
		return LWAO_ERR_TOO_MUCH_DATA;
	
	wrtBytes = write(drv->hFileDSP, data, dataSize);
	if (wrtBytes == -1)
		return 0xFF;
	return LWAO_ERR_OK;
}


UINT32 lwaodSADA_GetLatency(void* drvObj)
{
	DRV_SADA* drv = (DRV_SADA*)drvObj;
	int retVal;
	int bytesBehind;
	
	// TODO: Find out what the Output-Delay call for SADA is.
//	retVal = ioctl(drv->hFileDSP, SNDCTL_DSP_GETODELAY, &bytesBehind);
//	if (retVal)
//		return 0;
	bytesBehind = 0;
	
	return bytesBehind * 1000 / drv->waveFmt.nAvgBytesPerSec;
}

static void SadaThread(void* Arg)
{
	DRV_SADA* drv = (DRV_SADA*)Arg;
	UINT32 didBuffers;	// number of processed buffers
	UINT32 bufBytes;
	ssize_t wrtBytes;
	
	lwauSignal_Wait(drv->hSignal);	// wait until the initialization is done
	
	while(drv->devState == 1)
	{
		didBuffers = 0;
		lwauMutex_Lock(drv->hMutex);
		if (! drv->pauseThread && drv->FillBuffer != NULL)
		{
			bufBytes = drv->FillBuffer(drv->audDrvPtr, drv->userParam, drv->bufSize, drv->bufSpace);
			wrtBytes = write(drv->hFileDSP, drv->bufSpace, bufBytes);
			didBuffers ++;
		}
		lwauMutex_Unlock(drv->hMutex);
		if (! didBuffers)
			Sleep(1);
		
		while(drv->FillBuffer == NULL && drv->devState == 1)
			Sleep(1);
	}
	
	return;
}
