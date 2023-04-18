// Audio Stream - Advanced Linux Sound Architecture
//	- libasound

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <alsa/asoundlib.h>

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


typedef struct _alsa_driver
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
	snd_pcm_t* hPCM;
	volatile UINT8 pauseThread;
	UINT8 canPause;
	
	void* userParam;
	LWAOFUNC_FILLBUF FillBuffer;
} DRV_ALSA;


UINT8 lwaodALSA_IsAvailable(void);
UINT8 lwaodALSA_Init(void);
UINT8 lwaodALSA_Deinit(void);
const LWAO_DEV_LIST* lwaodALSA_GetDeviceList(void);
LWAO_OPTS* lwaodALSA_GetDefaultOpts(void);

UINT8 lwaodALSA_Create(void** retDrvObj);
UINT8 lwaodALSA_Destroy(void* drvObj);
UINT8 lwaodALSA_Start(void* drvObj, UINT32 deviceID, LWAO_OPTS* options, void* audDrvParam);
UINT8 lwaodALSA_Stop(void* drvObj);
UINT8 lwaodALSA_Pause(void* drvObj);
UINT8 lwaodALSA_Resume(void* drvObj);

UINT8 lwaodALSA_SetCallback(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam);
UINT32 lwaodALSA_GetBufferSize(void* drvObj);
UINT8 lwaodALSA_IsBusy(void* drvObj);
UINT8 lwaodALSA_WriteData(void* drvObj, UINT32 dataSize, void* data);

UINT32 lwaodALSA_GetLatency(void* drvObj);
static void LWA_API AlsaThread(void* Arg);
static UINT8 WriteBuffer(DRV_ALSA* drv, UINT32 dataSize, void* data);


LWAO_DRIVER lwaoDrv_ALSA =
{
	{LWAO_DTYPE_OUT, LWAO_DSIG_ALSA, "ALSA"},
	
	lwaodALSA_IsAvailable,
	lwaodALSA_Init, lwaodALSA_Deinit,
	lwaodALSA_GetDeviceList, lwaodALSA_GetDefaultOpts,
	
	lwaodALSA_Create, lwaodALSA_Destroy,
	lwaodALSA_Start, lwaodALSA_Stop,
	lwaodALSA_Pause, lwaodALSA_Resume,
	
	lwaodALSA_SetCallback, lwaodALSA_GetBufferSize,
	lwaodALSA_IsBusy, lwaodALSA_WriteData,
	
	lwaodALSA_GetLatency,
};


static char* alsaDevNames[1] = {"default"};
static LWAO_OPTS defOptions;
static LWAO_DEV_LIST deviceList;

static UINT8 isInit = 0;
static UINT32 activeDrivers;

UINT8 lwaodALSA_IsAvailable(void)
{
	return 1;
}

UINT8 lwaodALSA_Init(void)
{
	UINT32 numDevs;
	UINT32 curDev;
	UINT32 devLstID;
	
	if (isInit)
		return LWAO_ERR_WASDONE;
	
	// TODO: Make device list (like aplay -l)
	// This will need a LUT for deviceID -> hw:num1,num2.
	deviceList.devCount = 1;
	deviceList.devNames = alsaDevNames;
	
	
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

UINT8 lwaodALSA_Deinit(void)
{
	UINT32 curDev;
	
	if (! isInit)
		return LWAO_ERR_WASDONE;
	
	deviceList.devCount = 0;
	deviceList.devNames = NULL;
	
	isInit = 0;
	
	return LWAO_ERR_OK;
}

const LWAO_DEV_LIST* lwaodALSA_GetDeviceList(void)
{
	return &deviceList;
}

LWAO_OPTS* lwaodALSA_GetDefaultOpts(void)
{
	return &defOptions;
}


UINT8 lwaodALSA_Create(void** retDrvObj)
{
	DRV_ALSA* drv;
	UINT8 retVal8;
	
	drv = (DRV_ALSA*)malloc(sizeof(DRV_ALSA));
	drv->devState = 0;
	drv->hPCM = NULL;
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
		lwaodALSA_Destroy(drv);
		*retDrvObj = NULL;
		return LWAO_ERR_API_ERR;
	}
	*retDrvObj = drv;
	
	return LWAO_ERR_OK;
}

UINT8 lwaodALSA_Destroy(void* drvObj)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	
	if (drv->devState != 0)
		lwaodALSA_Stop(drvObj);
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

