#ifndef __TYPES_HH__
#define __TYPES_HH__

#include "picojson.h"

using namespace std;

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned long long ull;
typedef u32 *HVArray;
typedef picojson::object JSON;

struct proto_stats {
	u32 count;
	u64 bytes;
};

#endif /* __TYPES_HH__ */
