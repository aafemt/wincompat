/**************************************************
*
* Implementation of environment-related functions
*
****************************************************/

#include <string.h>
#include <unistd.h>

#include "winbase.h"

DWORD GetCurrentDirectory(DWORD nBufferLength, char* lpBuffer)
{
	char* value = get_current_dir_name();
	if (value == nullptr)
	{
		return 0;
	}

	size_t len = strlen(value);
	if (len < nBufferLength)
	{
		strcpy(lpBuffer, value);
	}
	// else... It is strange, but GetCurrentDirectory doesn't set ERROR_BUFFER_OVERFLOW
	free(value);

	return len;
}