UINT8 lwaodALSA_Start(void* drvObj, UINT32 deviceID, LWAO_OPTS* options, void* audDrvParam)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	UINT64 tempInt64;
	int retVal;
	UINT8 retVal8;
	snd_pcm_hw_params_t* sndParams;
	snd_pcm_format_t sndPcmFmt;
	snd_pcm_uframes_t periodSize;
	snd_pcm_uframes_t bufferSize;
	snd_pcm_uframes_t oldBufSize;
	int rateDir = 0;
	
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
	//drv->bufSize = drv->waveFmt.nBlockAlign * drv->bufSmpls;
	drv->bufCount = options->numBuffers ? options->numBuffers : 10;
	
	if (drv->waveFmt.wBitsPerSample == 8)
		sndPcmFmt = SND_PCM_FORMAT_U8;
	else if (drv->waveFmt.wBitsPerSample == 16)
		sndPcmFmt = SND_PCM_FORMAT_S16;
	else if (drv->waveFmt.wBitsPerSample == 24)
		sndPcmFmt = SND_PCM_FORMAT_S24;
	else if (drv->waveFmt.wBitsPerSample == 32)
		sndPcmFmt = SND_PCM_FORMAT_S32;
	else
		return 0xCF;
	
	retVal = snd_pcm_open(&drv->hPCM, alsaDevNames[0], SND_PCM_STREAM_PLAYBACK, 0x00);
	if (retVal < 0)
		return 0xC0;		// snd_pcm_open() failed
	
	snd_pcm_hw_params_alloca(&sndParams);
	snd_pcm_hw_params_any(drv->hPCM, sndParams);
	
	snd_pcm_hw_params_set_access(drv->hPCM, sndParams, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(drv->hPCM, sndParams, sndPcmFmt);
	snd_pcm_hw_params_set_channels(drv->hPCM, sndParams, drv->waveFmt.nChannels);
	snd_pcm_hw_params_set_rate_near(drv->hPCM, sndParams, &drv->waveFmt.nSamplesPerSec, &rateDir);
	if (rateDir)
		drv->waveFmt.nAvgBytesPerSec = drv->waveFmt.nSamplesPerSec * drv->waveFmt.nBlockAlign;
	
	periodSize = drv->bufSmpls;
	bufferSize = drv->bufSmpls * drv->bufCount;
	oldBufSize = bufferSize;
	//printf("Wanted buffers: segment %u, count %u, total %u\n", drv->bufSmpls, drv->bufCount, oldBufSize);
	snd_pcm_hw_params_set_period_size_near(drv->hPCM, sndParams, &periodSize, &rateDir);
	if (rateDir)
	{
		drv->bufSmpls = periodSize;
		//printf("Fixed segment %u\n", drv->bufSmpls);
	}
	snd_pcm_hw_params_set_buffer_size_near(drv->hPCM, sndParams, &bufferSize);
	if (bufferSize != oldBufSize)
	{
		drv->bufCount = (bufferSize + periodSize / 2) / periodSize;
		//printf("fix count %u, total %u (%u)\n", drv->bufCount, bufferSize, drv->bufCount * periodSize);
	}
	
	drv->canPause = (UINT8)snd_pcm_hw_params_can_pause(sndParams);
	
	retVal = snd_pcm_hw_params(drv->hPCM, sndParams);
	// disabled for now, as it sometimes causes a double-free crash
	//snd_pcm_hw_params_free(sndParams);
	if (retVal < 0)
	{
		snd_pcm_close(drv->hPCM);	drv->hPCM = NULL;
		return 0xCF;
	}
	
	lwauSignal_Reset(drv->hSignal);
	retVal8 = lwauThread_Init(&drv->hThread, &AlsaThread, drv);
	if (retVal8)
	{
		snd_pcm_close(drv->hPCM);	drv->hPCM = NULL;
		return 0xC8;	// CreateThread failed
	}
	
	drv->bufSize = drv->waveFmt.nBlockAlign * drv->bufSmpls;
	drv->bufSpace = (UINT8*)malloc(drv->bufSize);
	
	drv->devState = 1;
	drv->pauseThread = 0x00;
	lwauSignal_Signal(drv->hSignal);
	
	return LWAO_ERR_OK;
}

UINT8 lwaodALSA_Stop(void* drvObj)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	int retVal;
	
	if (drv->devState != 1)
		return 0xD8;	// is already stopped (or stopping)
	
	drv->devState = 2;
	
	lwauThread_Join(drv->hThread);
	lwauThread_Deinit(drv->hThread);	drv->hThread = NULL;
	
	if (drv->canPause)
		retVal = snd_pcm_pause(drv->hPCM, 0);
	retVal = snd_pcm_drain(drv->hPCM);
	
	free(drv->bufSpace);	drv->bufSpace = NULL;
	
	retVal = snd_pcm_close(drv->hPCM);
	if (retVal < 0)
		return 0xC4;		// close failed -- but why ???
	drv->hPCM = NULL;
	
	drv->devState = 0;
	
	return LWAO_ERR_OK;
}

