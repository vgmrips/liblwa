// Audio Stream - API functions
#define _CRTDBG_MAP_ALLOC
//#include <stdio.h>
#include <stdlib.h>
//#include <string.h>	// for memset

#include "../lwa_types.h"

#include "lwao.h"
#include "../utils/lwauMutex.h"


#ifdef LWAO_DRIVER_WAVEWRITE
extern LWAO_DRIVER lwaoDrv_WaveWrt;
#endif

#ifdef LWAO_DRIVER_WINMM
extern LWAO_DRIVER lwaoDrv_WinMM;
#endif
#ifdef LWAO_DRIVER_DSOUND
extern LWAO_DRIVER lwaoDrv_DSound;
#endif
#ifdef LWAO_DRIVER_XAUD2
extern LWAO_DRIVER lwaoDrv_XAudio2;
#endif
#ifdef LWAO_DRIVER_WASAPI
extern LWAO_DRIVER lwaoDrv_WASAPI;
#endif

#ifdef LWAO_DRIVER_OSS
extern LWAO_DRIVER lwaoDrv_OSS;
#endif
#ifdef LWAO_DRIVER_SADA
extern LWAO_DRIVER lwaoDrv_SADA;
#endif
#ifdef LWAO_DRIVER_ALSA
extern LWAO_DRIVER lwaoDrv_ALSA;
#endif
#ifdef LWAO_DRIVER_LIBAO
extern LWAO_DRIVER lwaoDrv_LibAO;
#endif
#ifdef LWAO_DRIVER_PULSE
extern LWAO_DRIVER lwaoDrv_Pulse;
#endif

#ifdef LWAO_DRIVER_CA
extern LWAO_DRIVER lwaoDrv_CA;
#endif

LWAO_DRIVER* audDrivers[] =
{
#ifdef LWAO_DRIVER_WAVEWRITE
	&lwaoDrv_WaveWrt,
#endif
#ifdef LWAO_DRIVER_WINMM
	&lwaoDrv_WinMM,
#endif
#ifdef LWAO_DRIVER_DSOUND
	&lwaoDrv_DSound,
#endif
#ifdef LWAO_DRIVER_XAUD2
	&lwaoDrv_XAudio2,
#endif
#ifdef LWAO_DRIVER_WASAPI
	&lwaoDrv_WASAPI,
#endif
#ifdef LWAO_DRIVER_OSS
	&lwaoDrv_OSS,
#endif
#ifdef LWAO_DRIVER_SADA
	&lwaoDrv_SADA,
#endif
#ifdef LWAO_DRIVER_ALSA
	&lwaoDrv_ALSA,
#endif
#ifdef LWAO_DRIVER_LIBAO
	&lwaoDrv_LibAO,
#endif
#ifdef LWAO_DRIVER_PULSE
	&lwaoDrv_Pulse,
#endif
#ifdef LWAO_DRIVER_CA
	&lwaoDrv_CA,
#endif
	NULL
};


typedef struct _audio_driver_load
{
	uint8_t flags;
	LWAO_DRIVER* drvStruct;
} LWAO_D_LOAD;

typedef struct _audio_driver_instance LWAO_D_INSTANCE;
struct _audio_driver_instance
{
	uint32_t ID;	// -1 = unused
	LWAO_DRIVER* drvStruct;
	LWAO_OPTS drvOpts;
	void* drvData;
};

#define ADFLG_ENABLE	0x01
#define ADFLG_IS_INIT	0x02
#define ADFLG_AVAILABLE	0x80

#define ADID_UNUSED		(uint32_t)-1


//uint8_t lwaoInit(void);
//uint8_t lwaoDeinit(void);
//uint32_t lwaoGetDriverCount(void);
//uint8_t lwaoGetDriverInfo(uint32_t drvID, LWAO_DINFO** retDrvInfo);
static LWAO_D_INSTANCE* GetFreeRunSlot(void);
//uint8_t lwaodInit(uint32_t drvID, void** retDrvStruct);
//uint8_t lwaodDeinit(void** drvStruct);
//const LWAO_DEV_LIST* lwaodGetDeviceList(void* drvStruct);
//LWAO_OPTS* lwaodGetOptions(void* drvStruct);
//uint8_t lwaodStart(void* drvStruct, uint32_t devID);
//uint8_t lwaodStop(void* drvStruct);
//uint8_t lwaodPause(void* drvStruct);
//uint8_t lwaodResume(void* drvStruct);
//uint8_t lwaodSetCallback(void* drvStruct, LWAOFUNC_FILLBUF FillBufCallback, void* userParam);
//uint32_t lwaodGetBufferSize(void* drvStruct);
//uint8_t lwaodIsBusy(void* drvStruct);
//uint8_t lwaodWriteData(void* drvStruct, uint32_t dataSize, void* data);
//uint32_t lwaodGetLatency(void* drvStruct);


static uint32_t audDrvCount = 0;
static LWAO_D_LOAD* audDrvLoaded = NULL;

static uint32_t runDevCount = 0;
static LWAO_D_INSTANCE* runDevices = NULL;

static uint8_t isInit = 0;


