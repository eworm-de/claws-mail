/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto
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
#include <stdlib.h>
#include <errno.h>

#if HAVE_LOCALE_H
#  include <locale.h>
#endif

#if HAVE_ICONV
#  include <iconv.h>
#endif

#include "intl.h"
#include "codeconv.h"
#include "unmime.h"
#include "base64.h"
#include "quoted-printable.h"
#include "utils.h"
#include "prefs_common.h"

typedef enum
{
	JIS_ASCII,
	JIS_KANJI,
	JIS_HWKANA,
	JIS_AUXKANJI
} JISState;

#define SUBST_CHAR	'_'
#define ESC		'\033'

#define iseuckanji(c) \
	(((c) & 0xff) >= 0xa1 && ((c) & 0xff) <= 0xfe)
#define iseuchwkana1(c) \
	(((c) & 0xff) == 0x8e)
#define iseuchwkana2(c) \
	(((c) & 0xff) >= 0xa1 && ((c) & 0xff) <= 0xdf)
#define iseucaux(c) \
	(((c) & 0xff) == 0x8f)
#define isunprintableeuckanji(c) \
	(((c) & 0xff) >= 0xa9 && ((c) & 0xff) <= 0xaf)
#define issjiskanji1(c) \
	((((c) & 0xff) >= 0x81 && ((c) & 0xff) <= 0x9f) || \
	 (((c) & 0xff) >= 0xe0 && ((c) & 0xff) <= 0xfc))
#define issjiskanji2(c) \
	((((c) & 0xff) >= 0x40 && ((c) & 0xff) <= 0x7e) || \
	 (((c) & 0xff) >= 0x80 && ((c) & 0xff) <= 0xfc))
#define issjishwkana(c) \
	(((c) & 0xff) >= 0xa1 && ((c) & 0xff) <= 0xdf)

#define K_IN()				\
	if (state != JIS_KANJI) {	\
		*out++ = ESC;		\
		*out++ = '$';		\
		*out++ = 'B';		\
		state = JIS_KANJI;	\
	}

#define K_OUT()				\
	if (state != JIS_ASCII) {	\
		*out++ = ESC;		\
		*out++ = '(';		\
		*out++ = 'B';		\
		state = JIS_ASCII;	\
	}

#define HW_IN()				\
	if (state != JIS_HWKANA) {	\
		*out++ = ESC;		\
		*out++ = '(';		\
		*out++ = 'I';		\
		state = JIS_HWKANA;	\
	}

#define AUX_IN()			\
	if (state != JIS_AUXKANJI) {	\
		*out++ = ESC;		\
		*out++ = '$';		\
		*out++ = '(';		\
		*out++ = 'D';		\
		state = JIS_AUXKANJI;	\
	}

void conv_jistoeuc(gchar *outbuf, gint outlen, const gchar *inbuf)
{
	const guchar *in = inbuf;
	guchar *out = outbuf;
	JISState state = JIS_ASCII;

	while (*in != '\0') {
		if (*in == ESC) {
			in++;
			if (*in == '$') {
				if (*(in + 1) == '@' || *(in + 1) == 'B') {
					state = JIS_KANJI;
					in += 2;
				} else if (*(in + 1) == '(' &&
					   *(in + 2) == 'D') {
					state = JIS_AUXKANJI;
					in += 3;
				} else {
					/* unknown escape sequence */
					state = JIS_ASCII;
				}
			} else if (*in == '(') {
				if (*(in + 1) == 'B' || *(in + 1) == 'J') {
					state = JIS_ASCII;
					in += 2;
				} else if (*(in + 1) == 'I') {
					state = JIS_HWKANA;
					in += 2;
				} else {
					/* unknown escape sequence */
					state = JIS_ASCII;
				}
			} else {
				/* unknown escape sequence */
				state = JIS_ASCII;
			}
		} else if (*in == 0x0e) {
			state = JIS_HWKANA;
			in++;
		} else if (*in == 0x0f) {
			state = JIS_ASCII;
			in++;
		} else {
			switch (state) {
			case JIS_ASCII:
				*out++ = *in++;
				break;
			case JIS_KANJI:
				*out++ = *in++ | 0x80;
				if (*in == '\0') break;
				*out++ = *in++ | 0x80;
				break;
			case JIS_HWKANA:
				*out++ = 0x8e;
				*out++ = *in++ | 0x80;
				break;
			case JIS_AUXKANJI:
				*out++ = 0x8f;
				*out++ = *in++ | 0x80;
				if (*in == '\0') break;
				*out++ = *in++ | 0x80;
				break;
			}
		}
	}

	*out = '\0';
}

