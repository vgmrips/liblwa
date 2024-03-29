// Audio Stream - Open Sound System
#define ENABLE_OSS_THREAD

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <fcntl.h>	// for open() constants
#include <unistd.h>	// for I/O calls
#include <sys/soundcard.h>	// OSS sound stuff
#include <sys/ioctl.h>

#ifdef ENABLE_OSS_THREAD
#include <unistd.h>		// for usleep()
#define	Sleep(msec)	usleep(msec * 1000)
#endif

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


typedef struct _oss_parameters
{
	int fragment;	// SNDCTL_DSP_SETFRAGMENT
	int format;		// SNDCTL_DSP_SETFMT
	int channels;	// SNDCTL_DSP_CHANNELS
	int smplrate;	// SNDCTL_DSP_SPEED
} lwaodOSS_PARAMS;
typedef struct _oss_driver
{
	void* audDrvPtr;
	volatile uint8_t devState;	// 0 - not running, 1 - running, 2 - terminating
	
	WAVEFORMAT waveFmt;
	uint32_t bufSmpls;
	uint32_t bufSize;
	uint32_t bufSizeBits;
	uint32_t bufCount;
	uint8_t* bufSpace;
	
#ifdef ENABLE_OSS_THREAD
	LWAU_THREAD* hThread;
#endif
	LWAU_SIGNAL* hSignal;
	LWAU_MUTEX* hMutex;
	int hFileDSP;
	volatile uint8_t pauseThread;
	
	void* userParam;
	LWAOFUNC_FILLBUF FillBuffer;
	lwaodOSS_PARAMS ossParams;
} DRV_OSS;


uint8_t lwaodOSS_IsAvailable(void);
uint8_t lwaodOSS_Init(void);
uint8_t lwaodOSS_Deinit(void);
const LWAO_DEV_LIST* lwaodOSS_GetDeviceList(void);
LWAO_OPTS* lwaodOSS_GetDefaultOpts(void);

uint8_t lwaodOSS_Create(void** retDrvObj);
uint8_t lwaodOSS_Destroy(void* drvObj);
static uint32_t GetNearestBitVal(uint32_t value);
uint8_t lwaodOSS_Start(void* drvObj, uint32_t deviceID, LWAO_OPTS* options, void* audDrvParam);
uint8_t lwaodOSS_Stop(void* drvObj);
uint8_t lwaodOSS_Pause(void* drvObj);
uint8_t lwaodOSS_Resume(void* drvObj);

uint8_t lwaodOSS_SetCallback(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam);
uint32_t lwaodOSS_GetBufferSize(void* drvObj);
uint8_t lwaodOSS_IsBusy(void* drvObj);
uint8_t lwaodOSS_WriteData(void* drvObj, uint32_t dataSize, void* data);

uint32_t lwaodOSS_GetLatency(void* drvObj);
static void LWA_API OssThread(void* Arg);


LWAO_DRIVER lwaoDrv_OSS =
{
	{LWAO_DTYPE_OUT, LWAO_DSIG_OSS, "OSS"},
	
	lwaodOSS_IsAvailable,
	lwaodOSS_Init, lwaodOSS_Deinit,
	lwaodOSS_GetDeviceList, lwaodOSS_GetDefaultOpts,
	
	lwaodOSS_Create, lwaodOSS_Destroy,
	lwaodOSS_Start, lwaodOSS_Stop,
	lwaodOSS_Pause, lwaodOSS_Resume,
	
	lwaodOSS_SetCallback, lwaodOSS_GetBufferSize,
	lwaodOSS_IsBusy, lwaodOSS_WriteData,
	
	lwaodOSS_GetLatency,
};


static char* ossDevNames[1] = {"/dev/dsp"};
static LWAO_OPTS defOptions;
static LWAO_DEV_LIST deviceList;

static uint8_t isInit = 0;
static uint32_t activeDrivers;

uint8_t lwaodOSS_IsAvailable(void)
{
	int hFile;
	
	hFile = open(ossDevNames[0], O_WRONLY);
	if (hFile < 0)
		return 0;
	close(hFile);
	
	return 1;
}

uint8_t lwaodOSS_Init(void)
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

uint8_t lwaodOSS_Deinit(void)
{
	if (! isInit)
		return LWAO_ERR_WASDONE;
	
	deviceList.devCount = 0;
	deviceList.devNames = NULL;
	
	isInit = 0;
	
	return LWAO_ERR_OK;
}

