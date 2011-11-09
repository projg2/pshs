/* pretty small http server
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <signal.h>

#include <getopt.h>

#include <event2/event.h>
#include <event2/http.h>

#include "handlers.h"
#include "network.h"

static void term_handler(evutil_socket_t fd, short what, void *data) {
	struct event_base *evb = data;

	event_base_loopbreak(evb);
}

const struct option opts[] = {
	{ "help", no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'V' },

	{ "port", required_argument, NULL, 'p' },
	{ "no-upnp", required_argument, NULL, 'U' },

	{ 0, 0, 0, 0 }
};

const char opt_help[] =
"Usage: %s [options] file [...]\n"
"\n"
"Options:\n"
"    --help, -h           print this help message\n"
"    --version, -V        print program version and exit\n"
"\n"
#ifdef HAVE_LIBMINIUPNPC
"    --no-upnp, -U        disable port redirection using UPnP\n"
#endif
"    --port N, -p N       set port to listen on (default: random)\n";

int main(int argc, char *argv[]) {
	int opt;
	unsigned int port = 0;
	int upnp = 1;
	char *tmp;

	struct event_base *evb;
	struct evhttp *http;

	struct event *sigevents[5];
	const int sigs[5] = { SIGINT, SIGTERM, SIGHUP, SIGUSR1, SIGUSR2 };
	int i;

	const char *extip;

	while ((opt = getopt_long(argc, argv, "hVp:U", opts, NULL)) != -1) {
		switch (opt) {
			case 'p':
				port = strtol(optarg, &tmp, 0);
				if (*tmp || !port || port >= 0xffff) {
					printf("Invalid port number: %s\n", optarg);
					return 1;
				}
				break;
			case 'U':
				upnp = 0;
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

	event_enable_debug_mode();

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
	extip = init_external_ip(port, upnp);

	printf("Ready to share %d files.\n", argc - optind);
	printf("Bound to 0.0.0.0:%d.\n", port);
	if (extip)
		printf("Files available at: http://%s:%d/\n", extip, port);

	for (i = 0; i < 5; i++) {
		sigevents[i] = evsignal_new(evb, sigs[i], term_handler, evb);
		if (!sigevents[i])
			printf("evsignal_new(%d) failed.\n", sigs[i]);
		else
			event_add(sigevents[i], NULL);
	}

	event_base_dispatch(evb);

	destroy_external_ip(port);
	destroy_content_type();

	for (i = 0; i < 5; i++) {
		if (sigevents[i])
			event_free(sigevents[i]);
	}
	evhttp_free(http);
	event_base_free(evb);

	return 0;
}
