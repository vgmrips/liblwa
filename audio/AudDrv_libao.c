// Audio Stream - libao
//	- libao

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <ao/ao.h>

#include <unistd.h>		// for usleep()
#define	Sleep(msec)	usleep(msec * 1000)

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


typedef struct _libao_driver
{
	void* audDrvPtr;
	volatile UINT8 devState;	// 0 - not running, 1 - running, 2 - terminating
	
	WAVEFORMAT waveFmt;
	UINT32 bufSmpls;
	UINT32 bufSize;
	UINT32 bufCount;
	UINT8* bufSpace;
	
	LWAU_THREAD* hThread;
	LWAU_SIGNAL* hSignal;
	LWAU_MUTEX* hMutex;
	ao_device* hDevAO;
	volatile UINT8 pauseThread;
	
	void* userParam;
	LWAOFUNC_FILLBUF FillBuffer;
	ao_sample_format aoParams;
} DRV_AO;


UINT8 LibAO_IsAvailable(void);
UINT8 LibAO_Init(void);
UINT8 LibAO_Deinit(void);
const LWAO_DEV_LIST* LibAO_GetDeviceList(void);
LWAO_OPTS* LibAO_GetDefaultOpts(void);

UINT8 LibAO_Create(void** retDrvObj);
UINT8 LibAO_Destroy(void* drvObj);
UINT8 LibAO_Start(void* drvObj, UINT32 deviceID, LWAO_OPTS* options, void* audDrvParam);
UINT8 LibAO_Stop(void* drvObj);
UINT8 LibAO_Pause(void* drvObj);
UINT8 LibAO_Resume(void* drvObj);

UINT8 LibAO_SetCallback(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam);
UINT32 LibAO_GetBufferSize(void* drvObj);
UINT8 LibAO_IsBusy(void* drvObj);
UINT8 LibAO_WriteData(void* drvObj, UINT32 dataSize, void* data);

UINT32 LibAO_GetLatency(void* drvObj);
static void AoThread(void* Arg);


LWAO_DRIVER lwaoDrv_LibAO =
{
	{LWAO_DTYPE_OUT, LWAO_DSIG_LIBAO, "libao"},
	
	LibAO_IsAvailable,
	LibAO_Init, LibAO_Deinit,
	LibAO_GetDeviceList, LibAO_GetDefaultOpts,
	
	LibAO_Create, LibAO_Destroy,
	LibAO_Start, LibAO_Stop,
	LibAO_Pause, LibAO_Resume,
	
	LibAO_SetCallback, LibAO_GetBufferSize,
	LibAO_IsBusy, LibAO_WriteData,
	
	LibAO_GetLatency,
};


static char* aoDevNames[1] = {"default"};
static LWAO_OPTS defOptions;
static LWAO_DEV_LIST deviceList;

static UINT8 isInit = 0;
static UINT32 activeDrivers;

UINT8 LibAO_IsAvailable(void)
{
	return 1;
}

UINT8 LibAO_Init(void)
{
	// TODO: generate a list of all libao drivers
	//UINT32 numDevs;
	//UINT32 curDev;
	//UINT32 devLstID;
	
	if (isInit)
		return LWAO_ERR_WASDONE;
	
	ao_initialize();
	deviceList.devCount = 1;
	deviceList.devNames = aoDevNames;
	
	
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

UINT8 LibAO_Deinit(void)
{
	//UINT32 curDev;
	
	if (! isInit)
		return LWAO_ERR_WASDONE;
	
	deviceList.devCount = 0;
	deviceList.devNames = NULL;
	
	ao_shutdown();
	
	isInit = 0;
	
	return LWAO_ERR_OK;
}

const LWAO_DEV_LIST* LibAO_GetDeviceList(void)
{
	return &deviceList;
}

LWAO_OPTS* LibAO_GetDefaultOpts(void)
{
	return &defOptions;
}


UINT8 LibAO_Create(void** retDrvObj)
{
	DRV_AO* drv;
	UINT8 retVal8;
	
	drv = (DRV_AO*)malloc(sizeof(DRV_AO));
	drv->devState = 0;
	drv->hDevAO = NULL;
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
		LibAO_Destroy(drv);
		*retDrvObj = NULL;
		return LWAO_ERR_API_ERR;
	}
	*retDrvObj = drv;
	
	return LWAO_ERR_OK;
}

