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
#include <locale.h>
#ifdef HAVE_NL_LANGINFO
#	include <langinfo.h>
#endif

#include <event2/event.h>
#include <event2/http.h>

#include "content-type.h"
#include "handlers.h"
#include "network.h"
#include "qrencode.h"

/**
 * term_handler
 * @fd: the signal no
 * @what: (unused)
 * @data: the event base
 *
 * Handle SIGTERM or a similar signal -- terminate the main loop.
 */
static void term_handler(evutil_socket_t fd, short what, void* data)
{
	struct event_base* evb = data;
	const char* sig = "unknown";

	switch (fd)
	{
		case SIGINT: sig = "SIGINT"; break;
		case SIGTERM: sig = "SIGTERM"; break;
		case SIGHUP: sig = "SIGHUP"; break;
		case SIGUSR1: sig = "SIGUSR1"; break;
		case SIGUSR2: sig = "SIGUSR2"; break;
	}

	fprintf(stderr, "Terminating due to signal %s.\n", sig);

	event_base_loopbreak(evb);
}

const struct option opts[] =
{
	{ "help", no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'V' },

	{ "bind", required_argument, NULL, 'b' },
	{ "port", required_argument, NULL, 'p' },
	{ "ssl", no_argument, NULL, 's' },
	{ "no-upnp", no_argument, NULL, 'U' },

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
#ifdef HAVE_LIBSSL
"    --ssl, -s            enable SSL/TLS socket\n"
#endif
"    --bind IP, -b IP     bind the server to IP address\n"
"    --port N, -p N       set port to listen on (default: random)\n";

int main(int argc, char* argv[])
{
	/* temporary variables */
	int opt, i;
	char* tmp;

	/* default config */
	const char* bindip = "0.0.0.0";
	unsigned int port = 0;
	int ssl = 0;
	int upnp = 1;

	/* main variables */
	struct event_base* evb;
	struct evhttp* http;

	struct event* sigevents[5];
	const int sigs[5] = { SIGINT, SIGTERM, SIGHUP, SIGUSR1, SIGUSR2 };

	const char* extip;

	setlocale(LC_ALL, "");

	while ((opt = getopt_long(argc, argv, "hVb:p:sU", opts, NULL)) != -1)
	{
		switch (opt)
		{
			case 'b':
				bindip = optarg;
				break;
			case 'p':
				port = strtol(optarg, &tmp, 0);
				/* port needs to be uint16 */
				if (*tmp || !port || port >= 0xffff)
				{
					fprintf(stderr, "Invalid port number: %s\n", optarg);
					return 1;
				}
				break;
			case 's':
				ssl = 1;
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

	/* no files supplied */
	if (argc == optind)
	{
		fprintf(stderr, opt_help, argv[0]);
		return 1;
	}

	/* Remove ./ prefixes from filenames, they're known to cause trouble. */
	for (i = optind; i < argc; i++)
	{
		if (argv[i][0] == '.' && argv[i][1] == '/')
			argv[i] += 2;
	}

	srandom(time(NULL));

	evb = event_base_new();
	if (!evb)
	{
		fprintf(stderr, "event_base_new() failed.\n");
		return 1;
	}
	http = evhttp_new(evb);
	if (!http)
	{
		fprintf(stderr, "evhttp_new() failed.\n");
		return 1;
	}
	/* we're just a small download server, GET & HEAD should handle it all */
	evhttp_set_allowed_methods(http, EVHTTP_REQ_GET | EVHTTP_REQ_HEAD);
	/* generic callback - file download */
	evhttp_set_gencb(http, handle_file, &argv[optind]);
	/* index callback */
	evhttp_set_cb(http, "/", handle_index, &argv[optind]);

	/* if no port was provided, choose a nice random value */
	if (!port)
	{
		/* generate a random port between 0x400 and 0x7fff
		 * e.g. above the privileged ports but below outgoing */
		port = random() % 0x7bff + 0x400;
	}

	if (evhttp_bind_socket(http, bindip, port))
	{
		fprintf(stderr, "evhttp_bind_socket(%s, %d) failed.\n",
				bindip, port);
		return 1;
	}

#ifdef HAVE_NL_LANGINFO
	tmp = nl_langinfo(CODESET);
	if (!*tmp)
#endif
		tmp = NULL;

	/* init helper modules */
	init_charset(tmp);
	init_content_type();
	extip = init_external_ip(port, bindip, upnp);

	if (ssl)
	{
		if (!init_ssl(http, extip))
			return 1;
	}

	fprintf(stderr, "Ready to share %d files.\n", argc - optind);
	fprintf(stderr, "Bound to %s:%d.\n", bindip, port);
	if (extip)
	{
		int bytes_written;
		char* buf;
		char* urlenc = 0;

		if (argc - optind == 1)
		{
			urlenc = evhttp_encode_uri(argv[optind]);
			if (!urlenc)
				fprintf(stderr, "Warning: urlencode for %s failed\n", argv[optind]);
		}

		fputs("Server reachable at: ", stderr);
		bytes_written = fprintf(stderr, "http%s://%s:%d/%s\n",
				ssl ? "s" : "", extip, port,
				urlenc ? urlenc : "");

		buf = malloc(bytes_written); /* has + 1 for NUL thanks to \n */
		if (buf)
		{
			sprintf(buf, "http%s://%s:%d/%s",
				ssl ? "s" : "", extip, port,
				urlenc ? urlenc : "");
			print_qrcode(buf);
			free(buf);
		}

		if (urlenc)
			free(urlenc);
	}

	/* init signal handlers */
	for (i = 0; i < 5; i++)
	{
		sigevents[i] = evsignal_new(evb, sigs[i], term_handler, evb);
		if (!sigevents[i])
			fprintf(stderr, "evsignal_new(%d) failed.\n", sigs[i]);
		else
			event_add(sigevents[i], NULL);
	}

	/* ignore SIGPIPE in case of interrupted connection */
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		fputs("warning: unable to override SIGPIPE, may terminate"
				"on interrupted connections.", stderr);

	/* run the loop */
	event_base_dispatch(evb);

	/* clean up external modules */
	destroy_ssl();
	destroy_external_ip(port);
	destroy_content_type();

	/* clean up signal handlers */
	for (i = 0; i < 5; i++)
	{
		if (sigevents[i])
			event_free(sigevents[i]);
	}

	evhttp_free(http);
	event_base_free(evb);

	return 0;
}
