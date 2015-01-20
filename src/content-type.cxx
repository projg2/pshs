/* pshs -- Content-Type guessing support
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"

#include <iostream>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>

#ifdef HAVE_LIBMAGIC
#	include <magic.h>

/* XXX: make this local, pass within funcs? */
static magic_t magic;
#endif

#include "content-type.h"

/**
 * ContentType::ContentType
 *
 * Init Content-Type guessing algos. If libmagic is enabled, initialize it
 * and load the database.
 */
ContentType::ContentType(void)
{
#ifdef HAVE_LIBMAGIC
	magic = magic_open(MAGIC_MIME);
	if (!magic)
		std::cerr << "magic_open() failed: " << strerror(errno) << std::endl;
	else
	{
		if (magic_load(magic, NULL))
		{
			std::cerr << "magic_open() failed: " << magic_error(magic) << std::endl;
			magic_close(magic);
			magic = NULL;
		}
	}
#endif
}

/**
 * ContentType::~ContentType
 *
 * Clean up after Content-Type guessing. Unload libmagic if enabled.
 */
ContentType::~ContentType(void)
{
#ifdef HAVE_LIBMAGIC
	if (magic)
		magic_close(magic);
#endif
}

/**
 * ContentType::guess
 * @fd: open file descriptor
 *
 * Guess file format for open file @fd and return it as a string.
 *
 * The passed descriptor will be duplicated before using so it does not need to
 * be reopened/seeked back.
 *
 * Returns: file MIME type
 */
const char* ContentType::guess(int fd)
{
#ifdef HAVE_LIBMAGIC
	if (magic)
	{
		/* we have to always dup() it;
		 * even if we seek it back to 0, mmap() won't like an used file */
		int dupfd = dup(fd);

		if (dupfd == -1)
			std::cerr << "dup() failed (for Content-Type guessing): "
				<< strerror(errno) << std::endl;
		else
		{
			const char* ct = magic_descriptor(magic, dupfd);

			close(dupfd);
			if (ct)
				return ct;
			std::cerr << "magic_descriptor() failed: " << magic_error(magic)
				<< std::endl;
		}
	}
#endif

	return "application/octet-stream";
}
