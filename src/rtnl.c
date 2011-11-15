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

		if (!out->addr || addr->ifa_scope < out->scope) {
			out->addr = inet_ntoa(in->sin_addr);
			out->scope = addr->ifa_scope;
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
