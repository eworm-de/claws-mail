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
#include <glib/gi18n.h>

/* Claws Mail includes */
#include <folder.h>
#include <alertpanel.h>
#include <common/utils.h>

/* Local includes */
#include "rssyl_subscribe.h"
#include "opml_import.h"

gint rssyl_folder_depth(FolderItem *item)
{
	gint i;

	for( i = -1; item != NULL; item = folder_item_parent(item), i++ ) {}
	return i;
}

/* This gets called from the libfeed's OPML parser as a user function for
 * each <outline ...> from the .opml file.
 * It creates a folder or subscribes a feed, while keeping track of the
 * location inside folder hierarchy (using depth and linked list of parent
 * folders up to the root). */
void rssyl_opml_import_func(gchar *title, gchar *url, gint depth, gpointer data)
{
	OPMLImportCtx *ctx = (OPMLImportCtx *)data;
	gchar *tmp = NULL;
	FolderItem *new_item;
	gboolean nulltitle = FALSE;
	gint i = 1;

	debug_print("depth %d, ctx->depth %d\n", depth, ctx->depth);
	while (depth < ctx->depth) {
		/* We've gone up at least one level, need to find correct parent */
		ctx->current = g_slist_delete_link(ctx->current, ctx->current);
		ctx->depth--;
	}

	debug_print("OPML_IMPORT: %s %s (%s)\n",
			(url != NULL ? "feed": "folder"), title, url);

	if( title == NULL ) {
		debug_print("NULL title received, substituting a placeholder title\n");
		title = g_strdup(_("Untitled"));
		nulltitle = TRUE;
	}

	/* If URL is not given, then it's a folder */
	if( url == NULL ) {
		/* Find an unused name for new folder */
		tmp = g_strdup(title);
		while (folder_find_child_item_by_name((FolderItem *)ctx->current->data, tmp)) {
			debug_print("RSSyl: Folder '%s' already exists, trying another name\n",
					title);
			g_free(tmp);
			tmp = g_strdup_printf("%s__%d", title, ++i);
		}

		/* Create the folder */
		new_item = folder_create_folder((FolderItem *)ctx->current->data, tmp);
		if (!new_item) {
			alertpanel_error(_("Can't create the folder '%s'."), tmp);
			g_free(tmp);
		}

		if (nulltitle) {
			g_free(title);
			title = NULL;
		}

		ctx->current = g_slist_prepend(ctx->current, new_item);
		ctx->depth++;
	} else {
		/* We have URL, try to add new feed... */
		new_item = rssyl_subscribe((FolderItem *)ctx->current->data,
				url, RSSYL_SHOW_ERRORS);
		/* ...and rename it if needed */
		if (new_item != NULL && strcmp(title, new_item->name)) {
			if (folder_item_rename(new_item, title) < 0) {
				alertpanel_error(_("Error while subscribing feed\n"
							"%s\n\nFolder name '%s' is not allowed."),
						url, title);
			}
		}
	}
}