const LWAO_DEV_LIST* lwaodOSS_GetDeviceList(void)
{
	return &deviceList;
}

LWAO_OPTS* lwaodOSS_GetDefaultOpts(void)
{
	return &defOptions;
}


uint8_t lwaodOSS_Create(void** retDrvObj)
{
	DRV_OSS* drv;
	uint8_t retVal8;
	
	drv = (DRV_OSS*)malloc(sizeof(DRV_OSS));
	drv->devState = 0;
	drv->hFileDSP = 0;
#ifdef ENABLE_OSS_THREAD
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
		lwaodOSS_Destroy(drv);
		*retDrvObj = NULL;
		return LWAO_ERR_API_ERR;
	}
	*retDrvObj = drv;
	
	return LWAO_ERR_OK;
}

uint8_t lwaodOSS_Destroy(void* drvObj)
{
	DRV_OSS* drv = (DRV_OSS*)drvObj;
	
	if (drv->devState != 0)
		lwaodOSS_Stop(drvObj);
#ifdef ENABLE_OSS_THREAD
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

static uint32_t GetNearestBitVal(uint32_t value)
{
	// Example:
	//	16 -> nearest 16 (1<<4)
	//	23 -> nearest 16 (1<<4)
	//	24 -> nearest 32 (1<<5)
	// So I'll check, if the "nearest?" value is >= original value * 1.5
	uint8_t curBit;
	uint32_t newVal;
	
	value += value / 2;	// value *= 1.5
	for (curBit = 4, newVal = (1 << curBit); curBit < 32; curBit ++, newVal <<= 1)
	{
		if (newVal > value)
			return curBit - 1;
	}
	return 0;
}

uint8_t lwaodOSS_Start(void* drvObj, uint32_t deviceID, LWAO_OPTS* options, void* audDrvParam)
{
	DRV_OSS* drv = (DRV_OSS*)drvObj;
	uint64_t tempInt64;
	int retVal;
#ifdef ENABLE_OSS_THREAD
	uint8_t retVal8;
#endif
	
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
	drv->bufSize = drv->waveFmt.nBlockAlign * drv->bufSmpls;
	//printf("Wanted buffer size: %u\n", drv->bufSize);
	// We have the estimated buffer size now.
	drv->bufSizeBits = GetNearestBitVal(drv->bufSize);
	//Since OSS can only use buffers of the size 2^x, we need to recalculate everything here.
	drv->bufSize = 1 << drv->bufSizeBits;
	//printf("Used buffer size: %u\n", drv->bufSize);
	drv->bufSmpls = drv->bufSize / drv->waveFmt.nBlockAlign;
	drv->bufCount = options->numBuffers ? options->numBuffers : 10;
	
	drv->ossParams.fragment = (drv->bufCount << 16) | (drv->bufSizeBits << 0);
	// Note: While in the OSS documentation, not all systems define the 24 and 32-bit formats.
	if (drv->waveFmt.wBitsPerSample == 8)
		drv->ossParams.format = AFMT_U8;
	else if (drv->waveFmt.wBitsPerSample == 16)
		drv->ossParams.format = AFMT_S16_NE;
#ifdef AFMT_S24_PACKED
	else if (drv->waveFmt.wBitsPerSample == 24)
		drv->ossParams.format = AFMT_S24_PACKED;
#endif
#ifdef AFMT_S32_NE
	else if (drv->waveFmt.wBitsPerSample == 32)
		drv->ossParams.format = AFMT_S32_NE;
#endif
	else
		return LWAO_ERR_BAD_FORMAT;
	drv->ossParams.channels = drv->waveFmt.nChannels;
	drv->ossParams.smplrate = drv->waveFmt.nSamplesPerSec;
	
	drv->hFileDSP = open(ossDevNames[0], O_WRONLY);
	if (drv->hFileDSP < 0)
	{
		drv->hFileDSP = 0;
		return LWAO_ERR_DEV_OPEN_FAIL;
	}
	
	retVal = ioctl(drv->hFileDSP, SNDCTL_DSP_SETFRAGMENT, &drv->ossParams.fragment);
	if (retVal)
		printf("Error setting Fragment Size!\n");
	retVal = ioctl(drv->hFileDSP, SNDCTL_DSP_SETFMT, &drv->ossParams.format);
	if (retVal)
		printf("Error setting Format!\n");
	retVal = ioctl(drv->hFileDSP, SNDCTL_DSP_CHANNELS, &drv->ossParams.channels);
	if (retVal)
		printf("Error setting Channels!\n");
	retVal = ioctl(drv->hFileDSP, SNDCTL_DSP_SPEED, &drv->ossParams.smplrate);
	if (retVal)
		printf("Error setting Sample Rate!\n");
	
	lwauSignal_Reset(drv->hSignal);
#ifdef ENABLE_OSS_THREAD
	retVal8 = lwauThread_Init(&drv->hThread, &OssThread, drv);
	if (retVal8)
	{
		retVal = close(drv->hFileDSP);
		drv->hFileDSP = 0;
		return LWAO_ERR_THREAD_FAIL;
	}
#endif
	
	drv->bufSpace = (uint8_t*)malloc(drv->bufSize);
	
	drv->devState = 1;
	drv->pauseThread = 0x00;
	lwauSignal_Signal(drv->hSignal);
	
	return LWAO_ERR_OK;
}

uint8_t lwaodOSS_Stop(void* drvObj)
{
	DRV_OSS* drv = (DRV_OSS*)drvObj;
	int retVal;
	
	if (drv->devState != 1)
		return LWAO_ERR_NOT_RUNNING;
	
	drv->devState = 2;
	
#ifdef ENABLE_OSS_THREAD
	lwauThread_Join(drv->hThread);
	lwauThread_Deinit(drv->hThread);	drv->hThread = NULL;
#endif
	
	free(drv->bufSpace);	drv->bufSpace = NULL;
	
	retVal = close(drv->hFileDSP);
	if (retVal)
		return LWAO_ERR_DEV_CLOSE_FAIL;	// close failed -- but why ???
	drv->hFileDSP = 0;
	drv->devState = 0;
	
	return LWAO_ERR_OK;
}

uint8_t lwaodOSS_Pause(void* drvObj)
{
	DRV_OSS* drv = (DRV_OSS*)drvObj;
	
	if (! drv->hFileDSP)
		return LWAO_ERR_NOT_RUNNING;
	
	drv->pauseThread |= 0x01;
	return LWAO_ERR_OK;
}

uint8_t lwaodOSS_Resume(void* drvObj)
{
	DRV_OSS* drv = (DRV_OSS*)drvObj;
	
	if (! drv->hFileDSP)
		return LWAO_ERR_NOT_RUNNING;
	
	drv->pauseThread &= ~0x01;
	return LWAO_ERR_OK;
}


uint8_t lwaodOSS_SetCallback(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam)
{
	DRV_OSS* drv = (DRV_OSS*)drvObj;
	
#ifdef ENABLE_OSS_THREAD
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

uint32_t lwaodOSS_GetBufferSize(void* drvObj)
{
	DRV_OSS* drv = (DRV_OSS*)drvObj;
	
	return drv->bufSize;
}

uint8_t lwaodOSS_IsBusy(void* drvObj)
{
	DRV_OSS* drv = (DRV_OSS*)drvObj;
	
	if (drv->FillBuffer != NULL)
		return LWAO_ERR_BAD_MODE;
	
	//return LWAO_ERR_BUSY;
	return LWAO_ERR_OK;
}

uint8_t lwaodOSS_WriteData(void* drvObj, uint32_t dataSize, void* data)
{
	DRV_OSS* drv = (DRV_OSS*)drvObj;
	ssize_t wrtBytes;
	
	if (dataSize > drv->bufSize)
		return LWAO_ERR_TOO_MUCH_DATA;
	
	wrtBytes = write(drv->hFileDSP, data, dataSize);
	if (wrtBytes == -1)
		return LWAO_ERR_WRITE_ERROR;
	return LWAO_ERR_OK;
}


uint32_t lwaodOSS_GetLatency(void* drvObj)
{
	DRV_OSS* drv = (DRV_OSS*)drvObj;
	int retVal;
	int bytesBehind;
	
	retVal = ioctl(drv->hFileDSP, SNDCTL_DSP_GETODELAY, &bytesBehind);
	if (retVal)
		return 0;
	
	return bytesBehind * 1000 / drv->waveFmt.nAvgBytesPerSec;
}

static void LWA_API OssThread(void* Arg)
{
	DRV_OSS* drv = (DRV_OSS*)Arg;
	uint32_t didBuffers;	// number of processed buffers
	uint32_t bufBytes;
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
