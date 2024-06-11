/* pshs -- QRCode printing support
 * (c) 2011 Michał Górny
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "config.h"

#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>

#include <stdio.h>
#ifdef HAVE_LIBQRENCODE
#	include <qrencode.h>
#endif

#include "qrencode.h"

#ifdef HAVE_LIBQRENCODE
static const int qr_margin = 3;
#endif

void print_qrcode(const char* data)
{
#ifdef HAVE_LIBQRENCODE
	int x, y;

	std::unique_ptr<QRcode, std::function<void(QRcode*)>>
		qr{QRcode_encodeString(data, 0, QR_ECLEVEL_L, QR_MODE_8, 1), QRcode_free};

	if (!qr)
	{
		if (errno == ENOMEM)
			throw std::bad_alloc();
		else if (errno == ERANGE)
		{
			std::cerr << "Unable to print QRcode, URL too long" << std::endl;
			return;
		}
		else
			throw std::runtime_error("QRcode_encodeString() failed");
	}

	/* add some margin to ease scanning */
	for (y = 0; y < (qr_margin + 1) / 2; ++y)
	{
		for (x = 0; x < qr->width + 2 * qr_margin; ++x)
			std::cerr << "\xe2\x96\x88";
		std::cerr << '\n';
	}

	/* we need to encode two rows at once to get shape close to square */
	for (y = 0; y < qr->width; y += 2)
	{
		unsigned char* row1 = qr->data + y * qr->width;
		unsigned char* row2 = row1 + qr->width;

		for (x = 0; x < qr_margin; ++x)
			std::cerr << "\xe2\x96\x88";

		for (x = 0; x < qr->width; ++x)
		{
			unsigned char bit1 = row1[x] & 1;
			/* make sure not to go past last row if width is odd */
			unsigned char bit2 = y+1 < qr->width ? row2[x] & 1 : 0;

			if (bit1 && bit2)
				std::cerr << ' ';
			else if (bit1)
				std::cerr << "\xe2\x96\x84"; /* lower half block */
			else if (bit2)
				std::cerr << "\xe2\x96\x80"; /* upper half block */
			else
				std::cerr << "\xe2\x96\x88"; /* full block */
		}

		for (x = 0; x < qr_margin; ++x)
			std::cerr << "\xe2\x96\x88";

		std::cerr << '\n';
	}

	/* add some margin to ease scanning
	 * (if width is odd, we have a blank line at the bottom
	 * which we can count into margin) */
	for (y = 0; y < (qr_margin + 1 - (qr->width % 2)) / 2; ++y)
	{
		for (x = 0; x < qr->width + 2 * qr_margin; ++x)
			std::cerr << "\xe2\x96\x88";
		std::cerr << std::endl;
	}
#endif
}
