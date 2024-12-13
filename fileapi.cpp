#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <attr/xattr.h>
#include <linux/xattr.h>
#include <linux/posix_acl.h>
#include <linux/posix_acl_xattr.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#include <memory>

#include "acl.h"
#include "handleapi.h"
#include "fileapi.h"
#include "winbase.h"
#include "winerror.h"

#include "file_holder.h"

namespace
{
    // These blocks are close together to let system save space in lock table merging the locks
    constexpr off_t ModeOffset = 0;     // Base offset for open mode locks
    constexpr off_t ShareOffset = 3;    // Base offset for share deny locks

    // Offsets for particular locks
    constexpr off_t ReadLock    = 0;
    constexpr off_t WriteLock   = 1;
    constexpr off_t DeleteLock  = 2;
}


struct FileHandle: public handle_t
{
    ~FileHandle();
public:
    int handle = -1;
    std::string lockFileName;
    int lockFile = -1;

    FileHandle(int h): handle(h) { type = handle_type::HANDLE_FILE; }

    int GetWaitableHandle() override { return handle; }
};

FileHandle::~FileHandle()
{
    if (handle != -1)
    {
        if (lockFile != -1 && flock(handle, LOCK_EX) == 0)
        {
            struct flock lock;

            // Try to exclusively lock whole range.
            // If any lock is held by any other descriptor - it would fail.
            // If lock is success it is the only opened handle for this file
            // so we can delete lock file.
            lock.l_type = F_WRLCK;
            lock.l_whence = SEEK_SET;
            lock.l_start = ModeOffset;
            lock.l_len = ShareOffset + DeleteLock - ModeOffset;
            lock.l_pid = 0;
            if (fcntl(lockFile, F_OFD_SETLK, &lock) == 0)
            {
                shm_unlink(lockFileName.c_str());
            }

            flock(handle, LOCK_UN);
        }
        close(handle);
    }
    if (lockFile != -1)
        close(lockFile);
}

HANDLE CreateFileHandle(int handle)
{
    return new FileHandle(handle);
}