void conv_euctojis(gchar *outbuf, gint outlen, const gchar *inbuf)
{
	const guchar *in = inbuf;
	guchar *out = outbuf;
	JISState state = JIS_ASCII;

	while (*in != '\0') {
		if (isascii(*in)) {
			K_OUT();
			*out++ = *in++;
		} else if (iseuckanji(*in)) {
			if (iseuckanji(*(in + 1))) {
				K_IN();
				*out++ = *in++ & 0x7f;
				*out++ = *in++ & 0x7f;
			} else {
				K_OUT();
				*out++ = SUBST_CHAR;
				in++;
				if (*in != '\0' && !isascii(*in)) {
					*out++ = SUBST_CHAR;
					in++;
				}
			}
		} else if (iseuchwkana1(*in)) {
			in++;
			if (iseuchwkana2(*in)) {
				HW_IN();
				*out++ = *in++ & 0x7f;
			} else {
				K_OUT();
				if (*in != '\0' && !isascii(*in)) {
					*out++ = SUBST_CHAR;
					in++;
				}
			}
		} else if (iseucaux(*in)) {
			in++;
			if (iseuckanji(*in) && iseuckanji(*(in + 1))) {
				AUX_IN();
				*out++ = *in++ & 0x7f;
				*out++ = *in++ & 0x7f;
			} else {
				K_OUT();
				if (*in != '\0' && !isascii(*in)) {
					*out++ = SUBST_CHAR;
					in++;
					if (*in != '\0' && !isascii(*in)) {
						*out++ = SUBST_CHAR;
						in++;
					}
				}
			}
		} else {
			K_OUT();
			*out++ = SUBST_CHAR;
			in++;
		}
	}

	K_OUT();
	*out = '\0';
}

void conv_sjistoeuc(gchar *outbuf, gint outlen, const gchar *inbuf)
{
	const guchar *in = inbuf;
	guchar *out = outbuf;

	while (*in != '\0') {
		if (isascii(*in)) {
			*out++ = *in++;
		} else if (issjiskanji1(*in)) {
			if (issjiskanji2(*(in + 1))) {
				guchar out1 = *in;
				guchar out2 = *(in + 1);
				guchar row;

				row = out1 < 0xa0 ? 0x70 : 0xb0;
				if (out2 < 0x9f) {
					out1 = (out1 - row) * 2 - 1;
					out2 -= out2 > 0x7f ? 0x20 : 0x1f;
				} else {
					out1 = (out1 - row) * 2;
					out2 -= 0x7e;
				}

				*out++ = out1 | 0x80;
				*out++ = out2 | 0x80;
				in += 2;
			} else {
				*out++ = SUBST_CHAR;
				in++;
				if (*in != '\0' && !isascii(*in)) {
					*out++ = SUBST_CHAR;
					in++;
				}
			}
		} else if (issjishwkana(*in)) {
			*out++ = 0x8e;
			*out++ = *in++;
		} else {
			*out++ = SUBST_CHAR;
			in++;
		}
	}

	*out = '\0';
}

void conv_anytoeuc(gchar *outbuf, gint outlen, const gchar *inbuf)
{
	switch (conv_guess_encoding(inbuf)) {
	case C_ISO_2022_JP:
		conv_jistoeuc(outbuf, outlen, inbuf);
		break;
	case C_SHIFT_JIS:
		conv_sjistoeuc(outbuf, outlen, inbuf);
		break;
	default:
		strncpy2(outbuf, inbuf, outlen);
		break;
	}
}

void conv_anytojis(gchar *outbuf, gint outlen, const gchar *inbuf)
{
	switch (conv_guess_encoding(inbuf)) {
	case C_EUC_JP:
		conv_euctojis(outbuf, outlen, inbuf);
		break;
	default:
		strncpy2(outbuf, inbuf, outlen);
		break;
	}
}

void conv_unreadable_eucjp(gchar *str)
{
	register guchar *p = str;

	while (*p != '\0') {
		if (isascii(*p)) {
			/* convert CR+LF -> LF */
			if (*p == '\r' && *(p + 1) == '\n')
				memmove(p, p + 1, strlen(p));
			/* printable 7 bit code */
			p++;
		} else if (iseuckanji(*p)) {
			if (iseuckanji(*(p + 1)) && !isunprintableeuckanji(*p))
				/* printable euc-jp code */
				p += 2;
			else {
				/* substitute unprintable code */
				*p++ = SUBST_CHAR;
				if (*p != '\0') {
					if (isascii(*p))
						p++;
					else
						*p++ = SUBST_CHAR;
				}
			}
		} else if (iseuchwkana1(*p)) {
			if (iseuchwkana2(*(p + 1)))
				/* euc-jp hankaku kana */
				p += 2;
			else
				*p++ = SUBST_CHAR;
		} else if (iseucaux(*p)) {
			if (iseuckanji(*(p + 1)) && iseuckanji(*(p + 2))) {
				/* auxiliary kanji */
				p += 3;
			} else
				*p++ = SUBST_CHAR;
		} else
			/* substitute unprintable 1 byte code */
			*p++ = SUBST_CHAR;
	}
}

