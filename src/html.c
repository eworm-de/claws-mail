/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Hiroyuki Yamamoto and the Claws Mail team
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
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "html.h"
#include "codeconv.h"
#include "utils.h"

#define SC_HTMLBUFSIZE	8192
#define HR_STR		"------------------------------------------------"

typedef struct _SC_HTMLSymbol	SC_HTMLSymbol;

struct _SC_HTMLSymbol
{
	gchar *const key;
	gchar *const val;
};

static SC_HTMLSymbol symbol_list[] = {
	{"&lt;", "\74"},
	{"&gt;", "\76"},
	{"&amp;", "\46"},
	{"&quot;", "\42"},
	{"&lsquo;", "\47"},
	{"&rsquo;", "\47"},
	{"&ldquo;", "\42"},
	{"&rdquo;", "\42"},
	{"&laquo;", "\302\253"},
	{"&raquo;", "\302\273"},
	{"&nbsp;", "\40"},
	{"&trade;", "\50\124\115\51"},
	{"&hellip;", "\56\56\56"},
	{"&bull;", "\52"},
	{"&ndash;", "\55"},
	{"&mdash;", "\55\55"},
	{"&euro;", "\105\125\122"},
	{"&cent;", "\302\242"},
	{"&pound;", "\302\243"},
	{"&curren;", "\302\244"},
	{"&yen;", "\302\245"},
	{"&copy;", "\302\251"},
	{"&reg;", "\302\256"},
	{"&iquest;", "\302\277"},
	{"&iexcl;", "\302\241"}
};

static SC_HTMLSymbol ascii_symbol_list[] = {
	{"&iexcl;" , "\302\241"},
	{"&brvbar;", "\302\246"},
	{"&copy;"  , "\302\251"},
	{"&laquo;" , "\302\253"},
	{"&reg;"   , "\302\256"},

	{"&sup2;"  , "\302\262"},
	{"&sup3;"  , "\302\263"},
	{"&acute;" , "\302\264"},
	{"&cedil;" , "\302\270"},
	{"&sup1;"  , "\302\271"},
	{"&raquo;" , "\302\273"},
	{"&frac14;", "\302\274"},
	{"&frac12;", "\302\275"},
	{"&frac34;", "\302\276"},
	{"&iquest;", "\302\277"},

	{"&Agrave;", "\303\200"},
	{"&Aacute;", "\303\201"},
	{"&Acirc;" , "\303\202"},
	{"&Atilde;", "\303\203"},
	{"&AElig;" , "\303\206"},
	{"&Egrave;", "\303\210"},
	{"&Eacute;", "\303\211"},
	{"&Ecirc;" , "\303\212"},
	{"&Igrave;", "\303\214"},
	{"&Iacute;", "\303\215"},
	{"&Icirc;" , "\303\216"},

	{"&Ntilde;", "\303\221"},
	{"&Ograve;", "\303\222"},
	{"&Oacute;", "\303\223"},
	{"&Ocirc;" , "\303\224"},
	{"&Otilde;", "\303\225"},
	{"&Ugrave;", "\303\231"},
	{"&Uacute;", "\303\232"},
	{"&Ucirc;" , "\303\233"},
	{"&Yacute;", "\303\235"},

	{"&agrave;", "\303\240"},
	{"&aacute;", "\303\241"},
	{"&acirc;" , "\303\242"},
	{"&atilde;", "\303\243"},
	{"&aelig;" , "\303\246"},
	{"&egrave;", "\303\250"},
	{"&eacute;", "\303\251"},
	{"&ecirc;" , "\303\252"},
	{"&igrave;", "\303\254"},
	{"&iacute;", "\303\255"},
	{"&icirc;" , "\303\256"},

	{"&ntilde;", "\303\261"},
	{"&ograve;", "\303\262"},
	{"&oacute;", "\303\263"},
	{"&ocirc;" , "\303\264"},
	{"&otilde;", "\303\265"},
	{"&ugrave;", "\303\271"},
	{"&uacute;", "\303\272"},
	{"&ucirc;" , "\303\273"},
	{"&yacute;", "\303\275"}
};

typedef struct _SC_HTMLAltSymbol	SC_HTMLAltSymbol;

struct _SC_HTMLAltSymbol
{
	gint key;
	gchar *const val;
};

