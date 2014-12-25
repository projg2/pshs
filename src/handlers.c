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

#ifdef HAVE_STDINT_H
#	include <stdint.h>
#endif
#ifdef HAVE_INTTYPES_H
#	include <inttypes.h>
#endif
#ifndef SCNdMAX
#	define SCNdMAX "ld"
#endif
#ifndef PRIdMAX
#	define PRIdMAX "ld"
#endif

#include <event2/buffer.h>

#include "handlers.h"
#include "content-type.h"
#include "index.h"

char ct_buf[80];

/**
 * init_charset
 * @charset: new charset or %NULL
 *
 * Set the new charset (file name encoding).
 */
void init_charset(const char* charset)
{
	strcpy(ct_buf, "text/html"); /* 9b */
	if (charset && strlen(charset) < sizeof(ct_buf) - 20)
	{
		strcpy(&ct_buf[9], "; charset=");
		strcpy(&ct_buf[19], charset);
	}
}

/**
 * has_file
 * @path: requested path
 * @served: null-terminated served file list
 *
 * Check whether requested @path is on the @served list.
 *
 * Returns: 1 if it is, 0 otherwise
 */
static int has_file(const char* path, const char* const* served)
{
	for (; *served; served++)
	{
		if (!strcmp(path, *served))
			return 1;
	}

	return 0;
}

/**
 * print_req
 * @req: the request object
 *
 * Log the request source and the requested path to stdout.
 */
static void print_req(struct evhttp_request* req)
{
	const char* uri = evhttp_request_get_uri(req);
	struct evhttp_connection* conn = evhttp_request_get_connection(req);

	char* addr;
	ev_uint16_t port;

	assert(conn);
	evhttp_connection_get_peer(conn, &addr, &port);
	printf("[%s:%d] %s\n", addr, port, uri);
}

/**
 * handle_file
 * @req: the request object
 * @data: served file list
 *
 * Handle the request for regular file. Check whether the file is served, get
 * its type, send correct headers and the file contents.
 *
 * If file is not served, 404 is sent back. If file is unreadable somehow, 500
 * is sent instead.
 */
void handle_file(struct evhttp_request* req, void* data)
{
	const char** argv = data;
	const char* vpath = evhttp_request_get_uri(req);
	char* dpath;

	/* Chop the leading slash. */
	assert(vpath[0] == '/');
	vpath++;

	print_req(req);

	dpath = evhttp_decode_uri(vpath);
	if (!dpath)
	{
		fprintf(stderr, "Unable to decode URI: %s\n", vpath);
		evhttp_send_error(req, 500, "Internal Server Error");
		return;
	}

	if (!has_file(dpath, argv))
		evhttp_send_error(req, 404, "Not Found");
	else
	{
		int fd = open(dpath, O_RDONLY);

		if (fd == -1)
			fprintf(stderr, "open(%s) failed: %s\n", dpath, strerror(errno));
		else
		{
			struct stat st;

			/* we need to have a regular file here,
			 * with static Content-Length */
			if (fstat(fd, &st))
				fprintf(stderr, "fstat(%s) failed: %s\n", dpath, strerror(errno));
			else if (!S_ISREG(st.st_mode))
				fprintf(stderr, "fstat(%s) says it is not a regular file\n", dpath);
			else
			{
				struct evbuffer* buf = evbuffer_new();
				struct evkeyvalq* inhead = evhttp_request_get_input_headers(req);
				struct evkeyvalq* headers = evhttp_request_get_output_headers(req);

				const char* range;
				intmax_t first = 0, last = -1;
				ev_off_t size = st.st_size;

				assert(inhead);
				assert(headers);

				range = evhttp_find_header(inhead, "Range");
				if (range)
				{
					/* We support only single byte range,
					 * so fail on ',' or invalid bytes=%d-%d */
					if (strchr(range, ',') || sscanf(range,
								"bytes = %" SCNdMAX " - %" SCNdMAX,
								&first, &last) < 1)
					{
						evhttp_send_error(req, 501, "Not Implemented");
						close(fd);
						free(dpath);
						return;
					}
				}

				if (first < 0)
					first += size;
				if (last < 0)
					last += size;

				if (range && first > last)
				{
					evhttp_send_error(req, 416, "Requested Range Not Satisfiable");
					close(fd);
					free(dpath);
					return;
				}

				/* Good Content-Type is nice for users. */
				if (evhttp_add_header(headers, "Content-Type",
							guess_content_type(fd)))
					fprintf(stderr, "evhttp_add_header(Content-Type) failed\n");

				/* Send the file. */
#if 0 /* breaks ssl support */
				evbuffer_set_flags(buf, EVBUFFER_FLAG_DRAINS_TO_FD);
#endif
				if (size != 0)
					evbuffer_add_file(buf, fd, first, last - first + 1);
				if (range)
				{
					char lenbuf[96]; /* XXX */
					sprintf(lenbuf, "bytes %" PRIdMAX "-%" PRIdMAX
							"/%" PRIdMAX, first, last, (intmax_t) size);

					if (evhttp_add_header(headers, "Content-Range", lenbuf))
						fprintf(stderr, "evhttp_add_header(Content-Range) failed\n");
					evhttp_send_reply(req, 206, "Partial Content", buf);
				} else
					evhttp_send_reply(req, 200, "OK", buf);

				evbuffer_free(buf);
				free(dpath);
				return;
			}

			close(fd);
		}
		evhttp_send_error(req, 500, "Internal Server Error");
	}

	free(dpath);
}

/**
 * handle_index
 * @req: the request object
 * @data: served filelist
 *
 * Handle index (/) request. Send back the HTML filelist.
 */
void handle_index(struct evhttp_request* req, void* data)
{
	const char** argv = data;
	struct evbuffer* buf = evbuffer_new();
	struct evkeyvalq* headers = evhttp_request_get_output_headers(req);

	print_req(req);

	assert(headers);
	if (evhttp_add_header(headers, "Content-Type",
				"text/html; charset=utf-8"))
		fprintf(stderr, "evhttp_add_header(Content-Type) failed\n");

	generate_index(buf, argv);

	evhttp_send_reply(req, 200, "OK", buf);
	evbuffer_free(buf);
}
