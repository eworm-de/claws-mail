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
#endif

/* Global includes */
#include <glib.h>
#include <glib/gi18n.h>

/* Claws Mail includes */
#include <common/defs.h>
#include <prefs_gtk.h>
#include <mainwindow.h>

/* Local includes */
#include "rssyl.h"
#include "rssyl_prefs.h"
#include "rssyl_feed.h"

static RPrefsPage rssyl_gtk_prefs_page;
RPrefs rssyl_prefs;

static void destroy_rssyl_prefs_page(PrefsPage *page);
static void create_rssyl_prefs_page(PrefsPage *page,
		GtkWindow *window, gpointer data);
static void save_rssyl_prefs(PrefsPage *page);
static void rssyl_apply_prefs(void);

	static PrefParam param[] = {
		{ "refresh_interval", PREF_DEFAULT_REFRESH, &rssyl_prefs.refresh, P_INT,
			NULL, NULL, NULL },
		{ "refresh_on_startup", "FALSE", &rssyl_prefs.refresh_on_startup, P_BOOL,
			NULL, NULL, NULL },
		{ "refresh_enabled", "TRUE", &rssyl_prefs.refresh_enabled, P_BOOL,
			NULL, NULL, NULL },
		{ "cookies_path", "", &rssyl_prefs.cookies_path, P_STRING,
			NULL, NULL, NULL },
		{ "ssl_verify_peer", "TRUE", &rssyl_prefs.ssl_verify_peer, P_BOOL,
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

	rssyl_gtk_prefs_page.page.path = path;
	rssyl_gtk_prefs_page.page.create_widget = create_rssyl_prefs_page;
	rssyl_gtk_prefs_page.page.destroy_widget = destroy_rssyl_prefs_page;
	rssyl_gtk_prefs_page.page.save_page = save_rssyl_prefs;
	rssyl_gtk_prefs_page.page.weight = 30.0;

	prefs_gtk_register_page((PrefsPage *) &rssyl_gtk_prefs_page);
}

void rssyl_prefs_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) &rssyl_gtk_prefs_page);
}

/* Toggle the refresh timeout spinbutton sensitivity after the
 * checkbutton was toggled. */
static gboolean
rssyl_refresh_enabled_toggled_cb(GtkToggleButton *tb, gpointer data)
{
	gtk_widget_set_sensitive(GTK_WIDGET(data),
			gtk_toggle_button_get_active(tb));
	return FALSE;
}

