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
#include <stdlib.h>

#if (HAVE_WCTYPE_H && HAVE_WCHAR_H)
#  include <wchar.h>
#  include <wctype.h>
#endif

#if HAVE_LOCALE_H
#  include <locale.h>
#endif

#if HAVE_LIBJCONV
#  include <jconv.h>
#endif

#include "intl.h"
#include "codeconv.h"
#include "unmime.h"
#include "base64.h"
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
#if !HAVE_LIBJCONV
	conv->code_conv_func = conv_get_code_conv_func(charset);
#endif
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
#if HAVE_LIBJCONV
	gchar *str;

	str = conv_codeset_strdup(inbuf, conv->charset_str, NULL);
	if (!str)
		return -1;
	else {
		strncpy2(outbuf, str, outlen);
		g_free(str);
	}
#else /* !HAVE_LIBJCONV */
	conv->code_conv_func(outbuf, outlen, inbuf);
#endif

	return 0;
}

gchar *conv_codeset_strdup(const gchar *inbuf,
			   const gchar *src_codeset, const gchar *dest_codeset)
{
	gchar *buf;
	size_t len;
#if HAVE_LIBJCONV
	gint actual_codeset;
	const gchar *const *codesets;
	gint n_codesets;
#else /* !HAVE_LIBJCONV */
	CharSet src_charset = C_AUTO, dest_charset = C_AUTO;
#endif

	if (!dest_codeset) {
		CodeConvFunc func;

		func = conv_get_code_conv_func(src_codeset);
		if (func != conv_noconv) {
			if (func == conv_jistodisp ||
			    func == conv_sjistodisp ||
			    func == conv_anytodisp)
				len = strlen(inbuf) * 2 + 1;
			else
				len = strlen(inbuf) + 1;
			buf = g_malloc(len);
			if (!buf) return NULL;
			func(buf, len, inbuf);
			buf = g_realloc(buf, strlen(buf) + 1);
			return buf;
		}
	}

	/* don't convert if src and dest codeset are identical */
	if (src_codeset && dest_codeset &&
	    !strcasecmp(src_codeset, dest_codeset))
		return g_strdup(inbuf);

#if HAVE_LIBJCONV
	if (src_codeset) {
		codesets = &src_codeset;
		n_codesets = 1;
	} else
		codesets = jconv_info_get_pref_codesets(&n_codesets);
	if (!dest_codeset) {
		dest_codeset = conv_get_current_charset_str();
		/* don't convert if current codeset is US-ASCII */
		if (!strcasecmp(dest_codeset, CS_US_ASCII))
			return g_strdup(inbuf);
	}

	if (jconv_alloc_conv(inbuf, strlen(inbuf), &buf, &len,
			     codesets, n_codesets,
			     &actual_codeset, dest_codeset)
	    == 0)
		return buf;
	else {
#if 0
		g_warning("code conversion from %s to %s failed\n",
			  codesets && codesets[0] ? codesets[0] : "(unknown)",
			  dest_codeset);
#endif /* 0 */
		return NULL;
	}
#else /* !HAVE_LIBJCONV */
	if (src_codeset) {
		if (!strcasecmp(src_codeset, CS_EUC_JP) ||
		    !strcasecmp(src_codeset, CS_EUCJP))
			src_charset = C_EUC_JP;
		else if (!strcasecmp(src_codeset, CS_SHIFT_JIS) ||
			 !strcasecmp(src_codeset, "SHIFT-JIS") ||
			 !strcasecmp(src_codeset, "SJIS"))
			src_charset = C_SHIFT_JIS;
		if (dest_codeset && !strcasecmp(dest_codeset, CS_ISO_2022_JP))
			dest_charset = C_ISO_2022_JP;
	}

	if ((src_charset == C_EUC_JP || src_charset == C_SHIFT_JIS) &&
	    dest_charset == C_ISO_2022_JP) {
		len = (strlen(inbuf) + 1) * 3;
		buf = g_malloc(len);
		if (buf) {
			if (src_charset == C_EUC_JP)
				conv_euctojis(buf, len, inbuf);
			else
				conv_anytojis(buf, len, inbuf);
			buf = g_realloc(buf, strlen(buf) + 1);
		}
	} else
		buf = g_strdup(inbuf);

	return buf;
#endif /* !HAVE_LIBJCONV */
}