HANDLE CreateFile(const char* lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAtrributes, [[maybe_unused]]HANDLE hTemplateFile)
{
    // MSDN: If there are existing open handles to a file,
    // the call fails unless they were all opened with FILE_SHARE_DELETE share mode
    if (dwFlagsAndAtrributes & FILE_FLAG_DELETE_ON_CLOSE)
    {
        dwDesiredAccess |= DELETE;
    }
    DWORD thisMode = dwDesiredAccess & (FILE_READ_DATA | FILE_WRITE_DATA | DELETE); // These flags are mapped directly
    // These flags more tricky
    if (dwDesiredAccess & GENERIC_ALL)
    {
        thisMode |= FILE_READ_DATA | FILE_WRITE_DATA | DELETE;
    }
    if (dwDesiredAccess & GENERIC_READ)
    {
        thisMode |= FILE_READ_DATA;
    }
    if (dwDesiredAccess & GENERIC_WRITE)
    {
        thisMode |= FILE_WRITE_DATA;
    }

    int flags = O_PATH;
    // TODO: Somehow handle cases when file is requested to be opened for DELETE only
    switch (thisMode & (FILE_READ_DATA | FILE_WRITE_DATA))
    {
    case FILE_READ_DATA:
        {
            flags = O_RDONLY;
            break;
        }
    case FILE_WRITE_DATA:
        {
            flags = O_WRONLY;
            break;
        }
    case FILE_READ_DATA | FILE_WRITE_DATA:
        {
            flags = O_RDWR;
            break;
        }
    }
    if (lpSecurityAttributes == nullptr || lpSecurityAttributes->bInheritHandle != TRUE)
    {
        flags |= O_CLOEXEC;
    }

    // Handle dispositions
    switch (dwCreationDisposition)
    {
    case CREATE_NEW:
        {
            flags |= O_CREAT | O_EXCL;
            break;
        }
    case CREATE_ALWAYS:
        {
            flags |= O_CREAT | O_TRUNC;
            break;
        }
    case OPEN_EXISTING:
        {
            // Default behavior
            break;
        }
    case OPEN_ALWAYS:
        {
            // This deviates Windows specs by not setting errno to ERROR_ALREADY_EXISTS on success
            // but I don't want to create loop trying to open-create file which is not robust anyway.
            flags |= O_CREAT;
            break;
        }
    case TRUNCATE_EXISTING:
        {
            flags |= O_TRUNC;
            break;
        }
    default:
        {
            errno = ERROR_INVALID_PARAMETER;
            return INVALID_HANDLE_VALUE;
        }
    }

    bool defaultAccess = true;
    uid_t desiredUser = -1;
    gid_t desiredGroup = -1;
    mode_t mode = 0664; // Sane default
    acl::AceSet perms;

    // Check if custom security is requested for newly created file
    if (flags & O_CREAT)
    {
        if (lpSecurityAttributes != nullptr && lpSecurityAttributes->lpSecurityDescriptor != nullptr)
        {
            SECURITY_DESCRIPTOR* desc = reinterpret_cast<SECURITY_DESCRIPTOR*>(lpSecurityAttributes->lpSecurityDescriptor);
            // Check if owner or group must be set.
            // There is an option to play with effective fs uid but it would also affect opening.
            // Besides chown() allow to change group to any group that the user is in
            // while setfsgid is limited to the primary one.
            // So force deferred settings.
            if ((desc->Control & SE_OWNER_DEFAULTED) == 0 && desc->Owner != nullptr)
            {
                uint32_t id;
                SID_NAME_USE t = acl::recognizeSid(desc->Owner, id);
                if (t == SidTypeUser)
                {
                    if (id != geteuid())
                    {
                        desiredUser = id;
                        defaultAccess = false;
                    }
                }
                else
                {
                    errno = ERROR_INVALID_SID;
                    return INVALID_HANDLE_VALUE;
                }
            }
            if ((desc->Control & SE_GROUP_DEFAULTED) == 0 && desc->Group != nullptr)
            {
                uint32_t id;
                SID_NAME_USE t = acl::recognizeSid(desc->Group, id);
                if (t == SidTypeGroup)
                {
                    if (id != getegid())
                    {
                        desiredGroup = id;
                        defaultAccess = false;
                    }
                }
                else
                {
                    errno = ERROR_INVALID_SID;
                    return INVALID_HANDLE_VALUE;
                }
            }
            if ((desc->Control & SE_DACL_PRESENT) != 0)
            {
                if (desc->Dacl == nullptr)
                {
                    // NULL DACL == full access
                    mode = 0666;
                }
                else
                {
                    perms.owner = desiredUser == (uid_t)-1 ? geteuid() : desiredUser;
                    perms.group = desiredGroup == (gid_t)-1 ? getegid() : desiredGroup;
                    // Parse DACL and check if it may fit standard bitmask
                    if (!perms.mergeDacl(desc->Dacl))
                    {
                        return INVALID_HANDLE_VALUE;
                    }
                    if (perms.size() <= 3)
                    {
                        // Only mandatory entries are set
                        mode = perms.toMode();
                    }
                    else
                    {
                        // Attributes must be set later
                        defaultAccess = false;
                    }
                }
            }
        }
        // else default security is used

        if (defaultAccess && (dwFlagsAndAtrributes & FILE_ATTRIBUTE_READONLY) != 0)
        {
            // there is no dedicated R/O flag on Linux so just revoke write permissions
            mode &= ~(mode_t)0222;
        }
    }

    int h = open(lpFileName, flags, defaultAccess ? mode : 0000);
    if (h != -1)
    {
        bool failure = false;
        bool fileCreated = false;
        struct stat s;
        if (fstat(h, &s) == 0)
        {
            if (!defaultAccess      // Permissions must be set explicitly
                && (0777 & s.st_mode) == 0  // This file perhaps has been created but it may indeed has zero mode
                && s.st_uid == geteuid()    // More probability.
                && s.st_gid == getegid()    // Even more
                // struct stat has no creation time so we cannot check it here to reduce false positive probability even more
                )
            {
                // File has been created.
                fileCreated = true;

                // Set proper owner and permissions if they were specified in security attributes
                if (desiredUser != (uid_t)-1 || desiredGroup != (gid_t)-1)
                {
                    if (fchown(h, desiredUser, desiredGroup) == -1)
                    {
                        failure = true;
                    }
                    else
                    {
                        // On success adjust credentials in stat structure to match desired
                        if (desiredUser != (uid_t)-1)
                            s.st_uid = desiredUser;
                        if (desiredGroup != (gid_t)-1)
                            s.st_gid = desiredGroup;
                    }
                }

                // Set permissions
                if (!failure)
                {
                    SECURITY_DESCRIPTOR* desc = reinterpret_cast<SECURITY_DESCRIPTOR*>(lpSecurityAttributes->lpSecurityDescriptor);
                    if ((desc->Control & SE_DACL_PRESENT) != 0 && desc->Dacl != nullptr)
                    {
                        // Windows in such case owerwrites every existing permissions and so we have to do
                        acl::Attributes attr(perms.toAttr());
                        if (fsetxattr(h, XATTR_NAME_POSIX_ACL_ACCESS, attr.get(), attr.size(), 0) == -1)
                        {
                            failure = true;
                        }
                    }
                    else
                    {
                        if (fchmod(h, (~S_IFMT & s.st_mode) | mode) == -1)
                        {
                            failure = true;
                        }
                    }
                }
                s.st_mode |= mode;
            }
        }
        else
        {
            failure = true;
        }

        // If anything bad happened above and file has been created - file must not exists after return
        if (failure)
        {
            int olderr = errno;
            // At this point fileCreate may be false if file has been created with default security.
            // TODO: Fix that somehow
            if (fileCreated)
            {
                unlink(lpFileName); // And don't check for errors
            }
            close(h);
            errno = olderr;
            return INVALID_HANDLE_VALUE;
        }

        std::unique_ptr<FileHandle> result(new FileHandle(h));
        // WINE rely on server keeping all opened files in the same memory so there is no way to be compatible with it.

        // It is not written in Windows docs but request to open file with no desired access don't block it
        // even if share mode is NONE.
        if (thisMode != 0)
        {
            // Lock the main file while messing with lock file to avoid race conditions.
            // If it fail... nothing bad happen. Perhaps.
            // Documentation insists that this lock is released automatically on descriptor close
            // so we don't have to care about RAII holder
            int lockRes = flock(result->handle, LOCK_EX);

            // Problem here that RO descriptors cannot place WR locks and vice versa.
            // So we must create another object specifically for locking purpose.
            // It could be a real file in temp dir but shared memory seems to be better
            // though its ability to accept locks is not explicitly documented.

            result->lockFileName = "/WinCompLck.";
            result->lockFileName += std::to_string(major(s.st_dev));
            result->lockFileName += '.';
            result->lockFileName += std::to_string(minor(s.st_dev));
            result->lockFileName += '.';
            result->lockFileName += std::to_string(s.st_ino);

            // Allow access to everyone because there is no way to synchronize rights with the main file
            result->lockFile = shm_open(result->lockFileName.c_str(), O_CREAT | O_RDWR, 0666);
            if (result->lockFile == -1)
            {
                return INVALID_HANDLE_VALUE;
            }
            // Force the lock file to be accessible to anybody.
            if (fchmod(result->lockFile, s.st_mode & 0666) != 0)
            {
                return INVALID_HANDLE_VALUE;
            }

            struct flock lock;
            // First check that requested mode is not prohibited by already set locks.
            if (thisMode & FILE_READ_DATA)
            {
                lock.l_type = F_WRLCK;
                lock.l_whence = SEEK_SET;
                lock.l_start = ShareOffset + ReadLock;
                lock.l_len = 1;
                lock.l_pid = 0;
                if (fcntl(result->lockFile, F_OFD_GETLK, &lock) == -1)
                {
                    return INVALID_HANDLE_VALUE;
                }
                if (lock.l_type != F_UNLCK)
                {
                    errno = ERROR_SHARING_VIOLATION;
                    return INVALID_HANDLE_VALUE;
                }
            }
            if (thisMode & FILE_WRITE_DATA)
            {
                lock.l_type = F_WRLCK;
                lock.l_whence = SEEK_SET;
                lock.l_start = ShareOffset + WriteLock;
                lock.l_len = 1;
                lock.l_pid = 0;
                if (fcntl(result->lockFile, F_OFD_GETLK, &lock) == -1)
                {
                    return INVALID_HANDLE_VALUE;
                }
                if (lock.l_type != F_UNLCK)
                {
                    errno = ERROR_SHARING_VIOLATION;
                    return INVALID_HANDLE_VALUE;
                }
            }
            if (thisMode & DELETE)
            {
                lock.l_type = F_WRLCK;
                lock.l_whence = SEEK_SET;
                lock.l_start = ShareOffset + DeleteLock;
                lock.l_len = 1;
                lock.l_pid = 0;
                if (fcntl(result->lockFile, F_OFD_GETLK, &lock) == -1)
                {
                    return INVALID_HANDLE_VALUE;
                }
                if (lock.l_type != F_UNLCK)
                {
                    errno = ERROR_SHARING_VIOLATION;
                    return INVALID_HANDLE_VALUE;
                }
            }

            // Then check if some already opened handles conflict with requested share mode
            if ((dwShareMode & FILE_SHARE_READ) == 0)
            {
                lock.l_type = F_WRLCK;
                lock.l_whence = SEEK_SET;
                lock.l_start = ModeOffset + ReadLock;
                lock.l_len = 1;
                lock.l_pid = 0;
                if (fcntl(result->lockFile, F_OFD_GETLK, &lock) == -1)
                {
                    return INVALID_HANDLE_VALUE;
                }
                if (lock.l_type != F_UNLCK)
                {
                    errno = ERROR_SHARING_VIOLATION;
                    return INVALID_HANDLE_VALUE;
                }
            }
            if ((dwShareMode & FILE_SHARE_WRITE) == 0)
            {
                lock.l_type = F_WRLCK;
                lock.l_whence = SEEK_SET;
                lock.l_start = ModeOffset + WriteLock;
                lock.l_len = 1;
                lock.l_pid = 0;
                if (fcntl(result->lockFile, F_OFD_GETLK, &lock) == -1)
                {
                    return INVALID_HANDLE_VALUE;
                }
                if (lock.l_type != F_UNLCK)
                {
                    errno = ERROR_SHARING_VIOLATION;
                    return INVALID_HANDLE_VALUE;
                }
            }
            if ((dwShareMode & FILE_SHARE_DELETE) == 0)
            {
                lock.l_type = F_WRLCK;
                lock.l_whence = SEEK_SET;
                lock.l_start = ModeOffset + DeleteLock;
                lock.l_len = 1;
                lock.l_pid = 0;
                if (fcntl(result->lockFile, F_OFD_GETLK, &lock) == -1)
                {
                    return INVALID_HANDLE_VALUE;
                }
                if (lock.l_type != F_UNLCK)
                {
                    errno = ERROR_SHARING_VIOLATION;
                    return INVALID_HANDLE_VALUE;
                }
            }

            // Finally set locks for open mode and share mode
            if (thisMode & FILE_READ_DATA)
            {
                lock.l_type = F_RDLCK;
                lock.l_whence = SEEK_SET;
                lock.l_start = ModeOffset + ReadLock;
                lock.l_len = 1;
                lock.l_pid = 0;
                if (fcntl(result->lockFile, F_OFD_SETLK, &lock) == -1)
                {
                    return INVALID_HANDLE_VALUE;
                }
            }
            if (thisMode & FILE_WRITE_DATA)
            {
                lock.l_type = F_RDLCK;
                lock.l_whence = SEEK_SET;
                lock.l_start = ModeOffset + WriteLock;
                lock.l_len = 1;
                lock.l_pid = 0;
                if (fcntl(result->lockFile, F_OFD_SETLK, &lock) == -1)
                {
                    return INVALID_HANDLE_VALUE;
                }
            }
            if (thisMode & DELETE)
            {
                lock.l_type = F_RDLCK;
                lock.l_whence = SEEK_SET;
                lock.l_start = ModeOffset + DeleteLock;
                lock.l_len = 1;
                lock.l_pid = 0;
                if (fcntl(result->lockFile, F_OFD_SETLK, &lock) == -1)
                {
                    return INVALID_HANDLE_VALUE;
                }
            }
            if ((dwShareMode & FILE_SHARE_READ) == 0)
            {
                lock.l_type = F_RDLCK;
                lock.l_whence = SEEK_SET;
                lock.l_start = ShareOffset + ReadLock;
                lock.l_len = 1;
                lock.l_pid = 0;
                if (fcntl(result->lockFile, F_OFD_SETLK, &lock) == -1)
                {
                    return INVALID_HANDLE_VALUE;
                }
            }
            if ((dwShareMode & FILE_SHARE_WRITE) == 0)
            {
                lock.l_type = F_RDLCK;
                lock.l_whence = SEEK_SET;
                lock.l_start = ShareOffset + WriteLock;
                lock.l_len = 1;
                lock.l_pid = 0;
                if (fcntl(result->lockFile, F_OFD_SETLK, &lock) == -1)
                {
                    return INVALID_HANDLE_VALUE;
                }
            }
            if ((dwShareMode & FILE_SHARE_DELETE) == 0)
            {
                lock.l_type = F_RDLCK;
                lock.l_whence = SEEK_SET;
                lock.l_start = ShareOffset + DeleteLock;
                lock.l_len = 1;
                lock.l_pid = 0;
                if (fcntl(result->lockFile, F_OFD_SETLK, &lock) == -1)
                {
                    return INVALID_HANDLE_VALUE;
                }
            }

            // Finally we can release EX lock if it has been aquired
            if (lockRes == 0)
                flock(result->handle, LOCK_UN);
        }

        // TODO: Move unlink to close from this temporary stub
        if (dwFlagsAndAtrributes & FILE_FLAG_DELETE_ON_CLOSE)
        {
            unlink(lpFileName);
        }

        if (dwFlagsAndAtrributes & FILE_ATTRIBUTE_TEMPORARY)
        {
            // I hope kernel can recognize this hint and avoid flushes as much as possible
            const uint64_t hint = RWH_WRITE_LIFE_SHORT;
            fcntl(h, F_SET_FILE_RW_HINT, &hint);
        }

        // Per specification the error must be set if CREATE_ALWAYS or OPEN_ALWAYS was used and file already exists
        errno = !fileCreated && (flags & O_CREAT) != 0 ? ERROR_ALREADY_EXISTS : 0;

        return result.release();
    }
    return INVALID_HANDLE_VALUE;
}

