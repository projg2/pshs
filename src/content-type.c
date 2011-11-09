/* pshs -- Content-Type guessing support
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "content-type.h"

void init_content_type(void) {
}

void destroy_content_type(void) {
}

int open_ct(const char *path, const char **ct, ev_off_t *size) {
	struct stat st;
	int fd = open(path, O_RDONLY);

	if (fd == -1)
		printf("open(%s) failed: %s\n", path, strerror(errno));
	else {
		struct stat st;

		if (fstat(fd, &st))
			printf("fstat(%s) failed: %s\n", path, strerror(errno));
		else if (!S_ISREG(st.st_mode))
			printf("fstat(%s) says it is not a regular file\n", path);
		else {
			*size = st.st_size;
			*ct = "application/octet-stream";
			return fd;
		}

		close(fd);
	}
	return -1;
}