void conv_unreadable_8bit(gchar *str)
{
	register guchar *p = str;

	while (*p != '\0') {
		/* convert CR+LF -> LF */
		if (*p == '\r' && *(p + 1) == '\n')
			memmove(p, p + 1, strlen(p));
		else if (!isascii(*p)) *p = SUBST_CHAR;
		p++;
	}
}

void conv_unreadable_latin(gchar *str)
{
	register guchar *p = str;

	while (*p != '\0') {
		/* convert CR+LF -> LF */
		if (*p == '\r' && *(p + 1) == '\n')
			memmove(p, p + 1, strlen(p));
		else if ((*p & 0xff) >= 0x80 && (*p & 0xff) <= 0x9f)
			*p = SUBST_CHAR;
		p++;
	}
}

#define NCV	'\0'

void conv_mb_alnum(gchar *str)
{
	static guchar char_tbl[] = {
		/* 0xa0 - 0xaf */
		NCV, ' ', NCV, NCV, ',', '.', NCV, ':',
		';', '?', '!', NCV, NCV, NCV, NCV, NCV,
		/* 0xb0 - 0xbf */
		NCV, NCV, NCV, NCV, NCV, NCV, NCV, NCV,
		NCV, NCV, NCV, NCV, NCV, NCV, NCV, NCV,
		/* 0xc0 - 0xcf */
		NCV, NCV, NCV, NCV, NCV, NCV, NCV, NCV,
		NCV, NCV, '(', ')', NCV, NCV, '[', ']',
		/* 0xd0 - 0xdf */
		'{', '}', NCV, NCV, NCV, NCV, NCV, NCV,
		NCV, NCV, NCV, NCV, '+', '-', NCV, NCV,
		/* 0xe0 - 0xef */
		NCV, '=', NCV, '<', '>', NCV, NCV, NCV,
		NCV, NCV, NCV, NCV, NCV, NCV, NCV, NCV
	};

	register guchar *p = str;
	register gint len;

	len = strlen(str);

	while (len > 1) {
		if (*p == 0xa3) {
			register guchar ch = *(p + 1);

			if (ch >= 0xb0 && ch <= 0xfa) {
				/* [a-zA-Z] */
				*p = ch & 0x7f;
				p++;
				len--;
				memmove(p, p + 1, len);
				len--;
			} else  {
				p += 2;
				len -= 2;
			}
		} else if (*p == 0xa1) {
			register guchar ch = *(p + 1);

			if (ch >= 0xa0 && ch <= 0xef &&
			    NCV != char_tbl[ch - 0xa0]) {
				*p = char_tbl[ch - 0xa0];
				p++;
				len--;
				memmove(p, p + 1, len);
				len--;
			} else {
				p += 2;
				len -= 2;
			}
		} else if (iseuckanji(*p)) {
			p += 2;
			len -= 2;
		} else {
			p++;
			len--;
		}
	}
}

CharSet conv_guess_encoding(const gchar *str)
{
	const guchar *p = str;
	CharSet guessed = C_US_ASCII;

	while (*p != '\0') {
		if (*p == ESC && (*(p + 1) == '$' || *(p + 1) == '(')) {
			if (guessed == C_US_ASCII)
				return C_ISO_2022_JP;
			p += 2;
		} else if (isascii(*p)) {
			p++;
		} else if (iseuckanji(*p) && iseuckanji(*(p + 1))) {
			if (*p >= 0xfd && *p <= 0xfe)
				return C_EUC_JP;
			else if (guessed == C_SHIFT_JIS) {
				if ((issjiskanji1(*p) &&
				     issjiskanji2(*(p + 1))) ||
				    issjishwkana(*p))
					guessed = C_SHIFT_JIS;
				else
					guessed = C_EUC_JP;
			} else
				guessed = C_EUC_JP;
			p += 2;
		} else if (issjiskanji1(*p) && issjiskanji2(*(p + 1))) {
			if (iseuchwkana1(*p) && iseuchwkana2(*(p + 1)))
				guessed = C_SHIFT_JIS;
			else
				return C_SHIFT_JIS;
			p += 2;
		} else if (issjishwkana(*p)) {
			guessed = C_SHIFT_JIS;
			p++;
		} else {
			p++;
		}
	}

	return guessed;
}

void conv_jistodisp(gchar *outbuf, gint outlen, const gchar *inbuf)
{
	conv_jistoeuc(outbuf, outlen, inbuf);
	conv_unreadable_eucjp(outbuf);
}

void conv_sjistodisp(gchar *outbuf, gint outlen, const gchar *inbuf)
{
	conv_sjistoeuc(outbuf, outlen, inbuf);
	conv_unreadable_eucjp(outbuf);
}

void conv_euctodisp(gchar *outbuf, gint outlen, const gchar *inbuf)
{
	strncpy2(outbuf, inbuf, outlen);
	conv_unreadable_eucjp(outbuf);
}

void conv_anytodisp(gchar *outbuf, gint outlen, const gchar *inbuf)
{
	conv_anytoeuc(outbuf, outlen, inbuf);
	conv_unreadable_eucjp(outbuf);
}