HANDLE CreateFile(const wchar_t* lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAtrributes, HANDLE hTemplateFile)
{
    if (lpFileName == nullptr)
    {
        errno = ERROR_PATH_NOT_FOUND;
        return INVALID_HANDLE_VALUE;
    }

    // Skip \\?\ prefix if used
    constexpr ptrdiff_t prefixLen = 4;
    if (wcsncmp(lpFileName, L"\\\\?\\", prefixLen) == 0)
    {
        lpFileName += prefixLen;
    }

    // Convert Unicode name into ANSI and reuse ANSI version of the function
    ssize_t n = wcstombs(nullptr, lpFileName, 0);
    if (n == (ssize_t)-1)
    {
        errno = ERROR_INVALID_NAME;
        return INVALID_HANDLE_VALUE;
    }
    std::unique_ptr<char[]> name(new char[n + 1]);
    wcstombs(name.get(), lpFileName, n + 1);

    return CreateFile(name.get(), dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAtrributes, hTemplateFile);
}

WINBOOL ReadFile(HANDLE hFile, void* lpBuffer, DWORD nNumberOfBytesToRead, DWORD* lpNumberOfBytesRead, OVERLAPPED* lpOverlapped)
{
    if (lpNumberOfBytesRead != nullptr)
        *lpNumberOfBytesRead = 0;

    if (hFile == INVALID_HANDLE_VALUE || hFile->type != handle_t::handle_type::HANDLE_FILE)
    {
        errno = ERROR_INVALID_HANDLE;
        return FALSE;
    }

    if (lpBuffer == nullptr)
    {
        errno = ERROR_NOACCESS;
        return FALSE;
    }

    FileHandle* handle = static_cast<FileHandle*>(hFile);

    ssize_t r = 0;
    if (lpOverlapped == nullptr)
    {
        // Synchronous execution
        // Despite MSDN Windows (10 at least) allows both lpOverlapped and lpNumberOfBytesRead to be nullptr
        // if (lpNumberOfBytesRead == nullptr)
        // {
        //     errno = ERROR_INVALID_PARAMETERS;
        //     return FALSE;
        // }
        r = read(handle->handle, lpBuffer, nNumberOfBytesToRead);
        if (r == -1)
        {
            return FALSE;
        }
    }
    else
    {
        off64_t offset = lpOverlapped->OffsetHigh;
        offset <<= 32;
        offset |= lpOverlapped->Offset;
        r = pread(handle->handle, lpBuffer, nNumberOfBytesToRead, offset);
        if (r == -1)
        {
            return FALSE;
        }
        if (r == 0)
        {
            errno = ERROR_HANDLE_EOF;
            return FALSE;
        }
        offset += r;
        lpOverlapped->Offset = offset & 0xFFFFFFFF;
        lpOverlapped->OffsetHigh = offset >> 32;
    }
    if (lpNumberOfBytesRead != nullptr)
    {
        *lpNumberOfBytesRead = r;
    }
    return TRUE;
}

