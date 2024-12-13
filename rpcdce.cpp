#include <fcntl.h>
#include <ifaddrs.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <time.h>
#include <linux/if_packet.h>
#include <net/if.h>

#include "file_holder.h"
#include "rpc.h"
#include "winerror.h"

#ifdef TESTING
#include <stdio.h>
#endif // TESTING

// Generate timestamp-based UUID according to RFC 4122 with some deviations.
// This routine must not be too fast to prevent from being called twice (ok, triple) in the same tick.
RPC_STATUS UuidCreateSequential(UUID* uuid)
{
	RPC_STATUS result = RPC_S_UUID_LOCAL_ONLY;

	// Generate some random bits everywhere as a good defaults for clock sequence and node id.
	if (getrandom(uuid, sizeof(UUID), 0) == -1)
	{
#ifdef TESTING
		printf("getrandom failed\n");
#endif // TESTING
		return errno;
	}
	uuid->Data3 &= ~0xf000;
	uuid->Data3 |= 0x1000;
	uuid->Data4[0] &= ~0xc0;
	uuid->Data4[0] |= 0x80;

	// Obtain MAC address.
	// According to the RFC it must be done inside of global lock.
	// Because it is hardly frequently changed value - do it outside of lock

	ifaddrs* addrs = nullptr;
	if (getifaddrs(&addrs) == 0)
	{
		for (ifaddrs* itr = addrs; itr != nullptr; itr = itr->ifa_next)
		{
			if ((itr->ifa_flags & IFF_LOOPBACK) == 0 &&
				itr->ifa_addr != nullptr && itr->ifa_addr->sa_family == AF_PACKET)
			{
				sockaddr_ll* addr = (sockaddr_ll*)itr->ifa_addr;
				unsigned l = addr->sll_halen;
				// Let's be a little paranoidal
				if (l > 6)
				{
					l = 6;
				}
				memcpy(&uuid->Data4[2], addr->sll_addr, l);
				result = RPC_S_OK;
				break;
			}
		}
		freeifaddrs(addrs);
	}

	if (result == RPC_S_UUID_LOCAL_ONLY)
	{
		uuid->Data4[2] |= 0x03; // Set local (just for sure) and multicast (per RFC) bits
	}

	// Get global lock and storage at the same time
	FileHolder state(shm_open("/WinComp.uuid", O_CREAT | O_RDWR | O_CLOEXEC, 0666));
	if (!state)
	{
#ifdef TESTING
		printf("shm_open failed\n");
#endif // TESTING
		return errno;
	}
	fchmod(state.get(), 0666); // To suppress umask
	if (ftruncate(state.get(), sizeof(UUID)) == -1)
	{
#ifdef TESTING
		printf("ftruncate failed\n");
#endif // TESTING
		return errno;
	}
	if (flock(state.get(), LOCK_EX) == -1)
	{
#ifdef TESTING
		printf("flock failed\n");
#endif // TESTING
		return errno;
	}
	UUID* stored = reinterpret_cast<UUID*>(mmap(nullptr, sizeof(UUID), PROT_READ | PROT_WRITE, MAP_SHARED, state.get(), 0));
	if ((stored) != MAP_FAILED)
	{
		struct autounmap
		{
			void* v;
			autounmap(void* newv) : v(newv) {}
			~autounmap()
			{
				munmap(v, sizeof(UUID));
			};
		} unmap{stored};

		timespec tv;
		if (clock_gettime(CLOCK_REALTIME, &tv) == -1)
		{
#ifdef TESTING
			printf("clock_gettime failed\n");
#endif // TESTING
			return errno;
		}
		// 100-nanosecond interval since 1970-01-01
		unsigned long long currentTime = (unsigned long long)tv.tv_sec * 1'000'000'0 + tv.tv_nsec / 100;
		// since 1582-10-15
		currentTime += 12219292800'000'000'0LL;

		if (memcmp(&stored->Data4[2], &uuid->Data4[2], 6) == 0)
		{
			// Stored state is ok, use it
			memcpy(uuid, stored, sizeof(UUID));

			// Check that clock wasn't rewind
			unsigned long long storedTime = uuid->Data1;
			storedTime |= (unsigned long long)uuid->Data2 << 32;
			storedTime |= (unsigned long long)(uuid->Data3 & 0x0FFF) << 48;

			if (storedTime < currentTime)
			{
				// Nothing to do, that's fine
			}
			else if (storedTime > currentTime + 1) // A little deviation from RFC that requires exact comparison
			{
				// Per RFC increment clock sequence
				unsigned clockSeq = (uuid->Data4[0] & 0x3F) << 8 | uuid->Data4[1];
				++clockSeq;
				uuid->Data4[1] = clockSeq & 0xFF;
				uuid->Data4[0] = (uuid->Data4[0] & 0xC0) | ((clockSeq >> 8) & 0x3F);
			}
			else
			{
				// This is very improbable case when CPU is faster than clock
				// Make sure that value is not repeated and hope that next call will be at the next tick.
				currentTime = storedTime + 1;
			}
		}
		//else // Either this is the first call or MAC address was changed. In both cases everything is already prepared

		// Fill rimestamp portion and that's all.
		uuid->Data1 = currentTime & 0xFFFF'FFFF;
		uuid->Data2 = (currentTime >> 32) & 0xFFFF;
		uuid->Data3 = (uuid->Data3 & 0xF000) | ((currentTime >> 48) & 0x0FFF);

		// Store the new state
		memcpy(stored, uuid, sizeof(UUID));
	}
	else
	{
#ifdef TESTING
		printf("mmap failed\n");
#endif // TESTING
		return errno;
	}
	// Do not unlink, let it live until reboot
	// Unlock is also done automatically on close.
	return result;
}

#ifdef TESTING
int main()
{
	UUID a = {};
	printf("UuidCreateSequential retuned %lu\n", UuidCreateSequential(&a));
	printf("UUID = {%08X-%04hX-%04hX-%02X%02X-%02X%02X%02X%02X%02X%02X}\n",
			a.Data1, a.Data2, a.Data3,
			a.Data4[0], a.Data4[1], a.Data4[2], a.Data4[3],
			a.Data4[4], a.Data4[5], a.Data4[6], a.Data4[7]);
	printf("Binary = %llx%llx\n", *((unsigned long long*)&a + 1), *(unsigned long long*)&a);
}
#endif // TESTING
