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

	// This routine is positioned as "safe" so we cannot rely on null-termination and use wcsrchr
	wchar_t* slash = nullptr;
	for (size_t i = 0; i < cchPath; ++i)
	{
		if (pszPath[i] == L'\0')
			break;

		if (pszPath[i] == L'/')
			slash = &pszPath[i];
	}

	if (slash == nullptr)
	{
		// No path separator at all. In this case Windows return empty string.
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

HRESULT PathCchAddBackslash(wchar_t* pszPath, size_t cchPath)
{
	// On windows this function crashes if pszPath is nullptr. Let's be smarter.
	if (pszPath == nullptr)
		return E_INVALIDARG;

	// This routine is positioned as "safe" so we cannot rely on null-termination
	for (size_t i = 0; i < cchPath; ++i)
	{
		if (pszPath[i] == L'\0')
		{
			// This is crazy, but Windows really returns S_FALSE on empty string and DON'T add backslash
			if (i == 0 || pszPath[i - 1] == L'/')
				return S_FALSE;

			if (i + 1 < cchPath)
			{
				pszPath[i++] = L'/';
				pszPath[i] = L'\0';
				return S_OK;
			}
			// else
			break;
		}
	}
	return E_NOT_SUFFICIENT_BUFFER;
}