CodeConvFunc conv_get_code_conv_func(const gchar *charset)
{
	CodeConvFunc code_conv;
	CharSet cur_charset;

	if (!charset) {
		cur_charset = conv_get_current_charset();
		if (cur_charset == C_EUC_JP || cur_charset == C_SHIFT_JIS)
			return conv_anytodisp;
		else
			return conv_noconv;
	}

	if (!strcasecmp(charset, CS_ISO_2022_JP) ||
	    !strcasecmp(charset, CS_ISO_2022_JP_2))
		code_conv = conv_jistodisp;
	else if (!strcasecmp(charset, CS_US_ASCII))
		code_conv = conv_ustodisp;
	else if (!strncasecmp(charset, CS_ISO_8859_1, 10))
		code_conv = conv_latintodisp;
#if !HAVE_LIBJCONV
	else if (!strncasecmp(charset, "ISO-8859-", 9))
		code_conv = conv_latintodisp;
#endif
	else if (!strcasecmp(charset, CS_SHIFT_JIS) ||
		 !strcasecmp(charset, "SHIFT-JIS")  ||
		 !strcasecmp(charset, "SJIS")       ||
		 !strcasecmp(charset, "X-SJIS"))
		code_conv = conv_sjistodisp;
	else if (!strcasecmp(charset, CS_EUC_JP) ||
		 !strcasecmp(charset, CS_EUCJP))
		code_conv = conv_euctodisp;
	else
		code_conv = conv_noconv;

	return code_conv;
}

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

#if !HAVE_LIBJCONV
static const struct {
	gchar *const locale;
	CharSet charset;
} locale_table[] = {
	{"ja_JP.eucJP"	, C_EUC_JP},
	{"ja_JP.ujis"	, C_EUC_JP},
	{"ja_JP.EUC"	, C_EUC_JP},
	{"ja_JP.SJIS"	, C_SHIFT_JIS},
	{"ja_JP.JIS"	, C_ISO_2022_JP},
	{"ja_JP"	, C_EUC_JP},
	{"ko_KR"	, C_EUC_KR},
	{"zh_CN.GB2312"	, C_GB2312},
	{"zh_CN"	, C_GB2312},
	{"zh_TW.eucTW"	, C_EUC_TW},
	{"zh_TW.Big5"	, C_BIG5},
	{"zh_TW"	, C_BIG5},

	{"ru_RU.KOI8-R"	, C_KOI8_R},
	{"ru_RU.CP1251"	, C_WINDOWS_1251},

	{"bg_BG"	, C_WINDOWS_1251},

	{"en_US"	, C_ISO_8859_1},
	{"ca_ES"	, C_ISO_8859_1},
	{"da_DK"	, C_ISO_8859_1},
	{"de_DE"	, C_ISO_8859_1},
	{"nl_NL"	, C_ISO_8859_1},
	{"et_EE"	, C_ISO_8859_1},
	{"fi_FI"	, C_ISO_8859_1},
	{"fr_FR"	, C_ISO_8859_1},
	{"is_IS"	, C_ISO_8859_1},
	{"it_IT"	, C_ISO_8859_1},
	{"no_NO"	, C_ISO_8859_1},
	{"pt_PT"	, C_ISO_8859_1},
	{"pt_BR"	, C_ISO_8859_1},
	{"es_ES"	, C_ISO_8859_1},
	{"sv_SE"	, C_ISO_8859_1},

	{"hr_HR"	, C_ISO_8859_2},
	{"hu_HU"	, C_ISO_8859_2},
	{"pl_PL"	, C_ISO_8859_2},
	{"ro_RO"	, C_ISO_8859_2},
	{"sk_SK"	, C_ISO_8859_2},
	{"sl_SI"	, C_ISO_8859_2},
	{"ru_RU"	, C_ISO_8859_5},
	{"el_GR"	, C_ISO_8859_7},
	{"iw_IL"	, C_ISO_8859_8},
	{"tr_TR"	, C_ISO_8859_9},

	{"th_TH"	, C_TIS_620},
	/* {"th_TH"	, C_WINDOWS_874}, */
	/* {"th_TH"	, C_ISO_8859_11}, */

	{"lt_LT.iso88594"	, C_ISO_8859_4},
	{"lt_LT.ISO8859-4"	, C_ISO_8859_4},
	{"lt_LT.ISO_8859-4"	, C_ISO_8859_4},
	{"lt_LT"		, C_ISO_8859_13},
	{"lv_LV"		, C_ISO_8859_13},

	{"C"			, C_US_ASCII},
	{"POSIX"		, C_US_ASCII},
	{"ANSI_X3.4-1968"	, C_US_ASCII},
};
#endif /* !HAVE_LIBJCONV */

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
	gint i;

