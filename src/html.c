/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999,2000 Hiroyuki Yamamoto
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

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "html.h"
#include "codeconv.h"
#include "utils.h"

#define HTMLBUFSIZE	8192
#define HR_STR		"------------------------------------------------"

typedef struct _HTMLSymbol	HTMLSymbol;

struct _HTMLSymbol
{
	gchar *const key;
	gchar *const val;
};

static HTMLSymbol symbol_list[] = {
	{"&lt;"    , "<"},
	{"&gt;"    , ">"},
	{"&amp;"   , "&"},
	{"&quot;"  , "\""},
	{"&nbsp;"  , " "},
	{"&trade;" , "(TM)"},

	{"&#153;", "(TM)"},
};

static HTMLSymbol ascii_symbol_list[] = {
	{"&iexcl;" , "^!"},
	{"&brvbar;", "|"},
	{"&copy;"  , "(C)"},
	{"&laquo;" , "<<"},
	{"&reg;"   , "(R)"},

	{"&sup2;"  , "^2"},
	{"&sup3;"  , "^3"},
	{"&acute;" , "'"},
	{"&cedil;" , ","},
	{"&sup1;"  , "^1"},
	{"&raquo;" , ">>"},
	{"&frac14;", "1/4"},
	{"&frac12;", "1/2"},
	{"&frac34;", "3/4"},
	{"&iquest;", "^?"},

	{"&Agrave;", "A`"},
	{"&Aacute;", "A'"},
	{"&Acirc;" , "A^"},
	{"&Atilde;", "A~"},
	{"&AElig;" , "AE"},
	{"&Egrave;", "E`"},
	{"&Eacute;", "E'"},
	{"&Ecirc;" , "E^"},
	{"&Igrave;", "I`"},
	{"&Iacute;", "I'"},
	{"&Icirc;" , "I^"},

	{"&Ntilde;", "N~"},
	{"&Ograve;", "O`"},
	{"&Oacute;", "O'"},
	{"&Ocirc;" , "O^"},
	{"&Otilde;", "O~"},
	{"&Ugrave;", "U`"},
	{"&Uacute;", "U'"},
	{"&Ucirc;" , "U^"},
	{"&Yacute;", "Y'"},

	{"&agrave;", "a`"},
	{"&aacute;", "a'"},
	{"&acirc;" , "a^"},
	{"&atilde;", "a~"},
	{"&aelig;" , "ae"},
	{"&egrave;", "e`"},
	{"&eacute;", "e'"},
	{"&ecirc;" , "e^"},
	{"&igrave;", "i`"},
	{"&iacute;", "i'"},
	{"&icirc;" , "i^"},

	{"&ntilde;", "n~"},
	{"&ograve;", "o`"},
	{"&oacute;", "o'"},
	{"&ocirc;" , "o^"},
	{"&otilde;", "o~"},
	{"&ugrave;", "u`"},
	{"&uacute;", "u'"},
	{"&ucirc;" , "u^"},
	{"&yacute;", "y'"},
};

