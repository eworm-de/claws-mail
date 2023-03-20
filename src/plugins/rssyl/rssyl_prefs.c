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
#include <common/defs.h>
#include <prefs_gtk.h>
#include <mainwindow.h>
#include <gtk/filesel.h>

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
	{ "refresh_interval", PREF_DEFAULT_REFRESH, &rssyl_prefs.refresh,
		P_INT, NULL, NULL, NULL },
	{ "refresh_on_startup", "FALSE", &rssyl_prefs.refresh_on_startup,
		P_BOOL,	NULL, NULL, NULL },
	{ "refresh_enabled", "TRUE", &rssyl_prefs.refresh_enabled,
		P_BOOL,	NULL, NULL, NULL },
	{ "cookies_path", "", &rssyl_prefs.cookies_path,
		P_STRING, NULL, NULL, NULL },
	{ "ssl_verify_peer", "TRUE", &rssyl_prefs.ssl_verify_peer,
		P_BOOL,	NULL, NULL, NULL },
	{ 0, 0, 0, 0, 0, 0, 0 }
};

void rssyl_prefs_init(void)
{
	static gchar *path[3];
	gchar *rcpath;

	path[0] = _("Plugins");
	path[1] = PLUGIN_NAME;		/* We don't need this translated */
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

/* Open a file select dialog and set file path to cookies entry */
static void
rssyl_prefs_cookies_browse_cb(GtkWidget* widget, gpointer data)
{
	gchar *filename;
	gchar *utf8_filename;
	GtkEntry *dest = GTK_ENTRY(data);

	filename = filesel_select_file_open(_("Select cookies file"), NULL);
	if (!filename) return;

	utf8_filename = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
	if (!utf8_filename) {
		g_warning("rssyl_prefs_cookies_browse_cb(): failed to convert character set");
		utf8_filename = g_strdup(filename);
	}
	gtk_entry_set_text(GTK_ENTRY(dest), utf8_filename);
	g_free(utf8_filename);
}

static void create_rssyl_prefs_page(PrefsPage *page,
		GtkWindow *window, gpointer data)
{
	RPrefsPage *prefs_page = (RPrefsPage *) page;
	GtkWidget *vbox, *vbox1, *vbox2;
	GtkWidget *frame;
	GtkWidget *refresh, *refresh_enabled, *refresh_hbox;
	GtkWidget *label;
	GtkWidget *refresh_on_startup;
	GtkAdjustment *refresh_adj;
	GtkWidget *cookies_path, *cookies_btn, *cookies_hbox;
	GtkWidget *ssl_verify_peer;

	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

	/* Refresh interval */
	refresh_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	refresh_enabled = gtk_check_button_new_with_label(
			_("Default refresh interval"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(refresh_enabled),
			rssyl_prefs.refresh_enabled);
	gtk_box_pack_start(GTK_BOX(refresh_hbox), refresh_enabled, FALSE, FALSE, 0);

	refresh_adj = gtk_adjustment_new(rssyl_prefs.refresh,
			1, 100000, 1, 10, 0);
	refresh = gtk_spin_button_new(GTK_ADJUSTMENT(refresh_adj), 1, 0);
	gtk_widget_set_sensitive(GTK_WIDGET(refresh), rssyl_prefs.refresh_enabled);
	g_signal_connect(G_OBJECT(refresh_enabled), "toggled",
			G_CALLBACK(rssyl_refresh_enabled_toggled_cb), refresh);
	gtk_box_pack_start(GTK_BOX(refresh_hbox), refresh, FALSE, FALSE, 0);

	label = gtk_label_new(_("minutes"));
	gtk_box_pack_start(GTK_BOX(refresh_hbox), label, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(vbox1), refresh_hbox, FALSE, FALSE, 0);

	/* Whether to refresh all feeds on CM startup */
	refresh_on_startup = gtk_check_button_new_with_label(
			_("Refresh all feeds on start-up"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(refresh_on_startup),
			rssyl_prefs.refresh_on_startup);
	gtk_box_pack_start(GTK_BOX(vbox1), refresh_on_startup, FALSE, FALSE, 0);

	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);

	/* Whether to verify SSL peer certificate */
	ssl_verify_peer = gtk_check_button_new_with_label(
			_("Verify TLS certificates validity for new feeds"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(ssl_verify_peer),
			rssyl_prefs.ssl_verify_peer);
	gtk_box_pack_start(GTK_BOX(vbox2), ssl_verify_peer, FALSE, FALSE, 0);

	/* Path to cookies file for libcurl to use */
	cookies_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	label = gtk_label_new(_("Path to cookies file"));
	gtk_box_pack_start(GTK_BOX(cookies_hbox), label, FALSE, FALSE, 0);

	cookies_path = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(cookies_path), rssyl_prefs.cookies_path);
	gtk_box_pack_start(GTK_BOX(cookies_hbox), cookies_path, TRUE, TRUE, 0);
	gtk_widget_set_tooltip_text(cookies_path,
			_("Path to Netscape-style cookies.txt file containing your cookies"));

	cookies_btn = gtkut_get_browse_file_btn(_("Bro_wse"));
	gtk_box_pack_start(GTK_BOX(cookies_hbox), cookies_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(cookies_btn), "clicked",
		G_CALLBACK(rssyl_prefs_cookies_browse_cb), cookies_path);
	gtk_box_pack_start(GTK_BOX(vbox2), cookies_hbox, FALSE, FALSE, 0);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);

	PACK_FRAME (vbox, frame, _("Refreshing"));
	gtk_container_set_border_width(GTK_CONTAINER(vbox1), 6);
	gtk_container_add(GTK_CONTAINER(frame), vbox1);

	PACK_FRAME (vbox, frame, _("Security and privacy"));
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), 6);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);

	gtk_widget_show_all(vbox);

	/* Store pointers to relevant widgets */
	prefs_page->page.widget = vbox;
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
		g_warning("failed to write RSSyl plugin configuration");
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
