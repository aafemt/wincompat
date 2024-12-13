#pragma once

#include "winnt.h"

enum MULTIPLE_TRUSTEE_OPERATION
{
	NO_MULTIPLE_TRUSTEE
};

enum TRUSTEE_FORM
{
	TRUSTEE_IS_SID,
	TRUSTEE_IS_NAME,
	TRUSTEE_BAD_FORM,
	TRUSTEE_IS_OBJECTS_AND_SID,
	TRUSTEE_IS_OBJECTS_AND_NAME
};

enum TRUSTEE_TYPE
{
	TRUSTEE_IS_UNKNOWN,
	TRUSTEE_IS_USER,
	TRUSTEE_IS_GROUP
};

struct TRUSTEE_W
{
	TRUSTEE_W* pMultipleTrustee;
	MULTIPLE_TRUSTEE_OPERATION MultipleTrusteeOperation;
	TRUSTEE_FORM TrusteeForm;
	TRUSTEE_TYPE TrusteeType;
	WCHAR* ptstrName;
};
struct TRUSTEE_A
{
	TRUSTEE_A* pMultipleTrustee;
	MULTIPLE_TRUSTEE_OPERATION MultipleTrusteeOperation;
	TRUSTEE_FORM TrusteeForm;
	TRUSTEE_TYPE TrusteeType;
	WCHAR* ptstrName;
};

enum ACCESS_MODE
{
	NOT_USED_ACCESS,
	GRANT_ACCESS,
	SET_ACCESS,
	DENY_ACCESS,
	REVOKE_ACCESS
};

// Bitmask defining inheritance of ACL entry
// It is BYTe because is stored in ACE::AceFlags field
constexpr BYTE NO_INHERITANCE						= 0x0;
constexpr BYTE SUB_OBJECTS_ONLY_INHERIT				= 0x1;
constexpr BYTE SUB_CONTAINERS_ONLY_INHERIT			= 0x2;
constexpr BYTE SUB_CONTAINERS_AND_OBJECTS_INHERIT	= 0x3;
constexpr BYTE INHERIT_NO_PROPAGATE					= 0x4;
constexpr BYTE INHERIT_ONLY							= 0x8;

struct EXPLICIT_ACCESS_W
{
	ACCESS_MASK grfAccessPermissions;
	ACCESS_MODE grfAccessMode;
	DWORD grfInheritance;
	TRUSTEE_W Trustee;
};
struct EXPLICIT_ACCESS_A
{
	DWORD grfAccessPermissions;
	ACCESS_MODE grfAccessMode;
	DWORD grfInheritance;
	TRUSTEE_A Trustee;
};
#ifdef UNICODE
typedef EXPLICIT_ACCESS_W EXPLICIT_ACCESS;
#else
typedef EXPLICIT_ACCESS_A EXPLICIT_ACCESS;
#endif

enum SE_OBJECT_TYPE
{
	SE_UNKNOWN_OBJECT_TYPE,
	SE_FILE_OBJECT
};
