#ifndef __AUDIOSTREAM_H__
#define __AUDIOSTREAM_H__

#ifdef __cplusplus
extern "C"
{
#endif

// Audio Drivers
/*
#define LWAO_DRIVER_WAVEWRITE

#ifdef _WIN32

#define LWAO_DRIVER_WINMM
#define LWAO_DRIVER_DSOUND
#define LWAO_DRIVER_XAUD2
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#define LWAO_DRIVER_WASAPI	// no WASAPI for MS VC6 or MinGW
#endif

#elif defined(APPLE)

#define LWAO_DRIVER_CA

#else

#define LWAO_DRIVER_OSS
//#define LWAO_DRIVER_SADA
#define LWAO_DRIVER_ALSA
#define LWAO_DRIVER_LIBAO
#define LWAO_DRIVER_PULSE

#endif
*/

#include "AudioStructs.h"
#include "lwao_export.h"
#include "../lwa_api.h"

/**
 * @brief Initializes the audio output system and loads audio drivers.
 *
 * @return error code. 0 = success, see LWAO_ERR constants
 */
LWAO_EXPORT UINT8 LWA_API lwaoInit(void);
/**
 * @brief Deinitializes the audio output system.
 *
 * @return error code. 0 = success, see LWAO_ERR constants
 */
LWAO_EXPORT UINT8 LWA_API lwaoDeinit(void);
/**
 * @brief Retrieve the number of loaded audio drivers.
 *
 * @return number of loaded audio drivers
 */
LWAO_EXPORT UINT32 LWA_API lwaoGetDriverCount(void);
/**
 * @brief Retrieve information about a certain audio driver.
 *
 * @param drvID ID of the audio driver
 * @param retDrvInfo buffer for returning driver information
 * @return error code. 0 = success, see LWAO_ERR constants
 */
LWAO_EXPORT UINT8 LWA_API lwaoGetDriverInfo(UINT32 drvID, LWAO_DINFO** retDrvInfo);

/**
 * @brief Creates an audio driver instance and initializes it.
 *
 * @param drvID ID of the audio driver
 * @param retDrvStruct buffer for returning the audio driver instance
 * @return error code. 0 = success, see LWAO_ERR constants
 */
LWAO_EXPORT UINT8 LWA_API lwaodInit(UINT32 drvID, void** retDrvStruct);
/**
 * @brief Deinitializes and destroys an audio driver instance.
 *
 * @param drvStruct pointer to audio driver instance
 * @return error code. 0 = success, see LWAO_ERR constants
 */
LWAO_EXPORT UINT8 LWA_API lwaodDeinit(void** drvStruct);
/**
 * @brief Retrieve the list of devices supported by an audio driver.
 *
 * @param drvStruct audio driver instance
 * @return pointer to a list of supported devices
 */
LWAO_EXPORT const LWAO_DEV_LIST* LWA_API lwaodGetDeviceList(void* drvStruct);
/**
 * @brief Retrieve the default configuration for an audio driver.
 *
 * @param drvStruct audio driver instance
 * @return pointer to the default configuration
 */
LWAO_EXPORT LWAO_OPTS* LWA_API lwaodGetOptions(void* drvStruct);
/**
 * @brief Retrieve an audio driver's data pointer for use with driver-specific calls.
 *
 * @param drvStruct audio driver instance
 * @return audio driver data pointer
 */
LWAO_EXPORT void* LWA_API lwaodGetDrvData(void* drvStruct);

/**
 * @brief Open an audio device and start an audio stream on that device.
 *
 * @param drvStruct audio driver instance
 * @param devID ID of the audio driver's device to be used
 * @return error code. 0 = success, see LWAO_ERR constants
 */
LWAO_EXPORT UINT8 LWA_API lwaodStart(void* drvStruct, UINT32 devID);
/**
 * @brief Stops the audio stream and close the device.
 *
 * @param drvStruct audio driver instance
 * @return error code. 0 = success, see LWAO_ERR constants
 */
LWAO_EXPORT UINT8 LWA_API lwaodStop(void* drvStruct);
/**
 * @brief Pause the audio stream.
 *
 * @param drvStruct audio driver instance
 * @return error code. 0 = success, see LWAO_ERR constants
 */
LWAO_EXPORT UINT8 LWA_API lwaodPause(void* drvStruct);
/**
 * @brief Resume the audio stream.
 *
 * @param drvStruct audio driver instance
 * @return error code. 0 = success, see LWAO_ERR constants
 */
LWAO_EXPORT UINT8 LWA_API lwaodResume(void* drvStruct);

/**
 * @brief Sets a callback function that is called whenever a buffer is free.
 *
 * @param drvStruct audio driver instance
 * @param FillBufCallback address of callback function
 * @param userParam pointer to user data
 * @return error code. 0 = success, see LWAO_ERR constants
 */
LWAO_EXPORT UINT8 LWA_API lwaodSetCallback(void* drvStruct, LWAOFUNC_FILLBUF FillBufCallback, void* userParam);

/**
 * @brief Returns the maximum number of bytes that can be written using lwaodWriteData().
 * @note Only valid after calling lwaodStart().
 *
 * @param drvStruct audio driver instance
 * @return current buffer size in bytes
 */
LWAO_EXPORT UINT32 LWA_API lwaodGetBufferSize(void* drvStruct);
/**
 * @brief Checks whether you can send more data to the audio driver or not.
 *
 * @param drvStruct audio driver instance
 * @return LWAO_ERR_OK: ready for more data, LWAO_ERR_BUSY: busy playing previous data, see LWAO_ERR constants for more error codes
 */
LWAO_EXPORT UINT8 LWA_API lwaodIsBusy(void* drvStruct);
/**
 * @brief Sends data to the audio driver in order to be played.
 *
 * @param drvStruct audio driver instance
 * @param dataSize size of the sample data in bytes
 * @param data sample data to be sent
 * @return latency in milliseconds
 */
LWAO_EXPORT UINT8 LWA_API lwaodWriteData(void* drvStruct, UINT32 dataSize, void* data);
/**
 * @brief Returns the current latency of the audio device in milliseconds.
 *
 * @param drvStruct audio driver instance
 * @return latency in milliseconds
 */
LWAO_EXPORT UINT32 LWA_API lwaodGetLatency(void* drvStruct);

#ifdef __cplusplus
}
#endif

#endif	// __AUDIOSTREAM_H__
