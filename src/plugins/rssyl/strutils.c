/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2005-2023 the Claws Mail Team and Andrej Kacian <andrej@kacian.sk>
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

/* Global includes */
#include <glib.h>
#include <stdlib.h>
#include <ctype.h>

/* Claws Mail includes */
#include <common/utils.h>
#include <entity.h>

/* Local includes */
/* (shouldn't be any) */

gchar *rssyl_strreplace(gchar *source, gchar *pattern,
		gchar *replacement)
{
	gchar *new, *w_new = NULL, *c;
	guint count = 0, final_length;
	size_t len_pattern, len_replacement;

	/*
	debug_print("RSSyl: ======= strreplace: '%s': '%s'->'%s'\n", source, pattern,
			replacement);
	*/

	g_return_val_if_fail(source != NULL, g_strdup(source));
	g_return_val_if_fail(pattern != NULL, g_strdup(source));

	g_return_val_if_fail(g_utf8_validate(source, -1, NULL), g_strdup(source));
	g_return_val_if_fail(g_utf8_validate(pattern, -1, NULL), g_strdup(source));

	len_pattern = strlen(pattern);
	len_replacement = strlen(replacement);

	c = source;
	while( ( c = g_strstr_len(c, strlen(c), pattern) ) ) {
		count++;
		c += len_pattern;
	}

	/*
	debug_print("RSSyl: ==== count = %d\n", count);
	*/

	final_length = strlen(source)
		- ( count * len_pattern )
		+ ( count * len_replacement );

	new = g_malloc(final_length + 1);
	memset(new, '\0', final_length + 1);

	/* 'c' will be our iterator over original string
	 * 'w_new' our iterator over the new string */
	c = source;
	w_new = new;

	/* Go until either end of string is reached, or until the
	 * remaining text is shorter than the pattern. */
	while( *c != '\0' && strlen(c) >= len_pattern) {
		if( !memcmp(c, pattern, len_pattern) ) {
			int i;
			for (i = 0; i < len_replacement; i++) {
				*w_new = replacement[i];
				w_new++;
			}
			c = c + len_pattern;
		} else {
			*w_new = *c;
			w_new++;
			c++;
		}
	}

	/* We broke off the above cycle because remaining text was not
	 * long enough for the pattern, so now we need to append the
	 * remaining text to the new string. */
	if (*c != '\0') {
		strncat(new, c, final_length - strlen(new));
	}

	return new;
}

typedef struct _RSSyl_HTMLSymbol RSSyl_HTMLSymbol;
struct _RSSyl_HTMLSymbol
{
	gchar *const key;
	gchar *const val;
};

static RSSyl_HTMLSymbol tag_list[] = {
	{ "<cite>", "\"" },
	{ "</cite>", "\"" },
	{ "<i>", "" },
	{ "</i>", "" },
	{ "<em>", "" },
	{ "</em>", "" },
	{ "<b>", "" },
	{ "</b>", "" },
	{ "<nobr>", "" },
	{ "</nobr>", "" },
	{ "<wbr>", "" },
	{ "<sub>", "" },
	{ "</sub>", "" },
	{ NULL, NULL }
};

static gchar *rssyl_replace_chrefs(gchar *string)
{
	char *new = g_malloc0(strlen(string) + 1), *ret;
	gchar *entity;
	int i, ii;

	/* &xx; */
	ii = 0;
	for (i = 0; i < strlen(string); ++i) {
		if (string[i] == '&') {
			entity = entity_decode(&(string[i]));
			if (entity != NULL) {
				g_strlcat(new, entity, strlen(string));
				ii += strlen(entity);
				g_free(entity);
				entity = NULL;
				while (string[++i] != ';');
				--i; /* loop will inc it again */
			} else {
				new[ii++] = string[i];
			}
		} else {
			new[ii++] = string[i];
		}
	}

	ret = g_strdup(new);
	g_free(new);
	return ret;
}

gchar *rssyl_replace_html_stuff(gchar *text,
		gboolean symbols, gboolean tags)
{
	gchar *tmp = NULL, *wtext = NULL;
	gint i;

	g_return_val_if_fail(text != NULL, NULL);

	if( symbols ) {
		wtext = rssyl_replace_chrefs(text);
	} else {
		wtext = g_strdup(text);
	}

	/* TODO: rewrite this part to work similarly to rssyl_replace_chrefs() */
	if( tags ) {
		for( i = 0; tag_list[i].key != NULL; i++ ) {
			if( g_strstr_len(text, strlen(text), tag_list[i].key) ) {
				tmp = rssyl_strreplace(wtext, tag_list[i].key, tag_list[i].val);
				g_free(wtext);
				wtext = tmp;
			}
		}
	}

	return wtext;
}

static gchar *rssyl_sanitize_string(gchar *str, gboolean strip_nl)
{
	gchar *new = NULL, *c = str, *n = NULL;

	if( str == NULL )
		return NULL;

	n = new = g_malloc(strlen(str) + 1);
	memset(new, '\0', strlen(str) + 1);

	while( *c != '\0' ) {
		if( !isspace(*c) || *c == ' ' || (!strip_nl && *c == '\n') ) {
			*n = *c;
			n++;
		}
		c++;
	}

	return new;
}

/* rssyl_format_string()
 * - return value needs to be freed
 */
gchar *rssyl_format_string(gchar *str, gboolean replace_html,
		gboolean strip_nl)
{
	gchar *res = NULL, *tmp = NULL;

	g_return_val_if_fail(str != NULL, NULL);

	if (replace_html)
		tmp = rssyl_replace_html_stuff(str, TRUE, TRUE);
	else
		tmp = g_strdup(str);

	res = rssyl_sanitize_string(tmp, strip_nl);
	g_free(tmp);

	g_strstrip(res);

	return res;
}

/* this functions splits a string into an array of string, by 
 * returning an array of pointers to positions of the delimiter
 * in the original string and replacing this delimiter with a
 * NULL. It does not duplicate memory, hence you should only
 * free the array and not its elements, and you should not
 * free the original string before you're done with the array.
 * maybe could be part of the core (utils.c).
 */
gchar **strsplit_no_copy(gchar *str, char delimiter)
{
	gchar **array = g_new(gchar *, 1);
	int i = 0;
	gchar *cur = str, *next;
	
	array[i] = cur;
	i++;
	while ((next = strchr(cur, delimiter)) != NULL) {
		*(next) = '\0';
		array = g_realloc(array, (sizeof(gchar *)) * (i + 1));
		array[i] = next + 1;
		cur = next + 1;
		i++;
	}
	array = g_realloc(array, (sizeof(gchar *)) * (i + 1));
	array[i] = NULL;
	return array;
}

/* This is a very dumb function - it just strips <, > and everything between
 * them. */
void strip_html(gchar *str)
{
	gchar *p = str;
	gboolean intag = FALSE;

	while (*p) {
		if (*p == '<')
			intag = TRUE;
		else if (*p == '>')
			intag = FALSE;

		if (*p == '<' || *p == '>' || intag)
			memmove(p, p + 1, strlen(p));
		else
			p++;
	}
}

gchar *my_normalize_url(const gchar *url)
{
	gchar *myurl = NULL;

	if (!strncmp(url, "feed://", 7))
		myurl = g_strdup(url+7);
	else if (!strncmp(url, "feed:", 5))
		myurl = g_strdup(url+5);
	else
		myurl = g_strdup(url);

	return g_strstrip(myurl);
}
