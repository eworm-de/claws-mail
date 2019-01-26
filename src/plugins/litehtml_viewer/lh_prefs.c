/*
 * Claws Mail -- A GTK+ based, lightweight, and fast e-mail client
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
};
typedef struct _LHPrefsPage LHPrefsPage;

static PrefParam param[] = {
	{ "enable_remote_content", "FALSE", &lh_prefs.enable_remote_content, P_BOOL,
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
}

static void create_lh_prefs_page(PrefsPage *page, GtkWindow *window,
		gpointer data)
{
	LHPrefsPage *prefs_page = (LHPrefsPage *)page;
	GtkWidget *vbox;
	GtkWidget *vbox_remote;
	GtkWidget *frame;
	GtkWidget *label;
	GtkWidget *enable_remote_content;

	vbox = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), VBOX_BORDER);
	gtk_widget_show(vbox);

	/* Enable remote content */
	vbox_remote = gtkut_get_options_frame(vbox, &frame, _("Remote resources"));

	label = gtk_label_new(_("Loading remote resources can lead to some privacy issues.\n"
				"When remote content loading is disabled, nothing will be requested\n"
				"from the network."));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

	enable_remote_content = gtk_check_button_new_with_label(_("Enable loading of remote content"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_remote_content),
			lh_prefs.enable_remote_content);

	gtk_box_pack_start(GTK_BOX(vbox_remote), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox_remote), enable_remote_content, FALSE, FALSE, 0);
	gtk_widget_show_all(vbox_remote);

	prefs_page->enable_remote_content = enable_remote_content;
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
