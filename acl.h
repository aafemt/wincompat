#pragma once

// This is internal header for ACL-related routines

#include <map>
#include <memory>

#include "winnt.h"

namespace acl
{
	struct AceSid
	{
		uint16_t type = 0;
		uint32_t id = -1;
		bool operator== (const AceSid& r) const
		{
			return type == r.type && id == r.id;
		}
		bool operator< (const AceSid& r) const
		{
			return type < r.type || (type == r.type && id < r.id);
		}
	};

	struct AcePerm
	{
		// Here is a tricky part:
		// Windows check ACEs in order and who come first - win.
		// But at the same time it allowed more that one ACE for the same id but different permissions.
		// So we must properly handle such cases "read denied + write allowed" and "read denied + read allowed".
		// The bad news that current Linux ACL works differently and apply the whole set of permissions
		// to current access token so properhandling is actually pointless because it is impossible
		// to handle cases "write is allowed but read is not denied for user + read allowed for group".
		// Windows in this case will allow read access but Linux deny.
		// TODO: Return to this problem when new ACL model borrowed from NFS is implemented in Linux kernel.
		mode_t inherited = 0;
		mode_t set = 0;
		mode_t value = 0;
	};

	class Attributes : public std::unique_ptr<unsigned char[]>
	{
		size_t l = 0;
	public:
		Attributes(size_t newSize): std::unique_ptr<unsigned char[]>(new unsigned char[newSize]), l(newSize)
		{
		}
		size_t size()
		{
			return l;
		}
	};

	// Linux kernel expects ACEs to have exact order and return EINVAL otherwise
	// so ordered map is exactly what we need.
	class AceSet : public std::map<AceSid, AcePerm>
	{
	public:
		// User and group which are used as supposed owner and group of object during ACL merges
		uid_t owner = -1;
		gid_t group = -1;

		bool mergeDacl(PACL dacl);
		mode_t toMode();
		Attributes toAttr();
	};

	SID_NAME_USE recognizeSid(PSID sid, uint32_t& id);
}
