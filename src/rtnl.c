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
struct addr_search_data {
	const char *addr;
	int scope;
	int islocal;
};

static int store_addr(const struct sockaddr_nl *sa, struct nlmsghdr *n, void *data) {
	struct addr_search_data *out = data;
	struct ifaddrmsg *addr = NLMSG_DATA(n);
	struct rtattr * rta_tb[IFA_MAX+1];
	char buf[256];

	parse_rtattr(rta_tb, IFA_MAX, IFA_RTA(addr),
			n->nlmsg_len - NLMSG_LENGTH(sizeof(*addr)));

	if (addr->ifa_family == AF_INET && rta_tb[IFA_LOCAL]) {
		struct sockaddr_in *in = (void*) rta_tb[IFA_LOCAL];
		/* using char[4] allows us to ignore endianness */
		unsigned char *binaddr = (void*) &(in->sin_addr.s_addr);
		enum {
			ISLOCAL_NO,
			ISLOCAL_NET,
			ISLOCAL_APIPA,
			ISLOCAL_HOST
		} islocal = ISLOCAL_NO;

		if (binaddr[0] == 127)
			islocal = ISLOCAL_HOST;
		else if (binaddr[0] == 10)
			islocal = ISLOCAL_NET;
		else if (binaddr[0] == 192 && binaddr[1] == 168)
			islocal = ISLOCAL_NET;
		else if (binaddr[0] == 169 && binaddr[1] == 254)
			islocal = ISLOCAL_APIPA;
		else if (binaddr[0] == 172 && binaddr[1] >= 16 && binaddr[1] < 32)
			islocal = ISLOCAL_NET;

		if (!out->addr || addr->ifa_scope < out->scope || islocal < out->islocal) {
			out->addr = inet_ntoa(in->sin_addr);
			out->scope = addr->ifa_scope;
			out->islocal = islocal;
		}
	}
}
#endif

const char *get_rtnl_external_ip(void) {
#ifdef HAVE_LIBNETLINK
	struct rtnl_handle rth;
	struct addr_search_data out = { NULL };

	if (rtnl_open(&rth, 0) < 0) {
		printf("rtnl_open() failed\n");
		return NULL;
	}

	if (rtnl_wilddump_request(&rth, AF_INET, RTM_GETADDR) >= 0) {
		if (rtnl_dump_filter(&rth, store_addr, &out, NULL, NULL) < 0)
			printf("rtnl_dump_filter() failed\n");
	} else
		printf("rtnl_wilddump_request() failed: %s\n", strerror(errno));

	rtnl_close(&rth);
	return out.addr;
#endif
	return NULL;
}
