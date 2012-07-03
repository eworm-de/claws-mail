/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2012 Hiroyuki Yamamoto and the Claws Mail team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#include <glib.h>
#include <ctype.h>

#include "utils.h"


#define MAX_LINELEN	76

#define IS_LBREAK(p) \
	((p)[0] == '\n' ? 1 : ((p)[0] == '\r' && (p)[1] == '\n') ? 2 : 0)

gint qp_encode(gboolean text, gchar *out, const guchar *in, gint len)
{
	/* counters of input/output characters */
	gint inc = 0;
	gint outc = 1; /* one character reserved for '=' soft line break */

	while(inc < len) {
		/* allow literal linebreaks in text */
		if(text) {
			if(IS_LBREAK(in)) {
				/* inserting linebreaks is the job of our caller */
				g_assert(outc <= MAX_LINELEN);
				*out = '\0';
				return inc + IS_LBREAK(in);
			}
			if(IS_LBREAK(in+1)) {
				/* free the reserved character since no softbreak
				 * will be needed after the current character */
				outc--;
				/* guard against whitespace before a literal linebreak */
				if(*in == ' ' || *in == '\t') {
					goto escape;
				}
			}
		}
		if(*in == '=') {
			goto escape;
		}
		/* Cave: Whitespace is unconditionally output literally,
		 * but according to the RFC it must not be output before a
		 * linebreak. 
		 * This requirement is obeyed by quoting all linebreaks
		 * and therefore ending all lines with '='. */
		else if((*in >= ' ' && *in <= '~') || *in == '\t') {
			if(outc + 1 <= MAX_LINELEN) {
				*out++ = *in++;
				outc++;
				inc++;
			}
			else break;
		}
		else {
escape:
			if(outc + 3 <= MAX_LINELEN) {
				*out++ = '=';
				outc++;
				get_hex_str(out, *in);
				out += 2;
				outc += 2;
				in++;
				inc++;
			}
			else break;
		}
	}
	g_assert(outc <= MAX_LINELEN);
	*out++ = '=';
	*out = '\0';
	return inc;
}

void qp_encode_line(gchar *out, const guchar *in) {
	while (*in != '\0') {
		in += qp_encode(TRUE, out, in, strlen(in));

		while(*out != '\0') out++;
		*out++ = '\n';
		*out++ = '\0';
	}
}

gint qp_decode_line(gchar *str)
{
	gchar *inp = str, *outp = str;

	while (*inp != '\0') {
		if (*inp == '=') {
			if (inp[1] && inp[2] &&
			    get_hex_value((guchar *)outp, inp[1], inp[2])
			    == TRUE) {
				inp += 3;
			} else if (inp[1] == '\0' || g_ascii_isspace(inp[1])) {
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

gint qp_decode_const(gchar *out, gint avail, const gchar *str)
{
	const gchar *inp = str;
	gchar *outp = out;

	while (*inp != '\0' && avail > 0) {
		if (*inp == '=') {
			if (inp[1] && inp[2] &&
			    get_hex_value((guchar *)outp, inp[1], inp[2])
			    == TRUE) {
				inp += 3;
			} else if (inp[1] == '\0' || g_ascii_isspace(inp[1])) {
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
		avail--;
	}

	*outp = '\0';

	return outp - out;
}

gint qp_decode_q_encoding(guchar *out, const gchar *in, gint inlen)
{
	const gchar *inp = in;
	guchar *outp = out;

	if (inlen < 0)
		inlen = G_MAXINT;

	while (inp - in < inlen && *inp != '\0') {
		if (*inp == '=' && inp + 3 - in <= inlen) {
			if (get_hex_value(outp, inp[1], inp[2]) == TRUE) {
				inp += 3;
			} else {
				*outp = *inp++;
			}
		} else if (*inp == '_') {
			*outp = ' ';
			inp++;
		} else {
			*outp = *inp++;
		}
		outp++;
	}

	*outp = '\0';

	return outp - out;
}

gint qp_get_q_encoding_len(const guchar *str)
{
	const guchar *inp = str;
	gint len = 0;

	while (*inp != '\0') {
		if (*inp == 0x20)
			len++;
		else if (*inp == '=' || *inp == '?' || *inp == '_' ||
			 *inp < 32 || *inp > 127 || g_ascii_isspace(*inp))
			len += 3;
		else
			len++;

		inp++;
	}

	return len;
}

void qp_q_encode(gchar *out, const guchar *in)
{
	const guchar *inp = in;
	gchar *outp = out;

	while (*inp != '\0') {
		if (*inp == 0x20)
			*outp++ = '_';
		else if (*inp == '=' || *inp == '?' || *inp == '_' ||
			 *inp < 32 || *inp > 127 || g_ascii_isspace(*inp)) {
			*outp++ = '=';
			get_hex_str(outp, *inp);
			outp += 2;
		} else
			*outp++ = *inp;

		inp++;
	}

	*outp = '\0';
}
