/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2001 Hiroyuki Yamamoto
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
#include <strings.h>
#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>

#include "intl.h"
#include "procheader.h"
#include "filter.h"
#include "folder.h"
#include "utils.h"

FolderItem *filter_get_dest_folder(GSList *fltlist, const gchar *file)
{
	static FolderItem *dummy = NULL;
	FolderItem *dest_folder = NULL;
	GSList *hlist, *cur;
	Filter *filter;

	g_return_val_if_fail(file != NULL, NULL);
	if (!fltlist) return NULL;

	hlist = procheader_get_header_list_from_file(file);
	if (!hlist) return NULL;

	for (cur = fltlist; cur != NULL; cur = cur->next) {
		filter = (Filter *)cur->data;
		if (filter_match_condition(filter, hlist)) {
			if (filter->action == FLT_NOTRECV) {
				if (!dummy) {
					dummy = folder_item_new(NULL, NULL);
					dummy->path = g_strdup(FILTER_NOT_RECEIVE);
				}
				dest_folder = dummy;
			} else
				dest_folder = folder_find_item_from_identifier
					(filter->dest);
			break;
		}
	}

	procheader_header_list_destroy(hlist);

	return dest_folder;
}

static gboolean strfind(const gchar *haystack, const gchar *needle)
{
	return strstr(haystack, needle) != NULL ? TRUE : FALSE;
}

static gboolean strnotfind(const gchar *haystack, const gchar *needle)
{
	return strstr(haystack, needle) != NULL ? FALSE : TRUE;
}

static gboolean strcasefind(const gchar *haystack, const gchar *needle)
{
	return strcasestr(haystack, needle) != NULL ? TRUE : FALSE;
}

static gboolean strcasenotfind(const gchar *haystack, const gchar *needle)
{
	return strcasestr(haystack, needle) != NULL ? FALSE : TRUE;
}

static gboolean strmatch_regex(const gchar *haystack, const gchar *needle)
{
	gint ret = 0;
	regex_t preg;
	regmatch_t pmatch[1];

	ret = regcomp(&preg, needle, 0);
	if (ret != 0) return FALSE;

	ret = regexec(&preg, haystack, 1, pmatch, 0);
	regfree(&preg);

	if (ret == REG_NOMATCH) return FALSE;

	if (pmatch[0].rm_so != -1)
		return TRUE;
	else
		return FALSE;
}

gboolean filter_match_condition(Filter *filter, GSList *hlist)
{
	Header *header;
	gboolean (*StrFind1)    (const gchar *hs, const gchar *nd);
	gboolean (*StrFind2)    (const gchar *hs, const gchar *nd);

	g_return_val_if_fail(filter->name1 != NULL, FALSE);

	if (FLT_IS_REGEX(filter->flag1))
		StrFind1 = strmatch_regex;
	else if (FLT_IS_CASE_SENS(filter->flag1))
		StrFind1 = FLT_IS_CONTAIN(filter->flag1)
			? strfind : strnotfind;
	else
		StrFind1 = FLT_IS_CONTAIN(filter->flag1)
			? strcasefind : strcasenotfind;

	if (FLT_IS_REGEX(filter->flag2))
		StrFind2 = strmatch_regex;
	if (FLT_IS_CASE_SENS(filter->flag2))
		StrFind2 = FLT_IS_CONTAIN(filter->flag2)
			? strfind : strnotfind;
	else
		StrFind2 = FLT_IS_CONTAIN(filter->flag2)
			? strcasefind : strcasenotfind;

	if (filter->cond == FLT_AND) {
		gboolean match1 = FALSE, match2 = FALSE;

		/* ignore second condition if not set */
		if (!filter->name2) match2 = TRUE;

		for (; hlist != NULL; hlist = hlist->next) {
			header = hlist->data;

			if (!match1 &&
			    procheader_headername_equal(header->name,
							filter->name1)) {
				if (!filter->body1 ||
				    StrFind1(header->body, filter->body1))
					match1 = TRUE;
			}
			if (!match2 &&
			    procheader_headername_equal(header->name,
							 filter->name2)) {
				if (!filter->body2 ||
				    StrFind2(header->body, filter->body2))
					match2 = TRUE;
			}

			if (match1 && match2) return TRUE;
		}
	} else if (filter->cond == FLT_OR) {
		for (; hlist != NULL; hlist = hlist->next) {
			header = hlist->data;

			if (procheader_headername_equal(header->name,
							filter->name1))
				if (!filter->body1 ||
				    StrFind1(header->body, filter->body1))
					return TRUE;
			if (filter->name2 &&
			    procheader_headername_equal(header->name,
							filter->name2))
				if (!filter->body2 ||
				    StrFind2(header->body, filter->body2))
					return TRUE;
		}
	}

	return FALSE;
}

