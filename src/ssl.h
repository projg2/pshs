/* pshs -- SSL/TLS support
 * (c) 2014 Michał Górny
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#ifndef _PSHS_CONTENT_SSL_H
#define _PSHS_CONTENT_SSL_H 1

#include <event2/http.h>

class SSLMod
{
public:
	SSLMod(evhttp* http, const char* extip, bool enable);
	~SSLMod();

	bool enabled;
};

#endif /*_PSHS_CONTENT_SSL_H*/
