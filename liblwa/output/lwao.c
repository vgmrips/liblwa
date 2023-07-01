// Audio Stream - API functions
#define _CRTDBG_MAP_ALLOC
//#include <stdio.h>
#include <stdlib.h>
//#include <string.h>	// for memset

#include "../stdtype.h"

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
	UINT8 flags;
	LWAO_DRIVER* drvStruct;
} LWAO_D_LOAD;

typedef struct _audio_driver_instance LWAO_D_INSTANCE;
struct _audio_driver_instance
{
	UINT32 ID;	// -1 = unused
	LWAO_DRIVER* drvStruct;
	LWAO_OPTS drvOpts;
	void* drvData;
};

#define ADFLG_ENABLE	0x01
#define ADFLG_IS_INIT	0x02
#define ADFLG_AVAILABLE	0x80

#define ADID_UNUSED		(UINT32)-1


//UINT8 lwaoInit(void);
//UINT8 lwaoDeinit(void);
//UINT32 lwaoGetDriverCount(void);
//UINT8 lwaoGetDriverInfo(UINT32 drvID, LWAO_DINFO** retDrvInfo);
static LWAO_D_INSTANCE* GetFreeRunSlot(void);
//UINT8 lwaodInit(UINT32 drvID, void** retDrvStruct);
//UINT8 lwaodDeinit(void** drvStruct);
//const LWAO_DEV_LIST* lwaodGetDeviceList(void* drvStruct);
//LWAO_OPTS* lwaodGetOptions(void* drvStruct);
//UINT8 lwaodStart(void* drvStruct, UINT32 devID);
//UINT8 lwaodStop(void* drvStruct);
//UINT8 lwaodPause(void* drvStruct);
//UINT8 lwaodResume(void* drvStruct);
//UINT8 lwaodSetCallback(void* drvStruct, LWAOFUNC_FILLBUF FillBufCallback, void* userParam);
//UINT32 lwaodGetBufferSize(void* drvStruct);
//UINT8 lwaodIsBusy(void* drvStruct);
//UINT8 lwaodWriteData(void* drvStruct, UINT32 dataSize, void* data);
//UINT32 lwaodGetLatency(void* drvStruct);


static UINT32 audDrvCount = 0;
static LWAO_D_LOAD* audDrvLoaded = NULL;

static UINT32 runDevCount = 0;
static LWAO_D_INSTANCE* runDevices = NULL;

static UINT8 isInit = 0;


UINT8 LWA_API lwaoInit(void)
{
	UINT32 curDrv;
	UINT32 curDrvLd;
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

UINT8 LWA_API lwaoDeinit(void)
{
	UINT32 curDev;
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

UINT32 LWA_API lwaoGetDriverCount(void)
{
	return audDrvCount;
}

UINT8 LWA_API lwaoGetDriverInfo(UINT32 drvID, LWAO_DINFO** retDrvInfo)
{
	if (drvID >= audDrvCount)
		return LWAO_ERR_INVALID_DRV;
	
	if (retDrvInfo != NULL)
		*retDrvInfo = &audDrvLoaded[drvID].drvStruct->drvInfo;
	return LWAO_ERR_OK;
}

static LWAO_D_INSTANCE* GetFreeRunSlot(void)
{
	UINT32 curDev;
	
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

UINT8 LWA_API lwaodInit(UINT32 drvID, void** retDrvStruct)
{
	LWAO_D_LOAD* tempDLoad;
	LWAO_DRIVER* tempDrv;
	LWAO_D_INSTANCE* audInst;
	void* drvData;
	UINT8 retVal;
	
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

UINT8 LWA_API lwaodDeinit(void** drvStruct)
{
	LWAO_D_INSTANCE* audInst;
	LWAO_DRIVER* aDrv;
	UINT8 retVal;
	
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

UINT8 LWA_API lwaodStart(void* drvStruct, UINT32 devID)
{
	LWAO_D_INSTANCE* audInst = (LWAO_D_INSTANCE*)drvStruct;
	LWAO_DRIVER* aDrv = audInst->drvStruct;
	UINT8 retVal;
	
	retVal = aDrv->Start(audInst->drvData, devID, &audInst->drvOpts, audInst);
	if (retVal)
		return retVal;
	
	return LWAO_ERR_OK;
}

UINT8 LWA_API lwaodStop(void* drvStruct)
{
	LWAO_D_INSTANCE* audInst = (LWAO_D_INSTANCE*)drvStruct;
	LWAO_DRIVER* aDrv = audInst->drvStruct;
	UINT8 retVal;
	
	retVal = aDrv->Stop(audInst->drvData);
	if (retVal)
		return retVal;
	
	return LWAO_ERR_OK;
}

UINT8 LWA_API lwaodPause(void* drvStruct)
{
	LWAO_D_INSTANCE* audInst = (LWAO_D_INSTANCE*)drvStruct;
	LWAO_DRIVER* aDrv = audInst->drvStruct;
	
	return aDrv->Pause(audInst->drvData);
}

UINT8 LWA_API lwaodResume(void* drvStruct)
{
	LWAO_D_INSTANCE* audInst = (LWAO_D_INSTANCE*)drvStruct;
	LWAO_DRIVER* aDrv = audInst->drvStruct;
	
	return aDrv->Resume(audInst->drvData);
}

UINT8 LWA_API lwaodSetCallback(void* drvStruct, LWAOFUNC_FILLBUF FillBufCallback, void* userParam)
{
	LWAO_D_INSTANCE* audInst = (LWAO_D_INSTANCE*)drvStruct;
	LWAO_DRIVER* aDrv = audInst->drvStruct;
	
	return aDrv->SetCallback(audInst->drvData, FillBufCallback, userParam);
}

UINT32 LWA_API lwaodGetBufferSize(void* drvStruct)
{
	LWAO_D_INSTANCE* audInst = (LWAO_D_INSTANCE*)drvStruct;
	LWAO_DRIVER* aDrv = audInst->drvStruct;
	
	return aDrv->GetBufferSize(audInst->drvData);
}

UINT8 LWA_API lwaodIsBusy(void* drvStruct)
{
	LWAO_D_INSTANCE* audInst = (LWAO_D_INSTANCE*)drvStruct;
	LWAO_DRIVER* aDrv = audInst->drvStruct;
	
	return aDrv->IsBusy(audInst->drvData);
}

UINT8 LWA_API lwaodWriteData(void* drvStruct, UINT32 dataSize, void* data)
{
	LWAO_D_INSTANCE* audInst = (LWAO_D_INSTANCE*)drvStruct;
	LWAO_DRIVER* aDrv = audInst->drvStruct;
	
	return aDrv->WriteData(audInst->drvData, dataSize, data);
}

UINT32 LWA_API lwaodGetLatency(void* drvStruct)
{
	LWAO_D_INSTANCE* audInst = (LWAO_D_INSTANCE*)drvStruct;
	LWAO_DRIVER* aDrv = audInst->drvStruct;
	
	return aDrv->GetLatency(audInst->drvData);
}