void conv_ustodisp(gchar *outbuf, gint outlen, const gchar *inbuf)
{
	strncpy2(outbuf, inbuf, outlen);
	conv_unreadable_8bit(outbuf);
}

void conv_latintodisp(gchar *outbuf, gint outlen, const gchar *inbuf)
{
	strncpy2(outbuf, inbuf, outlen);
	conv_unreadable_latin(outbuf);
}

void conv_noconv(gchar *outbuf, gint outlen, const gchar *inbuf)
{
	strncpy2(outbuf, inbuf, outlen);
}

CodeConverter *conv_code_converter_new(const gchar *charset)
{
	CodeConverter *conv;

	conv = g_new0(CodeConverter, 1);
	conv->code_conv_func = conv_get_code_conv_func(charset, NULL);
	conv->charset_str = g_strdup(charset);
	conv->charset = conv_get_charset_from_str(charset);

	return conv;
}

void conv_code_converter_destroy(CodeConverter *conv)
{
	g_free(conv->charset_str);
	g_free(conv);
}

gint conv_convert(CodeConverter *conv, gchar *outbuf, gint outlen,
		  const gchar *inbuf)
{
#if HAVE_ICONV
	if (conv->code_conv_func != conv_noconv)
		conv->code_conv_func(outbuf, outlen, inbuf);
	else {
		gchar *str;

		str = conv_codeset_strdup(inbuf, conv->charset_str, NULL);
		if (!str)
			return -1;
		else {
			strncpy2(outbuf, str, outlen);
			g_free(str);
		}
	}
#else /* !HAVE_ICONV */
	conv->code_conv_func(outbuf, outlen, inbuf);
#endif

	return 0;
}

gchar *conv_codeset_strdup(const gchar *inbuf,
			   const gchar *src_code, const gchar *dest_code)
{
	gchar *buf;
	size_t len;
	CodeConvFunc conv_func;

	conv_func = conv_get_code_conv_func(src_code, dest_code);
	if (conv_func != conv_noconv) {
		len = (strlen(inbuf) + 1) * 3;
		buf = g_malloc(len);
		if (!buf) return NULL;

		conv_func(buf, len, inbuf);
		return g_realloc(buf, strlen(buf) + 1);
	}

#if HAVE_ICONV
	if (!src_code)
		src_code = conv_get_outgoing_charset_str();
	if (!dest_code)
		dest_code = conv_get_current_charset_str();

	/* don't convert if current codeset is US-ASCII */
	if (!strcasecmp(dest_code, CS_US_ASCII))
		return g_strdup(inbuf);

	/* don't convert if src and dest codeset are identical */
	if (!strcasecmp(src_code, dest_code))
		return g_strdup(inbuf);

	return conv_iconv_strdup(inbuf, src_code, dest_code);
#else
	return g_strdup(inbuf);
#endif /* HAVE_ICONV */
}

CodeConvFunc conv_get_code_conv_func(const gchar *src_charset_str,
				     const gchar *dest_charset_str)
{
	CodeConvFunc code_conv = conv_noconv;
	CharSet src_charset;
	CharSet dest_charset;

	if (!src_charset_str)
		src_charset = conv_get_current_charset();
	else
		src_charset = conv_get_charset_from_str(src_charset_str);

	/* auto detection mode */
	if (!src_charset_str && !dest_charset_str) {
		if (src_charset == C_EUC_JP || src_charset == C_SHIFT_JIS)
			return conv_anytodisp;
		else
			return conv_noconv;
	}

	dest_charset = conv_get_charset_from_str(dest_charset_str);

	switch (src_charset) {
	case C_ISO_2022_JP:
	case C_ISO_2022_JP_2:
		if (dest_charset == C_AUTO)
			code_conv = conv_jistodisp;
		else if (dest_charset == C_EUC_JP)
			code_conv = conv_jistoeuc;
		break;
	case C_US_ASCII:
		if (dest_charset == C_AUTO)
			code_conv = conv_ustodisp;
		break;
	case C_ISO_8859_1:
#if !HAVE_ICONV
	case C_ISO_8859_2:
	case C_ISO_8859_4:
	case C_ISO_8859_5:
	case C_ISO_8859_7:
	case C_ISO_8859_8:
	case C_ISO_8859_9:
	case C_ISO_8859_11:
	case C_ISO_8859_13:
	case C_ISO_8859_15:
#endif
		if (dest_charset == C_AUTO)
			code_conv = conv_latintodisp;
		break;
	case C_SHIFT_JIS:
		if (dest_charset == C_AUTO)
			code_conv = conv_sjistodisp;
		else if (dest_charset == C_EUC_JP)
			code_conv = conv_sjistoeuc;
		break;
	case C_EUC_JP:
		if (dest_charset == C_AUTO)
			code_conv = conv_euctodisp;
		else if (dest_charset == C_ISO_2022_JP ||
			 dest_charset == C_ISO_2022_JP_2)
			code_conv = conv_euctojis;
		break;
	default:
		break;
	}

	return code_conv;
}

