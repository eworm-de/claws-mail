/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2016 Hiroyuki Yamamoto and the Claws Mail team
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
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "html.h"
#include "codeconv.h"
#include "utils.h"
#include "entity.h"
#include "file-utils.h"

#define SC_HTMLBUFSIZE	8192
#define HR_STR		"────────────────────────────────────────────────"
#define LI_STR		"• "

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

	cm_return_val_if_fail(fp != NULL, NULL);
	cm_return_val_if_fail(conv != NULL, NULL);

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
	parser->indent = 0;

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
	gchar buf2[SC_HTMLBUFSIZE*4];
	gint index;
	gint n;

	if (parser->fp == NULL)
		return SC_HTML_EOF;

	n = claws_fread(buf, 1, sizeof(buf) - 1, parser->fp);
	if (n == 0) {
		parser->state = SC_HTML_EOF;
		return SC_HTML_EOF;
	} else
		buf[n] = '\0';

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
		if (parser->indent > 0) {
			gint i, n = parser->indent;
			for (i = 0; i < n; i++)
				g_string_append_c(str, '>');
			g_string_append_c(str, ' ');
		}
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

	cm_return_val_if_fail(str != NULL, NULL);

	if (*str == '\0' || *str == '!') return NULL;

	Xstrdup_a(tmp, str, return NULL);

	tag = g_new0(SC_HTMLTag, 1);

	for (tmpp = tmp; *tmpp != '\0' && !g_ascii_isspace(*tmpp); tmpp++)
		;

	if (*tmpp == '\0') {
		tag->name = g_utf8_strdown(tmp, -1);
		return tag;
	} else {
		*tmpp++ = '\0';
		tag->name = g_utf8_strdown(tmp, -1);
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
					if (debug_get_mode()) {
						g_warning("sc_html_get_tag(): syntax error in tag: '%s'",
								  str);
					} else {
						gchar *cut = g_strndup(str, 100);
						g_warning("sc_html_get_tag(): syntax error in tag: '%s%s'",
								  cut, strlen(str)>100?"...":".");
						g_free(cut);
					}
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
		attr = g_new(SC_HTMLAttr, 1);
		attr->name = g_utf8_strdown(attr_name, -1);
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

static void decode_href(SC_HTMLParser *parser)
{
	gchar *tmp;
	SC_HTMLParser *tparser = g_new0(SC_HTMLParser, 1);

	tparser->str = g_string_new(NULL);
	tparser->buf = g_string_new(parser->href);
	tparser->bufp = tparser->buf->str;

	tmp = sc_html_parse(tparser);
	
	g_free(parser->href);
	parser->href = g_strdup(tmp);

	sc_html_parser_destroy(tparser);
}

static SC_HTMLState sc_html_parse_tag(SC_HTMLParser *parser)
{
	gchar buf[SC_HTMLBUFSIZE];
	SC_HTMLTag *tag;

	sc_html_get_parenthesis(parser, buf, sizeof(buf));

	tag = sc_html_get_tag(buf);

	parser->state = SC_HTML_UNKNOWN;
	if (!tag) return SC_HTML_UNKNOWN;

	if (!strcmp(tag->name, "br") || !strcmp(tag->name, "br/")) {
		parser->space = FALSE;
		sc_html_append_char(parser, '\n');
		parser->state = SC_HTML_BR;
	} else if (!strcmp(tag->name, "a")) {
		GList *cur;
		if (parser->href != NULL) {
			g_free(parser->href);
			parser->href = NULL;
		}
		for (cur = tag->attr; cur != NULL; cur = cur->next) {
			if (cur->data && !strcmp(((SC_HTMLAttr *)cur->data)->name, "href")) {
				g_free(parser->href);
				parser->href = g_strdup(((SC_HTMLAttr *)cur->data)->value);
				decode_href(parser);
				parser->state = SC_HTML_HREF_BEG;
				break;
			}
		}
		if (parser->href == NULL)
			parser->href = g_strdup("");
		parser->state = SC_HTML_HREF_BEG;
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
		sc_html_append_str(parser, HR_STR, -1);
		sc_html_append_char(parser, '\n');
		parser->state = SC_HTML_HR;
	} else if (!strcmp(tag->name, "div")    ||
		   !strcmp(tag->name, "ul")     ||
		   !strcmp(tag->name, "li")     ||
		   !strcmp(tag->name, "table")  ||
		   !strcmp(tag->name, "dd")     ||
		   !strcmp(tag->name, "tr")) {
		if (!parser->newline) {
			parser->space = FALSE;
			sc_html_append_char(parser, '\n');
		}
		if (!strcmp(tag->name, "li")) {
			sc_html_append_str(parser, LI_STR, -1);
		}
		parser->state = SC_HTML_NORMAL;
	} else if (tag->name[0] == 'h' && g_ascii_isdigit(tag->name[1])) {
		if (!parser->newline) {
			parser->space = FALSE;
			sc_html_append_char(parser, '\n');
		}
		sc_html_append_char(parser, '\n');
	} else if (!strcmp(tag->name, "blockquote")) {
		parser->state = SC_HTML_NORMAL;
		parser->indent++;
	} else if (!strcmp(tag->name, "/blockquote")) {
		parser->state = SC_HTML_NORMAL;
		parser->indent--;
	} else if (!strcmp(tag->name, "/table") ||
		   (tag->name[0] == '/' &&
		    tag->name[1] == 'h' &&
		    g_ascii_isdigit(tag->name[2]))) {
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
	gchar *entity;

	parser->state = SC_HTML_UNKNOWN;
	cm_return_if_fail(*parser->bufp == '&');

	entity = entity_decode(parser->bufp);
	if (entity != NULL) {
		sc_html_append_str(parser, entity, -1);
		g_free(entity);
		while (*parser->bufp++ != ';');
	} else {
		/* output literal `&' */
		sc_html_append_char(parser, *parser->bufp++);
	}
	parser->state = SC_HTML_NORMAL;
}

static gchar *sc_html_find_tag(SC_HTMLParser *parser, const gchar *tag)
{
	gchar *cur = parser->bufp;
	gint len = strlen(tag);

	if (cur == NULL)
		return NULL;

	while ((cur = strstr(cur, "<")) != NULL) {
		if (!g_ascii_strncasecmp(cur, tag, len))
			return cur;
		cur += 2;
	}
	return NULL;
}

static void sc_html_get_parenthesis(SC_HTMLParser *parser, gchar *buf, gint len)
{
	gchar *p;

	buf[0] = '\0';
	cm_return_if_fail(*parser->bufp == '<');

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
		while ((p = sc_html_find_tag(parser, "</style>")) == NULL)
			if (sc_html_read_line(parser) == SC_HTML_EOF) return;
		parser->bufp = p + 8;
		return;
	}
	if (!g_ascii_strncasecmp(parser->bufp, "<script", 7)) {
		parser->bufp += 7;
		while ((p = sc_html_find_tag(parser, "</script>")) == NULL)
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
