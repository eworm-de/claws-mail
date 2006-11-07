/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto and the Claws Mail Team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/*
 * The structure of this file has been borrowed from the structure of
 * the image_viewer plugin file. I also used it as an example of how to
 * build the preferences for the dillo plugin.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gtkutils.h"
#include "utils.h"
#include "prefs.h"
#include "prefs_gtk.h"
#include "prefswindow.h"

#include "dillo_prefs.h"

#define PREFS_BLOCK_NAME "Dillo"

DilloBrowserPrefs dillo_prefs;

typedef struct _DilloBrowserPage DilloBrowserPage;

struct _DilloBrowserPage {
        PrefsPage page;
        GtkWidget *local;
        GtkWidget *full;
};

static PrefParam param[] = {
        {"local_browse", "TRUE", &dillo_prefs.local, P_BOOL, NULL, NULL, NULL},
        {"full_window", "TRUE", &dillo_prefs.full, P_BOOL, NULL, NULL, NULL},
        {0,0,0,0,0,0,0}
};

static DilloBrowserPage prefs_page;

static void create_dillo_prefs_page	(PrefsPage *page,
					 GtkWindow *window,
					 gpointer   data);
static void destroy_dillo_prefs_page	(PrefsPage *page);
static void save_dillo_prefs		(PrefsPage *page);

void dillo_prefs_init(void)
{
	static gchar *path[3];
	gchar *rcpath;

	path[0] = _("Plugins");
	path[1] = _("Dillo Browser");
	path[2] = NULL;

        prefs_set_default(param);
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
        prefs_read_config(param, PREFS_BLOCK_NAME, rcpath, NULL);
	g_free(rcpath);
        
        prefs_page.page.path = path;
        prefs_page.page.create_widget = create_dillo_prefs_page;
        prefs_page.page.destroy_widget = destroy_dillo_prefs_page;
        prefs_page.page.save_page = save_dillo_prefs;

        prefs_gtk_register_page((PrefsPage *) &prefs_page);
}

void dillo_prefs_done(void)
{
        prefs_gtk_unregister_page((PrefsPage *) &prefs_page);
}

static void create_dillo_prefs_page(PrefsPage *page,
				    GtkWindow *window,
                                    gpointer data)
{
        DilloBrowserPage *prefs_page = (DilloBrowserPage *) page;

        GtkWidget *vbox;
        GtkWidget *local_checkbox;
        GtkWidget *full_checkbox;
        GtkWidget *label;
	GtkTooltips *local_tooltip;
	GtkTooltips *full_tooltip;

        vbox = gtk_vbox_new(FALSE, 3);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), VBOX_BORDER);
        gtk_widget_show(vbox);
        
	local_tooltip = gtk_tooltips_new();
        local_checkbox = gtk_check_button_new_with_label
				(_("Do not load remote links in mails"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(local_checkbox),
                                     dillo_prefs.local);
        gtk_box_pack_start(GTK_BOX(vbox), local_checkbox, FALSE, FALSE, 0);
        gtk_widget_show(local_checkbox);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(local_tooltip), local_checkbox,
			     _("Equivalent to Dillo's '--local' option"), NULL);
        
	label = gtk_label_new(_("You can still load remote links "
			      "by reloading the page"));
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtkut_widget_set_small_font_size (label);
        gtk_widget_show(label);

	full_tooltip = gtk_tooltips_new();
        full_checkbox = gtk_check_button_new_with_label
				(_("Full window mode (hide controls)"));
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(full_checkbox),
                                      dillo_prefs.full);
        gtk_box_pack_start(GTK_BOX(vbox), full_checkbox, FALSE, FALSE, 0);
        gtk_widget_show(full_checkbox);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(full_tooltip), full_checkbox,
			     _("Equivalent to Dillo's '--fullwindow' option"),
			     NULL);
        
        prefs_page->local = local_checkbox;
        prefs_page->full = full_checkbox;
        prefs_page->page.widget = vbox;
}

static void destroy_dillo_prefs_page(PrefsPage *page)
{
	/* Do nothing! */
}

static void save_dillo_prefs(PrefsPage *page)
{
        DilloBrowserPage *prefs_page = (DilloBrowserPage *) page;
        PrefFile *pref_file;
        gchar *rc_file_path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
                                          COMMON_RC, NULL);
        
        dillo_prefs.local = gtk_toggle_button_get_active
				(GTK_TOGGLE_BUTTON(prefs_page->local));
        dillo_prefs.full = gtk_toggle_button_get_active
				(GTK_TOGGLE_BUTTON(prefs_page->full));
        
        pref_file = prefs_write_open(rc_file_path);
        g_free(rc_file_path);
        
        if (!(pref_file) ||
	    (prefs_set_block_label(pref_file, PREFS_BLOCK_NAME) < 0))
          return;
        
        if (prefs_write_param(param, pref_file->fp) < 0) {
          g_warning("failed to write Dillo Plugin configuration\n");
          prefs_file_close_revert(pref_file);
          return;
        }
        fprintf(pref_file->fp, "\n");
        prefs_file_close(pref_file);
}