static HTMLSymbol eucjp_symbol_list[] = {
	{"&iexcl;" , "^!"},
	{"&cent;"  , "\xa1\xf1"},
	{"&pound;" , "\xa1\xf2"},
	{"&yen;"   , "\xa1\xef"},
	{"&brvbar;", "|"},
	{"&sect;"  , "\xa1\xf8"},
	{"&uml;"   , "\xa1\xaf"},
	{"&copy;"  , "(C)"},
	{"&laquo;" , "<<"},
	{"&reg;"   , "(R)"},

	{"&deg;"   , "\xa1\xeb"},
	{"&plusmn;", "\xa1\xde"},
	{"&sup2;"  , "^2"},
	{"&sup3;"  , "^3"},
	{"&acute;" , "'"},
	{"&micro;" , "\xa6\xcc"},
	{"&para;"  , "\xa2\xf9"},
	{"&middot;", "\xa1\xa6"},
	{"&cedil;" , ","},
	{"&sup1;"  , "^1"},
	{"&raquo;" , ">>"},
	{"&frac14;", "1/4"},
	{"&frac12;", "1/2"},
	{"&frac34;", "3/4"},
	{"&iquest;", "^?"},

	{"&Agrave;", "A`"},
	{"&Aacute;", "A'"},
	{"&Acirc;" , "A^"},
	{"&Atilde;", "A~"},
	{"&Auml;"  , "A\xa1\xaf"},
	{"&Aring;" , "A\xa1\xeb"},
	{"&AElig;" , "AE"},
	{"&Egrave;", "E`"},
	{"&Eacute;", "E'"},
	{"&Ecirc;" , "E^"},
	{"&Euml;"  , "E\xa1\xaf"},
	{"&Igrave;", "I`"},
	{"&Iacute;", "I'"},
	{"&Icirc;" , "I^"},
	{"&Iuml;"  , "I\xa1\xaf"},

	{"&Ntilde;", "N~"},
	{"&Ograve;", "O`"},
	{"&Oacute;", "O'"},
	{"&Ocirc;" , "O^"},
	{"&Otilde;", "O~"},
	{"&Ouml;"  , "O\xa1\xaf"},
	{"&times;" , "\xa1\xdf"},
	{"&Ugrave;", "U`"},
	{"&Uacute;", "U'"},
	{"&Ucirc;" , "U^"},
	{"&Uuml;"  , "U\xa1\xaf"},
	{"&Yacute;", "Y'"},

	{"&agrave;", "a`"},
	{"&aacute;", "a'"},
	{"&acirc;" , "a^"},
	{"&atilde;", "a~"},
	{"&auml;"  , "a\xa1\xaf"},
	{"&aring;" , "a\xa1\xeb"},
	{"&aelig;" , "ae"},
	{"&egrave;", "e`"},
	{"&eacute;", "e'"},
	{"&ecirc;" , "e^"},
	{"&euml;"  , "e\xa1\xaf"},
	{"&igrave;", "i`"},
	{"&iacute;", "i'"},
	{"&icirc;" , "i^"},
	{"&iuml;"  , "i\xa1\xaf"},

	{"&eth;"   , "\xa2\xdf"},
	{"&ntilde;", "n~"},
	{"&ograve;", "o`"},
	{"&oacute;", "o'"},
	{"&ocirc;" , "o^"},
	{"&otilde;", "o~"},
	{"&ouml;"  , "o\xa1\xaf"},
	{"&divide;", "\xa1\xe0"},
	{"&ugrave;", "u`"},
	{"&uacute;", "u'"},
	{"&ucirc;" , "u^"},
	{"&uuml;"  , "u\xa1\xaf"},
	{"&yacute;", "y'"},
	{"&yuml;"  , "y\xa1\xaf"},
};

