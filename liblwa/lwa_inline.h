#ifndef LWA_INLINE_H
#define LWA_INLINE_H

/*
 * liblwa INLINE macro
 */

#ifndef INLINE
#if defined(_MSC_VER)
#define INLINE	static __inline
#elif defined(__GNUC__)
#define INLINE	static __inline__
#else
#define INLINE	static inline
#endif
#endif

#endif	// LWA_INLINE_H