/* http://www.w3schools.com/html/html_entitiesref.asp */
static SC_HTMLAltSymbol alternate_symbol_list[] = {
	{  96, "\140"}, 		   /* backtick */
	{ 153, "\50\124\115\51"},  /* trademark */
	{ 161, "\302\241"}, 	   /* inverted exclamation mark &iexcl */
	{ 162, "\302\242"}, 	   /* cent (currency) &cent */
	{ 163, "\302\243"}, 	   /* pound (currency) &pound */
	{ 164, "\342\202\254"},    /* currency sign &curren */
	{ 165, "\302\245"}, 	   /* yen (currency) &yen */
	{ 169, "\302\251"}, 	   /* copyright sign &copy */
	{ 174, "\302\256"}, 	   /* registered sign &reg */
	{ 191, "\302\277"}, 	   /* inverted question mark &iquest */
	{ 338, "\117\105"}, 	   /* capital ligature OE &OElig */
	{ 339, "\157\145"}, 	   /* small ligature OE &oelig */
	{ 352, NULL},			   /* capital S w/caron &Scaron */
	{ 353, NULL},			   /* small S w/caron &scaron */
	{ 376, NULL},			   /* cap Y w/ diaeres &Yuml */
	{ 710, "\136"}, 		   /* circumflex accent &circ */
	{ 732, "\176"}, 		   /* small tilde &tilde */
	{8194, "\40"},			   /* en space &ensp */
	{8195, "\40"},			   /* em space &emsp */
	{8201, "\40"},			   /* thin space &thinsp */
	{8204, NULL},			   /* zero width non-joiner &zwnj */
	{8205, NULL},			   /* zero width joiner &zwj */
	{8206, NULL},			   /* l-t-r mark &lrm */
	{8207, NULL},			   /* r-t-l mark &rlm */
	{8211, "\55"},			   /* en dash &ndash */
	{8212, "\55\55"},		   /* em dash &mdash */
	{8216, "\47"},			   /* l single quot mark &lsquo */
	{8217, "\47"},			   /* r single quot mark &rsquo */
	{8218, "\54"},			   /* single low-9 quot &sbquo */
	{8220, "\134"}, 		   /* l double quot mark &ldquo */
	{8221, "\134"}, 		   /* r double quot mark &rdquo */
	{8222, "\42"},			   /* double low-9 quot &bdquo */
	{8224, NULL},			   /* dagger &dagger */
	{8225, NULL},			   /* double dagger &Dagger */
	{8226, "\52"},			   /* bullet &bull */
	{8230, "\56\56\56"},	   /* horizontal ellipsis &hellip */
	{8240, "\45\157"},		   /* per mile &permil */
	{8249, "\74"},			   /* l-pointing angle quot &lsaquo */
	{8250, "\76"},			   /* r-pointing angle quot &rsaquo */
	{8364, "\105\125\122"},    /* euro &euro */
	{8482, "\50\124\115\51"}   /* trademark &trade */
};

static GHashTable *default_symbol_table;
static GHashTable *alternate_symbol_table;

static SC_HTMLState sc_html_read_line	(SC_HTMLParser	*parser);
static void sc_html_append_char			(SC_HTMLParser	*parser,
					 gchar		 ch);
static void sc_html_append_str			(SC_HTMLParser	*parser,
					 const gchar	*str,
					 gint		 len);
static SC_HTMLState sc_html_parse_tag	(SC_HTMLParser	*parser);
static void sc_html_parse_special		(SC_HTMLParser	*parser);
static void sc_html_get_parenthesis		(SC_HTMLParser	*parser,
					 gchar		*buf,
					 gint		 len);