static HTMLSymbol latin_symbol_list[] = {
	{"&iexcl;" , "\xa1"},
	{"&cent;"  , "\xa2"},
	{"&pound;" , "\xa3"},
	{"&curren;", "\xa4"},
	{"&yen;"   , "\xa5"},
	{"&brvbar;", "\xa6"},
	{"&sect;"  , "\xa7"},
	{"&uml;"   , "\xa8"},
	{"&copy;"  , "\xa9"},
	{"&ordf;"  , "\xaa"},
	{"&laquo;" , "\xab"},
	{"&not;"   , "\xac"},
	{"&shy;"   , "\xad"},
	{"&reg;"   , "\xae"},
	{"&macr;"  , "\xaf"},

	{"&deg;"   , "\xb0"},
	{"&plusmn;", "\xb1"},
	{"&sup2;"  , "\xb2"},
	{"&sup3;"  , "\xb3"},
	{"&acute;" , "\xb4"},
	{"&micro;" , "\xb5"},
	{"&para;"  , "\xb6"},
	{"&middot;", "\xb7"},
	{"&cedil;" , "\xb8"},
	{"&sup1;"  , "\xb9"},
	{"&ordm;"  , "\xba"},
	{"&raquo;" , "\xbb"},
	{"&frac14;", "\xbc"},
	{"&frac12;", "\xbd"},
	{"&frac34;", "\xbe"},
	{"&iquest;", "\xbf"},

	{"&Agrave;", "\xc0"},
	{"&Aacute;", "\xc1"},
	{"&Acirc;" , "\xc2"},
	{"&Atilde;", "\xc3"},
	{"&Auml;"  , "\xc4"},
	{"&Aring;" , "\xc5"},
	{"&AElig;" , "\xc6"},
	{"&Ccedil;", "\xc7"},
	{"&Egrave;", "\xc8"},
	{"&Eacute;", "\xc9"},
	{"&Ecirc;" , "\xca"},
	{"&Euml;"  , "\xcb"},
	{"&Igrave;", "\xcc"},
	{"&Iacute;", "\xcd"},
	{"&Icirc;" , "\xce"},
	{"&Iuml;"  , "\xcf"},

	{"&ETH;"   , "\xd0"},
	{"&Ntilde;", "\xd1"},
	{"&Ograve;", "\xd2"},
	{"&Oacute;", "\xd3"},
	{"&Ocirc;" , "\xd4"},
	{"&Otilde;", "\xd5"},
	{"&Ouml;"  , "\xd6"},
	{"&times;" , "\xd7"},
	{"&Oslash;", "\xd8"},
	{"&Ugrave;", "\xd9"},
	{"&Uacute;", "\xda"},
	{"&Ucirc;" , "\xdb"},
	{"&Uuml;"  , "\xdc"},
	{"&Yacute;", "\xdd"},
	{"&THORN;" , "\xde"},
	{"&szlig;" , "\xdf"},

	{"&agrave;", "\xe0"},
	{"&aacute;", "\xe1"},
	{"&acirc;" , "\xe2"},
	{"&atilde;", "\xe3"},
	{"&auml;"  , "\xe4"},
	{"&aring;" , "\xe5"},
	{"&aelig;" , "\xe6"},
	{"&ccedil;", "\xe7"},
	{"&egrave;", "\xe8"},
	{"&eacute;", "\xe9"},
	{"&ecirc;" , "\xea"},
	{"&euml;"  , "\xeb"},
	{"&igrave;", "\xec"},
	{"&iacute;", "\xed"},
	{"&icirc;" , "\xee"},
	{"&iuml;"  , "\xef"},

	{"&eth;"   , "\xf0"},
	{"&ntilde;", "\xf1"},
	{"&ograve;", "\xf2"},
	{"&oacute;", "\xf3"},
	{"&ocirc;" , "\xf4"},
	{"&otilde;", "\xf5"},
	{"&ouml;"  , "\xf6"},
	{"&divide;", "\xf7"},
	{"&oslash;", "\xf8"},
	{"&ugrave;", "\xf9"},
	{"&uacute;", "\xfa"},
	{"&ucirc;" , "\xfb"},
	{"&uuml;"  , "\xfc"},
	{"&yacute;", "\xfd"},
	{"&thorn;" , "\xfe"},
	{"&yuml;"  , "\xff"},
};

static GHashTable *default_symbol_table;
static GHashTable *eucjp_symbol_table;
static GHashTable *latin_symbol_table;

static HTMLState html_read_line		(HTMLParser	*parser);
static void html_append_char		(HTMLParser	*parser,
					 gchar		 ch);
static void html_append_str		(HTMLParser	*parser,
					 const gchar	*str,
					 gint		 len);
static HTMLState html_parse_tag		(HTMLParser	*parser);
static void html_parse_special		(HTMLParser	*parser);
static void html_get_parenthesis	(HTMLParser	*parser,
					 gchar		*buf,
					 gint		 len);

#if 0
static gint g_str_case_equal		(gconstpointer	 v,
					 gconstpointer	 v2);
static guint g_str_case_hash		(gconstpointer	 key);
#endif