WINBOOL WriteFile(HANDLE hFile, const void* lpBuffer, DWORD nNumberOfBytesToWrite, DWORD* lpNumberOfBytesWritten, OVERLAPPED* lpOverlapped)
{
    if (lpNumberOfBytesWritten != nullptr)
        *lpNumberOfBytesWritten = 0;

    if (hFile == INVALID_HANDLE_VALUE || hFile->type != handle_t::handle_type::HANDLE_FILE)
    {
        errno = ERROR_INVALID_HANDLE;
        return FALSE;
    }

    if (lpBuffer == nullptr)
    {
        errno = ERROR_NOACCESS;
        return FALSE;
    }

    FileHandle* handle = static_cast<FileHandle*>(hFile);

    ssize_t r = 0;
    if (lpOverlapped == nullptr)
    {
        // Synchronous execution
        // Despite MSDN Windows (10 at least) allows both lpOverlapped and lpNumberOfBytesWritten to be nullptr
        // if (lpNumberOfBytesRead == nullptr)
        // {
        //     errno = ERROR_INVALID_PARAMETERS;
        //     return FALSE;
        // }
        r = write(handle->handle, lpBuffer, nNumberOfBytesToWrite);
        if (r == -1)
        {
            return FALSE;
        }
    }
    else
    {
        off64_t offset = lpOverlapped->OffsetHigh;
        offset <<= 32;
        offset |= lpOverlapped->Offset;
        r = pwrite(handle->handle, lpBuffer, nNumberOfBytesToWrite, offset);
        if (r == -1)
        {
            return FALSE;
        }
        offset += r;
        lpOverlapped->Offset = offset & 0xFFFFFFFF;
        lpOverlapped->OffsetHigh = offset >> 32;
    }
    if (lpNumberOfBytesWritten != nullptr)
    {
        *lpNumberOfBytesWritten = r;
    }
    return TRUE;
}