#if HAVE_ICONV
gchar *conv_iconv_strdup(const gchar *inbuf,
			 const gchar *src_code, const gchar *dest_code)
{
	iconv_t cd;
	const gchar *inbuf_p;
	gchar *outbuf;
	gchar *outbuf_p;
	gint in_size;
	gint in_left;
	gint out_size;
	gint out_left;
	gint n_conv;

	cd = iconv_open(dest_code, src_code);
	if (cd == (iconv_t)-1)
		return NULL;

	inbuf_p = inbuf;
	in_size = strlen(inbuf) + 1;
	in_left = in_size;
	out_size = in_size * 2;
	outbuf = g_malloc(out_size);
	outbuf_p = outbuf;
	out_left = out_size;

	while ((n_conv = iconv(cd, (gchar **)&inbuf_p, &in_left,
			       &outbuf_p, &out_left)) < 0) {
		if (EILSEQ == errno) {
			*outbuf_p = '\0';
			break;
		} else if (EINVAL == errno) {
			*outbuf_p = '\0';
			break;
		} else if (E2BIG == errno) {
			out_size *= 2;
			outbuf = g_realloc(outbuf, out_size);
			inbuf_p = inbuf;
			in_left = in_size;
			outbuf_p = outbuf;
			out_left = out_size;
		} else {
			g_warning("conv_iconv_strdup(): %s\n",
				  g_strerror(errno));
			*outbuf_p = '\0';
			break;
		}
	}

	iconv_close(cd);

	return outbuf;
}
#endif /* HAVE_ICONV */

static const struct {
	CharSet charset;
	gchar *const name;
} charsets[] = {
	{C_US_ASCII,		CS_US_ASCII},
	{C_US_ASCII,		CS_ANSI_X3_4_1968},
	{C_UTF_8,		CS_UTF_8},
	{C_ISO_8859_1,		CS_ISO_8859_1},
	{C_ISO_8859_2,		CS_ISO_8859_2},
	{C_ISO_8859_4,		CS_ISO_8859_4},
	{C_ISO_8859_5,		CS_ISO_8859_5},
	{C_ISO_8859_7,		CS_ISO_8859_7},
	{C_ISO_8859_8,		CS_ISO_8859_8},
	{C_ISO_8859_9,		CS_ISO_8859_9},
	{C_ISO_8859_11,		CS_ISO_8859_11},
	{C_ISO_8859_13,		CS_ISO_8859_13},
	{C_ISO_8859_15,		CS_ISO_8859_15},
	{C_BALTIC,		CS_BALTIC},
	{C_CP1251,		CS_CP1251},
	{C_WINDOWS_1251,	CS_WINDOWS_1251},
	{C_KOI8_R,		CS_KOI8_R},
	{C_KOI8_U,		CS_KOI8_U},
	{C_ISO_2022_JP,		CS_ISO_2022_JP},
	{C_ISO_2022_JP_2,	CS_ISO_2022_JP_2},
	{C_EUC_JP,		CS_EUC_JP},
	{C_EUC_JP,		CS_EUCJP},
	{C_SHIFT_JIS,		CS_SHIFT_JIS},
	{C_SHIFT_JIS,		CS_SHIFT__JIS},
	{C_SHIFT_JIS,		CS_SJIS},
	{C_ISO_2022_KR,		CS_ISO_2022_KR},
	{C_EUC_KR,		CS_EUC_KR},
	{C_ISO_2022_CN,		CS_ISO_2022_CN},
	{C_EUC_CN,		CS_EUC_CN},
	{C_GB2312,		CS_GB2312},
	{C_EUC_TW,		CS_EUC_TW},
	{C_BIG5,		CS_BIG5},
	{C_TIS_620,		CS_TIS_620},
	{C_WINDOWS_874,		CS_WINDOWS_874},
};

