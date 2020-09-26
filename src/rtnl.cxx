/* pshs -- netlink IP getting support
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"

#include <iostream>

#include <stdlib.h>

#ifdef HAVE_LIBNETLINK
#	include <stdio.h>
#	include <string.h>
#	include <errno.h>

#	include <sys/socket.h>

extern "C"
{
#		include <libnetlink.h>
};

#	include <netinet/in.h>
#	include <netinet/ip.h>
#	include <arpa/inet.h>
#endif

#include "network.h"

#ifdef HAVE_LIBNETLINK
enum is_local { /* most preferred first */
	ISLOCAL_NO, /* global address */
	ISLOCAL_NET, /* address reserved for local network */
	ISLOCAL_APIPA, /* automatic address assignment */
	ISLOCAL_HOST /* localhost address */
};

struct addr_search_data
{
	const char* addr;
	int scope;
	enum is_local islocal;
};

/**
 * store_addr
 * @sa: netlink address
 * @n: message header
 * @data: output buffer
 *
 * Handle netlink address message. Put the most useful IP address found in
 * output struct @data.
 *
 * Returns: 0
 */
static int store_addr(const struct sockaddr_nl* sa, struct nlmsghdr* n, void* data)
{
	struct addr_search_data* out = static_cast<addr_search_data*>(data);
	struct ifaddrmsg* addr = static_cast<ifaddrmsg*>(NLMSG_DATA(n));
	struct rtattr*  rta_tb[IFA_MAX+1];

	/* Based heavily on iproute2,
	 * IOW: I'm not sure that I want to know what happens here. */
	parse_rtattr(rta_tb, IFA_MAX, IFA_RTA(addr),
			n->nlmsg_len - NLMSG_LENGTH(sizeof(*addr)));

	if (addr->ifa_family == AF_INET && rta_tb[IFA_LOCAL])
	{
		/* Get the actual IP address */
		struct sockaddr_in* in = static_cast<sockaddr_in*>(
				static_cast<void*>(rta_tb[IFA_LOCAL]));
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

		/* prefer global scope, and global addresses */
		if (!out->addr || addr->ifa_scope < out->scope || islocal < out->islocal)
		{
			out->addr = inet_ntoa(in->sin_addr);
			out->scope = addr->ifa_scope;
			out->islocal = islocal;
		}
	}

	return 0;
}
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
#ifdef HAVE_LIBNETLINK
	struct rtnl_handle rth;
	struct addr_search_data out = { NULL, 0, ISLOCAL_NO };

	if (rtnl_open(&rth, 0) < 0)
	{
		std::cerr << "rtnl_open() failed" << std::endl;
		return NULL;
	}

	if (rtnl_wilddump_request(&rth, AF_INET, RTM_GETADDR) >= 0)
	{
		if (rtnl_dump_filter(&rth, store_addr, &out) < 0)
			std::cerr << "rtnl_dump_filter() failed" << std::endl;
	} else
		std::cerr << "rtnl_wilddump_request() failed: " << strerror(errno)
			<< std::endl;

	rtnl_close(&rth);
	return out.addr;
#endif
	return NULL;
}
