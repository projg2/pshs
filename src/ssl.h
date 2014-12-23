/* pshs -- SSL/TLS support
 * (c) 2014 Michał Górny
 * 2-clause BSD-licensed
 */

#pragma once

#ifndef _PSHS_CONTENT_SSL_H
#define _PSHS_CONTENT_SSL_H 1

#include <event2/http.h>

int init_ssl(struct evhttp* http, const char* extip);
void destroy_ssl();

#endif /*_PSHS_CONTENT_SSL_H*/
