/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2002 Thorsten Maerz
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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "defs.h"
#include "utils.h"
#include "w32_mailcap.h"

/* ------------------------------------------------------------------------- */

#define W32_MC_BUFSIZE	4095

typedef struct {
	gchar *extension;
	gchar *commandline;
} w32_mailcap_entry ;

w32_mailcap_entry	*mc_entry;
GSList			*w32_mailcap_list ;

gint w32_read_mailcap(GSList *mailcap_list, gchar *filename);
void w32_mailcap_key_destroy_func(gpointer data);
void w32_mailcap_value_destroy_func(gpointer data);

/* ------------------------------------------------------------------------- */

void w32_mailcap_free_cb(w32_mailcap_entry *entry, gpointer user_data)
{
	g_free(entry->commandline);
	g_free(entry->extension);
	g_free(entry);
}

gint w32_mailcap_list_search(const w32_mailcap_entry *haystack,
	const gchar *needle)
{
	int res=g_pattern_match_simple(haystack->extension, needle);
	if (res==TRUE)
		return 0;
	else
		return 1;
}

/* ------------------------------------------------------------------------- */

/* insert tab separated key/value pairs from "filename" into "mailcap_list"
 * returns number of read keys or negative values on error
 */
gint w32_read_mailcap(GSList *mailcap_list, gchar *filename)
{
	gint nkeys=0;
	gchar buf[W32_MC_BUFSIZE];
	FILE *mailcap;

	if ( !(mailcap=fopen(filename,"rb")) )
		return W32_MC_ERR_READ_MAILCAP ;

	while ( fgets(buf,W32_MC_BUFSIZE,mailcap) )
	{
		size_t n;
		gchar *key, *val;
		gchar *last_tab, *first_tab;

		/* skip comments */
		if ( buf[0]=='#' ) continue;

		/* strip trailing lf,cr, tab, space */
		for (n=strlen(buf)-1; n>=0; n--)
		{
			if (buf[n] == '\n'
				|| buf[n] == '\r'
				|| buf[n] == '\t'
				|| buf[n] == ' ')
				buf[n] = 0 ;
			else break;
		}

		/* tab or commandline missing ? */
		if ( (last_tab = strrchr(buf, '\t'))==NULL ) continue;
		/* extension missing ? */
		if ( (gulong)last_tab - (gulong)buf < 1) continue;

		/* extract (lowercase) keys and values */
		mc_entry=g_new0(w32_mailcap_entry, 1);
		val=g_strdup(last_tab + 1);
		/* find first tab */
		for (first_tab=buf;
			(first_tab<last_tab) && (first_tab[0]!='\t');
			first_tab++);

		key=g_strndup(buf,(gulong)first_tab - (gulong)buf);
		locale_to_utf8(&key);
		mc_entry->extension=g_utf8_strdown(key,-1);
		mc_entry->commandline=val;
		w32_mailcap_list = g_slist_append(w32_mailcap_list, mc_entry);
		nkeys++;
		g_free(key);
	}
	fclose(mailcap);
	return nkeys;
}

/* ------------------------------------------------------------------------- */

/* create: returns number of read keys or negative values on error */
gint w32_mailcap_create(void)
{
	gchar *sys_mailcap, *user_mailcap;

	if (w32_mailcap_list)
		return W32_MC_ERR_LIST_EXIST ;

	sys_mailcap = g_strconcat(get_installed_dir(), G_DIR_SEPARATOR_S,
		SYSCONFDIR, G_DIR_SEPARATOR_S, W32_MAILCAP_NAME, NULL);
	user_mailcap = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S,
		W32_MAILCAP_NAME, NULL);

	return w32_read_mailcap(w32_mailcap_list, user_mailcap)
		 + w32_read_mailcap(w32_mailcap_list, sys_mailcap);
}

/* destroy */
void w32_mailcap_free(void)
{
	g_slist_foreach(w32_mailcap_list, w32_mailcap_free_cb, NULL);
	g_slist_free(w32_mailcap_list);
}

/* lookup: returns associated application or NULL if not found or error */
gchar *w32_mailcap_lookup(const gchar *filename)
{
	char drive	[_MAX_DRIVE];
	char dir	[_MAX_DIR];
	char fname	[_MAX_FNAME];
	char ext	[_MAX_EXT];
	GSList *found_entry;
	gchar *ext_utf8, *ext_utf8_lc;
	gchar *result;

	if (!w32_mailcap_list) return NULL;
	if (!filename) return NULL;

	_splitpath( filename, drive, dir, fname, ext );
	if (!ext) return NULL;

	ext_utf8    = g_locale_to_utf8(ext, -1, NULL, NULL, NULL);
	ext_utf8_lc = g_utf8_strdown(ext_utf8,-1);
	found_entry = g_slist_find_custom(w32_mailcap_list, ext_utf8_lc,
			w32_mailcap_list_search);

	if (found_entry && found_entry->data)
		result = ((w32_mailcap_entry*)found_entry->data)->commandline;
	else 
		result = NULL;

	g_free(ext_utf8);
	g_free(ext_utf8_lc);
	return result;
}
