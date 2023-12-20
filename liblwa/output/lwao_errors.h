#ifndef LWAO_ERRORS_H
#define LWAO_ERRORS_H

/**
 * @file
 * @brief liblwa audio output return/error codes
 */

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @name lwao device error codes
 */
/**@{*/
#define LWAO_ERR_OK				0x00	/**< @brief The call completed successfully. */
#define LWAO_ERR_BUSY			0x01	/**< @brief The device is busy and does not (yet) accept more data. */
#define LWAO_ERR_TOO_MUCH_DATA	0x02	/**< @brief The data to be written is larger than the buffer size. */
#define LWAO_ERR_WASDONE		0x18	/**< @brief Init()/Deinit() was already called with success. */
#define LWAO_ERR_NODRVS			0x20	/**< @brief No audio drivers were found. */
#define LWAO_ERR_INVALID_DRV	0x21	/**< @brief The specified audio driver ID was invalid. */
#define LWAO_ERR_DRV_DISABLED	0x22	/**< @brief The audio driver was disabled, because it is not available. */
#define LWAO_ERR_INVALID_DEV	0x41	/**< @brief Start() was called with an invalid device ID. */
#define LWAO_ERR_NO_SUPPORT		0x80	/**< @brief The function call is not supported by this driver. */
#define LWAO_ERR_BAD_MODE		0x81	/**< @brief The audio driver is in the wrong mode for this call. */
#define LWAO_ERR_DEV_OPEN_FAIL	0xC0	/**< @brief Unable to open the device. */
#define LWAO_ERR_DEV_CLOSE_FAIL	0xC1	/**< @brief Unable to close the device. */
#define LWAO_ERR_IS_RUNNING 	0xC2	/**< @brief The audio device is already open. */
#define LWAO_ERR_NOT_RUNNING 	0xC3	/**< @brief The audio driver is not open. */
#define LWAO_ERR_WRITE_ERROR	0xC4	/**< @brief Unable to write data. */
#define LWAO_ERR_BAD_FORMAT 	0xC8	/**< @brief The wave format is not supported by this driver. */
#define LWAO_ERR_THREAD_FAIL 	0xCC	/**< @brief Unable to create a thread for audio streaming. */

#define LWAO_ERR_API_ERR		0xF0	/**< @brief Some API call inside the driver failed. */
#define LWAO_ERR_CALL_SPC_FUNC	0xF1	/**< @brief A driver-specific function has to be called first. @see lwao_drvfuncs.h */
/**@}*/


#ifdef __cplusplus
}
#endif

#endif	// LWAO_ERRORS_H
