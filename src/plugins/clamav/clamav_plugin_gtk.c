/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto and the Sylpheed-Claws Team
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

#include "defs.h"

#include <glib.h>
#include <gtk/gtk.h>

#include "intl.h"
#include "common/plugin.h"
#include "common/utils.h"
#include "prefs.h"
#include "folder.h"
#include "prefs_gtk.h"
#include "foldersel.h"
#include "clamav_plugin.h"

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
 	 
	GtkWidget *table;
	GtkWidget *enable_clamav;
  	GtkWidget *label1;
  	GtkWidget *label2;
  	GtkWidget *label3;
  	GtkWidget *enable_arc;
  	GtkWidget *arc_scanning;
  	GtkWidget *label24;
  	GtkObject *max_size_adj;
  	GtkWidget *max_size;
  	GtkWidget *mb;
  	GtkWidget *label26;
  	GtkWidget *recv_infected;
  	GtkWidget *label27;
  	GtkWidget *save_folder;
  	GtkWidget *save_folder_select;

  	table = gtk_table_new (6, 3, FALSE);
	gtk_widget_show(table);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

  	enable_clamav = gtk_check_button_new_with_label ("");
	gtk_widget_show (enable_clamav);
  	gtk_table_attach (GTK_TABLE (table), enable_clamav, 1, 2, 0, 1,
                    	  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    	  (GtkAttachOptions) (0), 0, 0);

  	label1 = gtk_label_new ("Enable ClamAV filtering");
  	gtk_widget_show (label1);
  	gtk_table_attach (GTK_TABLE (table), label1, 0, 1, 0, 1,
                    	  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    	  (GtkAttachOptions) (0), 2, 4);
	gtk_label_set_justify(GTK_LABEL(label1), GTK_JUSTIFY_LEFT);
  	gtk_misc_set_alignment (GTK_MISC (label1), 0, 0.5);

  	enable_arc = gtk_check_button_new_with_label ("");
	gtk_widget_show (enable_arc);
  	gtk_table_attach (GTK_TABLE (table), enable_arc, 1, 2, 1, 2,
                    	  (GtkAttachOptions) (GTK_FILL),
                    	  (GtkAttachOptions) (0), 0, 0);

  	arc_scanning = gtk_label_new ("Enable archive scanning");
	gtk_widget_show (arc_scanning);
  	gtk_table_attach (GTK_TABLE (table), arc_scanning, 0, 1, 1, 2,
                    	  (GtkAttachOptions) (GTK_FILL),
                    	  (GtkAttachOptions) (0), 2, 4);
  	gtk_misc_set_alignment (GTK_MISC (arc_scanning), 0, 0.5);

  	label24 = gtk_label_new ("Maximum message size");
	gtk_widget_show (label24);
  	gtk_table_attach (GTK_TABLE (table), label24, 0, 1, 4, 5,
                    	  (GtkAttachOptions) (GTK_FILL),
                    	  (GtkAttachOptions) (0), 2, 4);
  	gtk_misc_set_alignment (GTK_MISC (label24), 0, 0.5);

  	max_size_adj = gtk_adjustment_new (1, 0, 100, 1, 10, 10);
  	max_size = gtk_spin_button_new (GTK_ADJUSTMENT (max_size_adj), 1, 0);
	gtk_widget_show (max_size);
  	gtk_table_attach (GTK_TABLE (table), max_size, 1, 2, 4, 5,
                    	  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    	  (GtkAttachOptions) (0), 0, 0);

  	mb = gtk_label_new ("MB");
  	gtk_widget_show (mb);
  	gtk_table_attach (GTK_TABLE (table), mb, 2, 3, 4, 5,
                    	  (GtkAttachOptions) (GTK_FILL),
                    	  (GtkAttachOptions) (0), 0, 0);
  	gtk_misc_set_alignment (GTK_MISC (mb), 0, 0.5);

  	label26 = gtk_label_new ("Receive infected email");
	gtk_widget_show (label26);
 	gtk_table_attach (GTK_TABLE (table), label26, 0, 1, 5, 6,
                    	  (GtkAttachOptions) (GTK_FILL),
                    	  (GtkAttachOptions) (0), 2, 4);
  	gtk_misc_set_alignment (GTK_MISC (label26), 0, 0.5);

  	recv_infected = gtk_check_button_new_with_label ("");
	gtk_widget_show (recv_infected);
  	gtk_table_attach (GTK_TABLE (table), recv_infected, 1, 2, 5, 6,
                    	  (GtkAttachOptions) (GTK_FILL),
                    	  (GtkAttachOptions) (0), 0, 0);

  	label27 = gtk_label_new ("Save folder");
	gtk_widget_show (label27);
  	gtk_table_attach (GTK_TABLE (table), label27, 0, 1, 6, 7,
                    	  (GtkAttachOptions) (GTK_FILL),
                    	  (GtkAttachOptions) (0), 2, 4);
  	gtk_misc_set_alignment (GTK_MISC (label27), 0, 0.5);

  	save_folder = gtk_entry_new ();
	gtk_widget_show (save_folder);
  	gtk_table_attach (GTK_TABLE (table), save_folder, 1, 2, 6, 7,
                    	  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    	  (GtkAttachOptions) (0), 0, 0);

  	save_folder_select = gtk_button_new_with_label (" ... ");
	gtk_widget_show (save_folder_select);
  	gtk_table_attach (GTK_TABLE (table), save_folder_select, 2, 3, 6, 7,
                    	  (GtkAttachOptions) (0),
                    	  (GtkAttachOptions) (0), 0, 0);

	config = clamav_get_config();

	gtk_signal_connect(GTK_OBJECT(save_folder_select), "released", GTK_SIGNAL_FUNC(foldersel_cb), page);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_clamav), config->clamav_enable);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_arc), config->clamav_archive_enable);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(max_size), (float) config->clamav_max_size);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(recv_infected), config->clamav_receive_infected);
	if (config->clamav_save_folder != NULL)
		gtk_entry_set_text(GTK_ENTRY(save_folder), config->clamav_save_folder);
	
	page->enable_clamav = enable_clamav;
	page->enable_arc = enable_arc;
	page->max_size = max_size;
	page->recv_infected = recv_infected;
	page->save_folder = save_folder;

	page->page.widget = table;
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
	config->clamav_archive_enable = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->enable_arc));

	config->clamav_max_size = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(page->max_size));
	config->clamav_receive_infected = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->recv_infected));
	g_free(config->clamav_save_folder);
	config->clamav_save_folder = gtk_editable_get_chars(GTK_EDITABLE(page->save_folder), 0, -1);

	clamav_save_config();
}

