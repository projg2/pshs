/* pshs -- IP address getting support
 * (c) 2011-2020 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"

#include <iostream>

#include <stdlib.h>

#ifdef HAVE_GETIFADDRS
#	include <sys/types.h>
#	include <sys/socket.h>

#	include <netinet/in.h>
#	include <netinet/ip.h>

#	include <ifaddrs.h>
#	include <netdb.h>
#	include <string.h>
#endif

#include "network.h"

#if HAVE_GETIFADDRS
enum is_local { /* most preferred first */
	ISLOCAL_NO, /* global address */
	ISLOCAL_NET, /* address reserved for local network */
	ISLOCAL_APIPA, /* automatic address assignment */
	ISLOCAL_HOST, /* localhost address */
	ISLOCAL_MAX,
};
#endif

/**
 * get_rtnl_external_ip
 *
 * Try to get external IP from local network interfaces using netlink.
 *
 * Returns: best IP address found on the system, in a static buffer
 */
const char* get_rtnl_external_ip(void)
{
	const char* out = NULL;

#ifdef HAVE_GETIFADDRS
	struct ifaddrs* addrs;
	if (getifaddrs(&addrs) < 0)
	{
		std::cerr << "getifaddrs() failed: " << strerror(errno)
			<< std::endl;
		return NULL;
	}

	struct sockaddr* curr_addr = NULL;
	enum is_local curr_islocal = ISLOCAL_MAX;

	for (struct ifaddrs* addr = addrs; addr; addr = addr->ifa_next)
	{
		if (addr->ifa_addr == NULL)
			continue;

		int family = addr->ifa_addr->sa_family;
		if (family == AF_INET)
		{
			struct sockaddr_in* in = static_cast<sockaddr_in*>(
					static_cast<void*>(addr->ifa_addr));
			/* using char[4] allows us to ignore endianness */
			unsigned char* binaddr = static_cast<unsigned char*>(
					static_cast<void*>(&(in->sin_addr.s_addr)));

			enum is_local islocal = ISLOCAL_NO;
			if (binaddr[0] == 127) /* localhost */
				islocal = ISLOCAL_HOST;
			else if (binaddr[0] == 10)
				islocal = ISLOCAL_NET;
			else if (binaddr[0] == 192 && binaddr[1] == 168)
				islocal = ISLOCAL_NET;
			else if (binaddr[0] == 169 && binaddr[1] == 254)
				islocal = ISLOCAL_APIPA;
			else if (binaddr[0] == 172 && binaddr[1] >= 16 && binaddr[1] < 32)
				islocal = ISLOCAL_NET;

			/* prefer global addresses */
			if (islocal < curr_islocal)
			{
				curr_addr = addr->ifa_addr;
				curr_islocal = islocal;
			}
		}
		else if (family == AF_INET6)
		{
			// TODO: IPv6
		}
	}

	if (curr_addr)
	{
		static char host[NI_MAXHOST];
		socklen_t addr_len =
			curr_addr->sa_family == AF_INET ?
			sizeof(struct sockaddr_in) :
			sizeof(struct sockaddr_in6);
		if (getnameinfo(curr_addr, addr_len, host, sizeof(host),
				NULL, 0, NI_NUMERICHOST) == 0)
			out = host;
		else
			std::cerr << "getnameinfo() failed: " << strerror(errno)
				<< std::endl;
	}

	freeifaddrs(addrs);
#endif

	return out;
}
