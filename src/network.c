/* pshs -- network interfaces support
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_LIBMINIUPNPC
#	include <miniupnpc/miniupnpc.h>
#	include <miniupnpc/upnpcommands.h>
#	include <miniupnpc/upnperrors.h>

#	ifdef UPNPCOMMAND_HTTP_ERROR
#		define LIBMINIUPNPC_SO_5
#	endif
#	ifdef UPNPDISCOVER_SUCCESS /* miniupnpc-1.6 */
#		define LIBMINIUPNPC_SO_8
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
 * init_external_ip
 * @port: listening port
 * @bindip: IP the server is bound to
 * @use_upnp: whether UPnP is enabled via config
 *
 * Try to set up port forwardings and get the external IP. This tries to use
 * UPnP first, then falls back to bound IP or searching interfaces via netlink.
 *
 * Returns: pointer to ASCII repr of 'best' IP address, or %NULL
 */
const char *init_external_ip(unsigned int port, const char *bindip, int use_upnp) {
#ifdef HAVE_LIBMINIUPNPC
	/* use UPnP only if user wants to */
	if (use_upnp) {
#ifdef LIBMINIUPNPC_SO_8
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
		if (upnp_enabled) {
			/* UPnP likes ASCII */
			char strport[6];
			sprintf(strport, "%d", port);

			/* Set the port forwarding. */
			ret = UPNP_AddPortMapping(
					upnp_urls.controlURL,
#ifdef LIBMINIUPNPC_SO_5
					upnp_data.first.servicetype,
#else
					upnp_data.servicetype,
#endif
					strport, strport, lan_addr,
					"Pretty small HTTP server",
					"tcp",
#ifdef LIBMINIUPNPC_SO_8
					NULL,
#endif
					NULL);
			if (ret != UPNPCOMMAND_SUCCESS) {
				printf("UPNP_AddPortMapping() failed: %s\n", strupnperror(ret));
				upnp_enabled = 0;
				FreeUPNPUrls(&(upnp_urls));
			} else {
				static char extip[16];

				/* And then get external IP. */
				if (UPNP_GetExternalIPAddress(
						upnp_urls.controlURL,
#ifdef LIBMINIUPNPC_SO_5
						upnp_data.first.servicetype,
#else
						upnp_data.servicetype,
#endif
						extip) == UPNPCOMMAND_SUCCESS)
					return extip;
			}
		} else if (ret)
			FreeUPNPUrls(&(upnp_urls));
	}
#endif

	/* Fallback to bindip or netlink */
	return !strcmp(bindip, "0.0.0.0") ? get_rtnl_external_ip() : bindip;
}

/**
 * destroy_external_ip
 * @port: port the server was listening on
 *
 * Cleanup after init_external_ip(). If UPnP was used, remove the port
 * forwarding established then.
 */
void destroy_external_ip(unsigned int port) {
#ifdef HAVE_LIBMINIUPNPC
	if (upnp_enabled) {
		int ret;
		char strport[6];
		sprintf(strport, "%d", port);

		/* Remove the port forwarding when done. */
		ret = UPNP_DeletePortMapping(
				upnp_urls.controlURL,
#ifdef LIBMINIUPNPC_SO_5
				upnp_data.first.servicetype,
#else
				upnp_data.servicetype,
#endif
				strport, "tcp", NULL);
		if (ret != UPNPCOMMAND_SUCCESS)
			printf("UPNP_DeletePortMapping() failed: %s\n", strupnperror(ret));
		FreeUPNPUrls(&(upnp_urls));
	}
#endif
}
