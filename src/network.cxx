/* pshs -- network interfaces support
 * (c) 2011-2024 Michał Górny
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <iostream>
#include <string>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef HAVE_LIBMINIUPNPC
#	include <miniupnpc/miniupnpc.h>
#	include <miniupnpc/upnpcommands.h>
#	include <miniupnpc/upnperrors.h>
#endif

#include "network.h"

#ifdef HAVE_LIBMINIUPNPC
static const int discovery_delay = 1000; /* [ms] */

static struct UPNPUrls upnp_urls;
static struct IGDdatas upnp_data;

static char lan_addr[16];

static int upnp_enabled;
#endif

/**
 * ExternalIP::ExternalIP
 * @port: listening port
 * @bindip: IP the server is bound to
 * @use_upnp: whether UPnP is enabled via config
 *
 * Try to set up port forwardings and get the external IP. This tries to use
 * UPnP first, then falls back to bound IP or searching interfaces via netlink.
 */
ExternalIP::ExternalIP(unsigned int port, const char* bindip, bool use_upnp)
	: _port(port)
{
#ifdef HAVE_LIBMINIUPNPC
	/* use UPnP only if user wants to */
	if (use_upnp)
	{
		struct UPNPDev* devlist = upnpDiscover(discovery_delay, bindip, NULL, 0, 0, 2, NULL);
		static char extip[16];

#if MINIUPNPC_API_VERSION >= 18
		int ret = UPNP_GetValidIGD(devlist, &upnp_urls, &upnp_data,
				lan_addr, sizeof(lan_addr), extip, sizeof(extip));
#else
		int ret = UPNP_GetValidIGD(devlist, &upnp_urls, &upnp_data,
				lan_addr, sizeof(lan_addr));
#endif
		freeUPNPDevlist(devlist);

		/* ret=1 means we've got IGD,
		 * since API 18, ret=2 means we've got IGD without external IP,
		 * higher values mean we've got other UPnP device, so we need
		 * to clean up */
		upnp_enabled = (ret == 1);
#if MINIUPNPC_API_VERSION >= 18
		upnp_enabled |= (ret == 2);
#endif
		if (upnp_enabled)
		{
			/* UPnP likes ASCII */
			std::string strport{std::to_string(port)};

			/* Set the port forwarding. */
			ret = UPNP_AddPortMapping(
					upnp_urls.controlURL,
					upnp_data.first.servicetype,
					strport.c_str(), strport.c_str(), lan_addr,
					"Pretty small HTTP server",
					"TCP",
					NULL,
					NULL);
			if (ret != UPNPCOMMAND_SUCCESS)
			{
				std::cerr << "UPNP_AddPortMapping() failed: " << strupnperror(ret)
					<< std::endl;
				upnp_enabled = 0;
				FreeUPNPUrls(&(upnp_urls));
			}
			else
			{
				/* And then get external IP. */
#if MINIUPNPC_API_VERSION >= 18
				if (ret == 1)
#else
				if (UPNP_GetExternalIPAddress(
						upnp_urls.controlURL,
						upnp_data.first.servicetype,
						extip) == UPNPCOMMAND_SUCCESS)
#endif
				{
					addr = extip;
					return;
				}
			}
		} else if (ret > 0)
			FreeUPNPUrls(&(upnp_urls));
	}
#endif

	/* Fallback to bindip or netlink */
	if (!strcmp(bindip, "0.0.0.0") || !strcmp(bindip, "::"))
		addr = get_rtnl_external_ip(bindip);
	else
		addr = bindip;
}

/**
 * ExternalIP::~ExternalIP
 *
 * Cleanup after ExternalIP. If UPnP was used, remove the port
 * forwarding established then.
 */
ExternalIP::~ExternalIP()
{
#ifdef HAVE_LIBMINIUPNPC
	if (upnp_enabled)
	{
		int ret;
		std::string strport{std::to_string(_port)};

		/* Remove the port forwarding when done. */
		ret = UPNP_DeletePortMapping(
				upnp_urls.controlURL,
				upnp_data.first.servicetype,
				strport.c_str(), "TCP", NULL);
		if (ret != UPNPCOMMAND_SUCCESS)
			std::cerr << "UPNP_DeletePortMapping() failed: " << strupnperror(ret)
				<< std::endl;
		FreeUPNPUrls(&(upnp_urls));
	}
#endif
}

std::ostream& operator<<(std::ostream& os, const IPAddrPrinter& addr)
{
	bool ipv6 = !!strchr(addr.addr, ':');
	if (ipv6)
		os << '[';
	os << addr.addr;
	if (ipv6)
		os << ']';
	os << ':' << addr.port;
	return os;
}
