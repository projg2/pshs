/* pshs -- netlink IP getting support
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"

#include <stdlib.h>

#ifdef HAVE_LIBNETLINK
#	include <stdio.h>
#	include <string.h>
#	include <errno.h>

#	include <sys/socket.h>
#	include <libnetlink.h>

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

struct addr_search_data {
	const char *addr;
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
static int store_addr(const struct sockaddr_nl *sa, struct nlmsghdr *n, void *data) {
	struct addr_search_data *out = data;
	struct ifaddrmsg *addr = NLMSG_DATA(n);
	struct rtattr * rta_tb[IFA_MAX+1];

	/* Based heavily on iproute2,
	 * IOW: I'm not sure that I want to know what happens here. */
	parse_rtattr(rta_tb, IFA_MAX, IFA_RTA(addr),
			n->nlmsg_len - NLMSG_LENGTH(sizeof(*addr)));

	if (addr->ifa_family == AF_INET && rta_tb[IFA_LOCAL]) {
		/* Get the actual IP address */
		struct sockaddr_in *in = (void*) rta_tb[IFA_LOCAL];
		/* using char[4] allows us to ignore endianness */
		unsigned char *binaddr = (void*) &(in->sin_addr.s_addr);

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
		if (!out->addr || addr->ifa_scope < out->scope || islocal < out->islocal) {
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
const char *get_rtnl_external_ip(void) {
#ifdef HAVE_LIBNETLINK
	struct rtnl_handle rth;
	struct addr_search_data out = { NULL };

	if (rtnl_open(&rth, 0) < 0) {
		fprintf(stderr, "rtnl_open() failed\n");
		return NULL;
	}

	if (rtnl_wilddump_request(&rth, AF_INET, RTM_GETADDR) >= 0) {
#ifdef HAVE_RTNL_DUMP_FILTER_3ARG
		if (rtnl_dump_filter(&rth, store_addr, &out) < 0)
#else
		if (rtnl_dump_filter(&rth, store_addr, &out, NULL, NULL) < 0)
#endif
			fprintf(stderr, "rtnl_dump_filter() failed\n");
	} else
		fprintf(stderr, "rtnl_wilddump_request() failed: %s\n", strerror(errno));

	rtnl_close(&rth);
	return out.addr;
#endif
	return NULL;
}
