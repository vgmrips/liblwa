// Audio Stream - Wave Writer
#define _CRTDBG_MAP_ALLOC
#include <stdio.h>
#include <stdlib.h>
#include <string.h>	// for memcpy() etc.

#include "../lwa_types.h"
#include "../lwa_inline.h"

#include "lwao.h"


#ifdef _MSC_VER
#define	strdup	_strdup
#endif

#pragma pack(1)
typedef struct
{
	uint16_t wFormatTag;
	uint16_t nChannels;
	uint32_t nSamplesPerSec;
	uint32_t nAvgBytesPerSec;
	uint16_t nBlockAlign;
	uint16_t wBitsPerSample;
	//uint16_t cbSize;	// not required for WAVE_FORMAT_PCM
} WAVEFORMAT;	// from MSDN Help
#pragma pack()

#define WAVE_FORMAT_PCM	0x0001


typedef struct _wave_writer_driver
{
	void* audDrvPtr;
	uint8_t devState;	// 0 - not running, 1 - running, 2 - terminating
	
	char* fileName;
	FILE* hFile;
	WAVEFORMAT waveFmt;
	
	uint32_t hdrSize;
	uint32_t wrtDataBytes;
} DRV_WAV_WRT;


uint8_t lwaodWavWrt_IsAvailable(void);
uint8_t lwaodWavWrt_Init(void);
uint8_t lwaodWavWrt_Deinit(void);
const LWAO_DEV_LIST* lwaodWavWrt_GetDeviceList(void);
LWAO_OPTS* lwaodWavWrt_GetDefaultOpts(void);

uint8_t lwaodWavWrt_Create(void** retDrvObj);
uint8_t lwaodWavWrt_Destroy(void* drvObj);
LWAO_EXPORT uint8_t lwaodWavWrt_SetFileName(void* drvObj, const char* fileName);
LWAO_EXPORT const char* lwaodWavWrt_GetFileName(void* drvObj);

uint8_t lwaodWavWrt_Start(void* drvObj, uint32_t deviceID, LWAO_OPTS* options, void* audDrvParam);
uint8_t lwaodWavWrt_Stop(void* drvObj);
uint8_t lwaodWavWrt_PauseResume(void* drvObj);

uint8_t lwaodWavWrt_SetCallback(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam);
uint32_t lwaodWavWrt_GetBufferSize(void* drvObj);
uint8_t lwaodWavWrt_IsBusy(void* drvObj);
uint8_t lwaodWavWrt_WriteData(void* drvObj, uint32_t dataSize, void* data);
uint32_t lwaodWavWrt_GetLatency(void* drvObj);

INLINE size_t fputLE16(uint16_t Value, FILE* hFile);
INLINE size_t fputLE32(uint32_t Value, FILE* hFile);
INLINE size_t fputBE32(uint32_t Value, FILE* hFile);


LWAO_DRIVER lwaoDrv_WaveWrt =
{
	{LWAO_DTYPE_DISK, LWAO_DSIG_WAVEWRT, "WaveWrite"},
	
	lwaodWavWrt_IsAvailable,
	lwaodWavWrt_Init, lwaodWavWrt_Deinit,
	lwaodWavWrt_GetDeviceList, lwaodWavWrt_GetDefaultOpts,
	
	lwaodWavWrt_Create, lwaodWavWrt_Destroy,
	lwaodWavWrt_Start, lwaodWavWrt_Stop,
	lwaodWavWrt_PauseResume, lwaodWavWrt_PauseResume,
	
	lwaodWavWrt_SetCallback, lwaodWavWrt_GetBufferSize,
	lwaodWavWrt_IsBusy, lwaodWavWrt_WriteData,
	
	lwaodWavWrt_GetLatency,
};


static char* wavOutDevNames[1] = {"Wave Writer"};
static LWAO_OPTS defOptions;
static LWAO_DEV_LIST deviceList;

static uint8_t isInit = 0;
static uint32_t activeDrivers;

uint8_t lwaodWavWrt_IsAvailable(void)
{
	return 1;
}

uint8_t lwaodWavWrt_Init(void)
{
	if (isInit)
		return LWAO_ERR_WASDONE;
	
	deviceList.devCount = 1;
	deviceList.devNames = wavOutDevNames;
	
	
	memset(&defOptions, 0x00, sizeof(LWAO_OPTS));
	defOptions.sampleRate = 44100;
	defOptions.numChannels = 2;
	defOptions.numBitsPerSmpl = 16;
	defOptions.usecPerBuf = 0;	// the Wave Writer has no buffer
	defOptions.numBuffers = 0;
	
	
	activeDrivers = 0;
	isInit = 1;
	
	return LWAO_ERR_OK;
}

