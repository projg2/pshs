/* pshs -- Content-Type guessing support
 * (c) 2011 Michał Górny
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once
#ifndef _PSHS_CONTENT_TYPE_H
#define _PSHS_CONTENT_TYPE_H

class ContentType
{
public:
	ContentType();
	~ContentType();

	const char* guess(int fd);
};

#endif /*_PSHS_CONTENT_TYPE_H*/
