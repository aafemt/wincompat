#include <fcntl.h>
#include <memory>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <sys/stat.h>
#include <type_traits>
#include <unistd.h>
// Somewhere above is declared ssize_t required below so order of includes is important
#include <attr/xattr.h>
#include <linux/xattr.h>
#include <linux/posix_acl.h>
#include <linux/posix_acl_xattr.h>

#include "acl.h"
#include "aclapi.h"
#include "winerror.h"

#include "file_holder.h"

DWORD SetEntriesInAcl(unsigned cCountOfExplicitEntries, EXPLICIT_ACCESS_W* pListOfExplicitEntries, PACL OldAcl, PACL* NewAcl)
{
	// TODO: Make this function properly merge new ACL into old ACL
	if (OldAcl != nullptr || cCountOfExplicitEntries == 0)
	{
		return ERROR_INVALID_PARAMETER;
	}
	if (pListOfExplicitEntries == nullptr)
	{
		return ERROR_NOACCESS;
	}
	size_t resultLength = sizeof(ACL) + cCountOfExplicitEntries * (sizeof(ACE) + SECURITY_MAX_SID_SIZE);

	*NewAcl = (ACL*)malloc(resultLength);
	if (*NewAcl == nullptr)
	{
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	// Initialize new ACL
	(*NewAcl)->AclRevision = ACL_REVISION;
	(*NewAcl)->AclSize = resultLength;
	(*NewAcl)->AclCount = cCountOfExplicitEntries;

	// TODO: Put ACEs in proper order: DENY first, ALLOW then
	char* p = (*NewAcl)->AceBuffer;
	for (unsigned i = 0; i < cCountOfExplicitEntries; i++)
	{
		const EXPLICIT_ACCESS_W& ea = pListOfExplicitEntries[i];
		ACE* ace = reinterpret_cast<ACE*>(p);
		size_t size = sizeof(ACE);
		if (ea.grfAccessMode == SET_ACCESS || ea.grfAccessMode == GRANT_ACCESS)
		{
			ace->AceType = ACCESS_ALLOWED_ACE_TYPE;
		}
		else
		{
			ace->AceType = ACCESS_DENIED_ACE_TYPE;
		}
		ace->AceFlags = ea.grfInheritance;
		ace->Mask = ea.grfAccessPermissions;
		switch (ea.Trustee.TrusteeForm)
		{
		case TRUSTEE_IS_SID:
			{
				SID* srcSid = reinterpret_cast<SID*>(ea.Trustee.ptstrName);
				ace->Sid = *srcSid;
				for (unsigned j = 0; j < ace->Sid.SubAuthorityCount; j++)
				{
					ace->Sid.SubAuthority[j] = srcSid->SubAuthority[j];
				}
				size += sizeof(uint32_t) * ace->Sid.SubAuthorityCount;
				break;
			}
		default:
			{
				free(*NewAcl);
				*NewAcl = nullptr;
				return ERROR_INVALID_PARAMETER;
			}
		}
		p += size;
	}

	return ERROR_SUCCESS;
}

namespace acl
{
	/*
	struct AclHolder
	{
		acl_t value = nullptr;
		AclHolder() = default;
		AclHolder(acl_t acl)
		{
			value = acl;
		}
		~AclHolder()
		{
			if (value != nullptr)
			{
				acl_free(value);
			}
		}
		explicit operator bool()
		{
			return value != nullptr;
		}
		int addAce(acl_tag_t type, uint32_t id, acl_perm_t rights)
		{
			acl_entry_t e;
			acl_permset_t perm;
			if (acl_create_entry(&value, &e) == -1
				|| acl_set_tag_type(e, type) == -1
				|| acl_set_qualifier(e, &id) == -1
				|| acl_get_permset(e, &perm) == -1
				|| acl_add_perm(perm, rights) == -1)
			{
				return -1;
			}
			return 0;
		}
		int addMask()
		{
			return acl_calc_mask(&value);
		}
		void init(int count)
		{
			value = acl_init(count);
		}
	};
	*/

	constexpr uint32_t MAX_RID_PER_AUTHORITY = 100;
	constexpr uint32_t WORLD_RID = SECURITY_WORLD_SID_AUTHORITY * MAX_RID_PER_AUTHORITY + SECURITY_WORLD_RID;
	constexpr uint32_t OWNER_RID = SECURITY_CREATOR_SID_AUTHORITY * MAX_RID_PER_AUTHORITY + SECURITY_CREATOR_OWNER_RID;
	constexpr uint32_t GROUP_RID = SECURITY_CREATOR_SID_AUTHORITY * MAX_RID_PER_AUTHORITY + SECURITY_CREATOR_GROUP_RID;

	SID_NAME_USE recognizeSid(PSID sid, uint32_t& id)
	{
		switch (sid->IdentifierAuthority)
		{
		case SECURITY_WORLD_SID_AUTHORITY:
		case SECURITY_CREATOR_SID_AUTHORITY:
			{
				if (sid->SubAuthorityCount < 1)
					return SidTypeInvalid;

				id = sid->IdentifierAuthority * MAX_RID_PER_AUTHORITY + sid->SubAuthority[sid->SubAuthorityCount - 1];
				return SidTypeAlias;
			}
		case SECURITY_NON_UNIQUE_AUTHORITY:
			{
				if (sid->SubAuthorityCount != 2)
					return SidTypeInvalid;

				id = sid->SubAuthority[1];
				return static_cast<SID_NAME_USE>(sid->SubAuthority[0]);
			}
		default:
			break;
		}
		return SidTypeUnknown;
	}

	mode_t recognizeAccessMask(uint32_t mask)
	{
		mode_t result = 0;

		// Recognize generic rights
		if (mask & GENERIC_ALL)
		{
			return ACL_READ | ACL_WRITE | ACL_EXECUTE;
		}
		if (mask & GENERIC_EXECUTE)
		{
			result |= ACL_EXECUTE;
		}
		if (mask & GENERIC_WRITE)
		{
			result |= ACL_WRITE;
		}
		if (mask & GENERIC_READ)
		{
			result |= ACL_READ;
		}

		// Recognize standard rights
		//if (mask & READ_CONTROL)

		// Recognize specific rights
		if (mask & FILE_TRAVERSE) // The same as FILE_EXECUTE
		{
			result |= ACL_EXECUTE;
		}
		if (mask & FILE_READ_DATA)
		{
			result |= ACL_READ;
		}
		if (mask & FILE_WRITE_DATA)
		{
			result |= ACL_WRITE;
		}
		return result;
	}

	std::string getParentName(const char* name)
	{
		struct free_deleter
		{
			void operator()(char* ptr) const
			{
				free(ptr);
			}

		};

		std::unique_ptr<char, free_deleter> path(realpath(name, nullptr));
		if (strcmp(path.get(), "/") == 0)
		{
			// No parent for root
			return "";
		}

		// Description of dirname is vague so do it by hand instead of risking to get static buffer
		char* sep = strrchr(path.get(), '/');
		if (sep == nullptr)
		{
			// This is strange, root must be here always
			return "";
		}

		if (sep == path.get())
		{
			// the parent is root
			return "/";
		}

		*sep = '\0';
		return path.get();
	}

	uint16_t fromLe(__le16* value)
	{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		// Let's pray that hardware is tolerant to misalignment
		return *value;
#else
#error big endian not supported
#endif
	}

	uint32_t fromLe(__le32* value)
	{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		return *value;
#else
#error big endian not supported
#endif
	}

	void toLe(uint16_t value, __le16* buffer)
	{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		memcpy(buffer, &value, sizeof(value));
#else
#error big endian not supported
#endif
	}

	void toLe(uint32_t value, __le32* buffer)
	{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
		memcpy(buffer, &value, sizeof(value));
#else
#error big endian not supported
#endif
	}

	// Add more bits to existing mode
	void applyPerm(mode_t newMode, mode_t& oldMask, mode_t& oldMode, bool allow)
	{
		newMode &= ~oldMask; // Do not owerwrite existing bits because first come - win
		oldMask |= newMode;  // Mark new bits as set
		if (allow)
			oldMode |= newMode;  // Set them
		//else
		//	oldMode &= ~newMode; // Clear them. This is actually no-op.because of the first line

		// Problem here is with crazy ACLs such as "allow read, deny read, allow write" and "read write" request.
		// Windows will reject the request and so do we
		// but at the same time the request on another crazy ACL "allow read write, deny read" will be OK for Windows.
	}

	bool AceSet::mergeDacl(PACL dacl)
	{
		// AcePerm is initialized to "no access" which is good in this case
		AcePerm& ownerPerm = operator[](AceSid{ACL_USER_OBJ, (uint32_t)ACL_UNDEFINED_ID});
		AcePerm& groupPerm = operator[](AceSid{ACL_GROUP_OBJ, (uint32_t)ACL_UNDEFINED_ID});
		/*AcePerm& otherPerm =*/ operator[](AceSid{ACL_OTHER, (uint32_t)ACL_UNDEFINED_ID});

		// POSIX permissions are not cumulative so we have to spread "everyone" permissions to "every one" by hand.
		// We cannot use single pass and defer applying of "everyone" permissions to the end
		// because order is important and ACL "everyone allow" - "user deny" would end up in wrong
		// permissions.

		// First pass: gather all user and group SIDs
		char* p = dacl->AceBuffer;
		for (WORD i = 0; i < dacl->AclCount && p < dacl->AceBuffer + dacl->AclSize - sizeof(ACL); ++i)
		{
			ACE* ace = reinterpret_cast<ACE*>(p);
			p += sizeof(ACE) + ace->Sid.SubAuthorityCount * sizeof(SID::SubAuthority[0]);

			switch (ace->AceType)
			{
			case ACCESS_ALLOWED_ACE_TYPE:
			case ACCESS_DENIED_ACE_TYPE:
				{
					if ((ace->AceFlags & INHERIT_ONLY) != 0)
					{
						// Skip these ACEs
						continue;
					}

					uint32_t id = 0;
					SID_NAME_USE t = recognizeSid(&(ace->Sid), id);

					switch (t)
					{
					case SidTypeAlias:
						break; // Default ACEs are already added above
					case SidTypeUser:
						{
							if (id == 0)
							{
								break; // Root already has every right
							}
							operator[]({ ACL_USER, id });
							break;
						}
					case SidTypeGroup:
						{
							// Root group is not so special.
							operator[]({ ACL_GROUP, id });
							break;
						}
					default:
						{
							errno = ERROR_INVALID_SID;
							return false;
						}
					}
					break;
				}
			default:
				{
					errno = ERROR_CALL_NOT_IMPLEMENTED;
					return false;
				}
			}
		}

		// Second pass: apply permissions to them
		p = dacl->AceBuffer;
		for (WORD i = 0; i < dacl->AclCount && p < dacl->AceBuffer + dacl->AclSize - sizeof(ACL); ++i)
		{
			ACE* ace = reinterpret_cast<ACE*>(p);
			p += sizeof(ACE) + ace->Sid.SubAuthorityCount * sizeof(SID::SubAuthority[0]);

			bool allow = false;
			switch (ace->AceType)
			{
			case ACCESS_ALLOWED_ACE_TYPE:
				allow = true;
				[[fallthrough]];

			case ACCESS_DENIED_ACE_TYPE:
				{
					if ((ace->AceFlags & INHERIT_ONLY) != 0)
					{
						continue;
					}

					uint32_t id = 0;
					SID_NAME_USE t = recognizeSid(&(ace->Sid), id);
					mode_t m = recognizeAccessMask(ace->Mask);

					// Add bits set in m into ACE value but do not owerwrite those that already set (or clear)
					// because in Windows ACL first come ACE wins.

					// Because POSIX ACLs are not cumulative, conversion is a little tricky.
					// Details are in http://www.citi.umich.edu/projects/nfsv4/rfc/draft-ietf-nfs4-acl-mapping-05.txt

					switch (t)
					{
					case SidTypeAlias:
						{
							switch (id)
							{
							case WORLD_RID:
								{
									// What is allowed to everyone - is allowed to every one
									for (auto& a : *this)
									{
										AcePerm& perm = a.second;
										applyPerm(m, perm.set, perm.value, allow);
									}
									break;
								}
							case OWNER_RID:
								{
									// What is allowed to owner - allowed only to owner (whoever it is)
									applyPerm(m, ownerPerm.set, ownerPerm.value, allow);
									break;
								}
							case GROUP_RID:
								{
									// TODO: be smart and expand group permissions to every member
									applyPerm(m, groupPerm.set, groupPerm.value, allow);
									break;
								}
							default:
								errno = ERROR_INVALID_SID;
								return false;
							}
							break;
						}
					case SidTypeUser:
						{
							if (id != 0)
							{
								// Changing if owner must not mess with explicitly granted rights
								AcePerm& perm = operator[]({ ACL_USER, id });
								applyPerm(m, perm.set, perm.value, allow);
							}

							if (id == owner) // this user is also owner
							{
								applyPerm(m, ownerPerm.set, ownerPerm.value, allow);
							}
							break;
						}
					case SidTypeGroup:
						{
							// Changing if owner group must not mess with explicitly granted rights
							AcePerm& perm = operator[]({ ACL_GROUP, id });
							applyPerm(m, perm.set, perm.value, allow);

							if (id == group) // this group is also owner group
							{
								applyPerm(m, groupPerm.set, groupPerm.value, allow);
							}
							break;
						}
					default:
						{
							errno = ERROR_INVALID_SID;
							return false;
						}
					}
					break;
				}
			default:
				{
					errno = ERROR_CALL_NOT_IMPLEMENTED;
					return false;
				}
			}
		}
		return true;
	}

	mode_t AceSet::toMode()
	{
		// Use loop here to avoid unnecessary creation of entries.
		mode_t result = 0000;
		for (const auto& i : *this)
		{
			switch (i.first.type)
			{
			case ACL_USER_OBJ:
				{

					result |= (((i.second.inherited & ~i.second.set) | i.second.value) & 07) << 6;
					break;
				}
			case ACL_GROUP_OBJ:
				{
					result |= (((i.second.inherited & ~i.second.set) | i.second.value) & 07) << 3;
					break;
				}
			case ACL_OTHER:
				{
					result |= ((i.second.inherited & ~i.second.set) | i.second.value) & 07;
					break;
				}
			}
		}
		return result;
	}

	Attributes AceSet::toAttr()
	{
		size_t len = sizeof(posix_acl_xattr_header) + (size() + 1) * sizeof(posix_acl_xattr_entry);

		Attributes buffer(len);

		posix_acl_xattr_header* h = reinterpret_cast<posix_acl_xattr_header*>(buffer.get());
		toLe(POSIX_ACL_XATTR_VERSION, &h->a_version);

		posix_acl_xattr_entry* e = reinterpret_cast<posix_acl_xattr_entry*>(h + 1);
		uint16_t mask = 0;
		for (auto& a : *this)
		{
			uint16_t actualMode = ((a.second.inherited & ~a.second.set) | a.second.value) & 07;
			switch (a.first.type)
			{
			case ACL_USER_OBJ: break;
			case ACL_USER:
			case ACL_GROUP_OBJ:
			case ACL_GROUP:
				{
					// Not sure that this is right way to calculate mask but libacl do it this way.
					mask |= actualMode;
					break;
				}
			case ACL_OTHER:
				{
					// Put mask before last item
					toLe(ACL_MASK, &e->e_tag);
					toLe(mask, &e->e_perm);
					toLe((uint32_t)ACL_UNDEFINED_ID, &e->e_id);
					++e;
					break;
				}
			}
			toLe(a.first.type, &e->e_tag);
			toLe(actualMode, &e->e_perm);
			toLe(a.first.id, &e->e_id);
			++e;
		}
		return buffer;
	}
}

using namespace acl;

DWORD SetNamedSecurityInfo(char* pObjectName, SE_OBJECT_TYPE ObjectType, SECURITY_INFORMATION SecurityInfo,
						PSID psidOwner, PSID psidGroup, PACL pDacl, PACL)
{
	switch (ObjectType)
	{
	case SE_FILE_OBJECT:
		{
			FileHolder f(open(pObjectName, O_PATH));
			if (!f)
			{
				return errno;
			}
			struct stat st = {};
			if (fstat(f.get(), &st) != 0)
			{
				return errno;
			}
			if ((SecurityInfo & (OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION)) != 0)
			{
				// change owner user or group
				uid_t user = -1; // Do not change
				gid_t group = -1; // Do not change

				if (SecurityInfo & OWNER_SECURITY_INFORMATION)
				{
					if (psidOwner == nullptr)
						return ERROR_INVALID_PARAMETER;
					if (psidOwner->IdentifierAuthority != SECURITY_NON_UNIQUE_AUTHORITY)
						return ERROR_INVALID_SID; // TODO: Handle other types as well
					if (psidOwner->SubAuthorityCount != 2)
						return ERROR_INVALID_SID;
					switch (psidOwner->SubAuthority[0])
					{
					case SidTypeUser:
						user = psidOwner->SubAuthority[1];
						break;
					case SidTypeGroup:
						// On Windows groups can be owner but Linux cannot handle it
						group = psidOwner->SubAuthority[1];
						break;
					default:
						return ERROR_INVALID_SID;
					}
				}

				// TODO: Test if Windows does handle it at all
				if (SecurityInfo & GROUP_SECURITY_INFORMATION)
				{
					if (psidGroup == nullptr)
						return ERROR_INVALID_PARAMETER;
					if (psidGroup->IdentifierAuthority != SECURITY_NON_UNIQUE_AUTHORITY)
						return ERROR_INVALID_SID; // TODO: Handle other types as well
					if (psidGroup->SubAuthorityCount != 2 || psidGroup->SubAuthority[0] != SidTypeGroup)
						return ERROR_INVALID_SID;

					group = psidGroup->SubAuthority[1];
				}

				if ((user != (uid_t)-1 && user != st.st_uid) || (group != (gid_t)-1 && group != st.st_gid))
				{
					if (fchownat(f.get(), "", user, group, AT_EMPTY_PATH) != 0)
						return errno;
					st.st_uid = user;
					st.st_gid = group;
				}
			}
			if ((SecurityInfo & DACL_SECURITY_INFORMATION) != 0)
			{
				// Mess with privilegies
				bool isDir = (st.st_mode & S_IFMT) == S_IFDIR;

				mode_t fullAccess = ACL_READ | ACL_WRITE;
				if (isDir)
					fullAccess |= ACL_EXECUTE;

				AceSet forThis, forChildren;

				if ((SecurityInfo & PROTECTED_DACL_SECURITY_INFORMATION) == 0)
				{
					// TODO: Hanlde case when UNPROTECTED_DACL_SECURITY_INFORMATION is not set as well so
					// object must preserve current inheritance mode.

					// Get default ACL from parent directory
					std::string parent = getParentName(pObjectName);
					if (!parent.empty())
					{
						std::unique_ptr<unsigned char[]> buffer(new unsigned char[65536]);
						ssize_t l = getxattr(parent.c_str(), XATTR_NAME_POSIX_ACL_DEFAULT, buffer.get(), 65536);
						if (l < 0)
						{
							if (errno == ENODATA)
							{
								// No ACL set. Use parent's access mask.
								struct stat parentStats = {};
								if (stat(parent.c_str(), &parentStats) != 0)
								{
									return errno;
								}
								forThis.emplace(AceSid{ACL_USER_OBJ, (uint32_t)ACL_UNDEFINED_ID}, AcePerm{(parentStats.st_mode >> 6) & 007, 0, 0});
								forThis.emplace(AceSid{ACL_GROUP_OBJ, (uint32_t)ACL_UNDEFINED_ID}, AcePerm{(parentStats.st_mode >> 3) & 007, 0, 0});
								forThis.emplace(AceSid{ACL_OTHER, (uint32_t)ACL_UNDEFINED_ID}, AcePerm{parentStats.st_mode & 007, 0, 0});
							}
							else
								return errno;
						}
						else
						{
							posix_acl_xattr_header* h = reinterpret_cast<posix_acl_xattr_header*>(buffer.get());
							if (fromLe(&h->a_version) != POSIX_ACL_XATTR_VERSION)
							{
								return ERROR_UNKNOWN_REVISION;
							}

							for (posix_acl_xattr_entry* e = reinterpret_cast<posix_acl_xattr_entry*>(h + 1); reinterpret_cast<unsigned char*>(e) < buffer.get() + l; ++e)
							{
								if (e->e_tag != ACL_MASK)
								{
									forThis.emplace(AceSid{fromLe(&e->e_tag), fromLe(&e->e_id)}, AcePerm{fromLe(&e->e_perm), 0, 0});
								}
							}
						}
					}
					else // there is no parent such as we are called for a file in root directory
					{
						// Leave basic rights as is
						forThis.emplace(AceSid{ACL_USER_OBJ, (uint32_t)ACL_UNDEFINED_ID}, AcePerm{(st.st_mode >> 6) & 007, 0, 0});
						forThis.emplace(AceSid{ACL_GROUP_OBJ, (uint32_t)ACL_UNDEFINED_ID}, AcePerm{(st.st_mode >> 3) & 007, 0, 0});
						forThis.emplace(AceSid{ACL_OTHER, (uint32_t)ACL_UNDEFINED_ID}, AcePerm{st.st_mode & 007, 0, 0});
					}
				}
				else
				{
					// This object must not inherit ACLs from parent in any form
					// Just add three mandatory items with some same defaults
					forThis[AceSid{ACL_USER_OBJ, (uint32_t)ACL_UNDEFINED_ID}].inherited = fullAccess;
					forThis[AceSid{ACL_GROUP_OBJ, (uint32_t)ACL_UNDEFINED_ID}].inherited =  fullAccess;
					forThis[AceSid{ACL_OTHER, (uint32_t)ACL_UNDEFINED_ID}];
				}

				if (pDacl == nullptr)
				{
					// This is "full access to everyone"
					AcePerm& owner = forThis[AceSid{ACL_USER_OBJ, (uint32_t)ACL_UNDEFINED_ID}];
					owner.set = fullAccess;
					owner.value = fullAccess;

					AcePerm& group = forThis[AceSid{ACL_GROUP_OBJ, (uint32_t)ACL_UNDEFINED_ID}];
					group.set = fullAccess;
					group.value = fullAccess;

					AcePerm& others = forThis[AceSid{ACL_OTHER, (uint32_t)ACL_UNDEFINED_ID}];
					others.set = fullAccess;
					others.value = fullAccess;

					if (isDir)
					{
						// While there is no way to limit inheritance according to ACE flags - copy full ACL
						forChildren = forThis;
					}
				}
				else
				{
					// Scan given DACL and toss ACEs to access and default ACLs

					if (isDir)
					{
						// While there is no way to limit inheritance according to ACE flags - copy full ACL
						forChildren = forThis;
					}

					// First pass: gather all user and group SIDs
					char* p = pDacl->AceBuffer;
					for (WORD i = 0; i < pDacl->AclCount && p < pDacl->AceBuffer + pDacl->AclSize - sizeof(ACL); ++i)
					{
						ACE* ace = reinterpret_cast<ACE*>(p);
						p += sizeof(ACE) + ace->Sid.SubAuthorityCount * sizeof(SID::SubAuthority[0]);

						switch (ace->AceType)
						{
						case ACCESS_ALLOWED_ACE_TYPE:
						case ACCESS_DENIED_ACE_TYPE:
							{
								bool applyToThis = (ace->AceFlags & INHERIT_ONLY) == 0;
								bool applyToChild = (isDir) && (ace->AceFlags & SUB_CONTAINERS_AND_OBJECTS_INHERIT) != 0;
								if (!applyToThis && !applyToChild)
								{
									return ERROR_BAD_INHERITANCE_ACL;
								}

								uint32_t id = 0;
								SID_NAME_USE t = recognizeSid(&(ace->Sid), id);

								switch (t)
								{
								case SidTypeAlias:
									break; // Default ACEs are already added above
								case SidTypeUser:
									{
										if (id == 0)
										{
											break; // Root already has every right
										}
										AceSid sid{ ACL_USER, id };
										if (applyToThis)
										{
											forThis[sid];
										}
										if (applyToChild)
										{
											forChildren[sid];
										}
										break;
									}
								case SidTypeGroup:
									{
										// Root group also has every right, right?..
										// Wrong!
										AceSid sid{ ACL_GROUP, id };
										if (applyToThis)
										{
											forThis[sid];
										}
										if (applyToChild)
										{
											forChildren[sid];
										}
										break;
									}
								default:
									{
										return ERROR_INVALID_SID;
									}
								}
								break;
							}
						default:
							{
								return ERROR_CALL_NOT_IMPLEMENTED;
							}
						}
					}

					// Second pass: apply permissions to them
					p = pDacl->AceBuffer;
					for (WORD i = 0; i < pDacl->AclCount && p < pDacl->AceBuffer + pDacl->AclSize - sizeof(ACL); ++i)
					{
						ACE* ace = reinterpret_cast<ACE*>(p);
						p += sizeof(ACE) + ace->Sid.SubAuthorityCount * sizeof(SID::SubAuthority[0]);

						bool allow = false;
						switch (ace->AceType)
						{
						case ACCESS_ALLOWED_ACE_TYPE:
							allow = true;
							[[fallthrough]];

						case ACCESS_DENIED_ACE_TYPE:
							{
								bool applyToThis = (ace->AceFlags & INHERIT_ONLY) == 0;
								bool applyToChild = (isDir) && (ace->AceFlags & SUB_CONTAINERS_AND_OBJECTS_INHERIT) != 0;

								uint32_t id = 0;
								SID_NAME_USE t = recognizeSid(&(ace->Sid), id);
								mode_t m = recognizeAccessMask(ace->Mask);

								// Add bits set in m into ACE value but do not owerwrite those that already set (or clear)
								// because in Windows ACL first come ACE wins.

								// Because POSIX ACLs are not cumulative, conversion is a little tricky.
								// Details are in http://www.citi.umich.edu/projects/nfsv4/rfc/draft-ietf-nfs4-acl-mapping-05.txt

								switch (t)
								{
								case SidTypeAlias:
									{
										switch (id)
										{
										case WORLD_RID:
											{
												// What is allowed to everyone - is allowed to every one
												if (applyToThis)
												{
													for (auto& a : forThis)
													{
														AcePerm& perm = a.second;
														applyPerm(m, perm.set, perm.value, allow);
													}
												}
												if (applyToChild)
												{
													for (auto& a : forChildren)
													{
														AcePerm& perm = a.second;
														applyPerm(m, perm.set, perm.value, allow);
													}
												}
												break;
											}
										case OWNER_RID:
											{
												// What is allowed to owner - allowed only to owner (whoever it is)
												AceSid sid{ ACL_USER_OBJ, (uint32_t)ACL_UNDEFINED_ID };
												if (applyToThis)
												{
													AcePerm& perm = forThis[sid];
													applyPerm(m, perm.set, perm.value, allow);
												}
												if (applyToChild)
												{
													AcePerm& perm = forChildren[sid];
													applyPerm(m, perm.set, perm.value, allow);
												}
												break;
											}
										case GROUP_RID:
											{
												// TODO: be smart and expand group permissions to every member
												AceSid sid{ ACL_GROUP_OBJ, (uint32_t)ACL_UNDEFINED_ID };
												if (applyToThis)
												{
													AcePerm& perm = forThis[sid];
													applyPerm(m, perm.set, perm.value, allow);
												}
												if (applyToChild)
												{
													AcePerm& perm = forChildren[sid];
													applyPerm(m, perm.set, perm.value, allow);
												}
												break;
											}
										default:
											return ERROR_INVALID_SID;
										}
										break;
									}
								case SidTypeUser:
									{
										AceSid sid{ ACL_USER, id };
										if (applyToThis)
										{
											if (id != 0)
											{
												// Changing if owner must not mess with explicitly granted rights
												AcePerm& perm = forThis[sid];
												applyPerm(m, perm.set, perm.value, allow);
											}

											if (id == st.st_uid) // this user is also owner
											{
												AceSid ownerSid{ ACL_USER_OBJ, (uint32_t)ACL_UNDEFINED_ID };
												AcePerm& ownerPerm = forThis[ownerSid];
												applyPerm(m, ownerPerm.set, ownerPerm.value, allow);
											}
										}
										if (applyToChild)
										{
											// Windows ACEs are inherited with the same SID
											// while POSIX owner ACEs are inherited for any owner
											// so duplicate them.
											if (id != 0)
											{
												AcePerm& perm = forChildren[sid];
												applyPerm(m, perm.set, perm.value, allow);
											}

											if (id == st.st_uid) // this user is also owner
											{
												AceSid ownerSid{ ACL_USER_OBJ, (uint32_t)ACL_UNDEFINED_ID };
												AcePerm& ownerPerm = forChildren[ownerSid];
												applyPerm(m, ownerPerm.set, ownerPerm.value, allow);
											}
										}
										break;
									}
								case SidTypeGroup:
									{
										AceSid sid{ ACL_GROUP, id };
										AceSid groupSid{ ACL_GROUP_OBJ, (uint32_t)ACL_UNDEFINED_ID };
										if (applyToThis)
										{
											// Changing if owner group must not mess with explicitly granted rights
											AcePerm& perm = forThis[sid];
											applyPerm(m, perm.set, perm.value, allow);

											if (id == st.st_gid) // this group is also owner group
											{
												AcePerm& groupPerm = forThis[groupSid];
												applyPerm(m, groupPerm.set, groupPerm.value, allow);
											}
										}
										if (applyToChild)
										{
											AcePerm& perm = forChildren[sid];
											applyPerm(m, perm.set, perm.value, allow);

											if (id == st.st_gid) // this user is also owner
											{
												AcePerm& groupPerm = forChildren[groupSid];
												applyPerm(m, groupPerm.set, groupPerm.value, allow);
											}
										}
										break;
									}
								default:
									{
										return ERROR_INVALID_SID;
									}
								}
								break;
							}
						default:
							{
								return ERROR_CALL_NOT_IMPLEMENTED;
							}
						}
					}
				}
				// Transform maps into ACL attributes

				{
					Attributes buffer(forThis.toAttr());
					if (buffer.size() > 65536)
					{
						return E2BIG;
					}

					if (setxattr(pObjectName, XATTR_NAME_POSIX_ACL_ACCESS,
								buffer.get(), buffer.size(), 0) < 0)
					{
						return errno;
					}
				}

				// Repeat for childs
				if (isDir)
				{
					Attributes buffer(forChildren.toAttr());
					if (buffer.size() > 65536)
					{
						return E2BIG;
					}

					if (setxattr(pObjectName, XATTR_NAME_POSIX_ACL_DEFAULT,
								buffer.get(), buffer.size(), 0) < 0)
					{
						return errno;
					}

					// TODO: Propagate inheritable entries to children
				}
			}
			break;
		}
	default:
		{
			return ERROR_INVALID_PARAMETER;
		}
	}
	return ERROR_SUCCESS;
}
