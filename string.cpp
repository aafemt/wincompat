#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <wchar.h>

int strcat_s(char* strDestination, size_t numberOfElements, const char* strSource)
{
	if (strDestination == nullptr)
	{
		return (errno = EINVAL);
	}
	if (numberOfElements == 0)
	{
		// Though MSDN insists that strDestination[0] must be set to 0 and ERANGE returned, the reality is different
		return (errno = EINVAL);
	}
	char* eos = reinterpret_cast<char*>(memchr(strDestination, 0, numberOfElements));
	if (eos == nullptr)
	{
		// Though MSDN insists that unterminated destination must be not modified, the reality is different
		strDestination[0] = '\0';
		return (errno = EINVAL);
	}
	if (strSource == nullptr)
	{
		strDestination[0] = '\0';
		return (errno = EINVAL);
	}
	size_t srcLength = strlen(strSource);
	if ((eos - strDestination) + srcLength >= numberOfElements)
	{
		strDestination[0] = '\0';
		return (errno = ERANGE);
	}
	strcpy(eos, strSource);
	return 0;
}

int _wcsnicmp(const wchar_t* string1, const wchar_t* string2, size_t count)
{
	return wcsncasecmp(string1, string2, count);
}

int _wcsicmp(const wchar_t* string1, const wchar_t* string2)
{
	return wcscasecmp(string1, string2);
}
