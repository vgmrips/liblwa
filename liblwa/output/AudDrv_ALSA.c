// Audio Stream - Advanced Linux Sound Architecture
//	- libasound

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <alsa/asoundlib.h>

#include <unistd.h>		// for usleep()
#define	Sleep(msec)	usleep(msec * 1000)

#include "../lwa_types.h"

#include "lwao.h"
#include "../utils/lwauThread.h"
#include "../utils/lwauSignal.h"
#include "../utils/lwauMutex.h"


typedef struct
{
	uint16_t wFormatTag;
	uint16_t nChannels;
	uint32_t nSamplesPerSec;
	uint32_t nAvgBytesPerSec;
	uint16_t nBlockAlign;
	uint16_t wBitsPerSample;
} WAVEFORMAT;	// from MSDN Help

#define WAVE_FORMAT_PCM	0x0001


typedef struct _alsa_driver
{
	void* audDrvPtr;
	volatile uint8_t devState;	// 0 - not running, 1 - running, 2 - terminating
	
	WAVEFORMAT waveFmt;
	uint32_t bufSmpls;
	uint32_t bufSize;
	uint32_t bufCount;
	uint8_t* bufSpace;
	
	LWAU_THREAD* hThread;
	LWAU_SIGNAL* hSignal;
	LWAU_MUTEX* hMutex;
	snd_pcm_t* hPCM;
	volatile uint8_t pauseThread;
	uint8_t canPause;
	
	void* userParam;
	LWAOFUNC_FILLBUF FillBuffer;
} DRV_ALSA;


uint8_t lwaodALSA_IsAvailable(void);
uint8_t lwaodALSA_Init(void);
uint8_t lwaodALSA_Deinit(void);
const LWAO_DEV_LIST* lwaodALSA_GetDeviceList(void);
LWAO_OPTS* lwaodALSA_GetDefaultOpts(void);

uint8_t lwaodALSA_Create(void** retDrvObj);
uint8_t lwaodALSA_Destroy(void* drvObj);
uint8_t lwaodALSA_Start(void* drvObj, uint32_t deviceID, LWAO_OPTS* options, void* audDrvParam);
uint8_t lwaodALSA_Stop(void* drvObj);
uint8_t lwaodALSA_Pause(void* drvObj);
uint8_t lwaodALSA_Resume(void* drvObj);

uint8_t lwaodALSA_SetCallback(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam);
uint32_t lwaodALSA_GetBufferSize(void* drvObj);
uint8_t lwaodALSA_IsBusy(void* drvObj);
uint8_t lwaodALSA_WriteData(void* drvObj, uint32_t dataSize, void* data);

uint32_t lwaodALSA_GetLatency(void* drvObj);
static void LWA_API AlsaThread(void* Arg);
static uint8_t WriteBuffer(DRV_ALSA* drv, uint32_t dataSize, void* data);


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

static uint8_t isInit = 0;
static uint32_t activeDrivers;

uint8_t lwaodALSA_IsAvailable(void)
{
	return 1;
}