WINBOOL FlushFileBuffers(HANDLE hFile)
{
    if (hFile == INVALID_HANDLE_VALUE || hFile->type != handle_t::handle_type::HANDLE_FILE)
    {
        errno = ERROR_INVALID_HANDLE;
        return FALSE;
    }

    FileHandle* handle = static_cast<FileHandle*>(hFile);

    return fsync(handle->handle) == -1?FALSE:TRUE;
}

DWORD SetFilePointer(HANDLE hFile, int32_t lDistanceToMove, LONG* lpDistanceToMoveHigh, DWORD dwMoveMethod)
{
    LARGE_INTEGER newPos;
    newPos.QuadPart = lDistanceToMove; // Let sign expand
    if (lpDistanceToMoveHigh != nullptr)
    {
        // Override high bits
        newPos.HighPart = *lpDistanceToMoveHigh;
    }
    if (SetFilePointerEx(hFile, newPos, &newPos, dwMoveMethod) == FALSE)
    {
        return INVALID_SET_FILE_POINTER;
    }
    errno = 0;
    if (lpDistanceToMoveHigh != nullptr)
    {
        *lpDistanceToMoveHigh = newPos.HighPart;
    }
    else
    {
        if (newPos.HighPart != 0)
            return INVALID_SET_FILE_POINTER;
    }
    return newPos.LowPart;
}

