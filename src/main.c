/* pretty small http server
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/http.h>

#include "content-type.h"

static int has_file(const char *path, const char* const *served) {
	for (; *served; served++) {
		if (!strcmp(path, *served))
			return 1;
	}

	return 0;
}

void handle_file(struct evhttp_request *req, void *data) {
	const char **argv = data;
	const char *vpath = evhttp_request_get_uri(req);

	/* Chop the leading slash. */
	assert(vpath[0] == '/');
	vpath++;

	if (!has_file(vpath, argv))
		evhttp_send_error(req, 404, "Not Found");
	else {
		int fd = open(vpath, O_RDONLY);

		if (fd == -1)
			printf("open(%s) failed: %s\n", vpath, strerror(errno));
		else {
			struct stat st;

			if (fstat(fd, &st))
				printf("fstat(%s) failed: %s\n", vpath, strerror(errno));
			else if (!S_ISREG(st.st_mode))
				printf("fstat(%s) says it is not a regular file\n", vpath);
			else {
				struct evbuffer *buf = evbuffer_new();
				struct evkeyvalq *inhead = evhttp_request_get_input_headers(req);
				struct evkeyvalq *headers = evhttp_request_get_output_headers(req);

				const char *range;

				assert(inhead);
				assert(headers);

				range = evhttp_find_header(inhead, "Range");
				if (range) {
					evhttp_send_error(req, 501, "Not Implemented");
				} else {
					if (evhttp_add_header(headers, "Content-Type",
								guess_content_type(fd)))
						printf("evhttp_add_header(Content-Type) failed\n");

					evbuffer_add_file(buf, fd, 0, st.st_size);
					evhttp_send_reply(req, 200, "OK", buf);

					evbuffer_free(buf);
					return;
				}
			}

			close(fd);
		}
		evhttp_send_error(req, 500, "Internal Server Error");
	}
}

int main(int argc, char *argv[]) {
	struct event_base *evb;
	struct evhttp *http;

	evb = event_base_new();
	if (!evb) {
		printf("event_base_new() failed.\n");
		return 1;
	}
	http = evhttp_new(evb);
	if (!http) {
		printf("evhttp_new() failed.\n");
		return 1;
	}
	evhttp_set_allowed_methods(http, EVHTTP_REQ_GET | EVHTTP_REQ_HEAD);
	evhttp_set_gencb(http, handle_file, &argv[1]);
	if (evhttp_bind_socket(http, "0.0.0.0", 8080)) {
		printf("evhttp_bind_socket() failed.\n");
		return 1;
	}

	init_content_type();

	event_base_dispatch(evb);
	destroy_content_type();
	evhttp_free(http);
	event_base_free(evb);

	return 0;
}
