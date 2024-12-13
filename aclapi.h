#pragma once

#include "accctrl.h"

DWORD SetEntriesInAcl(unsigned cCountOfExplicitEntries, EXPLICIT_ACCESS_W* pListOfExplicitEntries, PACL OldAcl, PACL* NewAcl);
DWORD SetNamedSecurityInfo(char* pObjectName, SE_OBJECT_TYPE ObjectType, SECURITY_INFORMATION SecurityInfo,
						PSID psidOwner, PSID psidGroup, PACL pDacl, PACL pSacl);