WINBOOL SetFilePointerEx(HANDLE hFile, const LARGE_INTEGER& liDistanceToMove, LARGE_INTEGER* lpNewFilePointer, DWORD dwMoveMethod)
{
    static_assert(FILE_BEGIN == SEEK_SET, "FILE_BEGIN wrong");
    static_assert(FILE_CURRENT == SEEK_CUR, "FILE_CURRENT wrong");
    static_assert(FILE_END == SEEK_END, "FILE_END wrong");

    if (hFile == INVALID_HANDLE_VALUE || hFile->type != handle_t::handle_type::HANDLE_FILE)
    {
        errno = ERROR_INVALID_HANDLE;
        return FALSE;
    }

    FileHandle* handle = static_cast<FileHandle*>(hFile);

    off64_t newPos = lseek64(handle->handle, liDistanceToMove.QuadPart, dwMoveMethod);
    if (newPos == (__off64_t)-1)
        return FALSE;

    if (lpNewFilePointer != nullptr)
    {
        lpNewFilePointer->QuadPart = newPos;
    }
    return TRUE;
}

WINBOOL SetFileInformationByHandle(HANDLE hFile, FILE_INFO_BY_HANDLE_CLASS FileInformationClass, void* lpFileInformation, DWORD dwBufferSize)
{
    if (hFile == INVALID_HANDLE_VALUE || hFile->type != handle_t::handle_type::HANDLE_FILE)
    {
        errno = ERROR_INVALID_HANDLE;
        return FALSE;
    }
    if (lpFileInformation == nullptr)
    {
        errno = ERROR_NOACCESS;
        return FALSE;
    }

    FileHandle* handle = static_cast<FileHandle*>(hFile);
    switch (FileInformationClass)
    {
    case FileEndOfFileInfo:
        {
            if (dwBufferSize != sizeof(FILE_END_OF_FILE_INFO))
            {
                errno = ERROR_BAD_LENGTH;
                return false;
            }
            FILE_END_OF_FILE_INFO* info = reinterpret_cast<FILE_END_OF_FILE_INFO*>(lpFileInformation);

            return ftruncate64(handle->handle, info->EndOfFile.QuadPart) == 0?TRUE:FALSE;

            break;
        }
    default:
        {
            errno = ERROR_INVALID_PARAMETER;
            return FALSE;
        }
    }
    return TRUE;
}

