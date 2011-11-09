/* pshs -- Content-Type guessing support
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <errno.h>

#ifdef HAVE_LIBMAGIC
#	include <magic.h>

magic_t magic;
#endif

#include "content-type.h"

void init_content_type(void) {
#ifdef HAVE_LIBMAGIC
	magic = magic_open(MAGIC_MIME);
	if (!magic)
		printf("magic_open() failed: %s\n", strerror(errno));
	else {
		if (magic_load(magic, NULL)) {
			printf("magic_load() failed: %s\n", magic_error(magic));
			magic_close(magic);
			magic = NULL;
		}
	}
#endif
}

void destroy_content_type(void) {
#ifdef HAVE_LIBMAGIC
	if (magic)
		magic_close(magic);
#endif
}

const char *guess_content_type(int fd) {
#ifdef HAVE_LIBMAGIC
	if (magic) {
		int dupfd = dup(fd);

		if (dupfd == -1)
			printf("dup() failed: %s\n", strerror(errno));
		else {
			const char *ct = magic_descriptor(magic, dupfd);

			close(dupfd);
			if (ct)
				return ct;
			printf("magic_descriptor() failed: %s\n", magic_error(magic));
		}
	}
#endif

	return "application/octet-stream";
}
