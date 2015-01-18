/* pshs -- SSL/TLS support
 * (c) 2014 Michał Górny
 * 2-clause BSD-licensed
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
