/* pshs -- Content-Type guessing support
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#pragma once
#ifndef _PSHS_CONTENT_TYPE_H
#define _PSHS_CONTENT_TYPE_H

void init_content_type(void);
void destroy_content_type(void);

const char* guess_content_type(int fd);

#endif /*_PSHS_CONTENT_TYPE_H*/