UINT8 lwaodALSA_Pause(void* drvObj)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	int retVal;
	
	if (drv->hPCM == NULL)
		return 0xFF;
	
	if (drv->canPause)
	{
		retVal = snd_pcm_pause(drv->hPCM, 1);
		if (retVal < 0)
			return LWAO_ERR_API_ERR;
	}
	drv->pauseThread |= 0x01;
	return LWAO_ERR_OK;
}

UINT8 lwaodALSA_Resume(void* drvObj)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	int retVal;
	
	if (drv->hPCM == NULL)
		return 0xFF;
	
	if (drv->canPause)
	{
		retVal = snd_pcm_pause(drv->hPCM, 0);
		if (retVal < 0)
			return LWAO_ERR_API_ERR;
	}
	drv->pauseThread &= ~0x01;
	return LWAO_ERR_OK;
}


UINT8 lwaodALSA_SetCallback(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	
	drv->pauseThread |= 0x02;
	lwauMutex_Lock(drv->hMutex);
	drv->userParam = userParam;
	drv->FillBuffer = FillBufCallback;
	drv->pauseThread &= ~0x02;
	lwauMutex_Unlock(drv->hMutex);
	
	return LWAO_ERR_OK;
}

UINT32 lwaodALSA_GetBufferSize(void* drvObj)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	
	return drv->bufSize;
}

UINT8 lwaodALSA_IsBusy(void* drvObj)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	snd_pcm_sframes_t frmCount;
	
	if (drv->FillBuffer != NULL)
		return LWAO_ERR_BAD_MODE;
	
	frmCount = snd_pcm_avail_update(drv->hPCM);
	return (frmCount > 0) ? LWAO_ERR_OK : LWAO_ERR_BUSY;
}

UINT8 lwaodALSA_WriteData(void* drvObj, UINT32 dataSize, void* data)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	UINT8 retVal;
	
	if (dataSize > drv->bufSize)
		return LWAO_ERR_TOO_MUCH_DATA;
	
	lwauMutex_Lock(drv->hMutex);
	retVal = WriteBuffer(drv, dataSize, data);
	lwauMutex_Unlock(drv->hMutex);
	return retVal;
}


UINT32 lwaodALSA_GetLatency(void* drvObj)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	int retVal;
	snd_pcm_sframes_t smplsBehind;
	
	retVal = snd_pcm_delay(drv->hPCM, &smplsBehind);
	if (retVal < 0)
		return 0;
	return smplsBehind * 1000 / drv->waveFmt.nSamplesPerSec;
}

static void LWA_API AlsaThread(void* Arg)
{
	DRV_ALSA* drv = (DRV_ALSA*)Arg;
	UINT32 bufBytes;
	int retVal;
	
	lwauSignal_Wait(drv->hSignal);	// wait until the initialization is done
	
	while(drv->devState == 1)
	{
		// return values: 0 - timeout, 1 - ready, <0 - error
		retVal = snd_pcm_wait(drv->hPCM, 5);
		if (retVal < 0)
			Sleep(1);	// only wait a bit on errors
		
		lwauMutex_Lock(drv->hMutex);
		if (! drv->pauseThread && drv->FillBuffer != NULL)
		{
			// Note: On errors I try to send some data in order to call recovery functions.
			if (retVal != 0)
			{
				bufBytes = drv->FillBuffer(drv->audDrvPtr, drv->userParam, drv->bufSize, drv->bufSpace);
				retVal = WriteBuffer(drv, bufBytes, drv->bufSpace);
			}
		}
		lwauMutex_Unlock(drv->hMutex);
		
		while((drv->pauseThread || drv->FillBuffer == NULL) && drv->devState == 1)
			Sleep(1);
	}
	
	return;
}

static UINT8 WriteBuffer(DRV_ALSA* drv, UINT32 dataSize, void* data)
{
	int retVal;
	
	do
	{
		retVal = snd_pcm_writei(drv->hPCM, data, dataSize / drv->waveFmt.nBlockAlign);
	} while (retVal == -EAGAIN);
	if (retVal == -ESTRPIPE)
	{
		retVal = snd_pcm_resume(drv->hPCM);
		while(retVal == -EAGAIN)
		{
			Sleep(1);
			retVal = snd_pcm_resume(drv->hPCM);
		}
		if (retVal < 0)
			retVal = -EPIPE;
	}
	if (retVal == -EPIPE)
	{
		// buffer underrun
		snd_pcm_prepare(drv->hPCM);
	}
	
	return LWAO_ERR_OK;
}