WINBOOL SetEndOfFile(HANDLE hFile)
{
    if (hFile == INVALID_HANDLE_VALUE || hFile->type != handle_t::handle_type::HANDLE_FILE)
    {
        errno = ERROR_INVALID_HANDLE;
        return FALSE;
    }

    FileHandle* handle = static_cast<FileHandle*>(hFile);

    off64_t CurPos = lseek64(handle->handle, 0, SEEK_CUR);
    if (CurPos >= 0 && ftruncate64(handle->handle, CurPos) != -1)
    {
        return TRUE;
    }

    return FALSE;
}

WINBOOL GetFileSizeEx(HANDLE hFile, LARGE_INTEGER* lpFileSize)
{
    if (hFile == INVALID_HANDLE_VALUE || hFile->type != handle_t::handle_type::HANDLE_FILE)
    {
        errno = ERROR_INVALID_HANDLE;
        return FALSE;
    }
    if (lpFileSize == nullptr)
    {
        errno = ERROR_NOACCESS;
        return FALSE;
    }

    FileHandle* handle = static_cast<FileHandle*>(hFile);

    struct stat64 stats;
    if (fstat64(handle->handle, &stats) == -1)
    {
        return FALSE;
    }
    static_assert(sizeof(stats.st_size) == sizeof(lpFileSize->QuadPart), "Size of size mismatch");
    lpFileSize->QuadPart = stats.st_size;
    return TRUE;
}

const char* getTempEnv()
{
    const char* tmpdir = getenv("TMPDIR"); // Main POSIX candidate
    if (tmpdir == nullptr)
        tmpdir = getenv("TMP"); // These are non-standard
    if (tmpdir == nullptr)
        tmpdir = getenv("TEMP");
    if (tmpdir == nullptr)
        tmpdir = "/tmp/"; // Last resort fallback

    return tmpdir;
}

DWORD GetTempPath(DWORD nBufferLength, char* lpBuffer)
{
    const char* tmpdir = getTempEnv();
    size_t l = strlen(tmpdir);

    // Clean buffer just in case
    if (lpBuffer != nullptr && nBufferLength != 0)
    {
        *lpBuffer = '\0';
    }

    // Sanity checks
    if (l > 0)
    {
        if (l < nBufferLength)
        {
            memcpy(lpBuffer, tmpdir, l + 1);
            // Ensure directory separator at the end
            if (lpBuffer[l - 1] != '/')
            {
                lpBuffer[l++] = '/';
                if (l < nBufferLength)
                {
                    lpBuffer[l] = '\0';
                }
                else
                {
                    // Missing separator cannot fit, pretend that whole result is failure
                    *lpBuffer = '\0';
                }
            }
        }
    }
    return l;
}

