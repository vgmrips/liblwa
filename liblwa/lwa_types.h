#ifndef LWA_TYPES_H
#define LWA_TYPES_H

/*
 * liblwa integer types
 */

#if ! defined(_STDINT_H) && ! defined(__int8_t_defined)

#ifdef HAVE_STDINT_H
#include <stdint.h>
#else

#define __int8_t_defined
typedef unsigned char	uint8_t;
typedef   signed char	 int8_t;
typedef unsigned short	uint16_t;
typedef   signed short	 int16_t;
typedef unsigned int	uint32_t;
typedef   signed int	 int32_t;
#ifdef _MSC_VER
typedef unsigned __int64	uint64_t;
typedef   signed __int64	 int64_t;
#else
__extension__ typedef unsigned long long	uint64_t;
__extension__ typedef   signed long long	 int64_t;
#endif

#endif	// !HAVE_STDINT_H
#endif	// !__int8_t_defined

#endif	// LWA_TYPES_H
