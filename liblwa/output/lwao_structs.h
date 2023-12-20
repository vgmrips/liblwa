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

#include "../lwa_types.h"

/**
 * @brief audio driver options
 *
 * Contains settings related to the audio streaming and buffering.
 */
typedef struct _lwao_options
{
	uint32_t sampleRate;		/**< requested sample rate */
	uint8_t numChannels;		/**< number of channels, 1 = mono, 2 = stereo */
	uint8_t numBitsPerSmpl;	/**< bits per sample */
	
	uint32_t usecPerBuf;		/**< audio buffer size in microseconds */
	uint32_t numBuffers;		/**< number of audio buffers, total buffer size = (buffer size * count) */
} LWAO_OPTS;

/**
 * @brief audio device list
 */
typedef struct _lwao_device_list
{
	uint32_t devCount;	/**< number of devices in the list */
	char** devNames;	/**< list of strings of the device names */
} LWAO_DEV_LIST;


/** @brief function that fills a buffer with audio data and returns the number of written bytes */
typedef uint32_t (*LWAOFUNC_FILLBUF)(void* drvStruct, void* userParam, uint32_t bufSize, void* data);

/** @brief generic function that returns an error code (for init/deinit) */
typedef uint8_t (*LWAOFUNC_COMMON)(void);
/** @brief function that returns a list of available devices */
typedef const LWAO_DEV_LIST* (*LWAOFUNC_DEVLIST)(void);
/** @brief function that returns a pointer to the default driver options  */
typedef LWAO_OPTS* (*LWAOFUNC_DEFOPTS)(void);
/** @brief generic driver function that returns an error code (for stop/pause/resume) */
typedef uint8_t (*LWAOFUNC_DRVCOMMON)(void* drvObj);
/** @brief driver instance create function */
typedef uint8_t (*LWAOFUNC_DRVCREATE)(void** retDrvObj);
/** @brief driver "strt device" function */
typedef uint8_t (*LWAOFUNC_DRVSTART)(void* drvObj, uint32_t deviceID, LWAO_OPTS* options, void* audDrvParam);
/** @brief generic driver function that returns a 32-bit integer  */
typedef uint32_t (*LWAOFUNC_DRVRET32)(void* drvObj);
/** @brief driver function for setting a "fill buffer" callback function */
typedef uint8_t (*LWAOFUNC_DRVSETCB)(void* drvObj, LWAOFUNC_FILLBUF FillBufCallback, void* userParam);
/** @brief driver function for writing audio data */
typedef uint8_t (*LWAOFUNC_DRVWRTDATA)(void* drvObj, uint32_t dataSize, void* data);


/**
 * @brief audio driver information
 *
 * Basic audio driver identification and information data.
 */
typedef struct _lwao_driver_info
{
	uint8_t drvType;			/**< driver type code, see LWAO_DTYPE_* constants */
	uint8_t drvSig;			/**< driver signature code, see LWAO_DSIG_* constants */
	const char* drvName;	/**< driver name */
} LWAO_DINFO;
/**
 * @brief audio driver definition
 *
 * Audio driver information and functions. Set by the audio driver.
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
#define LWAO_DTYPE_NULL		0x00	/**< @brief null device */
#define LWAO_DTYPE_OUT		0x01	/**< @brief stream to speakers */
#define LWAO_DTYPE_DISK		0x02	/**< @brief write to disk */
/**@}*/

/**
 * @name lwao device signature codes
 */
/**@{*/
#define LWAO_DSIG_WAVEWRT	0x01	/**< @brief WAV Writer */
#define LWAO_DSIG_WINMM		0x10	/**< @brief [Windows] WinMM */
#define LWAO_DSIG_DSOUND	0x11	/**< @brief [Windows] DirectSound */
#define LWAO_DSIG_XAUD2		0x12	/**< @brief [Windows] XAudio2 */
#define LWAO_DSIG_WASAPI	0x13	/**< @brief [Windows] Windows Audio Session API */
#define LWAO_DSIG_OSS		0x20	/**< @brief [Linux] Open Sound System (/dev/dsp) */
#define LWAO_DSIG_SADA		0x21	/**< @brief [NetBSD] Solaris Audio Device Architecture (/dev/audio) */
#define LWAO_DSIG_ALSA		0x22	/**< @brief [Linux] Advanced Linux Sound Architecture */
#define LWAO_DSIG_PULSE		0x23	/**< @brief [Linux] PulseAudio */
#define LWAO_DSIG_CA		0x30	/**< @brief [macOS] Core Audio */
#define LWAO_DSIG_LIBAO		0x40	/**< @brief libao library */
/**@}*/


#ifdef __cplusplus
}
#endif

#endif	// LWAO_STRUCTS_H
