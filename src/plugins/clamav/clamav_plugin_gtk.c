/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto and the Sylpheed-Claws Team
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gtk/gtkutils.h>

#include "common/sylpheed.h"
#include "common/version.h"
#include "plugin.h"
#include "utils.h"
#include "prefs.h"
#include "folder.h"
#include "prefs_gtk.h"
#include "foldersel.h"
#include "clamav_plugin.h"
#include "statusbar.h"

struct ClamAvPage
{
	PrefsPage page;
	
	GtkWidget *enable_clamav;
	GtkWidget *enable_arc;
	GtkWidget *max_size;
	GtkWidget *recv_infected;
	GtkWidget *save_folder;
};

static void foldersel_cb(GtkWidget *widget, gpointer data)
{
	struct ClamAvPage *page = (struct ClamAvPage *) data;
	FolderItem *item;
	gchar *item_id;
	gint newpos = 0;
	
	item = foldersel_folder_sel(NULL, FOLDER_SEL_MOVE, NULL);
	if (item && (item_id = folder_item_get_identifier(item)) != NULL) {
		gtk_editable_delete_text(GTK_EDITABLE(page->save_folder), 0, -1);
		gtk_editable_insert_text(GTK_EDITABLE(page->save_folder), item_id, strlen(item_id), &newpos);
		g_free(item_id);
	}
}

static void clamav_create_widget_func(PrefsPage * _page, GtkWindow *window, gpointer data)
{
	struct ClamAvPage *page = (struct ClamAvPage *) _page;
	ClamAvConfig *config;
 	 
	GtkWidget *vbox1, *vbox2;
	GtkWidget *enable_clamav;
  	GtkWidget *label1;
  	GtkWidget *enable_arc;
  	GtkWidget *label2;
  	GtkObject *max_size_adj;
  	GtkWidget *max_size;
	GtkWidget *hbox1;
  	GtkWidget *recv_infected;
  	GtkWidget *save_folder;
  	GtkWidget *save_folder_select;
	GtkTooltips *tooltips;

	tooltips = gtk_tooltips_new();

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (vbox2, enable_clamav, _("Enable virus scanning"));
	PACK_CHECK_BUTTON (vbox2, enable_arc, _("Scan archive contents"));

 	SET_TOGGLE_SENSITIVITY (enable_clamav, enable_arc);

 	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);
	SET_TOGGLE_SENSITIVITY (enable_arc, hbox1);

  	label1 = gtk_label_new(_("Maximum attachment size"));
  	gtk_widget_show (label1);
  	gtk_box_pack_start (GTK_BOX (hbox1), label1, FALSE, FALSE, 0);
 	SET_TOGGLE_SENSITIVITY (enable_clamav, label1);

  	max_size_adj = gtk_adjustment_new (1, 1, 1024, 1, 10, 10);
  	max_size = gtk_spin_button_new (GTK_ADJUSTMENT (max_size_adj), 1, 0);
	gtk_widget_show (max_size);
  	gtk_box_pack_start (GTK_BOX (hbox1), max_size, FALSE, FALSE, 0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (max_size), TRUE);
	gtk_tooltips_set_tip(tooltips, max_size,
			     _("Message attachments larger than this will not be scanned"),
			     NULL);
 	SET_TOGGLE_SENSITIVITY (enable_clamav, max_size);

  	label2 = gtk_label_new(_("MB"));
	gtk_widget_show (label2);
  	gtk_box_pack_start (GTK_BOX (hbox1), label2, FALSE, FALSE, 0);
 	SET_TOGGLE_SENSITIVITY (enable_clamav, label2);

  	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

 	recv_infected = gtk_check_button_new_with_label(_("Save infected mail in"));
	gtk_widget_show (recv_infected);
	gtk_box_pack_start (GTK_BOX (hbox1), recv_infected, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(tooltips, recv_infected,
			     _("Save mail that contains viruses"),
			     NULL);
 	SET_TOGGLE_SENSITIVITY (enable_clamav, recv_infected);

  	save_folder = gtk_entry_new ();
	gtk_widget_show (save_folder);
	gtk_box_pack_start (GTK_BOX (hbox1), save_folder, TRUE, TRUE, 0);
	gtk_tooltips_set_tip(tooltips, save_folder,
			     _("Folder for storing infected mail. Leave empty to use the default trash folder"),
			     NULL);
	SET_TOGGLE_SENSITIVITY (recv_infected, save_folder);
 	SET_TOGGLE_SENSITIVITY (enable_clamav, save_folder);

	save_folder_select = gtkut_get_browse_directory_btn(_("_Browse"));
	gtk_widget_show (save_folder_select);
  	gtk_box_pack_start (GTK_BOX (hbox1), save_folder_select, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(tooltips, save_folder_select,
			     _("Click this button to select a folder for storing infected mail"),
			     NULL);
	SET_TOGGLE_SENSITIVITY (recv_infected, save_folder_select);
 	SET_TOGGLE_SENSITIVITY (enable_clamav, save_folder_select);

	config = clamav_get_config();

	g_signal_connect(G_OBJECT(save_folder_select), "clicked", 
			 G_CALLBACK(foldersel_cb), page);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_clamav), config->clamav_enable);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_arc), config->clamav_enable_arc);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(max_size), (float) config->clamav_max_size);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(recv_infected), config->clamav_recv_infected);
	if (config->clamav_save_folder != NULL)
		gtk_entry_set_text(GTK_ENTRY(save_folder), config->clamav_save_folder);
	
	page->enable_clamav = enable_clamav;
	page->enable_arc = enable_arc;
	page->max_size = max_size;
	page->recv_infected = recv_infected;
	page->save_folder = save_folder;

	page->page.widget = vbox1;
}

static void clamav_destroy_widget_func(PrefsPage *_page)
{
	debug_print("Destroying ClamAV widget\n");
}

static void clamav_save_func(PrefsPage *_page)
{
	struct ClamAvPage *page = (struct ClamAvPage *) _page;
	ClamAvConfig *config;

	debug_print("Saving ClamAV Page\n");

	config = clamav_get_config();

	config->clamav_enable = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->enable_clamav));
	config->clamav_enable_arc = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->enable_arc));

	config->clamav_max_size = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(page->max_size));
	config->clamav_recv_infected = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->recv_infected));
	g_free(config->clamav_save_folder);
	config->clamav_save_folder = gtk_editable_get_chars(GTK_EDITABLE(page->save_folder), 0, -1);

	clamav_save_config();
}

static struct ClamAvPage clamav_page;

static void gtk_message_callback(gchar *message)
{
	statusbar_print_all(message);
}

gint clamav_gtk_init(void)
{
	static gchar *path[3];

	path[0] = _("Plugins");
	path[1] = _("Clam AntiVirus");
	path[2] = NULL;

	clamav_page.page.path = path;
	clamav_page.page.create_widget = clamav_create_widget_func;
	clamav_page.page.destroy_widget = clamav_destroy_widget_func;
	clamav_page.page.save_page = clamav_save_func;
	clamav_page.page.weight = 35.0;
	
	prefs_gtk_register_page((PrefsPage *) &clamav_page);
	clamav_set_message_callback(gtk_message_callback);

	debug_print("ClamAV GTK plugin loaded\n");
	return 0;	
}

void clamav_gtk_done(void)
{
        prefs_gtk_unregister_page((PrefsPage *) &clamav_page);
}