static const struct {
	gchar *const locale;
	CharSet charset;
	CharSet out_charset;
} locale_table[] = {
	{"ja_JP.eucJP"	, C_EUC_JP	, C_ISO_2022_JP},
	{"ja_JP.ujis"	, C_EUC_JP	, C_ISO_2022_JP},
	{"ja_JP.EUC"	, C_EUC_JP	, C_ISO_2022_JP},
	{"ja_JP.SJIS"	, C_SHIFT_JIS	, C_ISO_2022_JP},
	{"ja_JP.JIS"	, C_ISO_2022_JP	, C_ISO_2022_JP},
	{"ja_JP"	, C_EUC_JP	, C_ISO_2022_JP},
	{"ko_KR"	, C_EUC_KR	, C_EUC_KR},
	{"zh_CN.GB2312"	, C_GB2312	, C_GB2312},
	{"zh_CN"	, C_GB2312	, C_GB2312},
	{"zh_TW.eucTW"	, C_EUC_TW	, C_BIG5},
	{"zh_TW.Big5"	, C_BIG5	, C_BIG5},
	{"zh_TW"	, C_BIG5	, C_BIG5},

	{"ru_RU.KOI8-R"	, C_KOI8_R	, C_KOI8_R},
	{"ru_RU.CP1251"	, C_WINDOWS_1251, C_KOI8_R},
	{"ru_RU"	, C_ISO_8859_5	, C_KOI8_R},
	{"ru_UA"	, C_KOI8_U	, C_KOI8_U},
	{"uk_UA"	, C_KOI8_U	, C_KOI8_U},
	{"be_BY"	, C_WINDOWS_1251, C_WINDOWS_1251},
	{"bg_BG"	, C_WINDOWS_1251, C_WINDOWS_1251},

	{"en_US"	, C_ISO_8859_1	, C_ISO_8859_1},
	{"ca_ES"	, C_ISO_8859_1	, C_ISO_8859_1},
	{"da_DK"	, C_ISO_8859_1	, C_ISO_8859_1},
	{"de_DE"	, C_ISO_8859_1	, C_ISO_8859_1},
	{"nl_NL"	, C_ISO_8859_1	, C_ISO_8859_1},
	{"et_EE"	, C_ISO_8859_1	, C_ISO_8859_1},
	{"fi_FI"	, C_ISO_8859_1	, C_ISO_8859_1},
	{"fr_FR"	, C_ISO_8859_1	, C_ISO_8859_1},
	{"is_IS"	, C_ISO_8859_1	, C_ISO_8859_1},
	{"it_IT"	, C_ISO_8859_1	, C_ISO_8859_1},
	{"no_NO"	, C_ISO_8859_1	, C_ISO_8859_1},
	{"pt_PT"	, C_ISO_8859_1	, C_ISO_8859_1},
	{"pt_BR"	, C_ISO_8859_1	, C_ISO_8859_1},
	{"es_ES"	, C_ISO_8859_1	, C_ISO_8859_1},
	{"sv_SE"	, C_ISO_8859_1	, C_ISO_8859_1},

	{"hr_HR"	, C_ISO_8859_2	, C_ISO_8859_2},
	{"hu_HU"	, C_ISO_8859_2	, C_ISO_8859_2},
	{"pl_PL"	, C_ISO_8859_2	, C_ISO_8859_2},
	{"ro_RO"	, C_ISO_8859_2	, C_ISO_8859_2},
	{"sk_SK"	, C_ISO_8859_2	, C_ISO_8859_2},
	{"sl_SI"	, C_ISO_8859_2	, C_ISO_8859_2},
	{"el_GR"	, C_ISO_8859_7	, C_ISO_8859_7},
	{"iw_IL"	, C_ISO_8859_8	, C_ISO_8859_8},
	{"tr_TR"	, C_ISO_8859_9	, C_ISO_8859_9},

	{"th_TH"	, C_TIS_620	, C_TIS_620},
	/* {"th_TH"	, C_WINDOWS_874}, */
	/* {"th_TH"	, C_ISO_8859_11}, */

	{"lt_LT.iso88594"	, C_ISO_8859_4	, C_ISO_8859_4},
	{"lt_LT.ISO8859-4"	, C_ISO_8859_4	, C_ISO_8859_4},
	{"lt_LT.ISO_8859-4"	, C_ISO_8859_4	, C_ISO_8859_4},
	{"lt_LT"		, C_ISO_8859_13	, C_ISO_8859_13},
	{"lv_LV"		, C_ISO_8859_13	, C_ISO_8859_13},

	{"C"			, C_US_ASCII	, C_US_ASCII},
	{"POSIX"		, C_US_ASCII	, C_US_ASCII},
	{"ANSI_X3.4-1968"	, C_US_ASCII	, C_US_ASCII},
};

const gchar *conv_get_charset_str(CharSet charset)
{
	gint i;

	for (i = 0; i < sizeof(charsets) / sizeof(charsets[0]); i++) {
		if (charsets[i].charset == charset)
			return charsets[i].name;
	}

	return NULL;
}

CharSet conv_get_charset_from_str(const gchar *charset)
{
	gint i;

	if (!charset) return C_AUTO;

	for (i = 0; i < sizeof(charsets) / sizeof(charsets[0]); i++) {
		if (!strcasecmp(charsets[i].name, charset))
			return charsets[i].charset;
	}

	return C_AUTO;
}

