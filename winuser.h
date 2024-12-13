#pragma once

#include <stdlib.h>
#include <string.h>

#include "windef.h"

struct GlobalMemory
{
    size_t AllocatedSize;
    char data[];
};

typedef GlobalMemory* HGLOBAL;

const unsigned GMEM_MOVEABLE = 0;
const unsigned GMEM_FIXED = 0;
const unsigned GMEM_ZEROINIT = 1;
const unsigned GHND = GMEM_MOVEABLE | GMEM_ZEROINIT;
const unsigned GPTR = GMEM_FIXED | GMEM_ZEROINIT;

inline HGLOBAL GlobalAlloc(unsigned uFlags, size_t dwBytes)
{
    GlobalMemory* result = (GlobalMemory*)malloc(dwBytes + sizeof(GlobalMemory));
    if (result != nullptr)
    {
        result->AllocatedSize = dwBytes;
        if ((uFlags & GMEM_ZEROINIT) != 0)
        {
            memset(result->data, 0, dwBytes);
        }
    }
    return result;
}
inline void* GlobalLock(HGLOBAL hMem) { return hMem->data; }
inline WINBOOL GlobalUnlock(HGLOBAL) { return TRUE; }

const unsigned CF_TEXT = 1;
//const unsigned CF_BITMAP = 2;
//const unsigned CF_UNICODETEXT = 13;
const unsigned CF_OEMTEXT = 1; // Pretend to have the same locale everywhere

WINBOOL OpenClipboard(HWND);
WINBOOL EmptyClipboard();
// HGLOBAL is not a right type here, but let it be while other clipboardable handles are not implemented.
void SetClipboardData(unsigned uFormat, HGLOBAL data);
WINBOOL CloseClipboard();
