/*
 * Copyright (C) 2006 Andrej Kacian <andrej@kacian.sk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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
#include <procheader.h>
#include <prefs_common.h>
#include <mainwindow.h>

/* Local includes */
#include "libfeed/feeditem.h"
#include "libfeed/date.h"
#include "rssyl.h"
#include "rssyl_feed.h"
#include "rssyl_prefs.h"
#include "rssyl_update_feed.h"
#include "strutils.h"

FolderItem *rssyl_feed_subscribe_new(FolderItem *parent, const gchar *url,
		gboolean verbose)
{
	gchar *myurl = NULL, *tmpname = NULL;
	FolderItem *new_item = NULL;
	RFolderItem *ritem = NULL;
	gboolean success = FALSE;

	g_return_val_if_fail(parent != NULL, FALSE);
	g_return_val_if_fail(url != NULL, FALSE);

	log_print(LOG_PROTOCOL, RSSYL_LOG_SUBSCRIBING, url);

	if( !strncmp(url, "feed://", 7) )
		myurl = g_strdup(url+7);
	else if( !strncmp(url, "feed:", 5) )
		myurl = g_strdup(url+5);
	else
		myurl = g_strdup(url);

	myurl = g_strchomp(myurl);

	folderview_freeze(mainwindow_get_mainwindow()->folderview);
	folder_item_update_freeze();

	/* Create a feed folder with generic name. */
	tmpname = g_strdup_printf("%s.%ld", RSSYL_NEW_FOLDER_NAME, (long int)time(NULL));
	new_item = folder_create_folder(parent, tmpname);
	g_free(tmpname);
	if( !new_item ) {
		if( verbose )
			alertpanel_error(_("Couldn't create folder for new feed '%s'."),
						myurl);
		g_free(myurl);
		return NULL;
	}

	/* Set it up as a RSSyl folder */
	ritem = (RFolderItem *)new_item;
	ritem->url = g_strdup(myurl);

	/* Try to update it, delete if failed.
	 * (it is renamed in rssyl_update_feed(). */
	if( (success = rssyl_update_feed(ritem, verbose)) == FALSE )
		new_item->folder->klass->remove_folder(new_item->folder, new_item);
	else {
		folder_item_scan(new_item);
		folder_write_list();
	}

	folder_item_update_thaw();
	folderview_thaw(mainwindow_get_mainwindow()->folderview);

	if( success )
		log_print(LOG_PROTOCOL, RSSYL_LOG_SUBSCRIBED, ritem->official_title,
				ritem->url);
	else {
		debug_print("RSSyl: Failed to add feed '%s'\n", myurl);
		g_free(myurl);
		return NULL;
	}

	return new_item;
}

MsgInfo *rssyl_feed_parse_item_to_msginfo(gchar *file, MsgFlags flags,
		gboolean a, gboolean b, FolderItem *item)
{
	MsgInfo *msginfo;

	g_return_val_if_fail(item != NULL, NULL);

	msginfo = procheader_parse_file(file, flags, a, b);
	if (msginfo)
		msginfo->folder = item;

	return msginfo;
}

gboolean rssyl_refresh_timeout_cb(gpointer data)
{
	RRefreshCtx *ctx = (RRefreshCtx *)data;
	time_t tt = time(NULL);
	gchar *tmpdate = NULL;

	g_return_val_if_fail(ctx != NULL, FALSE);

	if( prefs_common_get_prefs()->work_offline)
		return TRUE;

	if( ctx->ritem == NULL || ctx->ritem->url == NULL ) {
		debug_print("RSSyl: refresh_timeout_cb - ritem or url NULL\n");
		g_free(ctx);
		return FALSE;
	}

	if( ctx->id != ctx->ritem->refresh_id ) {
		tmpdate = createRFC822Date(&tt);
		debug_print("RSSyl: %s: timeout id changed, stopping: %d != %d\n",
				tmpdate, ctx->id, ctx->ritem->refresh_id);
		g_free(tmpdate);
		g_free(ctx);
		return FALSE;
	}

	tmpdate = createRFC822Date(&tt);
	debug_print(" %s: refresh %s (%d)\n", tmpdate, ctx->ritem->url,
			ctx->ritem->refresh_id);
	g_free(tmpdate);
	rssyl_update_feed(ctx->ritem, FALSE);

	return TRUE;
}

void rssyl_feed_start_refresh_timeout(RFolderItem *ritem)
{
	RRefreshCtx *ctx;
	guint source_id;
	RPrefs *rsprefs = NULL;

	g_return_if_fail(ritem != NULL);

	if( ritem->default_refresh_interval ) {
		rsprefs = rssyl_prefs_get();
		if( !rsprefs->refresh_enabled )
			return;
		ritem->refresh_interval = rsprefs->refresh;
	}

	ctx = g_new0(RRefreshCtx, 1);
	ctx->ritem = ritem;

	source_id = g_timeout_add(ritem->refresh_interval * 60 * 1000,
			(GSourceFunc)rssyl_refresh_timeout_cb, ctx );
	ritem->refresh_id = source_id;
	ctx->id = source_id;

	debug_print("RSSyl: start_refresh_timeout - %d min (id %d)\n",
			ritem->refresh_interval, ctx->id);
}
