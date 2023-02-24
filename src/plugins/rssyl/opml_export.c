/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2008-2023 the Claws Mail Team and Andrej Kacian <andrej@kacian.sk>
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
#include <errno.h>

/* Claws Mail includes */
#include <log.h>
#include <folder.h>
#include <common/utils.h>
#include <file-utils.h>

/* Local includes */
#include "libfeed/date.h"
#include "rssyl.h"
#include "opml_import.h"
#include "strutils.h"

#define RSSYL_OPML_FILE	"rssyl-feedlist.opml"

struct _RSSylOpmlCtx {
	FILE *f;
	gint depth;
};

typedef struct _RSSylOpmlCtx RSSylOpmlCtx;

static void rssyl_opml_export_func(FolderItem *item, gpointer data)
{
	RSSylOpmlCtx *ctx = (RSSylOpmlCtx *)data;
	RFolderItem *ritem = (RFolderItem *)item;
	gboolean isfolder = FALSE, err = FALSE;
	gboolean haschildren = FALSE;
	gchar *indent = NULL, *xmlurl = NULL;
	gchar *tmpoffn = NULL, *tmpurl = NULL, *tmpname = NULL;
	gint depth;

	if( !IS_RSSYL_FOLDER_ITEM(item) )
		return;

	if( folder_item_parent(item) == NULL )
		return;

	/* Check for depth and adjust indentation */
	depth = rssyl_folder_depth(item);
	if( depth < ctx->depth ) {
		for( ctx->depth--; depth <= ctx->depth; ctx->depth-- ) {
			indent = g_strnfill(ctx->depth + 1, '\t');
			err |= (fprintf(ctx->f, "%s</outline>\n", indent) < 0);
			g_free(indent);
		}
	}
	ctx->depth = depth;

	if( ritem->url == NULL ) {
		isfolder = TRUE;
	} else {
		tmpurl = rssyl_strreplace(ritem->url, "&", "&amp;");
		xmlurl = g_strdup_printf("xmlUrl=\"%s\"", tmpurl);
		g_free(tmpurl);
	}

	if( g_node_n_children(item->node) )
		haschildren = TRUE;

	indent = g_strnfill(ctx->depth + 1, '\t');

	tmpname = rssyl_strreplace(item->name, "&", "&amp;");
	 if( ritem->official_title != NULL )
		 tmpoffn = rssyl_strreplace(ritem->official_title, "&", "&amp;");
	 else
		 tmpoffn = g_strdup(tmpname);

	err |= (fprintf(ctx->f,
				"%s<outline title=\"%s\" text=\"%s\" description=\"%s\" type=\"%s\" %s%s>\n",
				indent, tmpname, tmpoffn, tmpoffn,
				(isfolder ? "folder" : "rss"),
				(xmlurl ? xmlurl : ""), (haschildren ? "" : "/")) < 0);

	g_free(indent);
	g_free(xmlurl);
	g_free(tmpname);
	g_free(tmpoffn);
	
	if( err ) {
		log_warning(LOG_PROTOCOL,
				_("RSSyl: Error while writing '%s' to feed export list.\n"),
				item->name);
		debug_print("Error while writing '%s' to feed_export list.\n",
				item->name);
	}
}

void rssyl_opml_export(void)
{
	FILE *f;
	gchar *opmlfile, *tmp;
	time_t tt = time(NULL);
	RSSylOpmlCtx *ctx = NULL;
	gboolean err = FALSE;

	opmlfile = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, RSSYL_DIR,
			G_DIR_SEPARATOR_S, RSSYL_OPML_FILE, NULL);

	if( g_file_test(opmlfile, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_REGULAR ) ) {
		if (g_remove(opmlfile) != 0) {
			log_warning(LOG_PROTOCOL,
					_("RSSyl: Couldn't delete old OPML file '%s': %s\n"),
					opmlfile, g_strerror(errno));
			debug_print("RSSyl: Couldn't delete old file '%s'\n", opmlfile);
			g_free(opmlfile);
			return;
		}
	}
	
	if( (f = claws_fopen(opmlfile, "w")) == NULL ) {
		log_warning(LOG_PROTOCOL,
				_("RSSyl: Couldn't open file '%s' for feed list exporting: %s\n"),
				opmlfile, g_strerror(errno));
		debug_print("RSSyl: Couldn't open feed list export file, returning.\n");
		g_free(opmlfile);
		return;
	}

	tmp = createRFC822Date(&tt);

	/* Write OPML header */
	err |= (fprintf(f,
				"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
				"<opml version=\"1.1\">\n"
				"\t<head>\n"
				"\t\t<title>RSSyl Feed List Export</title>\n"
				"\t\t<dateCreated>%s</dateCreated>\n"
				"\t</head>\n"
				"\t<body>\n",
				tmp) < 0);
	g_free(tmp);

	ctx = g_new0(RSSylOpmlCtx, 1);
	ctx->f = f;
	ctx->depth = 1;

	folder_func_to_all_folders(
			(FolderItemFunc)rssyl_opml_export_func, ctx);

	/* Print close all open <outline> tags if needed. */
	while( ctx->depth > 2 ) {
		ctx->depth--;
		tmp = g_strnfill(ctx->depth, '\t');
		err |= (fprintf(f, "%s</outline>\n", tmp) < 0);
		g_free(tmp);
	}

	err |= (fprintf(f,
				"\t</body>\n"
				"</opml>\n") < 0);

	if( err ) {
		log_warning(LOG_PROTOCOL, _("RSSyl: Error during writing feed export file.\n"));
		debug_print("RSSyl: Error during writing feed export file.\n");
	}

	debug_print("RSSyl: Feed export finished.\n");

	claws_safe_fclose(f);
	g_free(opmlfile);
	g_free(ctx);
}
