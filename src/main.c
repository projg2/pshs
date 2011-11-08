/* pretty small http server
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include <stdlib.h>
#include <stdio.h>

#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/http.h>

void handle_file(struct evhttp_request *req, void *data) {
	const char **argv = data;
	const char *vpath = evhttp_request_get_uri(req);

	evbuffer *buf = evbuffer_new();

	evbuffer_add_printf(buf, "testzor\n");
	evhttp_send_reply(req, 200, "OK", buf);

	evbuffer_free(buf);
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

	event_base_dispatch(evb);
	evhttp_free(http);
	event_base_free(evb);

	return 0;
}