uint8_t lwaodALSA_Init(void)
{
	uint32_t numDevs;
	uint32_t curDev;
	uint32_t devLstID;
	
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

uint8_t lwaodALSA_Deinit(void)
{
	uint32_t curDev;
	
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


uint8_t lwaodALSA_Create(void** retDrvObj)
{
	DRV_ALSA* drv;
	uint8_t retVal8;
	
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

uint8_t lwaodALSA_Destroy(void* drvObj)
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

uint8_t lwaodALSA_Start(void* drvObj, uint32_t deviceID, LWAO_OPTS* options, void* audDrvParam)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	uint64_t tempInt64;
	int retVal;
	uint8_t retVal8;
	snd_pcm_hw_params_t* sndParams;
	snd_pcm_format_t sndPcmFmt;
	snd_pcm_uframes_t periodSize;
	snd_pcm_uframes_t bufferSize;
	snd_pcm_uframes_t oldBufSize;
	int rateDir = 0;
	
	if (drv->devState != 0)
		return LWAO_ERR_IS_RUNNING;
	
	drv->audDrvPtr = audDrvParam;
	if (options == NULL)
		options = &defOptions;
	drv->waveFmt.wFormatTag = WAVE_FORMAT_PCM;
	drv->waveFmt.nChannels = options->numChannels;
	drv->waveFmt.nSamplesPerSec = options->sampleRate;
	drv->waveFmt.wBitsPerSample = options->numBitsPerSmpl;
	drv->waveFmt.nBlockAlign = drv->waveFmt.wBitsPerSample * drv->waveFmt.nChannels / 8;
	drv->waveFmt.nAvgBytesPerSec = drv->waveFmt.nSamplesPerSec * drv->waveFmt.nBlockAlign;
	
	tempInt64 = (uint64_t)options->sampleRate * options->usecPerBuf;
	drv->bufSmpls = (uint32_t)((tempInt64 + 500000) / 1000000);
	//drv->bufSize = drv->waveFmt.nBlockAlign * drv->bufSmpls;
	drv->bufCount = options->numBuffers ? options->numBuffers : 10;
	
	if (drv->waveFmt.wBitsPerSample == 8)
		sndPcmFmt = SND_PCM_FORMAT_U8;
	else if (drv->waveFmt.wBitsPerSample == 16)
		sndPcmFmt = SND_PCM_FORMAT_S16;
#if defined(LWA_LITTLE_ENDIAN)
	else if (drv->waveFmt.wBitsPerSample == 24)
		sndPcmFmt = SND_PCM_FORMAT_S24_3LE;
#elif defined(LWA_BIG_ENDIAN)
	else if (drv->waveFmt.wBitsPerSample == 24)
		sndPcmFmt = SND_PCM_FORMAT_S24_3BE;
#endif
	else if (drv->waveFmt.wBitsPerSample == 32)
		sndPcmFmt = SND_PCM_FORMAT_S32;
	else
		return LWAO_ERR_BAD_FORMAT;
	
	retVal = snd_pcm_open(&drv->hPCM, alsaDevNames[0], SND_PCM_STREAM_PLAYBACK, 0x00);
	if (retVal < 0)
		return LWAO_ERR_DEV_OPEN_FAIL;
	
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
	
	drv->canPause = (uint8_t)snd_pcm_hw_params_can_pause(sndParams);
	
	retVal = snd_pcm_hw_params(drv->hPCM, sndParams);
	// disabled for now, as it sometimes causes a double-free crash
	//snd_pcm_hw_params_free(sndParams);
	if (retVal < 0)
	{
		snd_pcm_close(drv->hPCM);	drv->hPCM = NULL;
		return LWAO_ERR_BAD_FORMAT;
	}
	
	lwauSignal_Reset(drv->hSignal);
	retVal8 = lwauThread_Init(&drv->hThread, &AlsaThread, drv);
	if (retVal8)
	{
		snd_pcm_close(drv->hPCM);	drv->hPCM = NULL;
		return LWAO_ERR_THREAD_FAIL;
	}
	
	drv->bufSize = drv->waveFmt.nBlockAlign * drv->bufSmpls;
	drv->bufSpace = (uint8_t*)malloc(drv->bufSize);
	
	drv->devState = 1;
	drv->pauseThread = 0x00;
	lwauSignal_Signal(drv->hSignal);
	
	return LWAO_ERR_OK;
}

uint8_t lwaodALSA_Stop(void* drvObj)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	int retVal;
	
	if (drv->devState != 1)
		return LWAO_ERR_NOT_RUNNING;
	
	drv->devState = 2;
	
	lwauThread_Join(drv->hThread);
	lwauThread_Deinit(drv->hThread);	drv->hThread = NULL;
	
	if (drv->canPause)
		retVal = snd_pcm_pause(drv->hPCM, 0);
	retVal = snd_pcm_drain(drv->hPCM);
	
	free(drv->bufSpace);	drv->bufSpace = NULL;
	
	retVal = snd_pcm_close(drv->hPCM);
	if (retVal < 0)
		return LWAO_ERR_DEV_CLOSE_FAIL;	// close failed -- but why ???
	drv->hPCM = NULL;
	
	drv->devState = 0;
	
	return LWAO_ERR_OK;
}

uint8_t lwaodALSA_Pause(void* drvObj)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	int retVal;
	
	if (drv->hPCM == NULL)
		return LWAO_ERR_NOT_RUNNING;
	
	if (drv->canPause)
	{
		retVal = snd_pcm_pause(drv->hPCM, 1);
		if (retVal < 0)
			return LWAO_ERR_API_ERR;
	}
	drv->pauseThread |= 0x01;
	return LWAO_ERR_OK;
}

uint8_t lwaodALSA_Resume(void* drvObj)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	int retVal;
	
	if (drv->hPCM == NULL)
		return LWAO_ERR_NOT_RUNNING;
	
	if (drv->canPause)
	{
		retVal = snd_pcm_pause(drv->hPCM, 0);
		if (retVal < 0)
			return LWAO_ERR_API_ERR;
	}
	drv->pauseThread &= ~0x01;
	return LWAO_ERR_OK;
}


uint8_t lwaodALSA_SetCallback(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam)
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

uint32_t lwaodALSA_GetBufferSize(void* drvObj)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	
	return drv->bufSize;
}

uint8_t lwaodALSA_IsBusy(void* drvObj)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	snd_pcm_sframes_t frmCount;
	
	if (drv->FillBuffer != NULL)
		return LWAO_ERR_BAD_MODE;
	
	frmCount = snd_pcm_avail_update(drv->hPCM);
	return (frmCount > 0) ? LWAO_ERR_OK : LWAO_ERR_BUSY;
}

uint8_t lwaodALSA_WriteData(void* drvObj, uint32_t dataSize, void* data)
{
	DRV_ALSA* drv = (DRV_ALSA*)drvObj;
	uint8_t retVal;
	
	if (dataSize > drv->bufSize)
		return LWAO_ERR_TOO_MUCH_DATA;
	
	lwauMutex_Lock(drv->hMutex);
	retVal = WriteBuffer(drv, dataSize, data);
	lwauMutex_Unlock(drv->hMutex);
	return retVal;
}


uint32_t lwaodALSA_GetLatency(void* drvObj)
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
	uint32_t bufBytes;
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

static uint8_t WriteBuffer(DRV_ALSA* drv, uint32_t dataSize, void* data)
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
