/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2013-2022 the Claws Mail team
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "file-utils.h"
#include "utils.h"
#include "alertpanel.h"
#include "folder.h"

static gboolean verify_folderlist_xml();

gboolean check_file_integrity()
{
	if (verify_folderlist_xml() != TRUE)
		return FALSE;

	return TRUE;
}

static gboolean verify_folderlist_xml()
{
	GNode *node;
	static gchar *filename = NULL;
	static gchar *bak = NULL;
	time_t date;
	struct tm *ts;
	gchar buf[BUFFSIZE];
	gboolean fileexists, bakexists;

	filename = folder_get_list_path();

	fileexists = is_file_exist(filename);

	bak = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
 			  FOLDER_LIST, ".bak", NULL);
	bakexists = is_file_exist(bak);
	
	if (bakexists) {
		date = get_file_mtime(bak);
		ts = localtime(&date);
		strftime(buf, sizeof(buf), "%a %d-%b-%Y %H:%M %Z", ts);
	}
	
	if (!fileexists && bakexists) {
		AlertValue aval;
		gchar *msg;

		msg = g_strdup_printf
			(_("The file %s is missing! "
			   "Do you want to use the backup file from %s?"), FOLDER_LIST,buf);
		aval = alertpanel(_("Warning"), msg, NULL, _("_No"), NULL, _("_Yes"),
				  NULL, NULL, ALERTFOCUS_FIRST);
		g_free(msg);
		if (aval != G_ALERTALTERNATE)
			return FALSE;
		else {
			if (copy_file(bak,filename,FALSE) < 0) {
				alertpanel_warning(_("Could not copy %s to %s"),bak,filename);
				return FALSE;
			}
			g_free(bak);
			return TRUE;
		}
	}

	if (fileexists) {
		node = xml_parse_file(filename);
		if (!node && is_file_exist(bak)) {
			AlertValue aval;
			gchar *msg;

			msg = g_strdup_printf
				(_("The file %s is empty or corrupted! "
				   "Do you want to use the backup file from %s?"), FOLDER_LIST,buf);
			aval = alertpanel(_("Warning"), msg, NULL, _("_No"), NULL, _("_Yes"),
					  NULL, NULL, ALERTFOCUS_FIRST);
			g_free(msg);
			if (aval != G_ALERTALTERNATE)
				return FALSE;
			else {
				if (copy_file(bak,filename,FALSE) < 0) {
					alertpanel_warning(_("Could not copy %s to %s"),bak,filename);
					return FALSE;
				}
				g_free(bak);
				return TRUE;
			}
		}
		xml_free_tree(node);
  	}

	return TRUE;
}
