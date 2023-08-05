#ifndef LWAO_STRUCTS_H
#define LWAO_STRUCTS_H

/**
 * @file
 * @brief liblwa audio output structures, types and defines
 */

#ifdef __cplusplus
extern "C"
{
#endif

#include "../stdtype.h"

/**
 * audio driver options
 */
typedef struct _lwao_options
{
	UINT32 sampleRate;		/**< requested sample rate */
	UINT8 numChannels;		/**< number of channels, 1 = mono, 2 = stereo */
	UINT8 numBitsPerSmpl;	/**< bits per sample */
	
	UINT32 usecPerBuf;		/**< audio buffer size in microseconds */
	UINT32 numBuffers;		/**< number of audio buffers, total buffer size = (buffer size * count) */
} LWAO_OPTS;

/**
 * audio device list
 */
typedef struct _lwao_device_list
{
	UINT32 devCount;	/**< number of devices in the list */
	char** devNames;	/**< list of strings of the device names */
} LWAO_DEV_LIST;


/** @brief function that fills a buffer with audio data and returns the number of written bytes */
typedef UINT32 (*LWAOFUNC_FILLBUF)(void* drvStruct, void* userParam, UINT32 bufSize, void* data);

/** @brief generic function that returns an error code (for init/deinit) */
typedef UINT8 (*LWAOFUNC_COMMON)(void);
/** @brief function that returns a list of available devices */
typedef const LWAO_DEV_LIST* (*LWAOFUNC_DEVLIST)(void);
/** @brief function that returns a pointer to the default driver options  */
typedef LWAO_OPTS* (*LWAOFUNC_DEFOPTS)(void);
/** @brief generic driver function that returns an error code (for stop/pause/resume) */
typedef UINT8 (*LWAOFUNC_DRVCOMMON)(void* drvObj);
/** @brief driver instance create function */
typedef UINT8 (*LWAOFUNC_DRVCREATE)(void** retDrvObj);
/** @brief driver "strt device" function */
typedef UINT8 (*LWAOFUNC_DRVSTART)(void* drvObj, UINT32 deviceID, LWAO_OPTS* options, void* audDrvParam);
/** @brief generic driver function that returns a 32-bit integer  */
typedef UINT32 (*LWAOFUNC_DRVRET32)(void* drvObj);
/** @brief driver function for setting a "fill buffer" callback function */
typedef UINT8 (*LWAOFUNC_DRVSETCB)(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam);
/** @brief driver function for writing audio data */
typedef UINT8 (*LWAOFUNC_DRVWRTDATA)(void* drvObj, UINT32 dataSize, void* data);


/**
 * audio driver information struct
 */
typedef struct _lwao_driver_info
{
	UINT8 drvType;			/**< driver type code, see LWAO_DTYPE_* constants */
	UINT8 drvSig;			/**< driver signature code, see LWAO_DSIG_* constants */
	const char* drvName;	/**< driver name */
} LWAO_DINFO;
/**
 * audio driver definition struct
 */
typedef struct _lwao_driver
{
	LWAO_DINFO drvInfo;					/**< general audio driver information */
	
	LWAOFUNC_COMMON IsAvailable;		/**< function to test whether or not the audio driver can be loaded */
	LWAOFUNC_COMMON Init;				/**< function to initialize the audio driver */
	LWAOFUNC_COMMON Deinit;				/**< function to deinitialize the audio driver */
	LWAOFUNC_DEVLIST GetDevList;		/**< function to get the list of available devices */
	LWAOFUNC_DEFOPTS GetDefOpts;		/**< function to get a struct with default options */
	
	LWAOFUNC_DRVCREATE Create;			/**< function to create a driver instance, returns driver structure */
	LWAOFUNC_DRVCOMMON Destroy;			/**< function to destroy a driver instance, note: caller must clear its reference pointer manually */
	LWAOFUNC_DRVSTART Start;			/**< function to open a device and start the audio stream */
	LWAOFUNC_DRVCOMMON Stop;			/**< function to stop the audio stream and close the device */
	
	LWAOFUNC_DRVCOMMON Pause;			/**< function to pause the audio stream */
	LWAOFUNC_DRVCOMMON Resume;			/**< function to resume the audio stream */
	
	LWAOFUNC_DRVSETCB SetCallback;		/**< function to set a callback that is called whenever new data is needed */
	LWAOFUNC_DRVRET32 GetBufferSize;	/**< function to return total buffer size in bytes, 0 = no buffer */
	LWAOFUNC_DRVCOMMON IsBusy;			/**< function to check whether or not new data can be written */
	LWAOFUNC_DRVWRTDATA WriteData;		/**< function to manually write data to the driver */
	
	LWAOFUNC_DRVRET32 GetLatency;		/**< function to return the current stream latency in milliseconds */
} LWAO_DRIVER;



/**
 * @name lwao device types
 */
/**@{*/
#define LWAO_DTYPE_NULL		0x00	/**< does nothing */
#define LWAO_DTYPE_OUT		0x01	/**< stream to speakers */
#define LWAO_DTYPE_DISK		0x02	/**< write to disk */
/**@}*/

/**
 * @name lwao device signature codes
 */
/**@{*/
#define LWAO_DSIG_WAVEWRT	0x01	/**< WAV Writer */
#define LWAO_DSIG_WINMM		0x10	/**< [Windows] WinMM */
#define LWAO_DSIG_DSOUND	0x11	/**< [Windows] DirectSound */
#define LWAO_DSIG_XAUD2		0x12	/**< [Windows] XAudio2 */
#define LWAO_DSIG_WASAPI	0x13	/**< [Windows] Windows Audio Session API */
#define LWAO_DSIG_OSS		0x20	/**< [Linux] Open Sound System (/dev/dsp) */
#define LWAO_DSIG_SADA		0x21	/**< [NetBSD] Solaris Audio Device Architecture (/dev/audio) */
#define LWAO_DSIG_ALSA		0x22	/**< [Linux] Advanced Linux Sound Architecture */
#define LWAO_DSIG_PULSE		0x23	/**< [Linux] PulseAudio */
#define LWAO_DSIG_CA		0x30	/**< [macOS] Core Audio */
#define LWAO_DSIG_LIBAO		0x40	/**< libao library */
/**@}*/


/**
 * @name lwao device error codes
 */
/**@{*/
#define LWAO_ERR_OK				0x00	/**< call completed successfully */
#define LWAO_ERR_BUSY			0x01	/**< The device is busy and does not (yet) accept more data. */
#define LWAO_ERR_TOO_MUCH_DATA	0x02	/**< The data to be written is larger than buffer size. */
#define LWAO_ERR_WASDONE		0x18	/**< Init()/Deinit() was already called with success. */
#define LWAO_ERR_NODRVS			0x20	/**< No audio drivers were found. */
#define LWAO_ERR_INVALID_DRV	0x21	/**< invalid audio driver ID */
#define LWAO_ERR_DRV_DISABLED	0x22	/**< The audio driver was disabled, because it is not available. */
#define LWAO_ERR_INVALID_DEV	0x41	/**< Start() was called with invalid device ID. */
#define LWAO_ERR_NO_SUPPORT		0x80	/**< The function call is not supported by this driver. */
#define LWAO_ERR_BAD_MODE		0x81	/**< The audio driver is in the wrong mode for this call. */

#define LWAO_ERR_FILE_ERR		0xC0	/**< [file writer] file write error */
#define LWAO_ERR_NOT_OPEN		0xC1	/**< [file writer] file is not open */

#define LWAO_ERR_API_ERR		0xF0	/**< some API call inside the driver failed */
#define LWAO_ERR_CALL_SPC_FUNC	0xF1	/**< A driver-specific function has to be called first. @see lwao_drvfuncs.h */
/**@}*/

#ifdef __cplusplus
}
#endif

#endif	// LWAO_STRUCTS_H
