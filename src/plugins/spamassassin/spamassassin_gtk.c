/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto and the Sylpheed-Claws Team
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
#include "plugin.h"
#include "common/utils.h"
#include "prefs.h"
#include "folder.h"
#include "prefswindow.h"
#include "foldersel.h"
#include "spamassassin.h"

struct SpamAssassinPage
{
	PrefsPage page;
	
	GtkWidget *enable;
	GtkWidget *hostname;
	GtkWidget *port;
	GtkWidget *max_size;
	GtkWidget *receive_spam;
	GtkWidget *save_folder;
};

static void foldersel_cb(GtkWidget *widget, gpointer data)
{
	struct SpamAssassinPage *page = (struct SpamAssassinPage *) data;
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

static void spamassassin_create_widget_func(PrefsPage * _page)
{
	struct SpamAssassinPage *page = (struct SpamAssassinPage *) _page;

	/* ------------------ code made by glade -------------------- */
	GtkWidget *table1;
	GtkWidget *label3;
	GtkWidget *label4;
	GtkWidget *label6;
	GtkWidget *label8;
	GtkWidget *label9;
	GtkWidget *hbox1;
	GtkWidget *hostname;
	GtkWidget *label5;
	GtkObject *port_adj;
	GtkWidget *port;
	GtkWidget *enable;
	GtkWidget *receive_spam;
	GtkWidget *label10;
	GtkObject *max_size_adj;
	GtkWidget *max_size;
	GtkWidget *save_folder;
	GtkWidget *button4;
	GtkWidget *label11;

	table1 = gtk_table_new(6, 3, FALSE);
	gtk_widget_show(table1);
	gtk_table_set_row_spacings(GTK_TABLE(table1), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table1), 8);

	label3 = gtk_label_new(_("Enable SpamAssassin Filtering"));
	gtk_widget_show(label3);
	gtk_table_attach(GTK_TABLE(table1), label3, 0, 1, 0, 1,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label3), 0, 0.5);

	label4 = gtk_label_new(_("SpamAssassin Server (spamd)"));
	gtk_widget_show(label4);
	gtk_table_attach(GTK_TABLE(table1), label4, 0, 1, 1, 2,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label4), 0, 0.5);

	label6 = gtk_label_new(_("Maximum Message Size"));
	gtk_widget_show(label6);
	gtk_table_attach(GTK_TABLE(table1), label6, 0, 1, 2, 3,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label6), 0, 0.5);

	label8 = gtk_label_new(_("Folder for saved Spam"));
	gtk_widget_show(label8);
	gtk_table_attach(GTK_TABLE(table1), label8, 0, 1, 4, 5,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify(GTK_LABEL(label8), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label8), 0, 0.5);

	label9 = gtk_label_new(_("Receive Spam"));
	gtk_widget_show(label9);
	gtk_table_attach(GTK_TABLE(table1), label9, 0, 1, 3, 4,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label9), 0, 0.5);

	hbox1 = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox1);
	gtk_table_attach(GTK_TABLE(table1), hbox1, 1, 2, 1, 2,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (GTK_FILL), 0, 0);

	hostname = gtk_entry_new();
	gtk_widget_show(hostname);
	gtk_box_pack_start(GTK_BOX(hbox1), hostname, TRUE, TRUE, 0);

	label5 = gtk_label_new(_(":"));
	gtk_widget_show(label5);
	gtk_box_pack_start(GTK_BOX(hbox1), label5, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label5), 8, 0);

	port_adj = gtk_adjustment_new(783, 1, 65535, 1, 10, 10);
	port =
	    gtk_spin_button_new(GTK_ADJUSTMENT(port_adj), 1, 0);
	gtk_widget_show(port);
	gtk_box_pack_start(GTK_BOX(hbox1), port, FALSE, TRUE, 0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(port), TRUE);

	enable = gtk_check_button_new_with_label("");
	gtk_widget_show(enable);
	gtk_table_attach(GTK_TABLE(table1), enable, 1, 2, 0, 1,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);

	receive_spam = gtk_check_button_new_with_label("");
	gtk_widget_show(receive_spam);
	gtk_table_attach(GTK_TABLE(table1), receive_spam, 1, 2, 3, 4,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);

	label10 =
	    gtk_label_new(_
			  ("Leave empty to use the default trash folder"));
	gtk_widget_show(label10);
	gtk_table_attach(GTK_TABLE(table1), label10, 1, 2, 5, 6,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify(GTK_LABEL(label10), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC(label10), 1, 0.5);

	max_size_adj = gtk_adjustment_new(250, 0, 10000, 10, 10, 10);
	max_size = gtk_spin_button_new(GTK_ADJUSTMENT(max_size_adj), 1, 0);
	gtk_widget_show(max_size);
	gtk_table_attach(GTK_TABLE(table1), max_size, 1, 2, 2, 3,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);

	save_folder = gtk_entry_new();
	gtk_widget_show(save_folder);
	gtk_table_attach(GTK_TABLE(table1), save_folder, 1, 2, 4, 5,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);

	button4 = gtk_button_new_with_label(_("..."));
	gtk_widget_show(button4);
	gtk_table_attach(GTK_TABLE(table1), button4, 2, 3, 4, 5,
			 (GtkAttachOptions) (GTK_SHRINK | GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);

	label11 = gtk_label_new(_("kB"));
	gtk_widget_show(label11);
	gtk_table_attach(GTK_TABLE(table1), label11, 2, 3, 2, 3,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label11), 0, 0.5);
	/* --------------------------------------------------------- */

	gtk_widget_set_usize(GTK_WIDGET(port), 64, -1);
	gtk_signal_connect(GTK_OBJECT(button4), "released", GTK_SIGNAL_FUNC(foldersel_cb), page);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable), spamassassin_enable);
	gtk_entry_set_text(GTK_ENTRY(hostname), spamassassin_hostname);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(port), (float) spamassassin_port);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(max_size), (float) spamassassin_max_size);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(receive_spam), spamassassin_receive_spam);
	gtk_entry_set_text(GTK_ENTRY(save_folder), spamassassin_save_folder);
	
	page->enable = enable;
	page->hostname = hostname;
	page->port = port;
	page->max_size = max_size;
	page->receive_spam = receive_spam;
	page->save_folder = save_folder;

	page->page.widget = table1;
}

