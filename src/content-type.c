/* pshs -- Content-Type guessing support
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"

const char *guess_content_type(int fd) {
	return "application/octet-stream";
}
