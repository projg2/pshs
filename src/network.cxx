/* pshs -- network interfaces support
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
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

#	ifndef MINIUPNPC_API_VERSION
#		ifdef UPNPCOMMAND_HTTP_ERROR
#			define MINIUPNPC_API_VERSION 5
#		else
#			define MINIUPNPC_API_VERSION 0
#		endif
#	endif
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
#if MINIUPNPC_API_VERSION >= 14
		struct UPNPDev* devlist = upnpDiscover(discovery_delay, bindip, NULL, 0, 0, 2, NULL);
#elif MINIUPNPC_API_VERSION >= 8
		struct UPNPDev* devlist = upnpDiscover(discovery_delay, bindip, NULL, 0, 0, NULL);
#else
		struct UPNPDev* devlist = upnpDiscover(discovery_delay, bindip, NULL, 0);
#endif

		int ret = UPNP_GetValidIGD(devlist, &upnp_urls, &upnp_data,
				lan_addr, sizeof(lan_addr));
		freeUPNPDevlist(devlist);

		/* ret=1 means we've got IGD,
		 * ret>1 means we've got something else, so we need to clean up */
		upnp_enabled = (ret == 1);
		if (upnp_enabled)
		{
			/* UPnP likes ASCII */
			std::string strport{std::to_string(port)};

			/* Set the port forwarding. */
			ret = UPNP_AddPortMapping(
					upnp_urls.controlURL,
#if MINIUPNPC_API_VERSION >= 5
					upnp_data.first.servicetype,
#else
					upnp_data.servicetype,
#endif
					strport.c_str(), strport.c_str(), lan_addr,
					"Pretty small HTTP server",
					"TCP",
#if MINIUPNPC_API_VERSION >= 8
					NULL,
#endif
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
				static char extip[16];

				/* And then get external IP. */
				if (UPNP_GetExternalIPAddress(
						upnp_urls.controlURL,
#if MINIUPNPC_API_VERSION >= 5
						upnp_data.first.servicetype,
#else
						upnp_data.servicetype,
#endif
						extip) == UPNPCOMMAND_SUCCESS)
				{
					addr = extip;
					return;
				}
			}
		} else if (ret)
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
#if MINIUPNPC_API_VERSION >= 5
				upnp_data.first.servicetype,
#else
				upnp_data.servicetype,
#endif
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
