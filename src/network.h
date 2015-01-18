/* pshs -- network interfaces support
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#pragma once
#ifndef _PSHS_NETWORK_H
#define _PSHS_NETWORK_H

class ExternalIP
{
	int _port;

public:
	ExternalIP(unsigned int port, const char* bindip, bool use_upnp);
	~ExternalIP();

	const char* addr;
};

const char* get_rtnl_external_ip(void);

#endif /*_PSHS_NETWORK_H*/
