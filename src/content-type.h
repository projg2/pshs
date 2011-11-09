/* pshs -- Content-Type guessing support
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#pragma once
#ifndef _PSHS_CONTENT_TYPE_H
#define _PSHS_CONTENT_TYPE_H

#include <event2/util.h>

void init_content_type(void);
void destroy_content_type(void);

int open_ct(const char *path, const char **ct, ev_off_t *size);

#endif /*_PSHS_CONTENT_TYPE_H*/