gchar *filter_get_str(Filter *filter)
{
	gchar *str;

	str = g_strdup_printf
		("%s\t%s\t%c\t%s\t%s\t%s\t%d\t%d\t%c",
		 filter->name1, filter->body1 ? filter->body1 : "",
		 filter->name2 ? (filter->cond == FLT_AND ? '&' : '|') : ' ',
		 filter->name2 ? filter->name2 : "",
		 filter->body2 ? filter->body2 : "",
		 filter->dest ? filter->dest : "",
		 (guint)filter->flag1, (guint)filter->flag2,
		 filter->action == FLT_MOVE    ? 'm' :
		 filter->action == FLT_NOTRECV ? 'n' :
		 filter->action == FLT_DELETE  ? 'd' : ' ');

	return str;
}

#define PARSE_ONE_PARAM(p, srcp) \
{ \
	p = strchr(srcp, '\t'); \
	if (!p) return NULL; \
	else \
		*p++ = '\0'; \
}

Filter *filter_read_str(const gchar *str)
{
	Filter *filter;
	gchar *tmp;
	gchar *name1, *body1, *op, *name2, *body2, *dest;
	gchar *flag1 = NULL, *flag2 = NULL, *action = NULL;

	Xstrdup_a(tmp, str, return NULL);

	name1 = tmp;
	PARSE_ONE_PARAM(body1, name1);
	PARSE_ONE_PARAM(op, body1);
	PARSE_ONE_PARAM(name2, op);
	PARSE_ONE_PARAM(body2, name2);
	PARSE_ONE_PARAM(dest, body2);
	if (strchr(dest, '\t')) {
		gchar *p;

		PARSE_ONE_PARAM(flag1, dest);
		PARSE_ONE_PARAM(flag2, flag1);
		PARSE_ONE_PARAM(action, flag2);
		if ((p = strchr(action, '\t'))) *p = '\0';
	}

	filter = g_new0(Filter, 1);
	filter->name1 = *name1 ? g_strdup(name1) : NULL;
	filter->body1 = *body1 ? g_strdup(body1) : NULL;
	filter->name2 = *name2 ? g_strdup(name2) : NULL;
	filter->body2 = *body2 ? g_strdup(body2) : NULL;
	filter->cond = (*op == '|') ? FLT_OR : FLT_AND;
	filter->dest = *dest ? g_strdup(dest) : NULL;

	filter->flag1 = FLT_CONTAIN;
	filter->flag2 = FLT_CONTAIN;
	if (flag1) filter->flag1 = (FilterFlag)strtoul(flag1, NULL, 10);
	if (flag2) filter->flag2 = (FilterFlag)strtoul(flag2, NULL, 10);

	if (!strcmp2(dest, FILTER_NOT_RECEIVE))
		filter->action = FLT_NOTRECV;
	else
		filter->action = FLT_MOVE;
	if (action) {
		switch (*action) {
		case 'm': filter->action = FLT_MOVE;	break;
		case 'n': filter->action = FLT_NOTRECV;	break;
		case 'd': filter->action = FLT_DELETE;	break;
		default:  g_warning("Invalid action: `%c'\n", *action);
		}
	}

	return filter;
}

void filter_free(Filter *filter)
{
	if (!filter) return;

	g_free(filter->name1);
	g_free(filter->body1);

	g_free(filter->name2);
	g_free(filter->body2);

	g_free(filter->dest);

	g_free(filter);
}
