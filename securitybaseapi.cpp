#include <errno.h>
#include <memory>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include "handleapi.h"
#include "securitybaseapi.h"
#include "winerror.h"

WINBOOL InitializeSecurityDescriptor(SECURITY_DESCRIPTOR* pSecurityDescriptor, DWORD dwRevision)
{
	if (dwRevision != SECURITY_DESCRIPTOR_REVISION)
	{
		errno = ERROR_UNKNOWN_REVISION;
		return FALSE;
	}
	memset(pSecurityDescriptor, 0, sizeof(SECURITY_DESCRIPTOR));
	pSecurityDescriptor->Revision = dwRevision;
	return TRUE;
}

WINBOOL CreateWellKnownSid(WELL_KNOWN_SID_TYPE WellKnownSidType, [[maybe_unused]] PSID DomainSid, PSID pSid, DWORD* cbSid)
{
	if (pSid == nullptr || cbSid == nullptr)
	{
		errno = ERROR_INVALID_PARAMETER;
		return FALSE;
	}

	uint32_t subAuth = 0;
	BYTE count = 1; // At least RID must be present
	BYTE authority = SECURITY_NULL_SID_AUTHORITY;
	uint32_t RID = 0;

	switch (WellKnownSidType)
	{
	case WinBuiltinAdministratorsSid:
		{
			authority = SECURITY_NON_UNIQUE_AUTHORITY; // This authority is not used by Windows
			count = 2;
			subAuth = SidTypeGroup;
			RID = 0; // 'root' group is close enough to Windows 'Administrators' group and it is common on all Linuxes
			break;
		}
	default:
		{
			errno = ERROR_INVALID_PARAMETER;
			return FALSE;
		}
	}

	if (authority == SECURITY_NULL_SID_AUTHORITY && DomainSid != nullptr) // In absence of better flag
	{
		count += DomainSid->SubAuthorityCount;
	}

	DWORD len = sizeof(SID) + count * sizeof(uint32_t);
	if (len > *cbSid)
	{
		errno = ERROR_INSUFFICIENT_BUFFER;
		return FALSE;
	}

	if (authority == SECURITY_NULL_SID_AUTHORITY && DomainSid != nullptr)
	{
		// Windows doesn't check validity of DomainSid, it simply append known RID to the end of it
		authority = DomainSid->IdentifierAuthority;
		for (unsigned i = 0; i < DomainSid->SubAuthorityCount; i++)
		{
			pSid->SubAuthority[i] = DomainSid->SubAuthority[i];
		}
	}
	else
	{
		if (count >= 2)
		{
			pSid->SubAuthority[0] = subAuth;
		}
	}
	pSid->IdentifierAuthority = authority;
	pSid->SubAuthorityCount = count;
	pSid->SubAuthority[count - 1] = RID;
	*cbSid = len;
	return TRUE;
}

namespace
{
	class TokenHandle: public handle_t
	{
	protected:
		DWORD allowedAccess;

		virtual WINBOOL tokenUser(void* lpInformation, DWORD dwInformationLength, DWORD* pdwReturnLength) = 0;
	public:
		TokenHandle(DWORD access): allowedAccess(access)
		{
			type = handle_type::HANDLE_TOKEN;
		}
		int GetWaitableHandle() override
		{
			errno = ERROR_ACCESS_DENIED;
			return -1;
		}
		WINBOOL getTokenUser(void* lpInformation, DWORD dwInformationLength, DWORD* pdwReturnLength);
	};

	WINBOOL TokenHandle::getTokenUser(void* lpInformation, DWORD dwInformationLength, DWORD* pdwReturnLength)
	{
		// Check whatever possible
		if (dwInformationLength < sizeof(TOKEN_USER))
		{
			errno = ERROR_INSUFFICIENT_BUFFER;
			return FALSE;
		}
		if ((allowedAccess & TOKEN_QUERY) == 0)
		{
			errno = ERROR_ACCESS_DENIED;
			return FALSE;
		}
		// Buffer must have suitable alignment for TOKEN_USER
		if ((reinterpret_cast<uintptr_t>(lpInformation) & (alignof(TOKEN_USER) - 1)) != 0)
		{
			errno = ERROR_NOACCESS;
			return FALSE;
		}

		// Then get real information from derived classes
		return tokenUser(lpInformation, dwInformationLength, pdwReturnLength);
	}