SC_HTMLParser *sc_html_parser_new(FILE *fp, CodeConverter *conv)
{
	SC_HTMLParser *parser;

	g_return_val_if_fail(fp != NULL, NULL);
	g_return_val_if_fail(conv != NULL, NULL);

	parser = g_new0(SC_HTMLParser, 1);
	parser->fp = fp;
	parser->conv = conv;
	parser->str = g_string_new(NULL);
	parser->buf = g_string_new(NULL);
	parser->bufp = parser->buf->str;
	parser->state = SC_HTML_NORMAL;
	parser->href = NULL;
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
#define SYMBOL_TABLE_REF_ADD(table, list) \
{ \
	gint i; \
 \
	for (i = 0; i < sizeof(list) / sizeof(list[0]); i++) \
		g_hash_table_insert(table, &list[i].key, list[i].val); \
}

	if (!default_symbol_table) {
		default_symbol_table =
			g_hash_table_new(g_str_hash, g_str_equal);
		SYMBOL_TABLE_ADD(default_symbol_table, symbol_list);
		SYMBOL_TABLE_ADD(default_symbol_table, ascii_symbol_list);
	}
	if (!alternate_symbol_table) {
		alternate_symbol_table =
			g_hash_table_new(g_int_hash, g_int_equal);
		SYMBOL_TABLE_REF_ADD(alternate_symbol_table, alternate_symbol_list);
	}

#undef SYMBOL_TABLE_ADD
#undef SYMBOL_TABLE_REF_ADD

	parser->symbol_table = default_symbol_table;
	parser->alt_symbol_table = alternate_symbol_table;

	return parser;
}

void sc_html_parser_destroy(SC_HTMLParser *parser)
{
	g_string_free(parser->str, TRUE);
	g_string_free(parser->buf, TRUE);
	g_free(parser->href);
	g_free(parser);
}

gchar *sc_html_parse(SC_HTMLParser *parser)
{
	parser->state = SC_HTML_NORMAL;
	g_string_truncate(parser->str, 0);

	if (*parser->bufp == '\0') {
		g_string_truncate(parser->buf, 0);
		parser->bufp = parser->buf->str;
		if (sc_html_read_line(parser) == SC_HTML_EOF)
			return NULL;
	}

	while (*parser->bufp != '\0') {
		switch (*parser->bufp) {
		case '<': {
			SC_HTMLState st;
			st = sc_html_parse_tag(parser);
			/* when we see an href, we need to flush the str
			 * buffer.  Then collect all the chars until we
			 * see the end anchor tag
			 */
			if (SC_HTML_HREF_BEG == st || SC_HTML_HREF == st)
				return parser->str->str;
			} 
			break;
		case '&':
			sc_html_parse_special(parser);
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
			sc_html_append_char(parser, *parser->bufp++);
		}
	}

	return parser->str->str;
}

static SC_HTMLState sc_html_read_line(SC_HTMLParser *parser)
{
	gchar buf[SC_HTMLBUFSIZE];
	gchar buf2[SC_HTMLBUFSIZE];
	gint index;

	if (fgets(buf, sizeof(buf), parser->fp) == NULL) {
		parser->state = SC_HTML_EOF;
		return SC_HTML_EOF;
	}

	if (conv_convert(parser->conv, buf2, sizeof(buf2), buf) < 0) {
		index = parser->bufp - parser->buf->str;

		conv_utf8todisp(buf2, sizeof(buf2), buf);
		g_string_append(parser->buf, buf2);

		parser->bufp = parser->buf->str + index;

		return SC_HTML_CONV_FAILED;
	}

	index = parser->bufp - parser->buf->str;

	g_string_append(parser->buf, buf2);

	parser->bufp = parser->buf->str + index;

	return SC_HTML_NORMAL;
}

static void sc_html_append_char(SC_HTMLParser *parser, gchar ch)
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

static void sc_html_append_str(SC_HTMLParser *parser, const gchar *str, gint len)
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

