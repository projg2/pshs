/* pretty small http server
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"

#include <array>
#include <functional>
#include <memory>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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
#include "ssl.h"

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
	struct event_base* evb = (struct event_base*) data;
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
"    --port N, -p N       set port to listen on (default: random)\n"
"    --prefix PFX, -P PFX require all URLs to start with the prefix PFX\n";

int main(int argc, char* argv[])
{
	/* temporary variables */
	int opt;
	char* tmp;

	/* default config */
	const char* prefix = 0;
	const char* bindip = "0.0.0.0";
	unsigned int port = 0;
	int ssl = 0;
	int upnp = 1;

	/* main variables */
	const std::array<int, 5> sigs{ SIGINT, SIGTERM, SIGHUP, SIGUSR1, SIGUSR2 };

	const char* extip;

	struct callback_data cb_data;

	setlocale(LC_ALL, "");

	while ((opt = getopt_long(argc, argv, "hVb:p:P:sU", opts, NULL)) != -1)
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
			case 'P':
				prefix = optarg;
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
	for (int i = optind; i < argc; ++i)
	{
		if (argv[i][0] == '.' && argv[i][1] == '/')
			argv[i] += 2;
	}

	srandom(time(NULL));

	cb_data.prefix = prefix;
	if (prefix)
		cb_data.prefix_len = strlen(prefix);
	cb_data.files = &argv[optind];

	std::unique_ptr<event_base, std::function<void(event_base*)>>
		evb{event_base_new(), event_base_free};
	if (!evb)
	{
		fprintf(stderr, "event_base_new() failed.\n");
		return 1;
	}

	std::unique_ptr<evhttp, std::function<void(evhttp*)>>
		http{evhttp_new(evb.get()), evhttp_free};
	if (!http)
	{
		fprintf(stderr, "evhttp_new() failed.\n");
		return 1;
	}
	/* we're just a small download server, GET & HEAD should handle it all */
	evhttp_set_allowed_methods(http.get(), EVHTTP_REQ_GET | EVHTTP_REQ_HEAD);
	/* generic callback - file download */
	evhttp_set_gencb(http.get(), handle_file, &cb_data);
	/* index callback */
	if (!prefix)
		evhttp_set_cb(http.get(), "/", handle_index, &cb_data);
	else
	{
		char* index_uri = (char*) malloc(cb_data.prefix_len + 3); /* 2x/ + \0 */

		if (!index_uri)
		{
			fprintf(stderr, "malloc() for index_uri failed.\n");
			return 1;
		}

		index_uri[0] = '/';
		strcpy(&index_uri[1], prefix);
		index_uri[cb_data.prefix_len + 1] = '/';
		index_uri[cb_data.prefix_len + 2] = 0;

		evhttp_set_cb(http.get(), index_uri, handle_index, &cb_data);

		free(index_uri);
	}

	/* if no port was provided, choose a nice random value */
	if (!port)
	{
		/* generate a random port between 0x400 and 0x7fff
		 * e.g. above the privileged ports but below outgoing */
		port = random() % 0x7bff + 0x400;
	}

	if (evhttp_bind_socket(http.get(), bindip, port))
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
		if (!init_ssl(http.get(), extip))
		{
			destroy_external_ip(port);
			destroy_content_type();
			return 1;
		}
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
		bytes_written = fprintf(stderr, "http%s://%s:%d/%s%s%s\n",
				ssl ? "s" : "", extip, port,
				prefix ? prefix : "",
				prefix ? "/" : "",
				urlenc ? urlenc : "");

		buf = (char*) malloc(bytes_written); /* has + 1 for NUL thanks to \n */
		if (buf)
		{
			sprintf(buf, "http%s://%s:%d/%s%s%s",
				ssl ? "s" : "", extip, port,
				prefix ? prefix : "",
				prefix ? "/" : "",
				urlenc ? urlenc : "");
			print_qrcode(buf);
			free(buf);
		}

		if (urlenc)
			free(urlenc);
	}

	std::array<std::unique_ptr<event, std::function<void(event*)>>, sigs.size()>
		sigevents{std::unique_ptr<event, std::function<void(event*)>>{nullptr, event_free}};

	/* init signal handlers */
	for (size_t i = 0; i < sigs.size(); ++i)
	{
		sigevents[i].reset(evsignal_new(evb.get(), sigs[i], term_handler, evb.get()));
		if (!sigevents[i])
			fprintf(stderr, "evsignal_new(%d) failed.\n", sigs[i]);
		else;
			event_add(sigevents[i].get(), NULL);
	}

	/* ignore SIGPIPE in case of interrupted connection */
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		fputs("warning: unable to override SIGPIPE, may terminate"
				"on interrupted connections.", stderr);

	/* run the loop */
	event_base_dispatch(evb.get());

	/* clean up external modules */
	destroy_ssl();
	destroy_external_ip(port);
	destroy_content_type();

	return 0;
}
