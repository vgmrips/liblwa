#ifdef _WIN32
//#define _WIN32_WINNT	0x500	// for GetConsoleWindow()
#include <windows.h>
#ifdef _DEBUG
#include <crtdbg.h>
#endif
#endif

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>

#ifdef _WIN32
int __cdecl _getch(void);	// from conio.h
#else
#define _getch	getchar
#endif

#include <liblwa/lwa_types.h>
#include <liblwa/output/lwao.h>
#include <liblwa/output/lwao_drvfuncs.h>


int main(int argc, char* argv[]);
#ifdef LWAO_DRIVER_DSOUND
static void SetupDirectSound(void* audDrv);
#endif
static uint32_t FillBuffer(void* drvStruct, void* userParam, uint32_t bufSize, void* Data);


#pragma pack(1)
typedef struct
{
	uint16_t lsb16;
	int8_t msb8;
} SMPL24BIT;
#pragma pack()


static uint32_t smplSize;
static void* audDrv;
static void* audDrvLog;

int main(int argc, char* argv[])
{
	uint8_t retVal;
	uint32_t drvCount;
	uint32_t curDrv;
	uint32_t idWavOut;
	uint32_t idWavOutDev;
	uint32_t idWavWrt;
	LWAO_DINFO* drvInfo;
	LWAO_OPTS* opts;
	LWAO_OPTS* optsLog;
	const LWAO_DEV_LIST* devList;
	
	lwaoInit();
	drvCount = lwaoGetDriverCount();
	if (! drvCount)
		goto Exit_Deinit;
	
#if 0
	idWavOut = (uint32_t)-1;
	idWavOutDev = 0;
	idWavWrt = (uint32_t)-1;
	for (curDrv = 0; curDrv < drvCount; curDrv ++)
	{
		lwaoGetDriverInfo(curDrv, &drvInfo);
		if (drvInfo->drvType == LWAO_DTYPE_OUT /*&& idWavOut == (uint32_t)-1*/)
			idWavOut = curDrv;
		else if (drvInfo->drvType == LWAO_DTYPE_DISK && idWavWrt == (uint32_t)-1)
			idWavWrt = curDrv;
	}
	if (idWavOut == (uint32_t)-1)
	{
		printf("Unable to find output driver\n");
		goto Exit_Deinit;
	}
#endif
	
	printf("Available Drivers:\n");
	for (curDrv = 0; curDrv < drvCount; curDrv ++)
	{
		lwaoGetDriverInfo(curDrv, &drvInfo);
		printf("    Driver %u: Type: %02X, Sig: %02X, Name: %s\n",
				curDrv, drvInfo->drvType, drvInfo->drvSig, drvInfo->drvName);
	}
	if (argc <= 1)
	{
		lwaoDeinit();
		printf("Usage:\n");
		printf("audiotest DriverID [DeviceID] [WaveLogDriverID]\n");
		return 0;
	}
	
	idWavOut = (uint32_t)-1;
	idWavOutDev = 0;
	idWavWrt = (uint32_t)-1;
	if (argc >= 2)
		idWavOut = strtoul(argv[1], NULL, 0);
	if (argc >= 3)
		idWavOutDev = strtoul(argv[2], NULL, 0);
	if (argc >= 4)
		idWavWrt = strtoul(argv[3], NULL, 0);
	if (idWavOut >= drvCount)
		idWavOut = drvCount - 1;
	
	lwaoGetDriverInfo(idWavOut, &drvInfo);
	printf("Using driver %s.\n", drvInfo->drvName);
	retVal = lwaodInit(idWavOut, &audDrv);
	if (retVal)
	{
		printf("WaveOut: Drv Init Error: %02X\n", retVal);
		goto Exit_Deinit;
	}

#ifdef LWAO_DRIVER_PULSE
	if (drvInfo->drvSig == LWAO_DSIG_PULSE)
	{
		void* pulseDrv;
		pulseDrv = lwaodGetDrvData(audDrv);
		lwaodPulse_SetStreamDesc(pulseDrv, "audiotest");
	}
#endif

#ifdef LWAO_DRIVER_DSOUND
	if (drvInfo->drvSig == LWAO_DSIG_DSOUND)
		SetupDirectSound(audDrv);
#endif
	
	if (idWavWrt == (uint32_t)-1)
	{
		audDrvLog = NULL;
	}
	else
	{
		lwaoGetDriverInfo(idWavWrt, &drvInfo);
		retVal = lwaodInit(idWavWrt, &audDrvLog);
		if (retVal)
		{
			printf("DiskWrt: Drv Init Error: %02X\n", retVal);
			audDrvLog = NULL;
		}
		if (audDrvLog != NULL)
		{
			if (drvInfo->drvSig == LWAO_DSIG_WAVEWRT)
			{
#ifdef LWAO_DRIVER_WAVEWRITE
				void* aDrv = lwaodGetDrvData(audDrvLog);
				lwaodWavWrt_SetFileName(aDrv, "waveOut.wav");
#endif
			}
			else if (drvInfo->drvSig == LWAO_DSIG_DSOUND)
			{
#ifdef LWAO_DRIVER_DSOUND
				SetupDirectSound(audDrvLog);
#endif
			}
		}
	}
	
	opts = lwaodGetOptions(audDrv);
	opts->numChannels = 1;
	opts->numBitsPerSmpl = 16;
	smplSize = opts->numChannels * opts->numBitsPerSmpl / 8;
	if (audDrvLog != NULL)
	{
		optsLog = lwaodGetOptions(audDrvLog);
		*optsLog = *opts;
	}
	
	devList = lwaodGetDeviceList(audDrv);
	printf("%u device%s found.\n", devList->devCount, (devList->devCount == 1) ? "" : "s");
	for (curDrv = 0; curDrv < devList->devCount; curDrv ++)
		printf("    Device %u: %s\n", curDrv, devList->devNames[curDrv]);
	
	lwaodSetCallback(audDrv, FillBuffer, NULL);
	printf("Opening Device %u ...\n", idWavOutDev);
	retVal = lwaodStart(audDrv, idWavOutDev);
	if (retVal)
	{
		printf("Dev Init Error: %02X\n", retVal);
		goto Exit_DrvDeinit;
	}
	
	if (audDrvLog != NULL)
	{
		retVal = lwaodStart(audDrvLog, 0);
		if (retVal)
		{
			printf("DiskWrt: Dev Init Error: %02X\n", retVal);
			lwaodDeinit(&audDrvLog);
		}
	}
	printf("Buffer Size: %u bytes\n", lwaodGetBufferSize(audDrv));
	
	while(1)
	{
		int inkey = getchar();
		if (toupper(inkey) == 'P')
			lwaodPause(audDrv);
		else if (toupper(inkey) == 'R')
			lwaodResume(audDrv);
		else
			break;
		while(getchar() != '\n')
			;
	}
	printf("Current Latency: %u ms\n", lwaodGetLatency(audDrv));
	
	retVal = lwaodStop(audDrv);
	if (audDrvLog != NULL)
		retVal = lwaodStop(audDrvLog);
	
Exit_DrvDeinit:
	lwaodDeinit(&audDrv);
	if (audDrvLog != NULL)
		lwaodDeinit(&audDrvLog);
Exit_Deinit:
	lwaoDeinit();
	printf("Done.\n");
	
#if defined(_DEBUG) && (_MSC_VER >= 1400)
	if (_CrtDumpMemoryLeaks())
		_getch();
#endif
	
	return 0;
}

