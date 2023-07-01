#ifndef LWAO_STRUCTS_H
#define LWAO_STRUCTS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "../stdtype.h"

typedef struct _audio_options
{
	UINT32 sampleRate;
	UINT8 numChannels;
	UINT8 numBitsPerSmpl;
	
	UINT32 usecPerBuf;
	UINT32 numBuffers;
} LWAO_OPTS;

typedef struct _audio_device_list
{
	UINT32 devCount;
	char** devNames;
} LWAO_DEV_LIST;


typedef UINT32 (*LWAOFUNC_FILLBUF)(void* drvStruct, void* userParam, UINT32 bufSize, void* data);

typedef UINT8 (*LWAOFUNC_COMMON)(void);
typedef const LWAO_DEV_LIST* (*LWAOFUNC_DEVLIST)(void);
typedef LWAO_OPTS* (*LWAOFUNC_DEFOPTS)(void);
typedef UINT8 (*LWAOFUNC_DRVCOMMON)(void* drvObj);
typedef UINT8 (*LWAOFUNC_DRVCREATE)(void** retDrvObj);
typedef UINT8 (*LWAOFUNC_DRVSTART)(void* drvObj, UINT32 deviceID, LWAO_OPTS* options, void* audDrvParam);
typedef UINT32 (*LWAOFUNC_DRVRET32)(void* drvObj);
typedef UINT8 (*LWAOFUNC_DRVSETCB)(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam);
typedef UINT8 (*LWAOFUNC_DRVWRTDATA)(void* drvObj, UINT32 dataSize, void* data);


typedef struct _lwao_driver_info
{
	UINT8 drvType;
	UINT8 drvSig;
	const char* drvName;
} LWAO_DINFO;
typedef struct _lwao_driver
{
	LWAO_DINFO drvInfo;
	
	LWAOFUNC_COMMON IsAvailable;
	LWAOFUNC_COMMON Init;
	LWAOFUNC_COMMON Deinit;
	LWAOFUNC_DEVLIST GetDevList;	// device list
	LWAOFUNC_DEFOPTS GetDefOpts;	// default options
	
	LWAOFUNC_DRVCREATE Create;	// returns driver structure
	LWAOFUNC_DRVCOMMON Destroy;	// Note: caller must clear its reference pointer manually
	LWAOFUNC_DRVSTART Start;
	LWAOFUNC_DRVCOMMON Stop;
	
	LWAOFUNC_DRVCOMMON Pause;
	LWAOFUNC_DRVCOMMON Resume;
	
	LWAOFUNC_DRVSETCB SetCallback;
	LWAOFUNC_DRVRET32 GetBufferSize;	// returns bytes, 0 = unbuffered
	LWAOFUNC_DRVCOMMON IsBusy;
	LWAOFUNC_DRVWRTDATA WriteData;
	
	LWAOFUNC_DRVRET32 GetLatency;	// returns msec
} LWAO_DRIVER;



#define LWAO_DTYPE_NULL		0x00	// does nothing
#define LWAO_DTYPE_OUT		0x01	// stream to speakers
#define LWAO_DTYPE_DISK		0x02	// write to disk

#define LWAO_DSIG_WAVEWRT	0x01	// WAV Writer
#define LWAO_DSIG_WINMM		0x10	// [Windows] WinMM
#define LWAO_DSIG_DSOUND	0x11	// [Windows] DirectSound
#define LWAO_DSIG_XAUD2		0x12	// [Windows] XAudio2
#define LWAO_DSIG_WASAPI	0x13	// [Windows] Windows Audio Session API
#define LWAO_DSIG_OSS		0x20	// [Linux] Open Sound System (/dev/dsp)
#define LWAO_DSIG_SADA		0x21	// [NetBSD] Solaris Audio Device Architecture (/dev/audio)
#define LWAO_DSIG_ALSA		0x22	// [Linux] Advanced Linux Sound Architecture
#define LWAO_DSIG_PULSE 	0x23	// [Linux] PulseAudio
#define LWAO_DSIG_CA		0x30	// [macOS] Core Audio
#define LWAO_DSIG_LIBAO		0x40	// libao



#define LWAO_ERR_OK				0x00
#define LWAO_ERR_BUSY			0x01
#define LWAO_ERR_TOO_MUCH_DATA	0x02	// data to be written is larger than buffer size
#define LWAO_ERR_WASDONE		0x18	// Init-call was already called with success
#define LWAO_ERR_NODRVS			0x20	// no Audio Drivers found
#define LWAO_ERR_INVALID_DRV	0x21	// invalid Audio Driver ID
#define LWAO_ERR_DRV_DISABLED	0x22	// driver was disabled
#define LWAO_ERR_INVALID_DEV	0x41
#define LWAO_ERR_NO_SUPPORT		0x80	// not supported
#define LWAO_ERR_BAD_MODE		0x81

#define LWAO_ERR_FILE_ERR		0xC0
#define LWAO_ERR_NOT_OPEN		0xC1

#define LWAO_ERR_API_ERR		0xF0
#define LWAO_ERR_CALL_SPC_FUNC	0xF1	// Needs a driver-specific function to be called first.

#ifdef __cplusplus
}
#endif

#endif	// LWAO_STRUCTS_H
