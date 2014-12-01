/* pshs -- QRCode printing support
 * (c) 2011 Michał Górny
 * 2-clause BSD-licensed
 */

#include "config.h"

#include <stdio.h>
#include <qrencode.h>

#include "qrencode.h"

static const int qr_margin = 3;

void print_qrcode(const char* data)
{
	QRcode* qr;
	int x, y;

	qr = QRcode_encodeString(data, 0, QR_ECLEVEL_L, QR_MODE_8, 1);
	if (!qr)
		return;

	/* add some margin to ease scanning */
	for (y = 0; y < (qr_margin + 1) / 2; ++y)
	{
		for (x = 0; x < qr->width + 2 * qr_margin; ++x)
			fputs("\xe2\x96\x88", stderr);
		fputc('\n', stderr);
	}

	/* we need to encode two rows at once to get shape close to square */
	for (y = 0; y < qr->width; y += 2)
	{
		char* row1 = qr->data + y * qr->width;
		char* row2 = row1 + qr->width;

		for (x = 0; x < qr_margin; ++x)
			fputs("\xe2\x96\x88", stderr);

		for (x = 0; x < qr->width; ++x)
		{
			char bit1 = row1[x] & 1;
			/* make sure not to go past last row if width is odd */
			char bit2 = y+1 < qr->width ? row2[x] & 1 : 0;

			if (bit1 && bit2)
				fputc(' ', stderr);
			else if (bit1)
				fputs("\xe2\x96\x84", stderr); /* lower half block */
			else if (bit2)
				fputs("\xe2\x96\x80", stderr); /* upper half block */
			else
				fputs("\xe2\x96\x88", stderr); /* full block */
		}

		for (x = 0; x < qr_margin; ++x)
			fputs("\xe2\x96\x88", stderr);

		fputc('\n', stderr);
	}

	/* add some margin to ease scanning
	 * (if width is odd, we have a blank line at the bottom
	 * which we can count into margin) */
	for (y = 0; y < (qr_margin + 1 - (qr->width % 2)) / 2; ++y)
	{
		for (x = 0; x < qr->width + 2 * qr_margin; ++x)
			fputs("\xe2\x96\x88", stderr);
		fputc('\n', stderr);
	}

	QRcode_free(qr);
}
