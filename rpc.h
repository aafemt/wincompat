#pragma once

#include <string.h>
#include <stdint.h>
// Let it be here for awhile

struct UUID
{
    uint32_t    Data1;
    uint16_t    Data2;
    uint16_t    Data3;
    unsigned char   Data4[8];
};


inline bool operator ==(const UUID& left, const UUID& right)
{
    return memcmp(&left, &right, sizeof(UUID)) == 0;
}

inline bool operator !=(const UUID& left, const UUID& right)
{
    return memcmp(&left, &right, sizeof(UUID)) != 0;
}

typedef long RPC_STATUS;

#include "rpcdce.h"
#include "rpcnterr.h"
