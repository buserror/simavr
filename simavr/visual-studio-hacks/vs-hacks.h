 //dirty windows hacks

#ifndef _VS_HACKS_H
#define _VS_HACKS_H

#ifdef _MSC_VER

#define __func__ __FUNCTION__

	//disallow inline, as VS doesn't allow them in all the places as gcc does
#define inline
#define __STDINT_H_

//disable attribute
#define __attribute__(x) 


//windows doesn't have this
#define __STDINT_H_

//lets fake it
typedef unsigned int uint32_t;
typedef unsigned char uint8_t;
typedef  char int8_t;
typedef int int32_t;
typedef unsigned short uint16_t;
typedef short int16_t;
typedef long long int64_t;
typedef unsigned long long uint64_t;

#define strdup _strdup


//popcount has a different name here
#  include <intrin.h>
#  define __builtin_popcount __popcnt

//basename doesn't exist, but we don't really need it
#define basename(boo) boo

#define strcasecmp _stricmp

#endif
#endif
