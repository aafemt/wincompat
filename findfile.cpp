/**************************************************
*
*   Implementation if FindFirstFile, FindNextFile
*   and FindClose functions
*
***************************************************/

#include <errno.h>
#include <glob.h>
#include <memory>
#include <string.h>
#include <sys/stat.h>

#include "winbase.h"
#include "fileapi.h"
#include "winerror.h"

#include "handleapi.h"

struct FindHandle: public handle_t
{
	glob_t g;
	size_t current = 0;

	FindHandle() : g{}
	{
		type = handle_type::HANDLE_FIND;
	}
	~FindHandle()
	{
		globfree(&g);
	}
	int GetWaitableHandle() override
	{
		errno = ERROR_INVALID_HANDLE;
		return -1;
	}
	WINBOOL fillStruct(WIN32_FIND_DATAA* findData);
	WINBOOL fillStruct(WIN32_FIND_DATAW* findData);
};

static FILETIME TimespecToFileTime(const timespec& t)
{
	// timespec is in nanoseconds, FILETIME is in hundreds of nanoseconds
	FILETIME result;
	unsigned long long tmp = (unsigned long long)t.tv_sec * 10'000'000 + t.tv_nsec / 100;
	result.dwHighDateTime = tmp >> 32;
	result.dwLowDateTime = tmp;
	return result;
}

static void setAttributes(mode_t mode, DWORD& attributes)
{
	// FILE_ATTRIBUTE_READONLY/HIDDEN/etc is left unhandled for a while because POSIX has no R/O attribute
	// analyze of privilegies is not what one would expect. Perhaps being on R/O mount device can count.

	switch (mode & S_IFMT)
	{
	case S_IFDIR:
		{
			attributes |= FILE_ATTRIBUTE_DIRECTORY;
			break;
		}
	case S_IFBLK:
	case S_IFCHR:
		{
			attributes |= FILE_ATTRIBUTE_DEVICE;
			break;
		}
	case S_IFLNK:
		{
			attributes |= FILE_ATTRIBUTE_REPARSE_POINT;
			break;
		}
	case S_IFREG:
		{
			attributes |= FILE_ATTRIBUTE_NORMAL;
			break;
		}
	// default: what could it be?..
	}
}

WINBOOL FindHandle::fillStruct(WIN32_FIND_DATAA* findData)
{
	if (current >= g.gl_pathc)
	{
		errno = ERROR_NO_MORE_FILES;
		return FALSE;
	}

	memset(findData, 0, sizeof(*findData));

	const char* fileName = g.gl_pathv[current++];
	const char* name = strrchr(fileName, '/');
	if (name == nullptr)
	{
		name = fileName;
	}
	else
	{
		++name;
	}

	size_t nameLen = strlen(name);
	if (nameLen >= sizeof(findData->cFileName))
	{
		errno = ENAMETOOLONG;
		return FALSE;
	}

	strcpy(findData->cFileName, name);

	// cAlternateFileName is allowed to be empty
	struct stat stats = {};
	if (stat(fileName, &stats) != 0)
	{
		return FALSE;
	}

	// Fill dwFileAttributes
	setAttributes(stats.st_mode, findData->dwFileAttributes);

	// File size
	findData->nFileSizeLow = stats.st_size;
	findData->nFileSizeHigh = stats.st_size >> 32;

	// File times
	// Birthtime is not supported
	findData->ftLastAccessTime = TimespecToFileTime(stats.st_atim);
	findData->ftLastWriteTime = TimespecToFileTime(stats.st_mtim);

	return TRUE;
}

