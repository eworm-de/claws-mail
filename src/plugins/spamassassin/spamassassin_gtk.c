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

#include "common/sylpheed.h"
#include "common/version.h"
#include "intl.h"
#include "plugin.h"
#include "common/utils.h"
#include "prefs.h"
#include "folder.h"
#include "prefs_gtk.h"
#include "foldersel.h"
#include "spamassassin.h"

struct SpamAssassinPage
{
	PrefsPage page;
	
	GtkWidget *enable;
	GtkWidget *hostname;
	GtkWidget *port;
	GtkWidget *receive_spam;
	GtkWidget *save_folder;
	GtkWidget *max_size;
	GtkWidget *timeout;
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

static void spamassassin_create_widget_func(PrefsPage * _page,
					    GtkWindow * window,
					    gpointer data)
{
	struct SpamAssassinPage *page = (struct SpamAssassinPage *) _page;
	SpamAssassinConfig *config;

	/*
	 * BEGIN GLADE CODE
	 * DO NOT EDIT
	 */
	GtkWidget *table1;
	GtkWidget *label3;
	GtkWidget *label4;
	GtkWidget *hbox1;
	GtkWidget *hostname;
	GtkWidget *label5;
	GtkObject *port_adj;
	GtkWidget *port;
	GtkWidget *enable;
	GtkWidget *label9;
	GtkWidget *receive_spam;
	GtkWidget *label8;
	GtkWidget *save_folder;
	GtkWidget *button4;
	GtkWidget *label6;
	GtkWidget *hbox3;
	GtkObject *max_size_adj;
	GtkWidget *max_size;
	GtkWidget *label11;
	GtkWidget *label12;
	GtkWidget *label13;
	GtkWidget *hbox4;
	GtkObject *timeout_adj;
	GtkWidget *timeout;
	GtkTooltips *tooltips;

	tooltips = gtk_tooltips_new();

	table1 = gtk_table_new(6, 3, FALSE);
	gtk_widget_show(table1);
	gtk_container_set_border_width(GTK_CONTAINER(table1), 8);
	gtk_table_set_row_spacings(GTK_TABLE(table1), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table1), 8);