DWORD GetTempPath(DWORD nBufferLength, wchar_t* lpBuffer)
{
    const char* tmpdir = getTempEnv();
    size_t l = strlen(tmpdir);

    // Clean buffer just in case
    if (lpBuffer != nullptr && nBufferLength != 0)
    {
        *lpBuffer = L'\0';
    }

    // Sanity checks
    if (l > 0)
    {
        size_t n = mbstowcs(nullptr, tmpdir, 0);
        if (n == (size_t)-1)
        {
            //errno = ???
            return 0;
        }

        if (n < nBufferLength)
        {
            n = mbstowcs(lpBuffer, tmpdir, nBufferLength);
            if (lpBuffer[n - 1] != L'/')
            {
                lpBuffer[n++] = L'/';
                if (n < nBufferLength)
                {
                    lpBuffer[n] = L'\0';
                }
                else
                {
                    // Missing separator cannot fit, pretend that whole result is failure
                    *lpBuffer = L'\0';
                }
            }
        }
        return n;
    }
    return 0;
}

// It doesn't really belong here but let it stay for a while...
FILE* _wfopen(const wchar_t* filename, const wchar_t* mode)
{
    size_t fn_len = wcstombs(nullptr, filename, 0);
    if (fn_len == (size_t)-1)
        return nullptr;

    std::unique_ptr<char[]> fn(new char[++fn_len]);
    wcstombs(fn.get(), filename, fn_len);

    size_t m_len = wcstombs(nullptr, mode, 0);
    if (m_len == (size_t)-1)
        return nullptr;

    std::unique_ptr<char[]> m(new char[++m_len]);
    wcstombs(m.get(), mode, m_len);

    return fopen(fn.get(), m.get());
}

WINBOOL CreateDirectory(const char* lpPathName, [[maybe_unused]]SECURITY_ATTRIBUTES* lpSecurityAttributes)
{
    if (mkdir(lpPathName, 0777) == 0) // For now request full access. TODO: recognize lpSecurityAttributes
    {
        return TRUE;
    }
    return FALSE;
}

WINBOOL DeleteFile(const char* lpFileName)
{
    if (unlink(lpFileName) == -1)
    {
        return FALSE;
    }
    return TRUE;
}

#ifdef TESTING
#include <stdio.h>
#include "windows.h"

HANDLE foo(int mode)
{
    DWORD acc = 0;
    DWORD sh = 0;
    if (mode & 0x00001)
        acc |= GENERIC_READ;
    if (mode & 0x00002)
        acc |= GENERIC_WRITE;
    if (mode & 0x00004)
        acc |= DELETE;
    if (mode & 0x00008)
        sh |= FILE_SHARE_READ;
    if (mode & 0x00010)
        sh |= FILE_SHARE_WRITE;
    if (mode & 0x00020)
        sh |= FILE_SHARE_DELETE;
    HANDLE result = CreateFile("aaa.txt", acc, sh, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (result != INVALID_HANDLE_VALUE)
    {
        printf("\tSUCCESS");
    }
    else
    {
        DWORD err = GetLastError();
        if (err == ERROR_SHARING_VIOLATION)
        {
            printf("\tSHARE_VIOLATION");
        }
        else
        {
            printf("\t%ld", err);
        }
    }
    return result;
}

int main()
{
    for (int mode1 = 0; mode1 < 077; mode1++)
    {
        for (int mode2 = 0; mode2 < 077; mode2++)
        {
            for (int mode3 = 0; mode3 < 077; mode3++)
            {
                printf("%x\t%x\t%x", mode1, mode2, mode3);
                HANDLE f1 = foo(mode1);
                HANDLE f2 = foo(mode2);
                HANDLE f3 = foo(mode3);
                printf("\n");
                if (f1 != INVALID_HANDLE_VALUE)
                    CloseHandle(f1);
                if (f2 != INVALID_HANDLE_VALUE)
                    CloseHandle(f2);
                if (f3 != INVALID_HANDLE_VALUE)
                    CloseHandle(f3);
            }
        }
    }
}
#endif // TESTING