CharSet conv_get_current_charset(void)
{
	static CharSet cur_charset = -1;
	const gchar *cur_locale;
	gint i;

	if (cur_charset != -1)
		return cur_charset;

	cur_locale = conv_get_current_locale();
	if (!cur_locale) {
		cur_charset = C_US_ASCII;
		return cur_charset;
	}

	if (strcasestr(cur_locale, "UTF-8")) {
		cur_charset = C_UTF_8;
		return cur_charset;
	}

	for (i = 0; i < sizeof(locale_table) / sizeof(locale_table[0]); i++) {
		const gchar *p;

		/* "ja_JP.EUC" matches with "ja_JP.eucJP", "ja_JP.EUC" and
		   "ja_JP". "ja_JP" matches with "ja_JP.xxxx" and "ja" */
		if (!strncasecmp(cur_locale, locale_table[i].locale,
				 strlen(locale_table[i].locale))) {
			cur_charset = locale_table[i].charset;
			return cur_charset;
		} else if ((p = strchr(locale_table[i].locale, '_')) &&
			 !strchr(p + 1, '.')) {
			if (strlen(cur_locale) == 2 &&
			    !strncasecmp(cur_locale, locale_table[i].locale, 2)) {
				cur_charset = locale_table[i].charset;
				return cur_charset;
			}
		}
	}

	cur_charset = C_AUTO;
	return cur_charset;
}

const gchar *conv_get_current_charset_str(void)
{
	static const gchar *codeset = NULL;

	if (!codeset)
		codeset = conv_get_charset_str(conv_get_current_charset());

	return codeset ? codeset : "US-ASCII";
}

CharSet conv_get_outgoing_charset(void)
{
	static CharSet out_charset = -1;
	const gchar *cur_locale;
	gint i;

	if (out_charset != -1)
		return out_charset;

	cur_locale = conv_get_current_locale();
	if (!cur_locale) {
		out_charset = C_AUTO;
		return out_charset;
	}

	for (i = 0; i < sizeof(locale_table) / sizeof(locale_table[0]); i++) {
		const gchar *p;

		if (!strncasecmp(cur_locale, locale_table[i].locale,
				 strlen(locale_table[i].locale))) {
			out_charset = locale_table[i].out_charset;
			break;
		} else if ((p = strchr(locale_table[i].locale, '_')) &&
			 !strchr(p + 1, '.')) {
			if (strlen(cur_locale) == 2 &&
			    !strncasecmp(cur_locale, locale_table[i].locale, 2)) {
				out_charset = locale_table[i].out_charset;
				break;
			}
		}
	}

#if !HAVE_ICONV
	/* encoding conversion without iconv() is only supported
	   on Japanese locale for now */
	if (out_charset == C_ISO_2022_JP)
		return out_charset;
	else
		return conv_get_current_charset();
#endif

	return out_charset;
}

const gchar *conv_get_outgoing_charset_str(void)
{
	CharSet out_charset;
	const gchar *str;

	if (prefs_common.outgoing_charset) {
		if (!isalpha(prefs_common.outgoing_charset[0])) {
			g_free(prefs_common.outgoing_charset);
			prefs_common.outgoing_charset = g_strdup(CS_AUTO);
		} else if (strcmp(prefs_common.outgoing_charset, CS_AUTO) != 0)
			return prefs_common.outgoing_charset;
	}

	out_charset = conv_get_outgoing_charset();
	str = conv_get_charset_str(out_charset);

	return str ? str : "US-ASCII";
}

const gchar *conv_get_current_locale(void)
{
	gchar *cur_locale;

	cur_locale = g_getenv("LC_ALL");
	if (!cur_locale) cur_locale = g_getenv("LC_CTYPE");
	if (!cur_locale) cur_locale = g_getenv("LANG");
	if (!cur_locale) cur_locale = setlocale(LC_CTYPE, NULL);

	debug_print("current locale: %s\n",
		    cur_locale ? cur_locale : "(none)");

	return cur_locale;
}

void conv_unmime_header_overwrite(gchar *str)
{
	gchar *buf;
	gint buflen;
	CharSet cur_charset;

	cur_charset = conv_get_current_charset();

	if (cur_charset == C_EUC_JP) {
		buflen = strlen(str) * 2 + 1;
		Xalloca(buf, buflen, return);
		conv_anytodisp(buf, buflen, str);
		unmime_header(str, buf);
	} else {
		buflen = strlen(str) + 1;
		Xalloca(buf, buflen, return);
		unmime_header(buf, str);
		strncpy2(str, buf, buflen);
	}
}

void conv_unmime_header(gchar *outbuf, gint outlen, const gchar *str,
			const gchar *charset)
{
	CharSet cur_charset;

	cur_charset = conv_get_current_charset();

	if (cur_charset == C_EUC_JP) {
		gchar *buf;
		gint buflen;

		buflen = strlen(str) * 2 + 1;
		Xalloca(buf, buflen, return);
		conv_anytodisp(buf, buflen, str);
		unmime_header(outbuf, buf);
	} else
		unmime_header(outbuf, str);
}

#define MAX_LINELEN	76
#define MIMESEP_BEGIN	"=?"
#define MIMESEP_END	"?="

#define B64LEN(len)	((len) / 3 * 4 + ((len) % 3 ? 4 : 0))