static void spamassassin_destroy_widget_func(PrefsPage *_page)
{
	debug_print("Destroying SpamAssassin widget\n");
}

static void spamassassin_save_func(PrefsPage *_page)
{
	struct SpamAssassinPage *page = (struct SpamAssassinPage *) _page;
	gchar *tmp;

	debug_print("Saving SpamAssassin Page\n");

	spamassassin_enable = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->enable));
	if (spamassassin_hostname)
		g_free(spamassassin_hostname);
	spamassassin_hostname = gtk_editable_get_chars(GTK_EDITABLE(page->hostname), 0, -1);
	tmp = gtk_editable_get_chars(GTK_EDITABLE(page->port), 0, -1);
	spamassassin_port = atoi(tmp);
	g_free(tmp);
	spamassassin_max_size = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(page->max_size));
	spamassassin_receive_spam = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->receive_spam));
	if (spamassassin_save_folder)
		g_free(spamassassin_save_folder);
	spamassassin_save_folder = gtk_editable_get_chars(GTK_EDITABLE(page->save_folder), 0, -1);

	spamassassin_save_config();
}

static void spamassassin_destroy_func(PrefsPage *_page)
{
	debug_print("Destroying SpamAssassin Page\n");
	g_free(_page);
}

static struct SpamAssassinPage *spamassassin_page;

gint plugin_init(gchar **error)
{
	struct SpamAssassinPage *page;

	page = g_new0(struct SpamAssassinPage, 1);
	page->page.path = _("Filtering/SpamAssassin");
	page->page.create_widget = spamassassin_create_widget_func;
	page->page.destroy_widget = spamassassin_destroy_widget_func;
	page->page.save_page = spamassassin_save_func;
	page->page.destroy_page = spamassassin_destroy_func;
	prefswindow_register_page((PrefsPage *) page);

	spamassassin_page = page;

	debug_print("SpamAssassin GTK plugin loaded\n");
	return 0;	
}

void plugin_done()
{
	prefswindow_unregister_page((PrefsPage *) spamassassin_page);
	g_free(spamassassin_page);

	debug_print("SpamAssassin GTK plugin unloaded\n");
}

const gchar *plugin_name()
{
	return "SpamAssassin GTK";
}

const gchar *plugin_desc()
{
	return "This plugin provides a Preferences page for the SpamAssassin "
	       "plugin.\n"
	       "\n"
	       "You will find the options in the Preferences window under "
	       "Filtering/SpamAssassin.\n"
	       "\n"
	       "With this plugin you can enable the filtering, change the "
	       "SpamAssassin server's host  and port, the maximum size that "
	       "a message is allowed to have (if the message is larger it "
	       "will not be checked), set the option if spam mails "
	       "should be received at all (default) and select the folder "
	       "where spam mails will be saved.\n";
}
