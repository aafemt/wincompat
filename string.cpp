#include <errno.h>
#include <unistd.h>
#include <string.h>

int strcat_s(char* strDestination, size_t numberOfElements, const char* strSource)
{
	if (strDestination == nullptr)
	{
		return (errno = EINVAL);
	}
	if (strSource == nullptr)
	{
		strDestination[0] = '\0';
		return (errno = EINVAL);
	}
	if (numberOfElements == 0)
	{
		// Even if MSDN insists that strDestination[0] must be set to 0, IMHO it is not safe
		strDestination[0] = '\0';
		return (errno = ERANGE);
	}
	char* eos = reinterpret_cast<char*>(memchr(strDestination, 0, numberOfElements));
	if (eos == nullptr)
	{
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