uint8_t LWA_API lwaoInit(void)
{
	uint32_t curDrv;
	uint32_t curDrvLd;
	LWAO_DRIVER* tempDrv;
	LWAO_D_LOAD* tempDLoad;
	
	if (isInit)
		return LWAO_ERR_WASDONE;
	
	audDrvCount = 0;
	while(audDrivers[audDrvCount] != NULL)
		audDrvCount ++;
	audDrvLoaded = (LWAO_D_LOAD*)malloc(audDrvCount * sizeof(LWAO_D_LOAD));
	
	curDrvLd = 0;
	for (curDrv = 0; curDrv < audDrvCount; curDrv ++)
	{
		tempDrv = audDrivers[curDrv];
		tempDLoad = &audDrvLoaded[curDrvLd];
		tempDLoad->drvStruct = tempDrv;
		if (tempDrv->IsAvailable())
			tempDLoad->flags = ADFLG_ENABLE | ADFLG_AVAILABLE;
		else
			tempDLoad->flags = 0x00;
		curDrvLd ++;
	}
	
	audDrvCount = curDrvLd;
	if (! audDrvCount)
	{
		free(audDrvLoaded);	audDrvLoaded = NULL;
		return LWAO_ERR_NODRVS;
	}
	
	//runDevCount = 0;
	//runDevices = NULL;
	runDevCount = 0x10;
	runDevices = (LWAO_D_INSTANCE*)realloc(runDevices, runDevCount * sizeof(LWAO_D_INSTANCE));
	for (curDrv = 0; curDrv < runDevCount; curDrv ++)
		runDevices[curDrv].ID = -1;
	
	isInit = 1;
	return LWAO_ERR_OK;
}

uint8_t LWA_API lwaoDeinit(void)
{
	uint32_t curDev;
	LWAO_D_INSTANCE* tempAIns;
	
	if (! isInit)
		return LWAO_ERR_WASDONE;
	
	for (curDev = 0; curDev < runDevCount; curDev ++)
	{
		tempAIns = &runDevices[curDev];
		if (tempAIns->ID != ADID_UNUSED)
		{
			tempAIns->drvStruct->Stop(tempAIns->drvData);
			tempAIns->drvStruct->Destroy(tempAIns->drvData);
		}
	}
	for (curDev = 0; curDev < audDrvCount; curDev ++)
	{
		if (audDrvLoaded[curDev].flags & ADFLG_IS_INIT)
			audDrvLoaded[curDev].drvStruct->Deinit();
	}
	
	free(runDevices);	runDevices = NULL;
	runDevCount = 0;
	free(audDrvLoaded);	audDrvLoaded = NULL;
	audDrvCount = 0;
	
	isInit = 0;
	return LWAO_ERR_OK;
}

uint32_t LWA_API lwaoGetDriverCount(void)
{
	return audDrvCount;
}

uint8_t LWA_API lwaoGetDriverInfo(uint32_t drvID, LWAO_DINFO** retDrvInfo)
{
	if (drvID >= audDrvCount)
		return LWAO_ERR_INVALID_DRV;
	
	if (retDrvInfo != NULL)
		*retDrvInfo = &audDrvLoaded[drvID].drvStruct->drvInfo;
	return LWAO_ERR_OK;
}

static LWAO_D_INSTANCE* GetFreeRunSlot(void)
{
	uint32_t curDev;
	
	for (curDev = 0; curDev < runDevCount; curDev ++)
	{
		if (runDevices[curDev].ID == ADID_UNUSED)
		{
			runDevices[curDev].ID = curDev;
			return &runDevices[curDev];
		}
	}
	return NULL;
	
	// Can't do realloc due to external use of references!!
	/*curDev = runDevCount;
	runDevCount ++;
	runDevices = (LWAO_D_INSTANCE*)realloc(runDevices, runDevCount * sizeof(LWAO_D_INSTANCE));
	runDevices[curDev].ID = curDev;
	return &runDevices[curDev];*/
}

uint8_t LWA_API lwaodInit(uint32_t drvID, void** retDrvStruct)
{
	LWAO_D_LOAD* tempDLoad;
	LWAO_DRIVER* tempDrv;
	LWAO_D_INSTANCE* audInst;
	void* drvData;
	uint8_t retVal;
	
	if (drvID >= audDrvCount)
		return LWAO_ERR_INVALID_DRV;
	tempDLoad = &audDrvLoaded[drvID];
	if (! (tempDLoad->flags & ADFLG_ENABLE))
		return LWAO_ERR_DRV_DISABLED;
	
	tempDrv = tempDLoad->drvStruct;
	if (! (tempDLoad->flags & ADFLG_IS_INIT))
	{
		retVal = tempDrv->Init();
		if (retVal)
			return retVal;
		tempDLoad->flags |= ADFLG_IS_INIT;
	}
	
	retVal = tempDrv->Create(&drvData);
	if (retVal)
		return retVal;
	audInst = GetFreeRunSlot();
	audInst->drvStruct = tempDrv;
	audInst->drvOpts = *tempDrv->GetDefOpts();
	audInst->drvData = drvData;
	*retDrvStruct = (void*)audInst;
	
	return LWAO_ERR_OK;
}

