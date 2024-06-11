/* pshs -- network interfaces support
 * (c) 2011 Michał Górny
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#ifndef _PSHS_NETWORK_H
#define _PSHS_NETWORK_H

#include <iostream>

class ExternalIP
{
	int _port;

public:
	ExternalIP(unsigned int port, const char* bindip, bool use_upnp);
	~ExternalIP();

	const char* addr;
};

const char* get_rtnl_external_ip(const char* bindip);

struct IPAddrPrinter
{
	const char* addr;
	int port;

	IPAddrPrinter(const char* new_addr, int new_port)
		: addr(new_addr), port(new_port)
	{
	}

	friend std::ostream& operator<<(std::ostream&, const IPAddrPrinter&);
};

#endif /*_PSHS_NETWORK_H*/
