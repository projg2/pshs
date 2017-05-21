/* pretty small http server
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"

#include <array>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>

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
	struct event_base* evb = static_cast<event_base*>(data);
	const char* sig = "unknown";

	switch (fd)
	{
		case SIGINT: sig = "SIGINT"; break;
		case SIGTERM: sig = "SIGTERM"; break;
		case SIGHUP: sig = "SIGHUP"; break;
		case SIGUSR1: sig = "SIGUSR1"; break;
		case SIGUSR2: sig = "SIGUSR2"; break;
	}

	std::cerr << "Terminating due to signal " << sig << ".\n";
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
	{ "redirect", no_argument, NULL, 'r' },

	{ 0, 0, 0, 0 }
};

const char opt_help[] =
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
"    --prefix PFX, -P PFX require all URLs to start with the prefix PFX\n"
"    --redirect, -r       redirect / to a single provided file\n";

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
	bool redirect = false;

	/* main variables */
	const std::array<int, 5> sigs{ SIGINT, SIGTERM, SIGHUP, SIGUSR1, SIGUSR2 };

	struct callback_data cb_data;

	setlocale(LC_ALL, "");

	while ((opt = getopt_long(argc, argv, "hVb:p:P:sUr", opts, NULL)) != -1)
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
					std::cerr << "Invalid port number: " << optarg << "\n";
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
				std::cout << PACKAGE_STRING "\n";
				return 0;
			case 'r':
				redirect = true;
				break;
			default:
				std::cout << "Usage: " << argv[0] << " [options] file [...]\n\n"
					<< opt_help;
				return 0;
		}
	}

	/* no files supplied */
	if (argc == optind)
	{
		std::cerr << "Usage: " << argv[0] << " [options] file [...]\n\n"
			<< opt_help;
		return 1;
	}

	/* redirect only supporst a single file */
	if ((argc == optind - 1) && redirect)
	{
		std::cerr << "--redirect only works with a single file\n";
		return 1;
	}

	/* Remove ./ prefixes from filenames, they're known to cause trouble. */
	for (int i = optind; i < argc; ++i)
	{
		if (argv[i][0] == '.' && argv[i][1] == '/')
			argv[i] += 2;
	}

	void (*handle_index)(evhttp_request*, void*) = redirect
		? handle_index_with_redirect : handle_index_with_list;

	srandom(time(NULL));

	cb_data.prefix = prefix;
	if (prefix)
		cb_data.prefix_len = strlen(prefix);
	cb_data.files = &argv[optind];

	std::unique_ptr<event_base, std::function<void(event_base*)>>
		evb{event_base_new(), event_base_free};
	if (!evb)
		throw std::runtime_error("event_base_new() failed");

	std::unique_ptr<evhttp, std::function<void(evhttp*)>>
		http{evhttp_new(evb.get()), evhttp_free};
	if (!http)
		throw std::runtime_error("evhttp_new() failed");
	/* we're just a small download server, GET & HEAD should handle it all */
	evhttp_set_allowed_methods(http.get(), EVHTTP_REQ_GET | EVHTTP_REQ_HEAD);
	/* generic callback - file download */
	evhttp_set_gencb(http.get(), handle_file, &cb_data);
	/* index callback */
	if (!prefix)
		evhttp_set_cb(http.get(), "/", handle_index, &cb_data);
	else
	{
		std::stringstream index_uri;
		index_uri << '/' << prefix << '/';

		evhttp_set_cb(http.get(), index_uri.str().c_str(), handle_index, &cb_data);
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
		std::cerr << "Unable to bind socket to " << bindip
			<< ':' << port << "\n";
		return 1;
	}

#ifdef HAVE_NL_LANGINFO
	tmp = nl_langinfo(CODESET);
	if (!*tmp)
#endif
		tmp = NULL;

	/* init helper modules */
	init_charset(tmp);
	ContentType ct;
	cb_data.ct = &ct;

	ExternalIP extip{port, bindip, upnp};
	SSLMod ssl_mod(http.get(), extip.addr, ssl);

	std::cerr << "Ready to share " << argc - optind << " files.\n"
		"Bound to " << bindip << ':' << port << '.' << std::endl;
	if (extip.addr)
	{
		std::stringstream server_uri;
		server_uri << "http";
		if (ssl)
			server_uri << 's';
		server_uri << "://" << extip.addr << ':' << port << '/';
		if (prefix)
			server_uri << prefix << '/';
		if (argc - optind == 1)
		{
			std::unique_ptr<char, std::function<void(char*)>>
				urlenc{evhttp_encode_uri(argv[optind]), free};
			if (!urlenc)
				throw std::bad_alloc();
			server_uri << urlenc.get();
		}

		std::cerr << "Server reachable at: " << server_uri.str() << std::endl;
		print_qrcode(server_uri.str().c_str());
	}

	std::array<std::unique_ptr<event, std::function<void(event*)>>, sigs.size()>
		sigevents;

	/* init signal handlers */
	for (size_t i = 0; i < sigs.size(); ++i)
	{
		sigevents[i] = {evsignal_new(evb.get(), sigs[i], term_handler, evb.get()), event_free};
		if (!sigevents[i])
			std::cerr << "evsignal_new(" << sigs[i] << ") failed." << std::endl;
		else;
			event_add(sigevents[i].get(), NULL);
	}

	/* ignore SIGPIPE in case of interrupted connection */
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		std::cerr << "warning: unable to override SIGPIPE, may terminate"
				"on interrupted connections." << std::endl;

	/* run the loop */
	event_base_dispatch(evb.get());

	return 0;
}