static uint32_t FillBuffer(void* drvStruct, void* userParam, uint32_t bufSize, void* data)
{
	uint8_t* SmplPtr8 = (uint8_t*)data;
	int16_t* SmplPtr16 = (int16_t*)data;
	SMPL24BIT* SmplPtr24 = (SMPL24BIT*)data;
	int32_t* SmplPtr32 = (int32_t*)data;
	uint32_t smplCount = bufSize / smplSize;
	uint32_t curSmpl;
	
	switch(smplSize)
	{
	case 2:
		for (curSmpl = 0; curSmpl < smplCount; curSmpl ++)
		{
			if ((curSmpl / (smplCount / 16)) < 15)
				SmplPtr16[curSmpl] = +0x1000;
			else
				SmplPtr16[curSmpl] = -0x1000;
		}
		break;
	case 1:
		for (curSmpl = 0; curSmpl < smplCount; curSmpl ++)
		{
			if ((curSmpl / (smplCount / 16)) < 15)
				SmplPtr8[curSmpl] = 0x90;
			else
				SmplPtr8[curSmpl] = 0x70;
		}
		break;
	case 3:
		for (curSmpl = 0; curSmpl < smplCount; curSmpl ++)
		{
			SmplPtr24[curSmpl].lsb16 = 0x00;
			if ((curSmpl / (smplCount / 16)) < 15)
				SmplPtr24[curSmpl].msb8 = +0x10;
			else
				SmplPtr24[curSmpl].msb8 = -0x10;
		}
		break;
	case 4:
		for (curSmpl = 0; curSmpl < smplCount; curSmpl ++)
		{
			if ((curSmpl / (smplCount / 16)) < 15)
				SmplPtr32[curSmpl] = +0x10000000;
			else
				SmplPtr32[curSmpl] = -0x10000000;
		}
		break;
	default:
		curSmpl = 0;
		break;
	}
	
	if (audDrvLog != NULL)
		lwaodWriteData(audDrvLog, curSmpl * smplSize, data);
	return curSmpl * smplSize;
}

#ifdef LWAO_DRIVER_DSOUND
static void SetupDirectSound(void* audDrv)
{
#ifdef _WIN32
	void* aDrv;
	HWND hWndConsole;
	
	aDrv = lwaodGetDrvData(audDrv);
#if _WIN32_WINNT >= 0x500
	hWndConsole = GetConsoleWindow();
#else
	hWndConsole = GetDesktopWindow();	// not as nice, but works
#endif
	lwaodDSound_SetHWnd(aDrv, hWndConsole);
#endif	// _WIN32
	
	return;
}
#endif