#if HAVE_LIBJCONV
	const gchar *cur_codeset;
#else
	const gchar *cur_locale;
#endif

	if (cur_charset != -1)
		return cur_charset;

#if HAVE_LIBJCONV
	cur_codeset = jconv_info_get_current_codeset();
	for (i = 0; i < sizeof(charsets) / sizeof(charsets[0]); i++) {
		if (!strcasecmp(cur_codeset, charsets[i].name)) {
			cur_charset = charsets[i].charset;
			return cur_charset;
		}
	}
#else
	cur_locale = g_getenv("LC_ALL");
	if (!cur_locale) cur_locale = g_getenv("LC_CTYPE");
	if (!cur_locale) cur_locale = g_getenv("LANG");
	if (!cur_locale) cur_locale = setlocale(LC_CTYPE, NULL);

	debug_print("current locale: %s\n",
		    cur_locale ? cur_locale : "(none)");

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

		/* "ja_JP.EUC" matches with "ja_JP.eucJP" and "ja_JP.EUC" */
		/* "ja_JP" matches with "ja_JP.xxxx" and "ja" */
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
#endif

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

#if HAVE_LIBJCONV
	gint i, j, n_pref_codesets;
	const gchar *const *pref_codesets;
#else
	CharSet cur_charset;
#endif

	if (out_charset != -1)
		return out_charset;

#if HAVE_LIBJCONV
	/* skip US-ASCII and UTF-8 */
	pref_codesets = jconv_info_get_pref_codesets(&n_pref_codesets);
	for (i = 0; i < n_pref_codesets; i++) {
		for (j = 3; j < sizeof(charsets) / sizeof(charsets[0]); j++) {
			if (!strcasecmp(pref_codesets[i], charsets[j].name)) {
				out_charset = charsets[j].charset;
				return out_charset;
			}
		}
	}

	for (i = 0; i < n_pref_codesets; i++) {
		if (!strcasecmp(pref_codesets[i], "UTF-8")) {
			out_charset = C_UTF_8;
			return out_charset;
		}
	}

	out_charset = C_AUTO;
#else
	cur_charset = conv_get_current_charset();
	switch (cur_charset) {
	case C_EUC_JP:
	case C_SHIFT_JIS:
		out_charset = C_ISO_2022_JP;
		break;
	default:
		out_charset = cur_charset;
	}
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

#define MAX_ENCLEN	75
#define MAX_LINELEN	76

#define B64LEN(len)	((len) / 3 * 4 + ((len) % 3 ? 4 : 0))