uint8_t lwaodWavWrt_Deinit(void)
{
	if (! isInit)
		return LWAO_ERR_WASDONE;
	
	deviceList.devCount = 0;
	deviceList.devNames = NULL;
	
	isInit = 0;
	
	return LWAO_ERR_OK;
}

const LWAO_DEV_LIST* lwaodWavWrt_GetDeviceList(void)
{
	return &deviceList;
}

LWAO_OPTS* lwaodWavWrt_GetDefaultOpts(void)
{
	return &defOptions;
}


uint8_t lwaodWavWrt_Create(void** retDrvObj)
{
	DRV_WAV_WRT* drv;
	
	drv = (DRV_WAV_WRT*)malloc(sizeof(DRV_WAV_WRT));
	drv->devState = 0;
	drv->fileName = NULL;
	drv->hFile = NULL;
	
	activeDrivers ++;
	*retDrvObj = drv;
	
	return LWAO_ERR_OK;
}

uint8_t lwaodWavWrt_Destroy(void* drvObj)
{
	DRV_WAV_WRT* drv = (DRV_WAV_WRT*)drvObj;
	
	if (drv->devState != 0)
		lwaodWavWrt_Stop(drvObj);
	
	if (drv->fileName != NULL)
		free(drv->fileName);
	
	free(drv);
	activeDrivers --;
	
	return LWAO_ERR_OK;
}

uint8_t lwaodWavWrt_SetFileName(void* drvObj, const char* fileName)
{
	DRV_WAV_WRT* drv = (DRV_WAV_WRT*)drvObj;
	
	if (drv->fileName != NULL)
		free(drv->fileName);
	drv->fileName = strdup(fileName);
	
	return LWAO_ERR_OK;
}

const char* lwaodWavWrt_GetFileName(void* drvObj)
{
	DRV_WAV_WRT* drv = (DRV_WAV_WRT*)drvObj;
	
	return drv->fileName;
}

uint8_t lwaodWavWrt_Start(void* drvObj, uint32_t deviceID, LWAO_OPTS* options, void* audDrvParam)
{
	DRV_WAV_WRT* drv = (DRV_WAV_WRT*)drvObj;
	uint32_t DataLen;
	
	if (drv->devState != 0)
		return LWAO_ERR_IS_RUNNING;
	if (drv->fileName == NULL)
		return LWAO_ERR_CALL_SPC_FUNC;
	
	drv->audDrvPtr = audDrvParam;
	if (options == NULL)
		options = &defOptions;
	drv->waveFmt.wFormatTag = WAVE_FORMAT_PCM;
	drv->waveFmt.nChannels = options->numChannels;
	drv->waveFmt.nSamplesPerSec = options->sampleRate;
	drv->waveFmt.wBitsPerSample = options->numBitsPerSmpl;
	drv->waveFmt.nBlockAlign = drv->waveFmt.wBitsPerSample * drv->waveFmt.nChannels / 8;
	drv->waveFmt.nAvgBytesPerSec = drv->waveFmt.nSamplesPerSec * drv->waveFmt.nBlockAlign;
	
	
	drv->hFile = fopen(drv->fileName, "wb");
	if (drv->hFile == NULL)
		return LWAO_ERR_DEV_OPEN_FAIL;
	
	drv->hdrSize = 0x00;
	fputBE32(0x52494646, drv->hFile);	// 'RIFF'
	fputLE32(0x00000000, drv->hFile);	// RIFF chunk length (dummy)
	drv->hdrSize += 0x08;
	
	fputBE32(0x57415645, drv->hFile);	// 'WAVE'
	fputBE32(0x666D7420, drv->hFile);	// 'fmt '
	DataLen = sizeof(WAVEFORMAT);
	fputLE32(DataLen, drv->hFile);		// format chunk legth
	
#ifndef VGM_BIG_ENDIAN
	fwrite(&drv->waveFmt, DataLen, 1, drv->hFile);
#else
	fputLE16(drv->waveFmt.wFormatTag,		drv->hFile);	// 0x00
	fputLE16(drv->waveFmt.nChannels,		drv->hFile);	// 0x02
	fputLE32(drv->waveFmt.nSamplesPerSec,	drv->hFile);	// 0x04
	fputLE32(drv->waveFmt.nAvgBytesPerSec,	drv->hFile);	// 0x08
	fputLE16(drv->waveFmt.nBlockAlign,		drv->hFile);	// 0x0C
	fputLE16(drv->waveFmt.wBitsPerSample,	drv->hFile);	// 0x0E
	//fputLE16(drv->waveFmt.cbSize,			drv->hFile);	// 0x10
#endif
	drv->hdrSize += 0x0C + DataLen;
	
	fputBE32(0x64617461, drv->hFile);	// 'data'
	fputLE32(0x00000000, drv->hFile);	// data chunk length (dummy)
	drv->hdrSize += 0x08;
	
	drv->wrtDataBytes = 0x00;
	drv->devState = 1;
	
	return LWAO_ERR_OK;
}

