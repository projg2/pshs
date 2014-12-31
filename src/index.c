/* pshs -- file index generation
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <event2/http.h>

#include "index.h"

/* Building parts of the index page. */

const char head[] = "<!DOCTYPE html>\n"
	"<html>"
		"<head>"
			"<meta charset='utf-8'/>"
			"<style type='text/css'>"
				"address {"
					"font-size: 6em;"
					"color: #eee;"
					"position: fixed;"
					"right: 2cm;"
					"bottom: 1cm;"
					"z-index: -1000;"
				"}"
			"</style>"
		"</head>"
		"<body>"
			"<ol>";

const char tail[] =
			"</ol>"
			"<address>pshs</address>"
		"</body>"
	"</html>";

/* Building parts of a single link. */

const char filenameprefix[] = "<li><a href='";
const char filenamemidfix[] = "'>";
const char filenamesuffix[] = "</a></li>";

/**
 * generate_index
 * @buf: target buffer
 * @files: filelist
 *
 * Generate HTML index of files in @filelist and write it to buffer @buf.
 */
void generate_index(struct evbuffer* buf, char* const* files)
{
	evbuffer_add_reference(buf, head, sizeof(head)-1, NULL, NULL);

	for (; *files; files++)
	{
		char* urlenc;
		char* htmlenc;

		urlenc = evhttp_encode_uri(*files);
		if (!urlenc)
		{
			fprintf(stderr, "urlencode failed for %s\n", *files);
			continue;
		}
		htmlenc = evhttp_htmlescape(*files);
		if (!htmlenc)
		{
			fprintf(stderr, "html-escape failed for %s\n", *files);
			free(urlenc);
			continue;
		}

		evbuffer_add_reference(buf, filenameprefix,
				sizeof(filenameprefix)-1, NULL, NULL);
		evbuffer_add(buf, urlenc, strlen(urlenc));
		evbuffer_add_reference(buf, filenamemidfix,
				sizeof(filenamemidfix)-1, NULL, NULL);
		evbuffer_add(buf, htmlenc, strlen(htmlenc));
		evbuffer_add_reference(buf, filenamesuffix,
				sizeof(filenamesuffix)-1, NULL, NULL);

		free(htmlenc);
		free(urlenc);
	}

	evbuffer_add_reference(buf, tail, sizeof(tail)-1, NULL, NULL);
}
