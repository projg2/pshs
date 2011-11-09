/* pshs -- network interfaces support
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#pragma once
#ifndef _PSHS_NETWORK_H
#define _PSHS_NETWORK_H

const char *init_external_ip(unsigned int port, int use_upnp);
void destroy_external_ip(unsigned int port);

#endif /*_PSHS_NETWORK_H*/