#if HAVE_LIBJCONV
void conv_encode_header(gchar *dest, gint len, const gchar *src,
			gint header_len)
{
	wchar_t *wsrc;
	wchar_t *wsrcp;
	gchar *destp;
	size_t line_len, mimehdr_len, mimehdr_begin_len;
	gchar *mimehdr_init = "=?";
	gchar *mimehdr_end = "?=";
	gchar *mimehdr_enctype = "?B?";
	const gchar *mimehdr_charset;

	/* g_print("src = %s\n", src); */
	mimehdr_charset = conv_get_outgoing_charset_str();

	/* convert to wide-character string */
	wsrcp = wsrc = strdup_mbstowcs(src);

	mimehdr_len = strlen(mimehdr_init) + strlen(mimehdr_end) +
		strlen(mimehdr_charset) + strlen(mimehdr_enctype);
	mimehdr_begin_len = strlen(mimehdr_init) +
		strlen(mimehdr_charset) + strlen(mimehdr_enctype);
	line_len = header_len;
	destp = dest;
	*dest = '\0';

	g_return_if_fail(wsrc != NULL);

	while (*wsrcp) {
		wchar_t *wp, *wtmp, *wtmpp;
		gint nspc = 0;
		gboolean str_is_non_ascii;

		/* irresponsible buffer overrun check */
		if ((len - (destp - dest)) < (MAX_LINELEN + 1) * 2) break;

		/* encode string including space
		   if non-ASCII string follows */
		if (is_next_nonascii(wsrcp)) {
			wp = wsrcp;
			while ((wp = find_wspace(wp)) != NULL)
				if (!is_next_nonascii(wp)) break;
			str_is_non_ascii = TRUE;
		} else {
			wp = find_wspace(wsrcp);
			str_is_non_ascii = FALSE;
		}

		if (wp != NULL) {
			wtmp = wcsndup(wsrcp, wp - wsrcp);
			wsrcp = wp + 1;
			while (iswspace(wsrcp[nspc])) nspc++;
		} else {
			wtmp = wcsdup(wsrcp);
			wsrcp += wcslen(wsrcp);
		}

		wtmpp = wtmp;

		do {
			gint tlen = 0;
			gchar *tmp; /* internal codeset */
			gchar *raw; /* converted, but not base64 encoded */
			register gchar *tmpp;
			gint raw_len;

			tmpp = tmp = g_malloc(wcslen(wtmpp) * MB_CUR_MAX + 1);
			*tmp = '\0';
			raw = g_strdup("");
			raw_len = 0;

			while (*wtmpp != (wchar_t)0) {
				gint mbl;
				gint dummy;
				gchar *raw_new = NULL;
				int raw_new_len = 0;
				const gchar *src_codeset;

				mbl = wctomb(tmpp, *wtmpp);
				if (mbl == -1) {
					g_warning("invalid wide character\n");
					wtmpp++;
					continue;
				}
				/* g_free(raw); */
				src_codeset = conv_get_current_charset_str();
				/* printf ("tmp = %s, tlen = %d, mbl\n",
					tmp, tlen, mbl); */
				if (jconv_alloc_conv(tmp, tlen + mbl,
						     &raw_new, &raw_new_len,
						     &src_codeset, 1,
						     &dummy, mimehdr_charset)
				    != 0) {
					g_warning("can't convert\n");
					tmpp[0] = '\0';
					wtmpp++;
					continue;
				}
				if (str_is_non_ascii) {
					gint dlen = mimehdr_len +
						B64LEN(raw_len);
					if ((line_len + dlen +
					     (*(wtmpp + 1) ? 0 : nspc) +
					     (line_len > 1 ? 1 : 0))
					    > MAX_LINELEN) {
						g_free(raw_new);
						if (tlen == 0) {
							*destp++ = '\n';
							*destp++ = ' ';
							line_len = 1;
							continue;
						} else {
							*tmpp = '\0';
							break;
						}
					}
				} else if ((line_len + tlen + mbl +
					    (*(wtmpp + 1) ? 0 : nspc) +
					    (line_len > 1 ? 1 : 0))
					   > MAX_LINELEN) {
					g_free(raw_new);
					if (1 + tlen + mbl +
					    (*(wtmpp + 1) ? 0 : nspc)
					    >= MAX_LINELEN) {
						*tmpp = '\0';
						break;
					}
					*destp++ = '\n';
					*destp++ = ' ';
					line_len = 1;
					continue;
				}

				tmpp += mbl;
				*tmpp = '\0';

				tlen += mbl;

				g_free(raw);
				raw = raw_new;
				raw_len = raw_new_len;

				wtmpp++;
			}
			/* g_print("tmp = %s, tlen = %d, mb_seqlen = %d\n",
				tmp, tlen, mb_seqlen); */

			if (tlen == 0 || raw_len == 0) {
				g_free(tmp);
				g_free(raw);
				continue;
			}

			if (line_len > 1 && destp > dest) {
				*destp++ = ' ';
				*destp = '\0';
				line_len++;
			}

			if (str_is_non_ascii) {
				g_snprintf(destp, len - strlen(dest), "%s%s%s",
					   mimehdr_init, mimehdr_charset,
					   mimehdr_enctype);
				destp += mimehdr_begin_len;
				line_len += mimehdr_begin_len;

				base64_encode(destp, raw, raw_len);
				line_len += strlen(destp);
				destp += strlen(destp);

				strcpy(destp, mimehdr_end);
				destp += strlen(mimehdr_end);
				line_len += strlen(mimehdr_end);
			} else {
				strcpy(destp, tmp);
				line_len += strlen(destp);
				destp += strlen(destp);
			}

			g_free(tmp);
			g_free(raw);
			/* g_print("line_len = %d\n\n", line_len); */
		} while (*wtmpp != (wchar_t)0);

		while (iswspace(*wsrcp)) {
			gint mbl;

			mbl = wctomb(destp, *wsrcp++);
			if (mbl != -1) {
				destp += mbl;
				line_len += mbl;
			}
		}
		*destp = '\0';

		g_free(wtmp);
	}

	g_free(wsrc);

	/* g_print("dest = %s\n", dest); */
}
#else /* !HAVE_LIBJCONV */

