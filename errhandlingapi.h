#pragma once

#include <errno.h>
#include "windef.h"

inline DWORD GetLastError()
{
    return errno;
}

inline void SetLastError(DWORD dwErrCode)
{
	errno = dwErrCode;
}
