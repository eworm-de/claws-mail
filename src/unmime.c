/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <string.h>
#include <ctype.h>

#include "codeconv.h"
#include "base64.h"

#define ENCODED_WORD_BEGIN	"=?"
#define ENCODED_WORD_END	"?="

static gboolean get_hex_value(gchar *out, gchar c1, gchar c2);

/* Decodes headers based on RFC2045 and RFC2047. */

void unmime_header(gchar *out, const gchar *str)
{
	const gchar *p = str;
	gchar *outp = out;
	const gchar *sp;
	const gchar *eword_begin_p, *encoding_begin_p, *text_begin_p,
		    *eword_end_p;
	gchar charset[32];
	gchar encoding;
	gchar *conv_str;
	gint len;

	while (*p != '\0') {
		gchar *decoded_text = NULL;

		eword_begin_p = strstr(p, ENCODED_WORD_BEGIN);
		if (!eword_begin_p) {
			strcpy(outp, p);
			return;
		}
		encoding_begin_p = strchr(eword_begin_p + 2, '?');
		if (!encoding_begin_p) {
			strcpy(outp, p);
			return;
		}
		text_begin_p = strchr(encoding_begin_p + 1, '?');
		if (!text_begin_p) {
			strcpy(outp, p);
			return;
		}
		eword_end_p = strstr(text_begin_p + 1, ENCODED_WORD_END);
		if (!eword_end_p) {
			strcpy(outp, p);
			return;
		}

		if (p == str) {
			memcpy(outp, p, eword_begin_p - p);
			outp += eword_begin_p - p;
			p = eword_begin_p;
		} else {
			/* ignore spaces between encoded words */
			for (sp = p; sp < eword_begin_p; sp++) {
				if (!isspace(*sp)) {
					memcpy(outp, p, eword_begin_p - p);
					outp += eword_begin_p - p;
					p = eword_begin_p;
					break;
				}
			}
		}

		len = MIN(sizeof(charset) - 1,
			  encoding_begin_p - (eword_begin_p + 2));
		memcpy(charset, eword_begin_p + 2, len);
		charset[len] = '\0';
		encoding = toupper(*(encoding_begin_p + 1));

		if (encoding == 'B') {
			decoded_text = g_malloc
				(eword_end_p - (text_begin_p + 1) + 1);
			len = base64_decode(decoded_text, text_begin_p + 1,
					    eword_end_p - (text_begin_p + 1));
			decoded_text[len] = '\0';
		} else if (encoding == 'Q') {
			const gchar *ep = text_begin_p + 1;
			gchar *dp;

			dp = decoded_text = g_malloc(eword_end_p - ep + 1);

			while (ep < eword_end_p) {
				if (*ep == '=' && ep + 3 <= eword_end_p) {
					if (get_hex_value(dp, ep[1], ep[2])
					    == TRUE) {
						ep += 3;
					} else {
						*dp = *ep++;
					}
				} else if (*ep == '_') {
					*dp = ' ';
					ep++;
				} else {
					*dp = *ep++;
				}
				dp++;
			}

			*dp = '\0';
		} else {
			memcpy(outp, p, eword_end_p + 2 - p);
			outp += eword_end_p + 2 - p;
			p = eword_end_p + 2;
			continue;
		}

		/* convert to locale encoding */
		conv_str = conv_codeset_strdup(decoded_text, charset, NULL);
		if (conv_str) {
			len = strlen(conv_str);
			memcpy(outp, conv_str, len);
			g_free(conv_str);
		} else {
			len = strlen(decoded_text);
			memcpy(outp, decoded_text, len);
		}
		outp += len;

		g_free(decoded_text);

		p = eword_end_p + 2;
	}

	*outp = '\0';
}

gint unmime_quoted_printable_line(gchar *str)
{
	gchar *inp = str, *outp = str;

	while (*inp != '\0') {
		if (*inp == '=') {
			if (inp[1] && inp[2] &&
			    get_hex_value(outp, inp[1], inp[2]) == TRUE) {
				inp += 3;
			} else if (inp[1] == '\0' || isspace(inp[1])) {
				/* soft line break */
				break;
			} else {
				/* broken QP string */
				*outp = *inp++;
			}
		} else {
			*outp = *inp++;
		}
		outp++;
	}

	*outp = '\0';

	return outp - str;
}

#define HEX_TO_INT(val, hex)			\
{						\
	gchar c = hex;				\
						\
	if ('0' <= c && c <= '9') {		\
		val = c - '0';			\
	} else if ('a' <= c && c <= 'f') {	\
		val = c - 'a' + 10;		\
	} else if ('A' <= c && c <= 'F') {	\
		val = c - 'A' + 10;		\
	} else {				\
		val = -1;			\
	}					\
}

static gboolean get_hex_value(gchar *out, gchar c1, gchar c2)
{
	gint hi, lo;

	HEX_TO_INT(hi, c1);
	HEX_TO_INT(lo, c2);

	if (hi == -1 || lo == -1)
		return FALSE;

	*out = (hi << 4) + lo;
	return TRUE;
}
