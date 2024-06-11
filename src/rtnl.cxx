/* pshs -- IP address getting support
 * (c) 2011-2020 Michał Górny
 * SPDX-License-Identifier: GPL-2.0-or-later
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

#ifdef HAVE_GETIFADDRS
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
 * @bindip: IP the server is bound to
 *
 * Try to get external IP from local network interfaces using netlink.
 *
 * Returns: best IP address found on the system, in a static buffer
 */
const char* get_rtnl_external_ip(const char* bindip)
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

	/* want IPv6 only if we're bound to IPv6 socket */
	bool want_ipv6 = !!strchr(bindip, ':');
	struct sockaddr* curr_addr = NULL;
	enum is_local curr_islocal = ISLOCAL_MAX;

	for (struct ifaddrs* addr = addrs; addr; addr = addr->ifa_next)
	{
		if (addr->ifa_addr == NULL)
			continue;

		int family = addr->ifa_addr->sa_family;
		enum is_local islocal;
		if (family == AF_INET)
		{
			struct sockaddr_in* in = static_cast<sockaddr_in*>(
					static_cast<void*>(addr->ifa_addr));
			/* using char[4] allows us to ignore endianness */
			unsigned char* binaddr = static_cast<unsigned char*>(
					static_cast<void*>(&(in->sin_addr.s_addr)));

			islocal = ISLOCAL_NO;
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
		}
		else if (family == AF_INET6)
		{
			if (!want_ipv6)
				continue;

			struct sockaddr_in6* in = static_cast<sockaddr_in6*>(
					static_cast<void*>(addr->ifa_addr));
			unsigned char* binaddr = in->sin6_addr.s6_addr;

			islocal = ISLOCAL_HOST;
			// check for ::1
			for (int i = 0; i < 15; ++i)
			{
				if (binaddr[i] != 0)
				{
					islocal = ISLOCAL_NO;
					break;
				}
			}
			if (binaddr[15] != 1)
				islocal = ISLOCAL_NO;
			if (islocal != ISLOCAL_HOST && binaddr[0] == 0xFE
					&& (binaddr[1] & 0xC0) == 0x80)
				islocal = ISLOCAL_APIPA;
		}
		else
			continue;

		/* prefer global addresses, and arbitrarily prefer IPv6 */
		bool update = false;
		if (islocal < curr_islocal)
			update = true;
		else if (islocal == curr_islocal && family == AF_INET6
				&& curr_addr->sa_family == AF_INET)
			update = true;
		if (update)
		{
			curr_addr = addr->ifa_addr;
			curr_islocal = islocal;
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
