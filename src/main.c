/* pretty small http server
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <getopt.h>

#include <event2/event.h>
#include <event2/http.h>

#include "handlers.h"

const struct option opts[] = {
	{ "help", no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'V' },

	{ "port", required_argument, NULL, 'p' },

	{ 0, 0, 0, 0 }
};

const char opt_help[] =
"Usage: %s [options] file [...]\n"
"\n"
"Options:\n"
"    --help, -h           print this help message\n"
"    --version, -V        print program version and exit\n"
"\n"
"    --port N, -p N       set port to listen on (default: random)\n";

int main(int argc, char *argv[]) {
	int opt;
	unsigned int port = 0;
	char *tmp;

	struct event_base *evb;
	struct evhttp *http;

	while ((opt = getopt_long(argc, argv, "hVp:", opts, NULL)) != -1) {
		switch (opt) {
			case 'p':
				port = strtol(optarg, &tmp, 0);
				if (*tmp || !port || port >= 0xffff) {
					printf("Invalid port number: %s\n", optarg);
					return 1;
				}
				break;
			case 'V':
				printf("%s\n", PACKAGE_STRING);
				return 0;
			default:
				printf(opt_help, argv[0]);
				return 0;
		}
	}

	if (argc - optind == 0) {
		printf(opt_help, argv[0]);
		return 1;
	}

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
	evhttp_set_gencb(http, handle_file, &argv[optind]);
	evhttp_set_cb(http, "/", handle_index, &argv[optind]);

	if (!port) {
		srandom(time(NULL));
		/* generate a random port between 0x400 and 0x7fff
		 * e.g. above the privileged ports but below outgoing */
		port = random() % 0x7bff + 0x400;
	}

	if (evhttp_bind_socket(http, "0.0.0.0", port)) {
		printf("evhttp_bind_socket(0.0.0.0, %d) failed.\n", port);
		return 1;
	}

	init_content_type();

	printf("Ready to share %d files.\n", argc - optind);
	printf("Bound to 0.0.0.0:%d.\n", port);

	event_base_dispatch(evb);
	destroy_content_type();
	evhttp_free(http);
	event_base_free(evb);

	return 0;
}