	label3 = gtk_label_new(_("Enable"));
	gtk_widget_show(label3);
	gtk_table_attach(GTK_TABLE(table1), label3, 0, 1, 0, 1,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label3), 0, 0.5);

	label4 = gtk_label_new(_("spamd "));
	gtk_widget_show(label4);
	gtk_table_attach(GTK_TABLE(table1), label4, 0, 1, 1, 2,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label4), 0, 0.5);

	hbox1 = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox1);
	gtk_table_attach(GTK_TABLE(table1), hbox1, 1, 2, 1, 2,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (GTK_FILL), 0, 0);

	hostname = gtk_entry_new();
	gtk_widget_show(hostname);
	gtk_box_pack_start(GTK_BOX(hbox1), hostname, TRUE, TRUE, 0);
	gtk_tooltips_set_tip(tooltips, hostname,
			     _("Hostname or IP address of spamd server"),
			     NULL);

	label5 = gtk_label_new(_(":"));
	gtk_widget_show(label5);
	gtk_box_pack_start(GTK_BOX(hbox1), label5, FALSE, FALSE, 0);
	gtk_misc_set_padding(GTK_MISC(label5), 8, 0);

	port_adj = gtk_adjustment_new(783, 1, 65535, 1, 10, 10);
	port = gtk_spin_button_new(GTK_ADJUSTMENT(port_adj), 1, 0);
	gtk_widget_show(port);
	gtk_box_pack_end(GTK_BOX(hbox1), port, FALSE, TRUE, 0);
	gtk_widget_set_usize(port, 64, -2);
	gtk_tooltips_set_tip(tooltips, port, _("Port of spamd server"),
			     NULL);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(port), TRUE);

	enable = gtk_check_button_new_with_label("");
	gtk_widget_show(enable);
	gtk_table_attach(GTK_TABLE(table1), enable, 1, 2, 0, 1,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_tooltips_set_tip(tooltips, enable,
			     _("Enable SpamAssassin filtering"), NULL);

	label9 = gtk_label_new(_("Save Spam"));
	gtk_widget_show(label9);
	gtk_table_attach(GTK_TABLE(table1), label9, 0, 1, 2, 3,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label9), 0, 0.5);

	receive_spam = gtk_check_button_new_with_label("");
	gtk_widget_show(receive_spam);
	gtk_table_attach(GTK_TABLE(table1), receive_spam, 1, 2, 2, 3,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_tooltips_set_tip(tooltips, receive_spam,
			     _
			     ("Save mails that where identified as spam to a folder"),
			     NULL);

	label8 = gtk_label_new(_("Save Folder"));
	gtk_widget_show(label8);
	gtk_table_attach(GTK_TABLE(table1), label8, 0, 1, 3, 4,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify(GTK_LABEL(label8), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(label8), 0, 0.5);

	save_folder = gtk_entry_new();
	gtk_widget_show(save_folder);
	gtk_table_attach(GTK_TABLE(table1), save_folder, 1, 2, 3, 4,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_tooltips_set_tip(tooltips, save_folder,
			     _
			     ("Folder that will be used to save spam. Leave empty to use the default trash folder"),
			     NULL);

	button4 = gtk_button_new_with_label(_("..."));
	gtk_widget_show(button4);
	gtk_table_attach(GTK_TABLE(table1), button4, 2, 3, 3, 4,
			 (GtkAttachOptions) (GTK_SHRINK | GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);

	label6 = gtk_label_new(_("Maximum Size"));
	gtk_widget_show(label6);
	gtk_table_attach(GTK_TABLE(table1), label6, 0, 1, 4, 5,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label6), 0, 0.5);

	hbox3 = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox3);
	gtk_table_attach(GTK_TABLE(table1), hbox3, 1, 2, 4, 5,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (GTK_FILL), 0, 0);

	max_size_adj = gtk_adjustment_new(250, 0, 10000, 10, 10, 10);
	max_size = gtk_spin_button_new(GTK_ADJUSTMENT(max_size_adj), 1, 0);
	gtk_widget_show(max_size);
	gtk_box_pack_end(GTK_BOX(hbox3), max_size, FALSE, TRUE, 0);
	gtk_widget_set_usize(max_size, 64, -2);
	gtk_tooltips_set_tip(tooltips, max_size,
			     _
			     ("Maximum size a message is allowed to have to be checked"),
			     NULL);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(max_size), TRUE);

	label11 = gtk_label_new(_("kB"));
	gtk_widget_show(label11);
	gtk_table_attach(GTK_TABLE(table1), label11, 2, 3, 4, 5,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label11), 0, 0.5);

	label12 = gtk_label_new(_("Timeout"));
	gtk_widget_show(label12);
	gtk_table_attach(GTK_TABLE(table1), label12, 0, 1, 5, 6,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label12), 0, 0.5);

	label13 = gtk_label_new(_("s"));
	gtk_widget_show(label13);
	gtk_table_attach(GTK_TABLE(table1), label13, 2, 3, 5, 6,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label13), 0, 0.5);

	hbox4 = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox4);
	gtk_table_attach(GTK_TABLE(table1), hbox4, 1, 2, 5, 6,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (GTK_FILL), 0, 0);

	timeout_adj = gtk_adjustment_new(30, 5, 300, 1, 1, 1);
	timeout = gtk_spin_button_new(GTK_ADJUSTMENT(timeout_adj), 1, 0);
	gtk_widget_show(timeout);
	gtk_box_pack_end(GTK_BOX(hbox4), timeout, FALSE, TRUE, 0);
	gtk_widget_set_usize(timeout, 64, -2);
	gtk_tooltips_set_tip(tooltips, timeout,
			     _
			     ("Maximum time allowed for the spam check. After the time the check will be aborted and the message delivered as none spam."),
			     NULL);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(timeout), TRUE);
	/*
	 * END GLADE CODE
	 */

	config = spamassassin_get_config();

	gtk_signal_connect(GTK_OBJECT(button4), "released", GTK_SIGNAL_FUNC(foldersel_cb), page);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable), config->enable);
	if (config->hostname != NULL)
		gtk_entry_set_text(GTK_ENTRY(hostname), config->hostname);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(port), (float) config->port);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(receive_spam), config->receive_spam);
	if (config->save_folder != NULL)
		gtk_entry_set_text(GTK_ENTRY(save_folder), config->save_folder);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(max_size), (float) config->max_size);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(timeout), (float) config->timeout);
	
	page->enable = enable;
	page->hostname = hostname;
	page->port = port;
	page->receive_spam = receive_spam;
	page->save_folder = save_folder;
	page->max_size = max_size;
	page->timeout = timeout;

	page->page.widget = table1;
}

