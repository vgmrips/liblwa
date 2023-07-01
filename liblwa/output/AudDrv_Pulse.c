// Audio Stream - PulseAudio
//	- libpulse-simple

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <pulse/simple.h>

#include <unistd.h>		// for usleep()
#define	Sleep(msec)	usleep(msec * 1000)

#include "../stdtype.h"

#include "lwao.h"
#include "../utils/lwauThread.h"
#include "../utils/lwauSignal.h"
#include "../utils/lwauMutex.h"


typedef struct _pulse_driver
{
	void* audDrvPtr;
	volatile UINT8 devState;	// 0 - not running, 1 - running, 2 - terminating
	
	pa_sample_spec pulseFmt;
	UINT32 bufSmpls;
	UINT32 bufSize;
	UINT32 bufCount;
	UINT8* bufSpace;
	
	LWAU_THREAD* hThread;
	LWAU_SIGNAL* hSignal;
	LWAU_MUTEX* hMutex;
	pa_simple* hPulse;
	volatile UINT8 pauseThread;
	UINT8 canPause;
	char* streamDesc;
	
	void* userParam;
	LWAOFUNC_FILLBUF FillBuffer;
} DRV_PULSE;


UINT8 lwaodPulse_IsAvailable(void);
UINT8 lwaodPulse_Init(void);
UINT8 lwaodPulse_Deinit(void);
const LWAO_DEV_LIST* lwaodPulse_GetDeviceList(void);
LWAO_OPTS* lwaodPulse_GetDefaultOpts(void);

UINT8 lwaodPulse_Create(void** retDrvObj);
UINT8 lwaodPulse_Destroy(void* drvObj);
UINT8 lwaodPulse_Start(void* drvObj, UINT32 deviceID, LWAO_OPTS* options, void* audDrvParam);
UINT8 lwaodPulse_Stop(void* drvObj);
UINT8 lwaodPulse_Pause(void* drvObj);
UINT8 lwaodPulse_Resume(void* drvObj);

UINT8 lwaodPulse_SetCallback(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam);
UINT32 lwaodPulse_GetBufferSize(void* drvObj);
UINT8 lwaodPulse_IsBusy(void* drvObj);
UINT8 lwaodPulse_WriteData(void* drvObj, UINT32 dataSize, void* data);

LWAO_EXPORT UINT8 lwaodPulse_SetStreamDesc(void* drvObj, const char* fileName);
LWAO_EXPORT const char* lwaodPulse_GetStreamDesc(void* drvObj);
UINT32 lwaodPulse_GetLatency(void* drvObj);
static void LWA_API PulseThread(void* Arg);


LWAO_DRIVER lwaoDrv_Pulse =
{
	{LWAO_DTYPE_OUT, LWAO_DSIG_PULSE, "PulseAudio"},
	
	lwaodPulse_IsAvailable,
	lwaodPulse_Init, lwaodPulse_Deinit,
	lwaodPulse_GetDeviceList, lwaodPulse_GetDefaultOpts,
	
	lwaodPulse_Create, lwaodPulse_Destroy,
	lwaodPulse_Start, lwaodPulse_Stop,
	lwaodPulse_Pause, lwaodPulse_Resume,
	
	lwaodPulse_SetCallback, lwaodPulse_GetBufferSize,
	lwaodPulse_IsBusy, lwaodPulse_WriteData,
	
	lwaodPulse_GetLatency,
};


static char* PulseDevNames[1] = {"default"};
static LWAO_OPTS defOptions;
static LWAO_DEV_LIST deviceList;

static UINT8 isInit = 0;
static UINT32 activeDrivers;

UINT8 lwaodPulse_IsAvailable(void)
{
	return 1;
}

