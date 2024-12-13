
#include "handleapi.h"
#include "winsock2.h"

int epoll_ctl(int epfd, int op, HANDLE fd, struct epoll_event* event)
{
	if (fd == INVALID_HANDLE_VALUE || fd->type == handle_t::handle_type::HANDLE_INVALID)
	{
		errno = EBADF;
		return -1;
	}
	return epoll_ctl(epfd, op, fd->GetWaitableHandle(), event);
}