static struct ClamAvPage clamav_page;

gint plugin_init(gchar **error)
{
	clamav_page.page.path = _("Filtering/Clam AntiVirus");
	clamav_page.page.create_widget = clamav_create_widget_func;
	clamav_page.page.destroy_widget = clamav_destroy_widget_func;
	clamav_page.page.save_page = clamav_save_func;
	
	prefs_gtk_register_page((PrefsPage *) &clamav_page);

	debug_print("ClamAV GTK plugin loaded\n");
	return 0;	
}

void plugin_done()
{
	prefs_gtk_unregister_page((PrefsPage *) &clamav_page);

	debug_print("ClamAV GTK plugin unloaded\n");
}

const gchar *plugin_name()
{
	return "Clam AntiVirus GTK";
}

const gchar *plugin_desc()
{
	return "This plugin provides a Preferences page for the Clam AntiVirus "
	       "plugin.\n"
	       "\n"
	       "You will find the options in the Other Preferences window "
	       "under Filtering/Clam AntiVirus.\n"
	       "\n"
	       "With this plugin you can enable the filtering, enable archive "
	       "scanning, set the maximum size of messages to be checked, (if "
	       "the message is larger it will not be checked), configure "
	       "whether infected mail should be received (default: Yes) "
	       "and select the folder where infected mail will be saved.\n";
}

const gchar *plugin_type()
{
	return "GTK";
}