static SC_HTMLTag *sc_html_get_tag(const gchar *str)
{
	SC_HTMLTag *tag;
	gchar *tmp;
	guchar *tmpp;

	g_return_val_if_fail(str != NULL, NULL);

	if (*str == '\0' || *str == '!') return NULL;

	Xstrdup_a(tmp, str, return NULL);

	tag = g_new0(SC_HTMLTag, 1);

	for (tmpp = tmp; *tmpp != '\0' && !g_ascii_isspace(*tmpp); tmpp++)
		;

	if (*tmpp == '\0') {
		g_strdown(tmp);
		tag->name = g_strdup(tmp);
		return tag;
	} else {
		*tmpp++ = '\0';
		g_strdown(tmp);
		tag->name = g_strdup(tmp);
	}

	while (*tmpp != '\0') {
		SC_HTMLAttr *attr;
		gchar *attr_name;
		gchar *attr_value;
		gchar *p;
		gchar quote;

		while (g_ascii_isspace(*tmpp)) tmpp++;
		attr_name = tmpp;

		while (*tmpp != '\0' && !g_ascii_isspace(*tmpp) &&
		       *tmpp != '=')
			tmpp++;
		if (*tmpp != '\0' && *tmpp != '=') {
			*tmpp++ = '\0';
			while (g_ascii_isspace(*tmpp)) tmpp++;
		}

		if (*tmpp == '=') {
			*tmpp++ = '\0';
			while (g_ascii_isspace(*tmpp)) tmpp++;

			if (*tmpp == '"' || *tmpp == '\'') {
				/* name="value" */
				quote = *tmpp;
				tmpp++;
				attr_value = tmpp;
				if ((p = strchr(attr_value, quote)) == NULL) {
					g_warning("sc_html_get_tag(): syntax error in tag: '%s'\n", str);
					return tag;
				}
				tmpp = p;
				*tmpp++ = '\0';
				while (g_ascii_isspace(*tmpp)) tmpp++;
			} else {
				/* name=value */
				attr_value = tmpp;
				while (*tmpp != '\0' && !g_ascii_isspace(*tmpp)) tmpp++;
				if (*tmpp != '\0')
					*tmpp++ = '\0';
			}
		} else
			attr_value = "";

		g_strchomp(attr_name);
		g_strdown(attr_name);
		attr = g_new(SC_HTMLAttr, 1);
		attr->name = g_strdup(attr_name);
		attr->value = g_strdup(attr_value);
		tag->attr = g_list_append(tag->attr, attr);
	}

	return tag;
}

static void sc_html_free_tag(SC_HTMLTag *tag)
{
	if (!tag) return;

	g_free(tag->name);
	while (tag->attr != NULL) {
		SC_HTMLAttr *attr = (SC_HTMLAttr *)tag->attr->data;
		g_free(attr->name);
		g_free(attr->value);
		g_free(attr);
		tag->attr = g_list_remove(tag->attr, tag->attr->data);
	}
	g_free(tag);
}

static SC_HTMLState sc_html_parse_tag(SC_HTMLParser *parser)
{
	gchar buf[SC_HTMLBUFSIZE];
	SC_HTMLTag *tag;

	sc_html_get_parenthesis(parser, buf, sizeof(buf));

	tag = sc_html_get_tag(buf);

	parser->state = SC_HTML_UNKNOWN;
	if (!tag) return SC_HTML_UNKNOWN;

	if (!strcmp(tag->name, "br")) {
		parser->space = FALSE;
		sc_html_append_char(parser, '\n');
		parser->state = SC_HTML_BR;
	} else if (!strcmp(tag->name, "a")) {
		GList *cur;
		for (cur = tag->attr; cur != NULL; cur = cur->next) {
			if (cur->data && !strcmp(((SC_HTMLAttr *)cur->data)->name, "href")) {
				g_free(parser->href);
				parser->href = g_strdup(((SC_HTMLAttr *)cur->data)->value);
				parser->state = SC_HTML_HREF_BEG;
				break;
			}
		}
	} else if (!strcmp(tag->name, "/a")) {
		parser->state = SC_HTML_HREF;
	} else if (!strcmp(tag->name, "p")) {
		parser->space = FALSE;
		if (!parser->empty_line) {
			parser->space = FALSE;
			if (!parser->newline) sc_html_append_char(parser, '\n');
			sc_html_append_char(parser, '\n');
		}
		parser->state = SC_HTML_PAR;
	} else if (!strcmp(tag->name, "pre")) {
		parser->pre = TRUE;
		parser->state = SC_HTML_PRE;
	} else if (!strcmp(tag->name, "/pre")) {
		parser->pre = FALSE;
		parser->state = SC_HTML_NORMAL;
	} else if (!strcmp(tag->name, "hr")) {
		if (!parser->newline) {
			parser->space = FALSE;
			sc_html_append_char(parser, '\n');
		}
		sc_html_append_str(parser, HR_STR "\n", -1);
		parser->state = SC_HTML_HR;
	} else if (!strcmp(tag->name, "div")    ||
		   !strcmp(tag->name, "ul")     ||
		   !strcmp(tag->name, "li")     ||
		   !strcmp(tag->name, "table")  ||
		   !strcmp(tag->name, "tr")     ||
		   (tag->name[0] == 'h' && g_ascii_isdigit(tag->name[1]))) {
		if (!parser->newline) {
			parser->space = FALSE;
			sc_html_append_char(parser, '\n');
		}
		parser->state = SC_HTML_NORMAL;
	} else if (!strcmp(tag->name, "/table") ||
		   (tag->name[0] == '/' &&
		    tag->name[1] == 'h' &&
		    g_ascii_isdigit(tag->name[1]))) {
		if (!parser->empty_line) {
			parser->space = FALSE;
			if (!parser->newline) sc_html_append_char(parser, '\n');
			sc_html_append_char(parser, '\n');
		}
		parser->state = SC_HTML_NORMAL;
	} else if (!strcmp(tag->name, "/div")   ||
		   !strcmp(tag->name, "/ul")    ||
		   !strcmp(tag->name, "/li")) {
		if (!parser->newline) {
			parser->space = FALSE;
			sc_html_append_char(parser, '\n');
		}
		parser->state = SC_HTML_NORMAL;
			}

	sc_html_free_tag(tag);

	return parser->state;
}

