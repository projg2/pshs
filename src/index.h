/* pshs -- file index generation
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#pragma once
#ifndef _PSHS_INDEX_H
#define _PSHS_INDEX_H

#include <event2/buffer.h>

void generate_index(struct evbuffer* buf, char* const* files);

#endif /*_PSHS_INDEX_H*/