static void create_rssyl_prefs_page(PrefsPage *page,
		GtkWindow *window, gpointer data)
{
	int row = 0;
	RPrefsPage *prefs_page = (RPrefsPage *) page;
	GtkWidget *table;
	GtkWidget *refresh, *refresh_enabled;
	GtkWidget *label;
	GtkWidget *refresh_on_startup;
	GtkObject *refresh_adj;
	GtkWidget *cookies_path;
	GtkWidget *ssl_verify_peer;
#if !(GTK_CHECK_VERSION(2, 12, 0))
	GtkTooltips *tooltips;

	tooltips = gtk_tooltips_new();
	gtk_tooltips_enable(tooltips);
#endif

	table = gtk_table_new(3, 2, FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(table), 5);
	gtk_table_set_row_spacings(GTK_TABLE(table), VSPACING_NARROW);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	/* Refresh interval */
	refresh_enabled = gtk_check_button_new_with_label(
			_("Default refresh interval in minutes"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(refresh_enabled),
			rssyl_prefs.refresh_enabled);
	gtk_table_attach(GTK_TABLE(table), refresh_enabled, 0, 1, row, row+1,
			GTK_FILL | GTK_EXPAND, 0, 0, 0);

	refresh_adj = gtk_adjustment_new(rssyl_prefs.refresh,
			1, 100000, 1, 10, 0);
	refresh = gtk_spin_button_new(GTK_ADJUSTMENT(refresh_adj), 1, 0);
	gtk_widget_set_sensitive(GTK_WIDGET(refresh), rssyl_prefs.refresh_enabled);
	gtk_table_attach(GTK_TABLE(table), refresh, 1, 2, row, row+1,
			GTK_FILL, 0, 0, 0);

	g_signal_connect(G_OBJECT(refresh_enabled), "toggled",
			G_CALLBACK(rssyl_refresh_enabled_toggled_cb), refresh);

	row++;
	/* Whether to refresh all feeds on CM startup */
	refresh_on_startup = gtk_check_button_new_with_label(
			_("Refresh all feeds on application start"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(refresh_on_startup),
			rssyl_prefs.refresh_on_startup);
	gtk_table_attach(GTK_TABLE(table), refresh_on_startup, 0, 2, row, row+1,
			GTK_FILL | GTK_EXPAND, 0, 0, 0);

	row++;
	/* Path to cookies file for libcurl to use */
	label = gtk_label_new(_("Path to cookies file"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
			GTK_FILL | GTK_EXPAND, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);

	cookies_path = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(cookies_path), rssyl_prefs.cookies_path);
	gtk_table_attach(GTK_TABLE(table), cookies_path, 1, 2, row, row+1,
			GTK_FILL, 0, 0, 0);
#if !(GTK_CHECK_VERSION(2, 12, 0))
	gtk_tooltips_set_tip(tooltips, cookies_path,
			_("Path to Netscape-style cookies.txt file containing your cookies"), NULL);
#else
	gtk_widget_set_tooltip_text(cookies_path,
			_("Path to Netscape-style cookies.txt file containing your cookies"));
#endif

	row++;

	/* Whether to verify SSL peer certificate */
	ssl_verify_peer = gtk_check_button_new_with_label(
			_("Verify SSL certificates validity for new feeds"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ssl_verify_peer),
			rssyl_prefs.ssl_verify_peer);
	gtk_table_attach(GTK_TABLE(table), ssl_verify_peer, 0, 2, row, row+1,
			GTK_FILL | GTK_EXPAND, 0, 0, 0);

	gtk_widget_show_all(table);

	/* Store pointers to relevant widgets */
	prefs_page->page.widget = table;
	prefs_page->refresh_enabled = refresh_enabled;
	prefs_page->refresh = refresh;
	prefs_page->refresh_on_startup = refresh_on_startup;
	prefs_page->cookies_path = cookies_path;
	prefs_page->ssl_verify_peer = ssl_verify_peer;
}

static void destroy_rssyl_prefs_page(PrefsPage *page)
{
	/* Do nothing! */
}

static void save_rssyl_prefs(PrefsPage *page)
{
	RPrefsPage *prefs_page = (RPrefsPage *)page;
	PrefFile *pref_file;
	gchar *rc_file_path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			COMMON_RC, NULL);

	/* Grab values from GTK widgets */
	rssyl_prefs.refresh_enabled = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(prefs_page->refresh_enabled));
	rssyl_prefs.refresh = gtk_spin_button_get_value_as_int(
			GTK_SPIN_BUTTON(prefs_page->refresh));
	rssyl_prefs.refresh_on_startup = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(prefs_page->refresh_on_startup));
	g_free(rssyl_prefs.cookies_path);
	rssyl_prefs.cookies_path = g_strdup(gtk_entry_get_text(
				GTK_ENTRY(prefs_page->cookies_path)));
	rssyl_prefs.ssl_verify_peer = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(prefs_page->ssl_verify_peer));

	/* Store prefs in rc file */
	pref_file = prefs_write_open(rc_file_path);
	g_free(rc_file_path);

	if( !pref_file || prefs_set_block_label(pref_file, PREFS_BLOCK_NAME) < 0 )
				return;

	if( prefs_write_param(param, pref_file->fp) < 0 ) {
		g_warning("Failed to write RSSyl plugin configuration\n");
		prefs_file_close_revert(pref_file);
		return;
	}

	fprintf(pref_file->fp, "\n");
	prefs_file_close(pref_file);

	rssyl_apply_prefs();
}

RPrefs *rssyl_prefs_get(void)
{
	return &rssyl_prefs;
}

static void rssyl_start_default_refresh_timeouts_func(FolderItem *item,
		gpointer data)
{
	RFolderItem *ritem = (RFolderItem *)item;
	guint prefs_interval = GPOINTER_TO_UINT(data);

	if( !IS_RSSYL_FOLDER_ITEM(item) )
		return;

	if( folder_item_parent(item) == NULL || ritem->url == NULL )
		return;

	/* Feeds which use default refresh interval */
	if( ritem->default_refresh_interval ) {
		/* Start a new timer if the default value has changed
		 * (ritem->refresh_interval should contain previous default
		 * value in this case). */
		if( ritem->refresh_interval != prefs_interval ) {
			ritem->refresh_interval = prefs_interval;
			rssyl_feed_start_refresh_timeout(ritem);
		}
	}
}

static void rssyl_start_default_refresh_timeouts(void)
{
	RPrefs *rsprefs = rssyl_prefs_get();

	folder_func_to_all_folders(
			(FolderItemFunc)rssyl_start_default_refresh_timeouts_func,
			GUINT_TO_POINTER(rsprefs->refresh));
}

/* rssyl_apply_prefs():
 * Do what's needed to start using newly set preferences */
static void rssyl_apply_prefs(void)
{
	/* Update refresh timeouts for feeds which use default interval. */
	rssyl_start_default_refresh_timeouts();

	/* Nothing else here, so far... */
}