static void sc_html_parse_special(SC_HTMLParser *parser)
{
	gchar symbol_name[9];
	gint n;
	const gchar *val;

	parser->state = SC_HTML_UNKNOWN;
	g_return_if_fail(*parser->bufp == '&');

	/* &foo; */
	for (n = 0; parser->bufp[n] != '\0' && parser->bufp[n] != ';'; n++)
		;
	if (n > 7 || parser->bufp[n] != ';') {
		/* output literal `&' */
		sc_html_append_char(parser, *parser->bufp++);
		parser->state = SC_HTML_NORMAL;
		return;
	}
	strncpy2(symbol_name, parser->bufp, n + 2);
	parser->bufp += n + 1;

	if ((val = g_hash_table_lookup(parser->symbol_table, symbol_name))
	    != NULL) {
		sc_html_append_str(parser, val, -1);
		parser->state = SC_HTML_NORMAL;
		return;
	} else if (symbol_name[1] == '#' && g_ascii_isdigit(symbol_name[2])) {
		gint ch;

		ch = atoi(symbol_name + 2);
		if ((ch > 0 && ch <= 127) ||
		    (ch >= 128 && ch <= 255 &&
		     parser->conv->charset == C_ISO_8859_1)) {
			sc_html_append_char(parser, ch);
			parser->state = SC_HTML_NORMAL;
			return;
		} else {
			const gchar *symb = g_hash_table_lookup(parser->alt_symbol_table, &ch);
			if (symb) {
				sc_html_append_str(parser, symb, -1);
				parser->state = SC_HTML_NORMAL;
				return;
			}
		}
	}

	sc_html_append_str(parser, symbol_name, -1);
}

static void sc_html_get_parenthesis(SC_HTMLParser *parser, gchar *buf, gint len)
{
	gchar *p;

	buf[0] = '\0';
	g_return_if_fail(*parser->bufp == '<');

	/* ignore comment / CSS / script stuff */
	if (!strncmp(parser->bufp, "<!--", 4)) {
		parser->bufp += 4;
		while ((p = strstr(parser->bufp, "-->")) == NULL)
			if (sc_html_read_line(parser) == SC_HTML_EOF) return;
		parser->bufp = p + 3;
		return;
	}
	if (!g_ascii_strncasecmp(parser->bufp, "<style", 6)) {
		parser->bufp += 6;
		while ((p = strcasestr(parser->bufp, "</style>")) == NULL)
			if (sc_html_read_line(parser) == SC_HTML_EOF) return;
		parser->bufp = p + 8;
		return;
	}
	if (!g_ascii_strncasecmp(parser->bufp, "<script", 7)) {
		parser->bufp += 7;
		while ((p = strcasestr(parser->bufp, "</script>")) == NULL)
			if (sc_html_read_line(parser) == SC_HTML_EOF) return;
		parser->bufp = p + 9;
		return;
	}

	parser->bufp++;
	while ((p = strchr(parser->bufp, '>')) == NULL)
		if (sc_html_read_line(parser) == SC_HTML_EOF) return;

	strncpy2(buf, parser->bufp, MIN(p - parser->bufp + 1, len));
	g_strstrip(buf);
	parser->bufp = p + 1;
}
