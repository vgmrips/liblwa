#ifndef LWAU_ERRORS_H
#define LWAU_ERRORS_H

/**
 * @file
 * @brief liblwa utilities: return/error codes
 */

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @name lwau error codes
 */
/**@{*/
#define LWAU_ERR_OK				0x00	/**< @brief The call completed successfully. */
#define LWAU_ERR_API_ERR		0xF0	/**< @brief Some system API call failed. */
#define LWAU_ERR_MEM_ERR		0xFF	/**< @brief Memory allocation error. */

#define LWAU_ERR_MTX_LOCKED		0x01	/**< @brief Mutex is already locked. */
/**@}*/

#ifdef __cplusplus
}
#endif

#endif	// LWAU_ERRORS_H