static void spamassassin_destroy_widget_func(PrefsPage *_page)
{
	debug_print("Destroying SpamAssassin widget\n");
}

static void spamassassin_save_func(PrefsPage *_page)
{
	struct SpamAssassinPage *page = (struct SpamAssassinPage *) _page;
	SpamAssassinConfig *config;

	debug_print("Saving SpamAssassin Page\n");

	config = spamassassin_get_config();

	/* enable */
	config->enable = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->enable));

	/* hostname */
	g_free(config->hostname);
	config->hostname = gtk_editable_get_chars(GTK_EDITABLE(page->hostname), 0, -1);

	/* port */
	config->port = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(page->port));

	/* receive_spam */
	config->receive_spam = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->receive_spam));

	/* save_folder */
	g_free(config->save_folder);
	config->save_folder = gtk_editable_get_chars(GTK_EDITABLE(page->save_folder), 0, -1);

	/* max_size */
	config->max_size = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(page->max_size));

	/* timeout */
	config->timeout = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(page->timeout));

	spamassassin_save_config();
}

static struct SpamAssassinPage spamassassin_page;

gint plugin_init(gchar **error)
{
	if ((sylpheed_get_version() > VERSION_NUMERIC)) {
		*error = g_strdup("Your sylpheed version is newer than the version the plugin was built with");
		return -1;
	}

	if ((sylpheed_get_version() < MAKE_NUMERIC_VERSION(0, 9, 3, 86))) {
		*error = g_strdup("Your sylpheed version is too old");
		return -1;
	}

	spamassassin_page.page.path = _("Filtering/SpamAssassin");
	spamassassin_page.page.create_widget = spamassassin_create_widget_func;
	spamassassin_page.page.destroy_widget = spamassassin_destroy_widget_func;
	spamassassin_page.page.save_page = spamassassin_save_func;
	spamassassin_page.page.weight = 35.0;

	prefs_gtk_register_page((PrefsPage *) &spamassassin_page);

	debug_print("SpamAssassin GTK plugin loaded\n");
	return 0;	
}

void plugin_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) &spamassassin_page);

	debug_print("SpamAssassin GTK plugin unloaded\n");
}

const gchar *plugin_name(void)
{
	return _("SpamAssassin GTK");
}

const gchar *plugin_desc(void)
{
	return _("This plugin provides a Preferences page for the SpamAssassin "
	         "plugin.\n"
	         "\n"
	         "You will find the options in the Other Preferences window "
	         "under Filtering/SpamAssassin.\n"
	         "\n"
	         "With this plugin you can enable the filtering, change the "
	         "SpamAssassin server host and port, set the maximum size of "
	         "messages to be checked, (if the message is larger it will "
	         "not be checked), configure whether spam mail should be received "
	         "(default: Yes) and select the folder where spam mail will be "
	         "saved.\n");
}

const gchar *plugin_type(void)
{
	return "GTK";
}