	class ThisProcessTokenHandle: public TokenHandle
	{
	public:
		ThisProcessTokenHandle(DWORD access): TokenHandle(access)
		{
		}
		WINBOOL tokenUser(void* lpInformation, DWORD dwInformationLength, DWORD* pdwReturnLength) override;
	};

	WINBOOL ThisProcessTokenHandle::tokenUser(void* lpInformation, DWORD dwInformationLength, DWORD* pdwReturnLength)
	{
		// Calculate offsets and needed size at first
		char* buffer = static_cast<char*>(lpInformation);
		*pdwReturnLength = (sizeof(TOKEN_USER) + alignof(SID) - 1) & ~(alignof(SID) - 1);
		SID* sid = reinterpret_cast<SID*>(buffer + *pdwReturnLength);
		*pdwReturnLength += sizeof(SID) + sizeof(SID::SubAuthority[0]) * 2;

		// Before this check we must not touch buffer because lpInformation can be nullptr and dwInformationLength is zero.
		if (*pdwReturnLength > dwInformationLength)
		{
			errno = ERROR_INSUFFICIENT_BUFFER;
			return FALSE;
		}

		sid->SubAuthorityCount = 2;
		sid->IdentifierAuthority = SECURITY_NON_UNIQUE_AUTHORITY;
		sid->SubAuthority[0] = SidTypeUser;
		uid_t uid = getuid();
		if (uid == 0)
		{
			// Consider sudo to be the same as elevated mode and try to return original user
			char* e = getenv("SUDO_UID");
			if (e != nullptr)
			{
				uid = atol(e); // TODO: check what sudo set for IDs bigger that INT_MAX
			}
		}
		sid->SubAuthority[1] = uid;

		TOKEN_USER* token = reinterpret_cast<TOKEN_USER*>(buffer);
		token->User.Sid = sid;
		token->User.Attributes = 0;

		return TRUE;
	}

}

// Though this function is declared in processthreadsapi.h, it suits here better to have access to token hanlde
WINBOOL OpenProcessToken(HANDLE ProcessHandle, DWORD DesiredAccess, HANDLE* TokenHandle)
{
	// TODO: Implement real process handle
    if (ProcessHandle != (HANDLE)(-1))
	{
		errno = ERROR_INVALID_HANDLE;
		return FALSE;
	}

	if (TokenHandle == nullptr)
	{
		errno = ERROR_NOACCESS;
		return FALSE;
	}

	*TokenHandle = new ThisProcessTokenHandle(DesiredAccess);

	return TRUE;
}

WINBOOL GetTokenInformation(HANDLE hToken, TOKEN_INFORMATION_CLASS InformationClass,
							void* lpInformation, DWORD dwInformationLength, DWORD* pdwReturnLength)
{
	if (hToken == INVALID_HANDLE_VALUE || hToken->type != handle_t::handle_type::HANDLE_TOKEN)
	{
		errno = ERROR_INVALID_HANDLE;
		return FALSE;
	}
	if (pdwReturnLength == nullptr)
	{
		errno = ERROR_NOACCESS;
		return FALSE;
	}

	TokenHandle* handle = static_cast<TokenHandle*>(hToken);
	switch (InformationClass)
	{
	case TokenUser:
		{
			return handle->getTokenUser(lpInformation, dwInformationLength, pdwReturnLength);
		}
	default:
		{
			errno = ERROR_INVALID_PARAMETER;
			return FALSE;
		}
	}
	return TRUE;
}

