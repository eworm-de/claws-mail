/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto
 * This file (C) 2005 Andrej Kacian <andrej@kacian.sk>
 *
 * - callback handler functions for GTK signals and timeouts
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

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "folderview.h"
#include "alertpanel.h"
#include "gtk/inputdialog.h"

#include "date.h"
#include "feed.h"
#include "rssyl.h"
#include "rssyl_gtk.h"
#include "rssyl_cb_gtk.h"
#include "prefs_common.h"

gboolean rssyl_props_cancel_cb(GtkWidget *widget, gpointer data)
{
	RSSylFolderItem *ritem = (RSSylFolderItem *)data;

	debug_print("RSSyl: Cancel pressed\n");
	gtk_widget_destroy(ritem->feedprop->window);
	return FALSE;
}

gboolean rssyl_props_ok_cb(GtkWidget *widget, gpointer data)
{
	RSSylFolderItem *ritem = (RSSylFolderItem *)data;

	debug_print("RSSyl: OK pressed\n");
	rssyl_gtk_prop_store(ritem);

	gtk_widget_destroy(ritem->feedprop->window);

	return FALSE;
}

gboolean rssyl_refresh_timeout_cb(gpointer data)
{
	RSSylRefreshCtx *ctx = (RSSylRefreshCtx *)data;
	time_t tt = time(NULL);
	gchar *tmpdate;

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
		debug_print(" %s: timeout id changed, stopping: %d != %d\n",
				tmpdate, ctx->id, ctx->ritem->refresh_id);
		g_free(tmpdate);
		g_free(ctx);
		return FALSE;
	}

	tmpdate = createRFC822Date(&tt);
	debug_print(" %s: refresh %s (%d)\n", tmpdate, ctx->ritem->url,
			ctx->ritem->refresh_id);
	g_free(tmpdate);
	rssyl_update_feed(ctx->ritem);

	return TRUE;
}

gboolean rssyl_default_refresh_interval_toggled_cb(GtkToggleButton *default_ri,
		gpointer data)
{
	gboolean active = gtk_toggle_button_get_active(default_ri);
	GtkWidget *refresh_interval = (GtkWidget *)data;

	debug_print("default is %s\n", ( active ? "ON" : "OFF" ) );

	gtk_widget_set_sensitive(refresh_interval, !active);

	return FALSE;
}

gboolean rssyl_default_expired_num_toggled_cb(GtkToggleButton *default_ex,
		gpointer data)
{
	gboolean active = gtk_toggle_button_get_active(default_ex);
	GtkWidget *expired_num = (GtkWidget *)data;

	debug_print("default is %s\n", ( active ? "ON" : "OFF" ) );

	gtk_widget_set_sensitive(expired_num, !active);

	return FALSE;
}

gboolean rssyl_fetch_comments_toggled_cb(GtkToggleButton *fetch_comments,
		gpointer data)
{
	gboolean active = gtk_toggle_button_get_active(fetch_comments);
	GtkWidget *num_comments = (GtkWidget *)data;

	debug_print("fetch comments is %s\n", (active ? "ON" : "OFF") );

	gtk_widget_set_sensitive(num_comments, active);

	return FALSE;
}

gboolean rssyl_props_key_press_cb(GtkWidget *widget, GdkEventKey *event,
		gpointer data)
{
	if (event) {
		switch (event->keyval) {
			case GDK_Escape:
				rssyl_props_cancel_cb(widget, data);
				return TRUE;
			case GDK_Return:
			case GDK_KP_Enter:
				rssyl_props_ok_cb(widget, data);
				return TRUE;
			default:
				break;
		}
	}

	return FALSE;
}
