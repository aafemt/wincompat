#pragma once

#include "winnt.h"

WINBOOL InitializeSecurityDescriptor(SECURITY_DESCRIPTOR* pSecurityDescriptor, DWORD dwRevision);
WINBOOL CreateWellKnownSid(WELL_KNOWN_SID_TYPE WellKnownSidType, PSID DomainSid, PSID pSid, DWORD* cbSid);
WINBOOL GetTokenInformation(HANDLE hToken, TOKEN_INFORMATION_CLASS InformationClass,
							void* lpInformation, DWORD dwInformationLength, DWORD* pdwReturnLength);
WINBOOL SetSecurityDescriptorOwner(SECURITY_DESCRIPTOR* pSecurityDescriptor, PSID pOwner, WINBOOL bOwnerDefaulted);
WINBOOL SetSecurityDescriptorDacl(SECURITY_DESCRIPTOR* pSecurityDescriptor, WINBOOL bDaclPresent, PACL pDacl, WINBOOL bDaclDefaulted);
WINBOOL SetSecurityDescriptorControl(SECURITY_DESCRIPTOR* pSecurityDescriptor,
										SECURITY_DESCRIPTOR_CONTROL ControlBitsOfInterest, SECURITY_DESCRIPTOR_CONTROL ControlBitsToSet);