UINT8 LibAO_Destroy(void* drvObj)
{
	DRV_AO* drv = (DRV_AO*)drvObj;
	
	if (drv->devState != 0)
		LibAO_Stop(drvObj);
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

UINT8 LibAO_Start(void* drvObj, UINT32 deviceID, LWAO_OPTS* options, void* audDrvParam)
{
	DRV_AO* drv = (DRV_AO*)drvObj;
	UINT64 tempInt64;
	int retVal;
	UINT8 retVal8;
	
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
	
	drv->aoParams.bits = drv->waveFmt.wBitsPerSample;
	drv->aoParams.rate = drv->waveFmt.nSamplesPerSec;
	drv->aoParams.channels = drv->waveFmt.nChannels;
	drv->aoParams.byte_format = AO_FMT_NATIVE;
	drv->aoParams.matrix = NULL;
	
	drv->hDevAO = ao_open_live(ao_default_driver_id(), &drv->aoParams, NULL);
	if (drv->hDevAO == NULL)
		return 0xC0;		// open() failed
	
	lwauSignal_Reset(drv->hSignal);
	retVal8 = lwauThread_Init(&drv->hThread, &AoThread, drv);
	if (retVal8)
	{
		retVal = ao_close(drv->hDevAO);
		drv->hDevAO = NULL;
		return 0xC8;	// CreateThread failed
	}
	
	drv->bufSpace = (UINT8*)malloc(drv->bufSize);
	
	drv->devState = 1;
	drv->pauseThread = 0x00;
	lwauSignal_Signal(drv->hSignal);
	
	return LWAO_ERR_OK;
}

UINT8 LibAO_Stop(void* drvObj)
{
	DRV_AO* drv = (DRV_AO*)drvObj;
	int retVal;
	
	if (drv->devState != 1)
		return 0xD8;	// is already stopped (or stopping)
	
	drv->devState = 2;
	
	lwauThread_Join(drv->hThread);
	lwauThread_Deinit(drv->hThread);	drv->hThread = NULL;
	
	free(drv->bufSpace);	drv->bufSpace = NULL;
	
	retVal = ao_close(drv->hDevAO);
	if (! retVal)
		return 0xC4;		// close failed -- but why ???
	drv->hDevAO = NULL;
	drv->devState = 0;
	
	return LWAO_ERR_OK;
}

UINT8 LibAO_Pause(void* drvObj)
{
	DRV_AO* drv = (DRV_AO*)drvObj;
	
	if (drv->hDevAO == NULL)
		return 0xFF;
	
	drv->pauseThread |= 0x01;
	return LWAO_ERR_OK;
}

UINT8 LibAO_Resume(void* drvObj)
{
	DRV_AO* drv = (DRV_AO*)drvObj;
	
	if (drv->hDevAO == NULL)
		return 0xFF;
	
	drv->pauseThread &= ~0x01;
	return LWAO_ERR_OK;
}


UINT8 LibAO_SetCallback(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam)
{
	DRV_AO* drv = (DRV_AO*)drvObj;
	
	drv->pauseThread |= 0x02;
	lwauMutex_Lock(drv->hMutex);
	drv->userParam = userParam;
	drv->FillBuffer = FillBufCallback;
	drv->pauseThread &= ~0x02;
	lwauMutex_Unlock(drv->hMutex);
	
	return LWAO_ERR_OK;
}

UINT32 LibAO_GetBufferSize(void* drvObj)
{
	DRV_AO* drv = (DRV_AO*)drvObj;
	
	return drv->bufSize;
}

UINT8 LibAO_IsBusy(void* drvObj)
{
	DRV_AO* drv = (DRV_AO*)drvObj;
	
	if (drv->FillBuffer != NULL)
		return LWAO_ERR_BAD_MODE;
	
	//return LWAO_ERR_BUSY;
	return LWAO_ERR_OK;
}

UINT8 LibAO_WriteData(void* drvObj, UINT32 dataSize, void* data)
{
	DRV_AO* drv = (DRV_AO*)drvObj;
	int retVal;
	
	if (dataSize > drv->bufSize)
		return LWAO_ERR_TOO_MUCH_DATA;
	
	retVal = ao_play(drv->hDevAO, data, dataSize);
	if (! retVal)
		return 0xFF;
	return LWAO_ERR_OK;
}


UINT32 LibAO_GetLatency(void* drvObj)
{
	//DRV_AO* drv = (DRV_AO*)drvObj;
	
	return 0;	// There's no API call that lets you receive the current latency.
}

static void AoThread(void* Arg)
{
	DRV_AO* drv = (DRV_AO*)Arg;
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
			retVal = ao_play(drv->hDevAO, (char*)drv->bufSpace, bufBytes);
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
