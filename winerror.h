/**************** WinAPI error codes **********************/

#pragma once

#include <errno.h>
#include "winnt.h"

#define ERROR_SUCCESS			(0)
#define NO_ERROR				(0) // DDE version
#define ERROR_ACCESS_DENIED		EACCES
#define ERROR_INVALID_HANDLE    EBADF
#define ERROR_FILE_NOT_FOUND    ENOENT
#define ERROR_PATH_NOT_FOUND    ENOENT	// It is not good that Linux makes no difference between them
#define ERROR_ALREADY_EXISTS	EEXIST
#define ERROR_INVALID_PARAMETER EINVAL
#define ERROR_NOACCESS          EFAULT
#define ERROR_NOT_ENOUGH_MEMORY	ENOMEM
#define ERROR_INSUFFICIENT_BUFFER	ENAMETOOLONG // May be not the best choice
#define ERROR_CALL_NOT_IMPLEMENTED	ENOSYS
#define ERROR_SHARING_VIOLATION	EAGAIN	// At least this is what used to be returned from flock()
#define ERROR_INVALID_NAME		EILSEQ	// Good for Unicode functions may be a bad choice for ANSI

// Linux seems to miss these errors so we can assign them to somethig beyond of usual range
#define ERROR_BAD_LENGTH        1024
#define ERROR_HANDLE_EOF        1038
#define ERROR_UNKNOWN_REVISION	1305	// May be it can be EINVAL
#define ERROR_NO_SUCH_GROUP		1319
#define ERROR_INVALID_ACL		1336
#define ERROR_INVALID_SID		1337
#define ERROR_INVALID_SECURITY_DESCR	1338
#define ERROR_BAD_INHERITANCE_ACL	1340
#define ERROR_FILE_CORRUPT		1392

#define ERROR_CTX_LICENSE_NOT_AVAILABLE	7053

#define S_OK		0
#define S_FALSE		1

const HRESULT SEVERITY_SUCCESS	= 0;
const HRESULT SEVERITY_ERROR	= 1;

const HRESULT FACILITY_WIN32	= 7;

const HRESULT E_NOTIMPL		= 0x80004001;

#define RPC_S_UUID_LOCAL_ONLY	1824