HTMLParser *html_parser_new(FILE *fp, CodeConverter *conv)
{
	HTMLParser *parser;

	g_return_val_if_fail(fp != NULL, NULL);
	g_return_val_if_fail(conv != NULL, NULL);

	parser = g_new0(HTMLParser, 1);
	parser->fp = fp;
	parser->conv = conv;
	parser->str = g_string_new(NULL);
	parser->buf = g_string_new(NULL);
	parser->bufp = parser->buf->str;
	parser->newline = TRUE;
	parser->empty_line = TRUE;
	parser->space = FALSE;
	parser->pre = FALSE;

#define SYMBOL_TABLE_ADD(table, list) \
{ \
	gint i; \
 \
	for (i = 0; i < sizeof(list) / sizeof(list[0]); i++) \
		g_hash_table_insert(table, list[i].key, list[i].val); \
}

	if (!default_symbol_table) {
		default_symbol_table =
			g_hash_table_new(g_str_hash, g_str_equal);
		SYMBOL_TABLE_ADD(default_symbol_table, symbol_list);
		SYMBOL_TABLE_ADD(default_symbol_table, ascii_symbol_list);
	}
	if (!eucjp_symbol_table) {
		eucjp_symbol_table =
			g_hash_table_new(g_str_hash, g_str_equal);
		SYMBOL_TABLE_ADD(eucjp_symbol_table, symbol_list);
		SYMBOL_TABLE_ADD(eucjp_symbol_table, eucjp_symbol_list);
	}
	if (!latin_symbol_table) {
		latin_symbol_table =
			g_hash_table_new(g_str_hash, g_str_equal);
		SYMBOL_TABLE_ADD(latin_symbol_table, symbol_list);
		SYMBOL_TABLE_ADD(latin_symbol_table, latin_symbol_list);
	}

#undef SYMBOL_TABLE_ADD

	if (conv->charset == C_ISO_8859_1)
		parser->symbol_table = latin_symbol_table;
	else if ((conv->charset == C_ISO_2022_JP   ||
		  conv->charset == C_ISO_2022_JP_2 ||
		  conv->charset == C_EUC_JP        ||
		  conv->charset == C_SHIFT_JIS) &&
		 conv_get_current_charset() == C_EUC_JP)
		parser->symbol_table = eucjp_symbol_table;
	else
		parser->symbol_table = default_symbol_table;

	return parser;
}

void html_parser_destroy(HTMLParser *parser)
{
	g_string_free(parser->str, TRUE);
	g_string_free(parser->buf, TRUE);
	g_free(parser);
}

gchar *html_parse(HTMLParser *parser)
{
	parser->state = HTML_NORMAL;
	g_string_truncate(parser->str, 0);

	if (*parser->bufp == '\0') {
		g_string_truncate(parser->buf, 0);
		parser->bufp = parser->buf->str;
		if (html_read_line(parser) == HTML_EOF)
			return NULL;
	}

	while (*parser->bufp != '\0') {
		switch (*parser->bufp) {
		case '<':
			if (parser->str->len == 0)
				html_parse_tag(parser);
			else
				return parser->str->str;
			break;
		case '&':
			html_parse_special(parser);
			break;
		case ' ':
		case '\t':
		case '\r':
		case '\n':
			if (parser->bufp[0] == '\r' && parser->bufp[1] == '\n')
				parser->bufp++;

			if (!parser->pre) {
				if (!parser->newline)
					parser->space = TRUE;

				parser->bufp++;
				break;
			}
			/* fallthrough */
		default:
			html_append_char(parser, *parser->bufp++);
		}
	}

	return parser->str->str;
}

static HTMLState html_read_line(HTMLParser *parser)
{
	gchar buf[HTMLBUFSIZE];
	gchar buf2[HTMLBUFSIZE];
	gint index;

	if (fgets(buf, sizeof(buf), parser->fp) == NULL) {
		parser->state = HTML_EOF;
		return HTML_EOF;
	}

	if (conv_convert(parser->conv, buf2, sizeof(buf2), buf) < 0) {
		g_warning("html_read_line(): code conversion failed\n");

		index = parser->bufp - parser->buf->str;

		g_string_append(parser->buf, buf);

		parser->bufp = parser->buf->str + index;

		return HTML_ERR;
	}

	index = parser->bufp - parser->buf->str;

	g_string_append(parser->buf, buf2);

	parser->bufp = parser->buf->str + index;

	return HTML_NORMAL;
}