UINT8 lwaodPulse_Init(void)
{
	if (isInit)
		return LWAO_ERR_WASDONE;
	
	deviceList.devCount = 1;
	deviceList.devNames = PulseDevNames;
	
	
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

UINT8 lwaodPulse_Deinit(void)
{
	if (! isInit)
		return LWAO_ERR_WASDONE;
	
	deviceList.devCount = 0;
	deviceList.devNames = NULL;
	
	isInit = 0;
	
	return LWAO_ERR_OK;
}

const LWAO_DEV_LIST* lwaodPulse_GetDeviceList(void)
{
	return &deviceList;
}

LWAO_OPTS* lwaodPulse_GetDefaultOpts(void)
{
	return &defOptions;
}


UINT8 lwaodPulse_Create(void** retDrvObj)
{
	DRV_PULSE* drv;
	UINT8 retVal8;
	
	drv = (DRV_PULSE*)malloc(sizeof(DRV_PULSE));
	drv->devState = 0;
	drv->hPulse = NULL;
	drv->hThread = NULL;
	drv->hSignal = NULL;
	drv->hMutex = NULL;
	drv->userParam = NULL;
	drv->FillBuffer = NULL;
	drv->streamDesc = strdup("libvgm");
	
	activeDrivers ++;
	retVal8  = lwauSignal_Init(&drv->hSignal, 0);
	retVal8 |= lwauMutex_Init(&drv->hMutex, 0);
	if (retVal8)
	{
		lwaodPulse_Destroy(drv);
		*retDrvObj = NULL;
		return LWAO_ERR_API_ERR;
	}
	*retDrvObj = drv;
	
	return LWAO_ERR_OK;
}

UINT8 lwaodPulse_Destroy(void* drvObj)
{
	DRV_PULSE* drv = (DRV_PULSE*)drvObj;
	
	if (drv->devState != 0)
		lwaodPulse_Stop(drvObj);
	if (drv->hThread != NULL)
	{
		lwauThread_Cancel(drv->hThread);
		lwauThread_Deinit(drv->hThread);
	}
	if (drv->hSignal != NULL)
		lwauSignal_Deinit(drv->hSignal);
	if (drv->hMutex != NULL)
		lwauMutex_Deinit(drv->hMutex);
	
	free(drv->streamDesc);
	free(drv);
	activeDrivers --;
	
	return LWAO_ERR_OK;
}

UINT8 lwaodPulse_SetStreamDesc(void* drvObj, const char* streamDesc)
{
	DRV_PULSE* drv = (DRV_PULSE*)drvObj;
	
	//The Simple API does not accept updates to the stream description
	if (drv->hPulse)
		return LWAO_ERR_WASDONE;
	
	free(drv->streamDesc);
	drv->streamDesc = strdup(streamDesc);
	
	return LWAO_ERR_OK;
}

const char* lwaodPulse_GetStreamDesc(void* drvObj)
{
	DRV_PULSE* drv = (DRV_PULSE*)drvObj;
	
	return drv->streamDesc;
}

UINT8 lwaodPulse_Start(void* drvObj, UINT32 deviceID, LWAO_OPTS* options, void* audDrvParam)
{
	DRV_PULSE* drv = (DRV_PULSE*)drvObj;
	UINT64 tempInt64;
	UINT8 retVal8;
	
	if (drv->devState != 0)
		return 0xD0;	// already running
	
	drv->audDrvPtr = audDrvParam;
	if (options == NULL)
		options = &defOptions;
	drv->pulseFmt.channels = options->numChannels;
	drv->pulseFmt.rate = options->sampleRate;
	
	tempInt64 = (UINT64)options->sampleRate * options->usecPerBuf;
	drv->bufSmpls = (UINT32)((tempInt64 + 500000) / 1000000);
	drv->bufSize = (options->numBitsPerSmpl * drv->pulseFmt.channels / 8) * drv->bufSmpls;
	drv->bufCount = options->numBuffers ? options->numBuffers : 10;
	
	if (options->numBitsPerSmpl == 8)
		drv->pulseFmt.format = PA_SAMPLE_U8;
	else if (options->numBitsPerSmpl == 16)
		drv->pulseFmt.format = PA_SAMPLE_S16NE;
	else if (options->numBitsPerSmpl == 24)
		drv->pulseFmt.format = PA_SAMPLE_S24NE;
	else if (options->numBitsPerSmpl == 32)
		drv->pulseFmt.format = PA_SAMPLE_S32NE;
	else
		return 0xCF;
	
	drv->canPause = 1;
	
	drv->hPulse = pa_simple_new(NULL, "libvgm", PA_STREAM_PLAYBACK, NULL, drv->streamDesc, &drv->pulseFmt, NULL, NULL, NULL);
	if(!drv->hPulse)
		return 0xC0;
	
	lwauSignal_Reset(drv->hSignal);
	retVal8 = lwauThread_Init(&drv->hThread, &PulseThread, drv);
	if (retVal8)
	{
		pa_simple_free(drv->hPulse);
		return 0xC8;	// CreateThread failed
	}
	
	drv->bufSpace = (UINT8*)malloc(drv->bufSize);
	
	drv->devState = 1;
	drv->pauseThread = 0x00;
	lwauSignal_Signal(drv->hSignal);
	
	return LWAO_ERR_OK;
}

UINT8 lwaodPulse_Stop(void* drvObj)
{
	DRV_PULSE* drv = (DRV_PULSE*)drvObj;
	
	if (drv->devState != 1)
		return 0xD8;	// is already stopped (or stopping)
	
	drv->devState = 2;
	
	lwauThread_Join(drv->hThread);
	lwauThread_Deinit(drv->hThread);	drv->hThread = NULL;
	
	free(drv->bufSpace);
	drv->bufSpace = NULL;
	
	pa_simple_free(drv->hPulse);
	
	drv->devState = 0;
	
	return LWAO_ERR_OK;
}

UINT8 lwaodPulse_Pause(void* drvObj)
{
	DRV_PULSE* drv = (DRV_PULSE*)drvObj;
	
	drv->pauseThread |= 0x01;
	return LWAO_ERR_OK;
}

UINT8 lwaodPulse_Resume(void* drvObj)
{
	DRV_PULSE* drv = (DRV_PULSE*)drvObj;
	
	drv->pauseThread &= ~0x01;
	return LWAO_ERR_OK;
}

UINT8 lwaodPulse_SetCallback(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam)
{
	DRV_PULSE* drv = (DRV_PULSE*)drvObj;
	
	drv->pauseThread |= 0x02;
	lwauMutex_Lock(drv->hMutex);
	drv->userParam = userParam;
	drv->FillBuffer = FillBufCallback;
	drv->pauseThread &= ~0x02;
	lwauMutex_Unlock(drv->hMutex);
	
	return LWAO_ERR_OK;
}

UINT32 lwaodPulse_GetBufferSize(void* drvObj)
{
	DRV_PULSE* drv = (DRV_PULSE*)drvObj;
	
	return drv->bufSize;
}

UINT8 lwaodPulse_IsBusy(void* drvObj)
{
	DRV_PULSE* drv = (DRV_PULSE*)drvObj;
	
	if (drv->FillBuffer != NULL)
		return LWAO_ERR_BAD_MODE;
	
	return LWAO_ERR_OK;
}

UINT8 lwaodPulse_WriteData(void* drvObj, UINT32 dataSize, void* data)
{
	DRV_PULSE* drv = (DRV_PULSE*)drvObj;
	int retVal;
	
	if (dataSize > drv->bufSize)
		return LWAO_ERR_TOO_MUCH_DATA;
	
	retVal = pa_simple_write(drv->hPulse, data, (size_t) dataSize, NULL);
	if (retVal > 0)
		return 0xFF;
	
	return LWAO_ERR_OK;
}


UINT32 lwaodPulse_GetLatency(void* drvObj)
{
	DRV_PULSE* drv = (DRV_PULSE*)drvObj;
	
	return (UINT32)(pa_simple_get_latency(drv->hPulse, NULL) / 1000);
}

static void LWA_API PulseThread(void* Arg)
{
	DRV_PULSE* drv = (DRV_PULSE*)Arg;
	UINT32 didBuffers;	// number of processed buffers
	UINT32 bufBytes;
	int retVal;
	
	lwauSignal_Wait(drv->hSignal);	// wait until the initialization is done
	
	while(drv->devState == 1)
	{
		didBuffers = 0;
		lwauMutex_Lock(drv->hMutex);
		if (! drv->pauseThread && drv->FillBuffer != NULL)
		{
			bufBytes = drv->FillBuffer(drv->audDrvPtr, drv->userParam, drv->bufSize, drv->bufSpace);
			retVal = pa_simple_write(drv->hPulse, drv->bufSpace, (size_t) bufBytes, NULL);
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
