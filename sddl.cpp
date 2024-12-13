#include <stdlib.h>
#include <stdio.h>

#include "sddl.h"
#include "winerror.h"

WINBOOL ConvertSidToStringSid(PSID Sid, char** StringSid)
{
	if (Sid == nullptr || StringSid == nullptr)
	{
		errno = ERROR_INVALID_PARAMETER;
		return FALSE;
	}
	size_t stringSize = 1 + 1 + 1 + 1 + 10 + Sid->SubAuthorityCount * (1 + 10) + 1;
	char* buffer = (char*)malloc(stringSize);
	if (buffer == nullptr)
	{
		errno = ERROR_NOT_ENOUGH_MEMORY;
		return FALSE;
	}
	char* p = buffer;
	int n = 0;
	int r = snprintf(p, stringSize, "S-1-%u", Sid->IdentifierAuthority);
	while (r > 0 && stringSize > 1 && n < Sid->SubAuthorityCount)
	{
		stringSize -= r;
		p += r;
		r = snprintf(p, stringSize, "-%u", Sid->SubAuthority[n]);
		++n;
	}
	if (r < 0)
	{
		free(buffer);
		return FALSE;
	}

	*StringSid = buffer;
	return TRUE;
}