static void html_append_char(HTMLParser *parser, gchar ch)
{
	GString *str = parser->str;

	if (!parser->pre && parser->space) {
		g_string_append_c(str, ' ');
		parser->space = FALSE;
	}

	g_string_append_c(str, ch);

	parser->empty_line = FALSE;
	if (ch == '\n') {
		parser->newline = TRUE;
		if (str->len > 1 && str->str[str->len - 2] == '\n')
			parser->empty_line = TRUE;
	} else
		parser->newline = FALSE;
}

static void html_append_str(HTMLParser *parser, const gchar *str, gint len)
{
	GString *string = parser->str;

	if (!parser->pre && parser->space) {
		g_string_append_c(string, ' ');
		parser->space = FALSE;
	}

	if (len == 0) return;
	if (len < 0)
		g_string_append(string, str);
	else {
		gchar *s;
		Xstrndup_a(s, str, len, return);
		g_string_append(string, s);
	}

	parser->empty_line = FALSE;
	if (string->len > 0 && string->str[string->len - 1] == '\n') {
		parser->newline = TRUE;
		if (string->len > 1 && string->str[string->len - 2] == '\n')
			parser->empty_line = TRUE;
	} else
		parser->newline = FALSE;
}

static HTMLState html_parse_tag(HTMLParser *parser)
{
	gchar buf[HTMLBUFSIZE];
	gchar *p;
	static gboolean is_in_href = FALSE;

	html_get_parenthesis(parser, buf, sizeof(buf));

	for (p = buf; *p != '\0'; p++) {
		if (isspace(*p)) {
			*p = '\0';
			break;
		}
	}

	parser->state = HTML_UNKNOWN;
	if (buf[0] == '\0') return parser->state;

	g_strdown(buf);

	if (!strcmp(buf, "br")) {
		parser->space = FALSE;
		html_append_char(parser, '\n');
		parser->state = HTML_BR;
	} else if (!strcmp(buf, "a")) {
	        /* look for tokens separated by space or = */
	        char* href_token = strtok(++p, " =");
		parser->state = HTML_NORMAL;
		while (href_token != NULL) {
		        /* look for href */
		        if (!strcmp(href_token, "href")) {
			        /* the next token is the url, between double
					 * quotes */
			        char* url = strtok(NULL, "\"");
					if (url && url[0] == '\'')
					  url = strtok(url,"\'");

				if (!url) break;
				html_append_str(parser, url, strlen(url));
				html_append_char(parser, ' ');
				/* start enforcing html link */
				parser->state = HTML_HREF;
				is_in_href = TRUE;
				break;
			}
			/* or get next token */
			href_token = strtok(NULL, " =");
		}
	} else if (!strcmp(buf, "/a")) {
	        /* stop enforcing html link */
	        parser->state = HTML_NORMAL;
	        is_in_href = FALSE;
	} else if (!strcmp(buf, "p")) {
		parser->space = FALSE;
		if (!parser->empty_line) {
			parser->space = FALSE;
			if (!parser->newline) html_append_char(parser, '\n');
			html_append_char(parser, '\n');
		}
		parser->state = HTML_PAR;
	} else if (!strcmp(buf, "pre")) {
		parser->pre = TRUE;
		parser->state = HTML_PRE;
	} else if (!strcmp(buf, "/pre")) {
		parser->pre = FALSE;
		parser->state = HTML_NORMAL;
	} else if (!strcmp(buf, "hr")) {
		if (!parser->newline) {
			parser->space = FALSE;
			html_append_char(parser, '\n');
		}
		html_append_str(parser, HR_STR "\n", -1);
		parser->state = HTML_HR;
	} else if (!strcmp(buf, "div")    ||
		   !strcmp(buf, "ul")     ||
		   !strcmp(buf, "li")     ||
		   !strcmp(buf, "table")  ||
		   !strcmp(buf, "tr")     ||
		   (buf[0] == 'h' && isdigit(buf[1]))) {
		if (!parser->newline) {
			parser->space = FALSE;
			html_append_char(parser, '\n');
		}
		parser->state = HTML_NORMAL;
	} else if (!strcmp(buf, "/table") ||
		   (buf[0] == '/' && buf[1] == 'h' && isdigit(buf[1]))) {
		if (!parser->empty_line) {
			parser->space = FALSE;
			if (!parser->newline) html_append_char(parser, '\n');
			html_append_char(parser, '\n');
		}
		parser->state = HTML_NORMAL;
	} else if (!strcmp(buf, "/div")   ||
		   !strcmp(buf, "/ul")    ||
		   !strcmp(buf, "/li")) {
		if (!parser->newline) {
			parser->space = FALSE;
			html_append_char(parser, '\n');
		}
		parser->state = HTML_NORMAL;
	}
	
	if (is_in_href == TRUE) {
	        /* when inside a link, everything will be written as
		 * clickable (see textview_show_thml in textview.c) */
	        parser->state = HTML_HREF;
	}

	return parser->state;
}