#define LBREAK_IF_REQUIRED(cond)				\
{								\
	if (len - (destp - dest) < MAX_LINELEN + 2) {		\
		*destp = '\0';					\
		return;						\
	}							\
								\
	if ((cond) && *srcp) {					\
		if (destp > dest && isspace(*(destp - 1)))	\
			destp--;				\
		else if (isspace(*srcp))			\
			srcp++;					\
		if (*srcp) {					\
			*destp++ = '\n';			\
			*destp++ = ' ';				\
			left = MAX_LINELEN - 1;			\
		}						\
	}							\
}

void conv_encode_header(gchar *dest, gint len, const gchar *src,
			gint header_len)
{
	const gchar *cur_encoding;
	const gchar *out_encoding;
	gint mimestr_len;
	gchar *mimesep_enc;
	gint left;
	const gchar *srcp = src;
	gchar *destp = dest;
	gboolean use_base64;

	if (MB_CUR_MAX > 1) {
		use_base64 = TRUE;
		mimesep_enc = "?B?";
	} else {
		use_base64 = FALSE;
		mimesep_enc = "?Q?";
	}

	cur_encoding = conv_get_current_charset_str();
	if (!strcmp(cur_encoding, "US-ASCII"))
		cur_encoding = "ISO-8859-1";
	out_encoding = conv_get_outgoing_charset_str();
	if (!strcmp(out_encoding, "US-ASCII"))
		out_encoding = "ISO-8859-1";

	mimestr_len = strlen(MIMESEP_BEGIN) + strlen(out_encoding) +
		strlen(mimesep_enc) + strlen(MIMESEP_END);

	left = MAX_LINELEN - header_len;

	while (*srcp) {
		LBREAK_IF_REQUIRED(left <= 0);

		while (isspace(*srcp)) {
			*destp++ = *srcp++;
			left--;
			LBREAK_IF_REQUIRED(left <= 0);
		}

		/* output as it is if the next word is ASCII string */
		if (!is_next_nonascii(srcp)) {
			gint word_len;

			word_len = get_next_word_len(srcp);
			LBREAK_IF_REQUIRED(left < word_len);
			while (word_len > 0) {
				LBREAK_IF_REQUIRED(left <= 0);
				*destp++ = *srcp++;
				left--;
				word_len--;
			}

			continue;
		}

		while (1) {
			gint mb_len = 0;
			gint cur_len = 0;
			gchar *part_str;
			gchar *out_str;
			gchar *enc_str;
			const gchar *p = srcp;
			gint out_str_len;
			gint out_enc_str_len;
			gint mime_block_len;
			gboolean cont = FALSE;

			while (*p != '\0') {
				if (isspace(*p) && !is_next_nonascii(p + 1))
					break;

				if (MB_CUR_MAX > 1) {
					mb_len = mblen(p, MB_CUR_MAX);
					if (mb_len < 0) {
						g_warning("conv_encode_header(): invalid multibyte character encountered\n");
						mb_len = 1;
					}
				} else
					mb_len = 1;

				Xstrndup_a(part_str, srcp, cur_len + mb_len, );
				out_str = conv_codeset_strdup
					(part_str, cur_encoding, out_encoding);
				if (!out_str) {
					g_warning("conv_encode_header(): code conversion failed\n");
					out_str = g_strdup(out_str);
				}
				out_str_len = strlen(out_str);

				if (use_base64)
					out_enc_str_len = B64LEN(out_str_len);
				else
					out_enc_str_len =
						qp_get_q_encoding_len(out_str);

				g_free(out_str);

				if (mimestr_len + out_enc_str_len <= left) {
					cur_len += mb_len;
					p += mb_len;
				} else if (cur_len == 0) {
					LBREAK_IF_REQUIRED(1);
					continue;
				} else {
					cont = TRUE;
					break;
				}
			}

			if (cur_len > 0) {
				Xstrndup_a(part_str, srcp, cur_len, );
				out_str = conv_codeset_strdup
					(part_str, cur_encoding, out_encoding);
				if (!out_str) {
					g_warning("conv_encode_header(): code conversion failed\n");
					out_str = g_strdup(out_str);
				}
				out_str_len = strlen(out_str);

				if (use_base64)
					out_enc_str_len = B64LEN(out_str_len);
				else
					out_enc_str_len =
						qp_get_q_encoding_len(out_str);

				Xalloca(enc_str, out_enc_str_len + 1, );
				if (use_base64)
					base64_encode(enc_str, out_str, out_str_len);
				else
					qp_q_encode(enc_str, out_str);

				g_free(out_str);

				/* output MIME-encoded string block */
				mime_block_len = mimestr_len + strlen(enc_str);
				g_snprintf(destp, mime_block_len + 1,
					   MIMESEP_BEGIN "%s%s%s" MIMESEP_END,
					   out_encoding, mimesep_enc, enc_str);
				destp += mime_block_len;
				srcp += cur_len;

				left -= mime_block_len;
			}

			LBREAK_IF_REQUIRED(cont);

			if (cur_len == 0)
				break;
		}
	}

	*destp = '\0';
}

#undef LBREAK_IF_REQUIRED