uint8_t lwaodWavWrt_Stop(void* drvObj)
{
	DRV_WAV_WRT* drv = (DRV_WAV_WRT*)drvObj;
	uint32_t riffLen;
	
	if (drv->devState != 1)
		return LWAO_ERR_NOT_RUNNING;
	
	drv->devState = 2;
	
	fseek(drv->hFile, drv->hdrSize - 0x04, SEEK_SET);
	fputLE32(drv->wrtDataBytes, drv->hFile);	// data chunk length
	
	fseek(drv->hFile, 0x0004, SEEK_SET);
	riffLen = drv->wrtDataBytes + drv->hdrSize - 0x08;
	fputLE32(riffLen, drv->hFile);				// RIFF chunk length
	
	fclose(drv->hFile);	drv->hFile = NULL;
	drv->devState = 0;
	
	return LWAO_ERR_OK;
}

uint8_t lwaodWavWrt_PauseResume(void* drvObj)
{
	return LWAO_ERR_NO_SUPPORT;
}


uint8_t lwaodWavWrt_SetCallback(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam)
{
	return LWAO_ERR_NO_SUPPORT;
}

uint32_t lwaodWavWrt_GetBufferSize(void* drvObj)
{
	return 0;
}

uint8_t lwaodWavWrt_IsBusy(void* drvObj)
{
	return LWAO_ERR_OK;
}

uint8_t lwaodWavWrt_WriteData(void* drvObj, uint32_t dataSize, void* data)
{
	DRV_WAV_WRT* drv = (DRV_WAV_WRT*)drvObj;
	uint32_t wrtBytes;
	
	if (drv->hFile == NULL)
		return LWAO_ERR_NOT_RUNNING;
	
	wrtBytes = (uint32_t)fwrite(data, 0x01, dataSize, drv->hFile);
	if (! wrtBytes)
		return LWAO_ERR_WRITE_ERROR;
	drv->wrtDataBytes += wrtBytes;
	
	return LWAO_ERR_OK;
}

uint32_t lwaodWavWrt_GetLatency(void* drvObj)
{
	return 0;
}


INLINE size_t fputLE16(uint16_t Value, FILE* hFile)
{
#ifndef VGM_BIG_ENDIAN
	return fwrite(&Value, 0x02, 1, hFile);
#else
	uint8_t dataArr[0x02];
	
	dataArr[0x00] = (Value & 0x00FF) >>  0;
	dataArr[0x01] = (Value & 0xFF00) >>  8;
	return fwrite(dataArr, 0x02, 1, hFile);
#endif
}

INLINE size_t fputLE32(uint32_t Value, FILE* hFile)
{
#ifndef VGM_BIG_ENDIAN
	return fwrite(&Value, 0x04, 1, hFile);
#else
	uint8_t dataArr[0x04];
	
	dataArr[0x00] = (Value & 0x000000FF) >>  0;
	dataArr[0x01] = (Value & 0x0000FF00) >>  8;
	dataArr[0x02] = (Value & 0x00FF0000) >> 16;
	dataArr[0x03] = (Value & 0xFF000000) >> 24;
	return fwrite(dataArr, 0x04, 1, hFile);
#endif
}

INLINE size_t fputBE32(uint32_t Value, FILE* hFile)
{
#ifndef VGM_BIG_ENDIAN
	uint8_t dataArr[0x04];
	
	dataArr[0x00] = (Value & 0xFF000000) >> 24;
	dataArr[0x01] = (Value & 0x00FF0000) >> 16;
	dataArr[0x02] = (Value & 0x0000FF00) >>  8;
	dataArr[0x03] = (Value & 0x000000FF) >>  0;
	return fwrite(dataArr, 0x04, 1, hFile);
#else
	return fwrite(&Value, 0x04, 1, hFile);
#endif
}