static void html_parse_special(HTMLParser *parser)
{
	gchar symbol_name[9];
	gint n;
	const gchar *val;

	parser->state = HTML_UNKNOWN;
	g_return_if_fail(*parser->bufp == '&');

	/* &foo; */
	for (n = 0; parser->bufp[n] != '\0' && parser->bufp[n] != ';'; n++)
		;
	if (n > 7 || parser->bufp[n] != ';') {
		/* output literal `&' */
		html_append_char(parser, *parser->bufp++);
		parser->state = HTML_NORMAL;
		return;
	}
	strncpy2(symbol_name, parser->bufp, n + 2);
	parser->bufp += n + 1;

	if ((val = g_hash_table_lookup(parser->symbol_table, symbol_name))
	    != NULL) {
		html_append_str(parser, val, -1);
		parser->state = HTML_NORMAL;
		return;
	} else if (symbol_name[1] == '#' && isdigit(symbol_name[2])) {
		gint ch;

		ch = atoi(symbol_name + 2);
		if ((ch > 0 && ch <= 127) ||
		    (ch >= 128 && ch <= 255 &&
		     parser->conv->charset == C_ISO_8859_1)) {
			html_append_char(parser, ch);
			parser->state = HTML_NORMAL;
			return;
		}
	}

	html_append_str(parser, symbol_name, -1);
}

static void html_get_parenthesis(HTMLParser *parser, gchar *buf, gint len)
{
	gchar *p;

	buf[0] = '\0';
	g_return_if_fail(*parser->bufp == '<');

	/* ignore comment / CSS / script stuff */
	if (!strncmp(parser->bufp, "<!--", 4)) {
		parser->bufp += 4;
		while ((p = strstr(parser->bufp, "-->")) == NULL)
			if (html_read_line(parser) == HTML_EOF) return;
		parser->bufp = p + 3;
		return;
	}
	if (!g_strncasecmp(parser->bufp, "<style", 6)) {
		parser->bufp += 6;
		while ((p = strcasestr(parser->bufp, "</style>")) == NULL)
			if (html_read_line(parser) == HTML_EOF) return;
		parser->bufp = p + 8;
		return;
	}
	if (!g_strncasecmp(parser->bufp, "<script", 7)) {
		parser->bufp += 7;
		while ((p = strcasestr(parser->bufp, "</script>")) == NULL)
			if (html_read_line(parser) == HTML_EOF) return;
		parser->bufp = p + 9;
		return;
	}

	parser->bufp++;
	while ((p = strchr(parser->bufp, '>')) == NULL)
		if (html_read_line(parser) == HTML_EOF) return;

	strncpy2(buf, parser->bufp, MIN(p - parser->bufp + 1, len));
	parser->bufp = p + 1;
}

/* these hash functions were taken from gstring.c in glib */
#if 0
static gint g_str_case_equal(gconstpointer v, gconstpointer v2)
{
	return strcasecmp((const gchar *)v, (const gchar *)v2) == 0;
}

static guint g_str_case_hash(gconstpointer key)
{
	const gchar *p = key;
	guint h = *p;

	if (h) {
		h = tolower(h);
		for (p += 1; *p != '\0'; p++)
			h = (h << 5) - h + tolower(*p);
	}

	return h;
}
#endif