WINBOOL SetSecurityDescriptorOwner(SECURITY_DESCRIPTOR* pSecurityDescriptor, PSID pOwner, WINBOOL bOwnerDefaulted)
{
	if (pSecurityDescriptor == nullptr) // Windows doesn't check that
	{
		errno = ERROR_INVALID_PARAMETER;
		return FALSE;
	}
	if (pSecurityDescriptor->Revision != SECURITY_DESCRIPTOR_REVISION)
	{
		errno = ERROR_INVALID_SECURITY_DESCR;
		return FALSE;
	}
	pSecurityDescriptor->Owner = pOwner;
	if (bOwnerDefaulted)
	{
		pSecurityDescriptor->Control |= SE_OWNER_DEFAULTED;
	}
	else
	{
		pSecurityDescriptor->Control &= ~SE_OWNER_DEFAULTED;
	}
	return TRUE;
}

WINBOOL SetSecurityDescriptorDacl(SECURITY_DESCRIPTOR* pSecurityDescriptor, WINBOOL bDaclPresent, PACL pDacl, WINBOOL bDaclDefaulted)
{
	if (pSecurityDescriptor == nullptr) // Windows doesn't check that
	{
		errno = ERROR_INVALID_PARAMETER;
		return FALSE;
	}
	if (pSecurityDescriptor->Revision != SECURITY_DESCRIPTOR_REVISION)
	{
		errno = ERROR_INVALID_SECURITY_DESCR;
		return FALSE;
	}
	if (bDaclPresent)
	{
		pSecurityDescriptor->Control |= SE_DACL_PRESENT;
		pSecurityDescriptor->Dacl = pDacl;
		if (bDaclDefaulted)
		{
			pSecurityDescriptor->Control |= SE_DACL_DEFAULTED;
		}
		else
		{
			pSecurityDescriptor->Control &= ~SE_DACL_DEFAULTED;
		}
	}
	else
	{
		pSecurityDescriptor->Control &= ~(SE_DACL_PRESENT | SE_DACL_DEFAULTED);
		pSecurityDescriptor->Dacl = nullptr;
	}
	return TRUE;
}

WINBOOL SetSecurityDescriptorControl(SECURITY_DESCRIPTOR* pSecurityDescriptor,
										SECURITY_DESCRIPTOR_CONTROL ControlBitsOfInterest, SECURITY_DESCRIPTOR_CONTROL ControlBitsToSet)
{
	if (pSecurityDescriptor == nullptr) // Windows doesn't check that
	{
		errno = ERROR_INVALID_PARAMETER;
		return FALSE;
	}
	if (pSecurityDescriptor->Revision != SECURITY_DESCRIPTOR_REVISION)
	{
		errno = ERROR_INVALID_SECURITY_DESCR;
		return FALSE;
	}
	pSecurityDescriptor->Control &= ~ControlBitsOfInterest;
	pSecurityDescriptor->Control |= ControlBitsToSet;
	return TRUE;
}

#ifdef TESTING

#include <stdio.h>
#include "sddl.h"
#include "windows.h"

int main()
{
	HANDLE token;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
	{
		printf("OpenProcessToken error %lu\n", GetLastError());
		return 1;
	}
	char buffer[1024] = {};
	DWORD l = 0;
	if (!GetTokenInformation(token, TokenUser, buffer, sizeof(buffer), &l))
	{
		printf("GetTokenInformation user name failed %lu\n", GetLastError());
		return 1;
	}
	else
	{
		printf("Token information length - %lu\n", l);
	}
	TOKEN_USER* user = reinterpret_cast<TOKEN_USER*>(buffer);
	char* sidString;
	if (ConvertSidToStringSid(user->User.Sid, &sidString))
	{
		printf("User SID: %s (%u), Attributes: %lx\n", sidString, reinterpret_cast<SID*>(user->User.Sid)->SubAuthorityCount, user->User.Attributes);
		LocalFree(sidString);
	}
	CloseHandle(token);
}

#endif // TESTING
