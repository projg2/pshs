/* pshs -- request handlers
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

#include "handlers.h"
#include "content-type.h"

static int has_file(const char *path, const char* const *served) {
	for (; *served; served++) {
		if (!strcmp(path, *served))
			return 1;
	}

	return 0;
}

static void print_req(struct evhttp_request *req) {
	const char *uri = evhttp_request_get_uri(req);
	struct evhttp_connection *conn = evhttp_request_get_connection(req);

	char *addr;
	ev_uint16_t port;

	assert(conn);
	evhttp_connection_get_peer(conn, &addr, &port);
	printf("[%s:%d] %s\n", addr, port, uri);
}

void handle_file(struct evhttp_request *req, void *data) {
	const char **argv = data;
	const char *vpath = evhttp_request_get_uri(req);

	/* Chop the leading slash. */
	assert(vpath[0] == '/');
	vpath++;

	print_req(req);

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

void handle_index(struct evhttp_request *req, void *data) {
	const char **argv = data;
	struct evbuffer *buf = evbuffer_new();
	struct evkeyvalq *headers = evhttp_request_get_output_headers(req);

	print_req(req);

	assert(headers);
	if (evhttp_add_header(headers, "Content-Type",
				"text/html; charset=utf-8"))
		printf("evhttp_add_header(Content-Type) failed\n");

	generate_index(buf, argv);

	evhttp_send_reply(req, 200, "OK", buf);
	evbuffer_free(buf);
}
