#pragma once

#include "winnt.h"

HRESULT PathCchRemoveFileSpec(wchar_t* pszPath, size_t cchPath);
HRESULT PathCchAddBackslash(wchar_t* pszPath, size_t cchPath);
