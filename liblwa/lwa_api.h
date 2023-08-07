#ifndef LWA_API_H
#define LWA_API_H

/*
 * liblwa API calling convention definition
 */

#ifdef _WIN32
#define LWA_API __stdcall
#else
#define LWA_API
#endif

#endif // LWA_API_H