WINBOOL FindHandle::fillStruct(WIN32_FIND_DATAW* findData)
{
	if (current >= g.gl_pathc)
	{
		errno = ERROR_NO_MORE_FILES;
		return FALSE;
	}

	memset(findData, 0, sizeof(*findData));

	const char* fileName = g.gl_pathv[current++];
	const char* name = strrchr(fileName, '/');
	if (name == nullptr)
	{
		name = fileName;
	}
	else
	{
		++name;
	}
	size_t nameLen = mbstowcs(findData->cFileName, name, sizeof(findData->cFileName));
	if (nameLen >= sizeof(findData->cFileName))
	{
		errno = ENAMETOOLONG;
		return FALSE;
	}

	// cAlternateFileName is allowed to be empty
	struct stat stats = {};
	if (stat(fileName, &stats) != 0)
	{
		return FALSE;
	}

	// Fill dwFileAttributes
	setAttributes(stats.st_mode, findData->dwFileAttributes);

	// File size
	findData->nFileSizeLow = stats.st_size;
	findData->nFileSizeHigh = stats.st_size >> 32;

	// File times
	// Birthtime is not supported
	findData->ftLastAccessTime = TimespecToFileTime(stats.st_atim);
	findData->ftLastWriteTime = TimespecToFileTime(stats.st_mtim);

	return TRUE;
}

static FindHandle* findFile(const char* lpFileName)
{
	std::unique_ptr<FindHandle> result(new FindHandle);

	switch (glob(lpFileName, GLOB_ERR | GLOB_NOSORT | GLOB_TILDE_CHECK, nullptr, &result->g))
	{
	case 0:
		{
			return result.release();
		}
	case GLOB_NOSPACE:
		{
			errno = ERROR_NOT_ENOUGH_MEMORY;
			break;
		}
	case GLOB_ABORTED:
		{
			// errno should be already set
			break;
		}
	case GLOB_NOMATCH:
		{
			errno = ERROR_FILE_NOT_FOUND;
			break;
		}
	default:
		{
			if (errno == 0)
			{
				errno = ERROR_NOT_SUPPORTED; // Any better idea?..
			}
		}
	}
	return nullptr;
}

HANDLE FindFirstFile(const char* lpFileName, WIN32_FIND_DATAA* lpFindFileData)
{
	std::unique_ptr<FindHandle> result(findFile(lpFileName));

	if (result && result->fillStruct(lpFindFileData))
	{
		return result.release();
	}
	return INVALID_HANDLE_VALUE;
}

HANDLE FindFirstFile(const wchar_t* lpFileName, WIN32_FIND_DATAW* lpFindFileData)
{
	char name[MAX_PATH];
    if (wcstombs(name, lpFileName, sizeof(name)) >= sizeof(name))
    {
        return INVALID_HANDLE_VALUE;
    }
	std::unique_ptr<FindHandle> result(findFile(name));

	if (result && result->fillStruct(lpFindFileData))
	{
		return result.release();
	}
	return INVALID_HANDLE_VALUE;
}

WINBOOL FindNextFile(HANDLE hFindFile, WIN32_FIND_DATAA* lpFindFileData)
{
	if (hFindFile == INVALID_HANDLE_VALUE || hFindFile->type != handle_t::handle_type::HANDLE_FIND)
	{
		errno = ERROR_INVALID_HANDLE;
		return FALSE;
	}

	return static_cast<FindHandle*>(hFindFile)->fillStruct(lpFindFileData);
}

WINBOOL FindNextFile(HANDLE hFindFile, WIN32_FIND_DATAW* lpFindFileData)
{
	if (hFindFile == INVALID_HANDLE_VALUE || hFindFile->type != handle_t::handle_type::HANDLE_FIND)
	{
		errno = ERROR_INVALID_HANDLE;
		return FALSE;
	}

	return static_cast<FindHandle*>(hFindFile)->fillStruct(lpFindFileData);
}

WINBOOL FindClose(volatile HANDLE& hFindFile)
{
	// Perhaps there is a point why FindClose exists in WinAPI but I don't found any
	// Still it does nothing with handles of different types
	if (hFindFile == INVALID_HANDLE_VALUE || hFindFile->type != handle_t::handle_type::HANDLE_FIND)
	{
		errno = ERROR_INVALID_HANDLE;
		return FALSE;
	}

	return CloseHandle(hFindFile);
}
