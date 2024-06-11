/* pshs -- request handlers
 * (c) 2011 Michał Górny
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#ifndef _PSHS_HANDLERS_H
#define _PSHS_HANDLERS_H

#include <event2/http.h>

// abstract
class ContentType;

struct callback_data
{
	const char* prefix;
	size_t prefix_len;
	char* const* files;

	ContentType* ct;
};

void init_charset(const char* charset);

void handle_file(struct evhttp_request* req, void* data);
void handle_index_with_list(struct evhttp_request* req, void* data);
void handle_index_with_redirect(struct evhttp_request* neq, void *data);

#endif /*_PSHS_HANDLERS_H*/