#define JIS_SEQLEN	3

void conv_encode_header(gchar *dest, gint len, const gchar *src,
			gint header_len)
{
	wchar_t *wsrc;
	wchar_t *wsrcp;
	gchar *destp;
	size_t line_len, mimehdr_len, mimehdr_begin_len;
	gchar *mimehdr_init = "=?";
	gchar *mimehdr_end = "?=";
	gchar *mimehdr_enctype = "?B?";
	const gchar *mimehdr_charset;

	/* g_print("src = %s\n", src); */
	mimehdr_charset = conv_get_outgoing_charset_str();
	if (strcmp(mimehdr_charset, "ISO-2022-JP") != 0) {
		/* currently only supports Japanese */
		strncpy2(dest, src, len);
		return;
	}

	/* convert to wide-character string */
	wsrcp = wsrc = strdup_mbstowcs(src);

	mimehdr_len = strlen(mimehdr_init) + strlen(mimehdr_end) +
		      strlen(mimehdr_charset) + strlen(mimehdr_enctype);
	mimehdr_begin_len = strlen(mimehdr_init) +
			    strlen(mimehdr_charset) + strlen(mimehdr_enctype);
	line_len = header_len;
	destp = dest;
	*dest = '\0';

	g_return_if_fail(wsrc != NULL);

	while (*wsrcp) {
		wchar_t *wp, *wtmp, *wtmpp;
		gint nspc = 0;
		gboolean str_is_non_ascii;

		/* irresponsible buffer overrun check */
		if ((len - (destp - dest)) < (MAX_LINELEN + 1) * 2) break;

		/* encode string including space
		   if non-ASCII string follows */
		if (is_next_nonascii(wsrcp)) {
			wp = wsrcp;
			while ((wp = find_wspace(wp)) != NULL)
				if (!is_next_nonascii(wp)) break;
			str_is_non_ascii = TRUE;
		} else {
			wp = find_wspace(wsrcp);
			str_is_non_ascii = FALSE;
		}

		if (wp != NULL) {
			wtmp = wcsndup(wsrcp, wp - wsrcp);
			wsrcp = wp + 1;
			while (iswspace(wsrcp[nspc])) nspc++;
		} else {
			wtmp = wcsdup(wsrcp);
			wsrcp += wcslen(wsrcp);
		}

		wtmpp = wtmp;

		do {
			gint prev_mbl = 1, tlen = 0, mb_seqlen = 0;
			gchar *tmp;
			register gchar *tmpp;

			tmpp = tmp = g_malloc(wcslen(wtmpp) * MB_CUR_MAX + 1);
			*tmp = '\0';

			while (*wtmpp != (wchar_t)0) {
				gint mbl;

				mbl = wctomb(tmpp, *wtmpp);
				if (mbl == -1) {
					g_warning("invalid wide character\n");
					wtmpp++;
					continue;
				}

				/* length of KI + KO */
				if (prev_mbl == 1 && mbl == 2)
					mb_seqlen += JIS_SEQLEN * 2;

				if (str_is_non_ascii) {
					gint dlen = mimehdr_len +
						B64LEN(tlen + mb_seqlen + mbl);

					if ((line_len + dlen +
					     (*(wtmpp + 1) ? 0 : nspc) +
					     (line_len > 1 ? 1 : 0))
					    > MAX_LINELEN) {
						if (tlen == 0) {
							*destp++ = '\n';
							*destp++ = ' ';
							line_len = 1;
							mb_seqlen = 0;
							continue;
						} else {
							*tmpp = '\0';
							break;
						}
					}
				} else if ((line_len + tlen + mbl +
					    (*(wtmpp + 1) ? 0 : nspc) +
					    (line_len > 1 ? 1 : 0))
					   > MAX_LINELEN) {
					if (1 + tlen + mbl +
					    (*(wtmpp + 1) ? 0 : nspc)
					    >= MAX_LINELEN) {
						*tmpp = '\0';
						break;
					}
					*destp++ = '\n';
					*destp++ = ' ';
					line_len = 1;
					continue;
				}

				tmpp += mbl;
				*tmpp = '\0';

				tlen += mbl;
				prev_mbl = mbl;

				wtmpp++;
			}
			/* g_print("tmp = %s, tlen = %d, mb_seqlen = %d\n",
				tmp, tlen, mb_seqlen); */

			if (tlen == 0) {
				g_free(tmp);
				continue;
			}

			if (line_len > 1 && destp > dest) {
				*destp++ = ' ';
				*destp = '\0';
				line_len++;
			}

			if (str_is_non_ascii) {
				gchar *tmp_jis;

				tmp_jis = g_new(gchar, tlen + mb_seqlen + 1);
				conv_euctojis(tmp_jis,
					      tlen + mb_seqlen + 1, tmp);
				g_snprintf(destp, len - strlen(dest), "%s%s%s",
					   mimehdr_init, mimehdr_charset,
					   mimehdr_enctype);
				destp += mimehdr_begin_len;
				line_len += mimehdr_begin_len;

				base64_encode(destp, tmp_jis, strlen(tmp_jis));
				line_len += strlen(destp);
				destp += strlen(destp);

				strcpy(destp, mimehdr_end);
				destp += strlen(mimehdr_end);
				line_len += strlen(mimehdr_end);

				g_free(tmp_jis);
			} else {
				strcpy(destp, tmp);
				line_len += strlen(destp);
				destp += strlen(destp);
			}

			g_free(tmp);
			/* g_print("line_len = %d\n\n", line_len); */
		} while (*wtmpp != (wchar_t)0);

		while (iswspace(*wsrcp)) {
			gint mbl;

			mbl = wctomb(destp, *wsrcp++);
			if (mbl != -1) {
				destp += mbl;
				line_len += mbl;
			}
		}
		*destp = '\0';

		g_free(wtmp);
	}

	g_free(wsrc);

	/* g_print("dest = %s\n", dest); */
}
#endif /* HAVE_LIBJCONV */
