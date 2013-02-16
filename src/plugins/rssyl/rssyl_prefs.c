/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto
 * This file (C) 2005 Andrej Kacian <andrej@kacian.sk>
 *
 * - Plugin preferences
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
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include "common/defs.h"
#include "common/utils.h"
#include "gtk/gtkutils.h"
#include "prefs_gtk.h"

#include "rssyl_prefs.h"

static RSSylPrefsPage rssyl_prefs_page;
RSSylPrefs rssyl_prefs;

static void destroy_rssyl_prefs_page(PrefsPage *page);
static void create_rssyl_prefs_page(PrefsPage *page,
		GtkWindow *window, gpointer data);
static void save_rssyl_prefs(PrefsPage *page);

static PrefParam param[] = {
	{ "refresh_interval", RSSYL_PREF_DEFAULT_REFRESH, &rssyl_prefs.refresh, P_INT,
		NULL, NULL, NULL },
	{ "expired_keep", RSSYL_PREF_DEFAULT_EXPIRED, &rssyl_prefs.expired, P_INT,
		NULL, NULL, NULL },
	{ "refresh_on_startup", FALSE, &rssyl_prefs.refresh_on_startup, P_BOOL,
		NULL, NULL, NULL },
	{ "cookies_path", "", &rssyl_prefs.cookies_path, P_STRING,
		NULL, NULL, NULL },
	{ 0, 0, 0, 0, 0, 0, 0 }
};

void rssyl_prefs_init(void)
{
	static gchar *path[3];
	gchar *rcpath;

	path[0] = _("Plugins");
	path[1] = "RSSyl";		/* We don't need this translated */
	path[2] = NULL;

	prefs_set_default(param);
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	prefs_read_config(param, PREFS_BLOCK_NAME, rcpath, NULL);
	g_free(rcpath);

	rssyl_prefs_page.page.path = path;
	rssyl_prefs_page.page.create_widget = create_rssyl_prefs_page;
	rssyl_prefs_page.page.destroy_widget = destroy_rssyl_prefs_page;
	rssyl_prefs_page.page.save_page = save_rssyl_prefs;
	rssyl_prefs_page.page.weight = 30.0;

	prefs_gtk_register_page((PrefsPage *) &rssyl_prefs_page);
}

void rssyl_prefs_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) &rssyl_prefs_page);
}

static void create_rssyl_prefs_page(PrefsPage *page,
		GtkWindow *window, gpointer data)
{
	RSSylPrefsPage *prefs_page = (RSSylPrefsPage *) page;
	GtkWidget *table;
	GtkWidget *refresh;
	GtkWidget *expired;
	GtkWidget *refresh_on_startup;
	GtkWidget *cookies_path;
	GtkWidget *label;
	GtkObject *refresh_adj, *expired_adj;

	table = gtk_table_new(RSSYL_NUM_PREFS, 2, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), VSPACING_NARROW);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	label = gtk_label_new(_("Default refresh interval in minutes"));
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
			GTK_FILL, 0, 0, 0);

	refresh_adj = gtk_adjustment_new(rssyl_prefs.refresh,
			0, 100000, 1, 10, 0);
	refresh = gtk_spin_button_new(GTK_ADJUSTMENT(refresh_adj), 1, 0);
	gtk_table_attach(GTK_TABLE(table), refresh, 1, 2, 0, 1,
			GTK_FILL, 0, 0, 0);
	CLAWS_SET_TIP(refresh,
			_("Set to 0 to disable automatic refreshing"));

	label = gtk_label_new(_("Default number of expired items to keep"));
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
			GTK_FILL, 0, 0, 0);

	expired_adj = gtk_adjustment_new(rssyl_prefs.expired,
			-1, 100000, 1, 10, 0);
	expired = gtk_spin_button_new(GTK_ADJUSTMENT(expired_adj), 1, 0);
	gtk_table_attach(GTK_TABLE(table), expired, 1, 2, 1, 2,
			GTK_FILL, 0, 0, 0);
	CLAWS_SET_TIP(expired,
			_("Set to -1 to keep expired items"));

	refresh_on_startup = gtk_check_button_new_with_label(
			_("Refresh all feeds on application start"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(refresh_on_startup),
			rssyl_prefs.refresh_on_startup);
	gtk_table_attach(GTK_TABLE(table), refresh_on_startup, 0, 2, 3, 4,
			GTK_FILL | GTK_EXPAND, 0, 0, 0);

	label = gtk_label_new(_("Path to cookies file"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 4, 5,
			GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);

	cookies_path = gtk_entry_new ();
	gtk_entry_set_text(GTK_ENTRY(cookies_path), rssyl_prefs.cookies_path);
	gtk_table_attach(GTK_TABLE(table), cookies_path, 1, 2, 4, 5,
			GTK_FILL | GTK_EXPAND, 0, 0, 0);
	CLAWS_SET_TIP(cookies_path,
			_("Path to Netscape-style cookies.txt file containing your cookies"));

	gtk_widget_show_all(table);

	prefs_page->page.widget = table;
	prefs_page->refresh = refresh;
	prefs_page->expired = expired;
	prefs_page->refresh_on_startup = refresh_on_startup;
	prefs_page->cookies_path = cookies_path;
}

static void destroy_rssyl_prefs_page(PrefsPage *page)
{
	/* Do nothing! */
}

static void save_rssyl_prefs(PrefsPage *page)
{
	RSSylPrefsPage *prefs_page = (RSSylPrefsPage *)page;
	PrefFile *pref_file;
	gchar *rc_file_path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			COMMON_RC, NULL);

	rssyl_prefs.refresh = gtk_spin_button_get_value_as_int(
			GTK_SPIN_BUTTON(prefs_page->refresh));
	rssyl_prefs.expired = gtk_spin_button_get_value_as_int(
			GTK_SPIN_BUTTON(prefs_page->expired));
	rssyl_prefs.refresh_on_startup = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(prefs_page->refresh_on_startup));
	g_free(rssyl_prefs.cookies_path);
	rssyl_prefs.cookies_path = g_strdup(gtk_entry_get_text(
			GTK_ENTRY(prefs_page->cookies_path)));

	pref_file = prefs_write_open(rc_file_path);
	g_free(rc_file_path);

	if( !pref_file || prefs_set_block_label(pref_file, PREFS_BLOCK_NAME) < 0 )
				return;

	if( prefs_write_param(param, pref_file->fp) < 0 ) {
		g_warning("Failed to write RSSyl plugin configuration\n");
		prefs_file_close_revert(pref_file);
		return;
	}

        if (fprintf(pref_file->fp, "\n") < 0) {
		FILE_OP_ERROR(rc_file_path, "fprintf");
		prefs_file_close_revert(pref_file);
	} else
	        prefs_file_close(pref_file);
}

RSSylPrefs *rssyl_prefs_get(void)
{
	return &rssyl_prefs;
}
