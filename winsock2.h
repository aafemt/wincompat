/* Compatibility header */

#pragma once

#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <poll.h>
#include <netdb.h>
#include <unistd.h>

#include "winnt.h"

#ifdef __WIN32__
#error This file is not for Windows!
#endif // __WIN32__

typedef int SOCKET;
typedef union sockaddr_gen {
  struct sockaddr Address;
  struct sockaddr_in AddressIn;
  struct sockaddr_in6 AddressIn6;
} sockaddr_gen;

typedef struct WSAData
{
  int wVersion;
  int wHighVersion;
} WSADATA;

typedef struct sockaddr_storage SOCKADDR_STORAGE;

const SOCKET INVALID_SOCKET = -1;
const int SOCKET_ERROR = -1;
const int SD_RECEIVE = SHUT_RD;
const int SD_SEND = SHUT_WR;
const int SD_BOTH = SHUT_RDWR;

inline int WSAStartup(WORD wVersionRequired, WSADATA* lpWSAData)
{
  // Pretend to support everything
  lpWSAData->wVersion = wVersionRequired;
  lpWSAData->wHighVersion = wVersionRequired;
  return 0;
}
inline int WSAGetLastError() { return errno; }
inline void WSASetLastError(int err) { errno = err; }
inline int closesocket(SOCKET s) { return close(s); }
int epoll_ctl(int epfd, int op, HANDLE fd, struct epoll_event* event);
inline int epoll_close(int fd) { return close(fd); }
inline int WSAPoll(pollfd* fdArray, nfds_t nfds, int timeout)
{
  return poll(fdArray, nfds, timeout);
}
inline int ioctlsocket(SOCKET s, long cmd, u_long* argp)
{
  if (cmd == FIONBIO)
  {
    int flags = fcntl(s, F_GETFL, 0);
    if (flags == -1)
      return SOCKET_ERROR;
    if (*argp == 0)
      flags &= ~O_NONBLOCK;
    else
      flags |= O_NONBLOCK;
    return fcntl(s, F_SETFL, flags);
  }
  return 0;
}

#define WSAEWOULDBLOCK EWOULDBLOCK
#define WSAEINTR EINTR
#define WSAEINPROGRESS EINPROGRESS
#define WSAECONNRESET ECONNRESET
#define WSAENOTSOCK ENOTSOCK
#define WSAESHUTDOWN ESHUTDOWN
#define WSAECONNREFUSED ECONNREFUSED
#define WSAETIMEDOUT ETIMEDOUT
// Network is always initialized on Linux and there is no way to kick it up
// I set it to the least possible error that can be handled by simple retry.
#define WSANOTINITIALISED ENETDOWN
#define WSAEAFNOSUPPORT EAFNOSUPPORT
