#include <fcntl.h>
#include <grp.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

#include "shlobj.h"
#include "winerror.h"

#define HRESULT_FROM_WIN32(code) (SEVERITY_ERROR << 31 | FACILITY_WIN32 << 16 | ((code) & 0xFFFF))

[[maybe_unused]]static bool getGidByName(const char* name, gid_t& gid)
{
	long bufLen = sysconf(_SC_GETGR_R_SIZE_MAX);
	if (bufLen == -1)
	{
		bufLen = 1024;
	}
	char* buf = (char*)malloc(bufLen);
	if (buf == nullptr)
	{
		errno = ENOMEM; // man doesn't mention if errno is set so make sure it is
		return false;
	}
	group result;
	group* res = nullptr;
	int err = 0;

	while ((err = getgrnam_r(name, &result, buf, bufLen, &res)) == ERANGE)
	{
		free(buf);
		bufLen *= 2;
		if (bufLen > 1024*1024) // sanity check
		{
			return false;
		}
		buf = (char*)malloc(bufLen);
		if (buf == nullptr)
		{
			errno = ENOMEM; // man doesn't mention if errno is set so make sure it is
			return false;
		}
	}
	free(buf); // it is actually not needed at all
	if (err != 0)
	{
		errno = err;
		return false;
	}
	if (res == nullptr)
	{
		errno = ERROR_NO_SUCH_GROUP;
		return false;
	}
	gid = result.gr_gid;
	return true;
}

// This routine follows FHS. https://refspecs.linuxfoundation.org/fhs.shtml
HRESULT SHGetFolderPath([[maybe_unused]]HWND hwnd, int csidl, [[maybe_unused]]HANDLE hToken, [[maybe_unused]]DWORD dwFlags, char* pszPath)
{
	bool create = (csidl & CSIDL_FLAG_CREATE) != 0;
	int op = csidl & ~CSIDL_FLAG_MASK;
	mode_t defaultMode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;
	switch (op)
	{
	case CSIDL_COMMON_APPDATA:
		{
			strcpy(pszPath, "/var/opt");
			break;
		}
	default:
		return E_NOTIMPL;
	}
	if (create)
	{
		if (mkdir(pszPath, defaultMode) != 0)
		{
			if (errno == EEXIST)
			{
				return S_OK;
			}
			else
			{
				return HRESULT_FROM_WIN32(errno);
			}
		}
		// mkdir is affected by umask so after creation we must adjust attributes
		if (chmod(pszPath, defaultMode) != 0)
		{
			return HRESULT_FROM_WIN32(errno);
		}
	}
	return S_OK;
}