uint8_t LWA_API lwaodDeinit(void** drvStruct)
{
	LWAO_D_INSTANCE* audInst;
	LWAO_DRIVER* aDrv;
	uint8_t retVal;
	
	if (drvStruct == NULL)
		return LWAO_ERR_OK;
	
	audInst = (LWAO_D_INSTANCE*)(*drvStruct);
	aDrv = audInst->drvStruct;
	if (audInst->ID == ADID_UNUSED)
		return LWAO_ERR_INVALID_DRV;
	
	retVal = aDrv->Stop(audInst->drvData);	// just in case
	// continue regardless of errors
	retVal = aDrv->Destroy(audInst->drvData);
	if (retVal)
		return retVal;
	
	*drvStruct = NULL;
	audInst->ID = ADID_UNUSED;
	audInst->drvStruct = NULL;
	audInst->drvData = NULL;
	return LWAO_ERR_OK;
}

const LWAO_DEV_LIST* LWA_API lwaodGetDeviceList(void* drvStruct)
{
	LWAO_D_INSTANCE* audInst = (LWAO_D_INSTANCE*)drvStruct;
	LWAO_DRIVER* aDrv = audInst->drvStruct;
	
	return aDrv->GetDevList();
}

LWAO_OPTS* LWA_API lwaodGetOptions(void* drvStruct)
{
	LWAO_D_INSTANCE* audInst = (LWAO_D_INSTANCE*)drvStruct;
	
	return &audInst->drvOpts;
}

void* LWA_API lwaodGetDrvData(void* drvStruct)
{
	LWAO_D_INSTANCE* audInst = (LWAO_D_INSTANCE*)drvStruct;
	
	if (audInst == NULL || audInst->ID == ADID_UNUSED)
		return NULL;
	
	return audInst->drvData;
}

uint8_t LWA_API lwaodStart(void* drvStruct, uint32_t devID)
{
	LWAO_D_INSTANCE* audInst = (LWAO_D_INSTANCE*)drvStruct;
	LWAO_DRIVER* aDrv = audInst->drvStruct;
	uint8_t retVal;
	
	retVal = aDrv->Start(audInst->drvData, devID, &audInst->drvOpts, audInst);
	if (retVal)
		return retVal;
	
	return LWAO_ERR_OK;
}

uint8_t LWA_API lwaodStop(void* drvStruct)
{
	LWAO_D_INSTANCE* audInst = (LWAO_D_INSTANCE*)drvStruct;
	LWAO_DRIVER* aDrv = audInst->drvStruct;
	uint8_t retVal;
	
	retVal = aDrv->Stop(audInst->drvData);
	if (retVal)
		return retVal;
	
	return LWAO_ERR_OK;
}

uint8_t LWA_API lwaodPause(void* drvStruct)
{
	LWAO_D_INSTANCE* audInst = (LWAO_D_INSTANCE*)drvStruct;
	LWAO_DRIVER* aDrv = audInst->drvStruct;
	
	return aDrv->Pause(audInst->drvData);
}

uint8_t LWA_API lwaodResume(void* drvStruct)
{
	LWAO_D_INSTANCE* audInst = (LWAO_D_INSTANCE*)drvStruct;
	LWAO_DRIVER* aDrv = audInst->drvStruct;
	
	return aDrv->Resume(audInst->drvData);
}

uint8_t LWA_API lwaodSetCallback(void* drvStruct, LWAOFUNC_FILLBUF FillBufCallback, void* userParam)
{
	LWAO_D_INSTANCE* audInst = (LWAO_D_INSTANCE*)drvStruct;
	LWAO_DRIVER* aDrv = audInst->drvStruct;
	
	return aDrv->SetCallback(audInst->drvData, FillBufCallback, userParam);
}

uint32_t LWA_API lwaodGetBufferSize(void* drvStruct)
{
	LWAO_D_INSTANCE* audInst = (LWAO_D_INSTANCE*)drvStruct;
	LWAO_DRIVER* aDrv = audInst->drvStruct;
	
	return aDrv->GetBufferSize(audInst->drvData);
}

uint8_t LWA_API lwaodIsBusy(void* drvStruct)
{
	LWAO_D_INSTANCE* audInst = (LWAO_D_INSTANCE*)drvStruct;
	LWAO_DRIVER* aDrv = audInst->drvStruct;
	
	return aDrv->IsBusy(audInst->drvData);
}

uint8_t LWA_API lwaodWriteData(void* drvStruct, uint32_t dataSize, void* data)
{
	LWAO_D_INSTANCE* audInst = (LWAO_D_INSTANCE*)drvStruct;
	LWAO_DRIVER* aDrv = audInst->drvStruct;
	
	return aDrv->WriteData(audInst->drvData, dataSize, data);
}

uint32_t LWA_API lwaodGetLatency(void* drvStruct)
{
	LWAO_D_INSTANCE* audInst = (LWAO_D_INSTANCE*)drvStruct;
	LWAO_DRIVER* aDrv = audInst->drvStruct;
	
	return aDrv->GetLatency(audInst->drvData);
}
