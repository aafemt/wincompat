/***********************************************
*
* Implementation of functions from pathcch.h
*
***********************************************/

#include <wchar.h>

#include "pathcch.h"
#include "winerror.h"

HRESULT PathCchRemoveFileSpec(wchar_t* pszPath, size_t cchPath)
{
	if (pszPath == nullptr || cchPath == 0)
		return E_INVALIDARG;

	wchar_t* slash = wcsrchr(pszPath, L'/');
	if (slash == nullptr)
	{
		// No path separator at all. In this case strahne Windows return empty string.
		*pszPath = L'\0';
	}
	else if (slash == pszPath)
	{
		// Nothing to remove
		if (slash[1] == L'\0')
			return S_FALSE;

		slash[1] = L'\0';
	}
	else
	{
		*slash = L'\0';
	}
	return S_OK;
}
