/*
 * Claws Mail -- A GTK based, lightweight, and fast e-mail client
 * Copyright(C) 2019 the Claws Mail Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write tothe Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include "prefs_gtk.h"
#include "common/defs.h"
#include "common/utils.h"
#include "gtk/gtkutils.h"

#include "lh_prefs.h"

#define PREFS_BLOCK_NAME "LiteHTML"

static void create_lh_prefs_page(PrefsPage *page, GtkWindow *window,
		gpointer data);
static void destroy_lh_prefs_page(PrefsPage *page);
static void save_lh_prefs_page(PrefsPage *page);
static void save_prefs(void);

LHPrefs lh_prefs;

struct _LHPrefsPage {
	PrefsPage page;
	GtkWidget *enable_remote_content;
	GtkWidget *image_cache_size;
	GtkWidget *default_font;
};
typedef struct _LHPrefsPage LHPrefsPage;

static PrefParam param[] = {
	{ "enable_remote_content", "FALSE", &lh_prefs.enable_remote_content, P_BOOL,
		NULL, NULL, NULL },
	{ "image_cache_size", "20", &lh_prefs.image_cache_size, P_INT,
		NULL, NULL, NULL },
	{ "default_font", "Sans 16", &lh_prefs.default_font, P_STRING,
		NULL, NULL, NULL },
	{ NULL, NULL, NULL, 0, NULL, NULL, NULL }
};

static LHPrefsPage lh_prefs_page;

LHPrefs *lh_prefs_get(void)
{
	return &lh_prefs;
}

void lh_prefs_init(void)
{
	static gchar *path[3];
	gchar *rcpath;

	path[0] = _("Plugins");
	path[1] = "LiteHTML";
	path[2] = NULL;

	prefs_set_default(param);
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	prefs_read_config(param, PREFS_BLOCK_NAME, rcpath, NULL);
	g_free(rcpath);

	lh_prefs_page.page.path = path;
	lh_prefs_page.page.create_widget = create_lh_prefs_page;
	lh_prefs_page.page.destroy_widget = destroy_lh_prefs_page;
	lh_prefs_page.page.save_page = save_lh_prefs_page;
	lh_prefs_page.page.weight = 30.0;
	prefs_gtk_register_page((PrefsPage *) &lh_prefs_page);
}

void lh_prefs_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) &lh_prefs_page);
}

static void create_lh_prefs_page(PrefsPage *page, GtkWindow *window,
		gpointer data)
{
	LHPrefsPage *prefs_page = (LHPrefsPage *)page;
	GtkWidget *vbox;
	GtkWidget *vbox_remote;
	GtkWidget *hbox;
	GtkWidget *frame;
	GtkWidget *label;
	GtkWidget *enable_remote_content;
	GtkWidget *image_cache_size;
	GtkWidget *default_font;
	GtkAdjustment *adj;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), VBOX_BORDER);
	gtk_widget_show(vbox);

	/* Enable remote content */
	vbox_remote = gtkut_get_options_frame(vbox, &frame, _("Remote resources"));

	label = gtk_label_new(_("Loading remote resources can lead to some privacy issues.\n"
				"When remote content loading is disabled, nothing will be requested\n"
				"from the network."));
	gtk_label_set_xalign(GTK_LABEL(label),0);
	gtk_label_set_yalign(GTK_LABEL(label),0);

	enable_remote_content = gtk_check_button_new_with_label(_("Enable loading of remote content"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_remote_content),
			lh_prefs.enable_remote_content);

	gtk_box_pack_start(GTK_BOX(vbox_remote), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox_remote), enable_remote_content, FALSE, FALSE, 0);
	gtk_widget_show_all(vbox_remote);

	/* Image cache size */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Size of image cache in megabytes"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	adj = gtk_adjustment_new(0, 0, 99999, 1, 10, 0);
	image_cache_size = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(image_cache_size), TRUE);
	gtk_spin_button_set_wrap(GTK_SPIN_BUTTON(image_cache_size), FALSE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(image_cache_size),
			lh_prefs.image_cache_size);
	gtk_box_pack_start(GTK_BOX(hbox), image_cache_size, FALSE, FALSE, 0);
	gtk_widget_show_all(hbox);

	/* Font */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Default font"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	default_font = gtk_font_button_new_with_font(lh_prefs.default_font);
	g_object_set(G_OBJECT(default_font), "use-font", TRUE, NULL);
	gtk_box_pack_start(GTK_BOX(hbox), default_font, FALSE, FALSE, 0);

	gtk_widget_show_all(hbox);

	prefs_page->enable_remote_content = enable_remote_content;
	prefs_page->image_cache_size = image_cache_size;
	prefs_page->default_font = default_font;
	prefs_page->page.widget = vbox;
}

static void destroy_lh_prefs_page(PrefsPage *page)
{
}

static void save_lh_prefs_page(PrefsPage *page)
{
	LHPrefsPage *prefs_page = (LHPrefsPage *)page;

	lh_prefs.enable_remote_content = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(prefs_page->enable_remote_content));

	lh_prefs.image_cache_size = gtk_spin_button_get_value_as_int(
			GTK_SPIN_BUTTON(prefs_page->image_cache_size));

	g_free(lh_prefs.default_font);
	lh_prefs.default_font = g_strdup(gtk_font_chooser_get_font(
			GTK_FONT_CHOOSER(prefs_page->default_font)));

	save_prefs();
}

static void save_prefs(void)
{
	PrefFile *pref_file;
	gchar *rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			COMMON_RC, NULL);

	pref_file = prefs_write_open(rcpath);

	if (!pref_file) {
		g_warning("failed to open configuration file '%s' for writing", rcpath);
		g_free(rcpath);
		return;
	}
	if (prefs_set_block_label(pref_file, PREFS_BLOCK_NAME) < 0) {
		g_warning("failed to set block label "PREFS_BLOCK_NAME);
		g_free(rcpath);
		return;
	}

	if (prefs_write_param(param, pref_file->fp) < 0) {
		g_warning("failed to write LiteHTML Viewer plugin configuration");
		prefs_file_close_revert(pref_file);
		g_free(rcpath);
		return;
	}

	if (fprintf(pref_file->fp, "\n") < 0) {
		FILE_OP_ERROR(rcpath, "fprintf");
		prefs_file_close_revert(pref_file);
	} else {
		debug_print("successfully saved LiteHTML Viewer plugin configuration\n");
		prefs_file_close(pref_file);
	}

	g_free(rcpath);
}
