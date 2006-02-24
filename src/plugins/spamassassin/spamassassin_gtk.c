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
#include "common/utils.h"
#include "prefs.h"
#include "folder.h"
#include "prefs_gtk.h"
#include "foldersel.h"
#include "spamassassin.h"
#include "statusbar.h"
#include "menu.h"

struct SpamAssassinPage
{
	PrefsPage page;
	
	GtkWidget *transport;
	GtkWidget *transport_notebook;
	GtkWidget *hostname;
	GtkWidget *colon;
	GtkWidget *port;
	GtkWidget *socket;
	GtkWidget *receive_spam;
	GtkWidget *save_folder;
	GtkWidget *max_size;
	GtkWidget *timeout;

	SpamAssassinTransport	trans;
};

struct Transport
{
	gchar			*name;
	SpamAssassinTransport	 transport;
	guint			 page;
	guint			 pageflags;
};

enum {
	PAGE_DISABLED = 0,
	PAGE_NETWORK  = 1,
	PAGE_UNIX     = 2,
};

enum {
    	NETWORK_HOSTNAME = 1,
};

struct Transport transports[] = {
	{ N_("Disabled"),	SPAMASSASSIN_DISABLED,			PAGE_DISABLED, 0 },
	{ N_("Localhost"),	SPAMASSASSIN_TRANSPORT_LOCALHOST,	PAGE_NETWORK, 0 },
	{ N_("TCP"),		SPAMASSASSIN_TRANSPORT_TCP,		PAGE_NETWORK, NETWORK_HOSTNAME },
	{ N_("Unix Socket"),	SPAMASSASSIN_TRANSPORT_UNIX,		PAGE_UNIX,    0 },
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

static void show_transport(struct SpamAssassinPage *page, struct Transport *transport)
{
	page->trans = transport->transport;

	switch (transport->page) {
	case PAGE_DISABLED:
		gtk_widget_show(page->hostname);
		gtk_widget_show(page->colon);
		gtk_widget_show(page->port);
		gtk_widget_hide(page->socket);
		gtk_widget_set_sensitive(page->hostname, FALSE);
		gtk_widget_set_sensitive(page->colon, FALSE);
		gtk_widget_set_sensitive(page->port, FALSE);
		gtk_widget_set_sensitive(page->max_size, FALSE);
		gtk_widget_set_sensitive(page->timeout, FALSE);
		break;
	case PAGE_UNIX:
		gtk_widget_hide(page->hostname);
		gtk_widget_hide(page->colon);
		gtk_widget_hide(page->port);
		gtk_widget_show(page->socket);
		gtk_widget_set_sensitive(page->socket, TRUE);
		gtk_widget_set_sensitive(page->max_size, TRUE);
		gtk_widget_set_sensitive(page->timeout, TRUE);
		break;
	case PAGE_NETWORK:
		gtk_widget_show(page->hostname);
		gtk_widget_show(page->colon);
		gtk_widget_show(page->port);
		gtk_widget_hide(page->socket);
		gtk_widget_set_sensitive(page->max_size, TRUE);
		gtk_widget_set_sensitive(page->timeout, TRUE);
		if (transport->pageflags & NETWORK_HOSTNAME) {
			gtk_widget_set_sensitive(page->hostname, TRUE);
			gtk_widget_set_sensitive(page->colon, TRUE);
			gtk_widget_set_sensitive(page->port, TRUE);
		} else {
			gtk_widget_set_sensitive(page->hostname, FALSE);
			gtk_widget_set_sensitive(page->colon, FALSE);
			gtk_widget_set_sensitive(page->port, TRUE);
		}
		break;
	default:
		break;
	}
	gtk_notebook_set_current_page(GTK_NOTEBOOK(page->transport_notebook), transport->page);
}

static void transport_sel_cb(GtkMenuItem *menuitem, gpointer data)
{
	struct SpamAssassinPage *page = (struct SpamAssassinPage *) data;
	struct Transport *transport;

	transport = (struct Transport *) g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID);
	show_transport(page, transport);
}

static void spamassassin_create_widget_func(PrefsPage * _page,
					    GtkWindow * window,
					    gpointer data)
{
	struct SpamAssassinPage *page = (struct SpamAssassinPage *) _page;
	SpamAssassinConfig *config;
	guint i, active;

	GtkWidget *table;
	GtkWidget *label3;
	GtkWidget *label4;
	GtkWidget *hbox4;
	GtkWidget *transport;
	GtkWidget *transport_menu;
	GtkWidget *transport_notebook;
	GtkWidget *hbox1;
	GtkWidget *spamd_hbox;
	GtkWidget *hostname;
	GtkWidget *colon;
	GtkObject *port_adj;
	GtkWidget *port;
	GtkWidget *socket;
	GtkWidget *label15;
	GtkWidget *hbox6;
	GtkObject *timeout_adj;
	GtkWidget *timeout;
	GtkWidget *label16;
	GtkWidget *receive_spam;
	GtkWidget *hbox3;
	GtkObject *max_size_adj;
	GtkWidget *max_size;
	GtkWidget *label11;
	GtkWidget *hbox2;
	GtkWidget *save_folder;
	GtkWidget *save_folder_select;
	GtkWidget *label6;
	GtkTooltips *tooltips;

	tooltips = gtk_tooltips_new();

	table = gtk_table_new(8, 3, FALSE);
	gtk_widget_show(table);
	gtk_container_set_border_width(GTK_CONTAINER(table), 8);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	hbox4 = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox4);
	gtk_table_attach(GTK_TABLE(table), hbox4, 0, 1, 0, 1,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (GTK_FILL), 0, 0);

	label3 = gtk_label_new(_("Transport"));
	gtk_widget_show(label3);
	gtk_box_pack_start(GTK_BOX(hbox4), label3, FALSE, FALSE, 0);

	transport = gtk_option_menu_new();
	gtk_widget_show(transport);
	gtk_box_pack_end(GTK_BOX(hbox4), transport, FALSE, FALSE, 0);
	transport_menu = gtk_menu_new();

	transport_notebook = gtk_notebook_new();
	gtk_widget_show(transport_notebook);
	gtk_table_attach(GTK_TABLE(table), transport_notebook, 1, 2, 1, 2,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (GTK_FILL), 0, 0);
	GTK_WIDGET_UNSET_FLAGS(transport_notebook, GTK_CAN_FOCUS);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(transport_notebook),
				   FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(transport_notebook),
				     FALSE);

	hbox1 = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox1);
	gtk_container_add(GTK_CONTAINER(transport_notebook), hbox1);

  	spamd_hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (spamd_hbox);
  	gtk_table_attach (GTK_TABLE (table), spamd_hbox, 0, 1, 1, 2,
                    	  (GtkAttachOptions) (GTK_FILL),
                    	  (GtkAttachOptions) (GTK_FILL), 0, 0);

	label4 = gtk_label_new(_("spamd"));
	gtk_widget_show(label4);
	gtk_box_pack_start(GTK_BOX(spamd_hbox), label4, FALSE, FALSE, 0);

	port_adj = gtk_adjustment_new(783, 1, 65535, 1, 10, 10);
	port = gtk_spin_button_new(GTK_ADJUSTMENT(port_adj), 1, 0);
	gtk_widget_show(port);
	gtk_box_pack_end(GTK_BOX(spamd_hbox), port, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(tooltips, port, _("Port of spamd server"),
			     NULL);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(port), TRUE);

	colon = gtk_label_new(_(":"));
	gtk_widget_show(colon);
	gtk_box_pack_end(GTK_BOX(spamd_hbox), colon, FALSE, FALSE, 0);

	hostname = gtk_entry_new();
	gtk_widget_show(hostname);
	gtk_box_pack_end(GTK_BOX(spamd_hbox), hostname, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(tooltips, hostname,
			     _("Hostname or IP address of spamd server"),
			     NULL);

	socket = gtk_entry_new();
	gtk_widget_show(socket);
	gtk_box_pack_end(GTK_BOX(spamd_hbox), socket, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(tooltips, socket, _("Path of Unix socket"),
			     NULL);

  	hbox3 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox3);
  	gtk_table_attach (GTK_TABLE (table), hbox3, 0, 1, 2, 3,
                    	  (GtkAttachOptions) (GTK_FILL),
                    	  (GtkAttachOptions) (GTK_FILL), 0, 0);

	label6 = gtk_label_new(_("Maximum Size"));
	gtk_widget_show(label6);
	gtk_box_pack_start(GTK_BOX(hbox3), label6, FALSE, FALSE, 0);

	max_size_adj = gtk_adjustment_new(250, 0, 10000, 10, 10, 10);
	max_size = gtk_spin_button_new(GTK_ADJUSTMENT(max_size_adj), 1, 0);
	gtk_widget_show(max_size);
	gtk_box_pack_end(GTK_BOX(hbox3), max_size, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(tooltips, max_size,
			_("Maximum size a message is allowed to have to be checked"),
			NULL);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(max_size), TRUE);

	label11 = gtk_label_new(_("kB"));
	gtk_widget_show(label11);
	gtk_table_attach(GTK_TABLE(table), label11, 1, 2, 2, 3,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (GTK_FILL), 0, 0);

	hbox6 = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox6);
	gtk_table_attach(GTK_TABLE(table), hbox6, 0, 1, 3, 4,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (GTK_FILL), 0, 0);

	label15 = gtk_label_new(_("Timeout"));
	gtk_widget_show(label15);
	gtk_box_pack_start(GTK_BOX(hbox6), label15, FALSE, FALSE, 0);

	timeout_adj = gtk_adjustment_new(60, 0, 10000, 10, 10, 10);
	timeout = gtk_spin_button_new(GTK_ADJUSTMENT(timeout_adj), 1, 0);
	gtk_widget_show(timeout);
	gtk_box_pack_end(GTK_BOX(hbox6), timeout, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(tooltips, timeout,
		_("Time that is allowed for checking. If the check takes longer the check will be aborted and the message will be handled as not spam."),
		NULL);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(timeout), TRUE);

	label16 = gtk_label_new(_("s"));
	gtk_widget_show(label16);
	gtk_table_attach(GTK_TABLE(table), label16, 1, 2, 3, 4,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (GTK_FILL), 0, 0);

	receive_spam = gtk_check_button_new_with_label(_("Save Spam"));
	gtk_widget_show(receive_spam);
	gtk_table_attach(GTK_TABLE(table), receive_spam, 0, 1, 4, 5,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_tooltips_set_tip(tooltips, receive_spam,
			     _("Save mails that where identified as spam"),
			     NULL);

  	hbox2 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox2);
  	gtk_table_attach (GTK_TABLE (table), hbox2, 0, 1, 5, 6,
                    	  (GtkAttachOptions) (GTK_FILL),
                    	  (GtkAttachOptions) (GTK_FILL), 0, 0);
	SET_TOGGLE_SENSITIVITY (receive_spam, hbox2);

  	label3 = gtk_label_new (_("Save folder"));
	gtk_widget_show (label3);
  	gtk_box_pack_start (GTK_BOX (hbox2), label3, FALSE, FALSE, 0);

	tooltips = gtk_tooltips_new();
  	save_folder = gtk_entry_new ();
	gtk_widget_show (save_folder);
	gtk_box_pack_start (GTK_BOX (hbox2), save_folder, TRUE, FALSE, 0);
	gtk_tooltips_set_tip(tooltips, save_folder,
			     _("Leave empty to use the default trash folder"),
			     NULL);

	save_folder_select = gtkut_get_browse_directory_btn(_("_Browse"));
	gtk_widget_show (save_folder_select);
  	gtk_box_pack_end (GTK_BOX (hbox2), save_folder_select, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(tooltips, save_folder_select,
			     _("Leave empty to use the default trash folder"),
			     NULL);

	config = spamassassin_get_config();

	g_signal_connect(G_OBJECT(save_folder_select), "released",
			 G_CALLBACK(foldersel_cb), page);

	if (config->hostname != NULL)
		gtk_entry_set_text(GTK_ENTRY(hostname), config->hostname);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(port), (float) config->port);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(receive_spam), config->receive_spam);
	if (config->save_folder != NULL)
		gtk_entry_set_text(GTK_ENTRY(save_folder), config->save_folder);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(max_size), (float) config->max_size);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(timeout), (float) config->timeout);
	
	page->transport = transport;
	page->transport_notebook = transport_notebook;
	page->hostname = hostname;
	page->colon = colon;
	page->port = port;
	page->socket = socket;
	page->receive_spam = receive_spam;
	page->save_folder = save_folder;
	page->max_size = max_size;
	page->timeout = timeout;

	active = 0;
	for (i = 0; i < (sizeof(transports) / sizeof(struct Transport)); i++) {
		GtkWidget *menuitem;

		menuitem = gtk_menu_item_new_with_label(gettext(transports[i].name));
		g_object_set_data(G_OBJECT(menuitem), MENU_VAL_ID, &transports[i]);
		g_signal_connect(G_OBJECT(menuitem), "activate",
				 G_CALLBACK(transport_sel_cb), page);
		gtk_widget_show(menuitem);
		gtk_menu_append(GTK_MENU(transport_menu), menuitem);

		if (config->transport == transports[i].transport) {
			show_transport(page, &transports[i]);
			active = i;
		}
	}
	gtk_option_menu_set_menu(GTK_OPTION_MENU(transport), transport_menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(transport), active);

	page->page.widget = table;
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
	config->transport = page->trans;

	/* hostname */
	g_free(config->hostname);
	config->hostname = gtk_editable_get_chars(GTK_EDITABLE(page->hostname), 0, -1);

	/* port */
	config->port = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(page->port));

	/* hostname */
	g_free(config->socket);
	config->socket = gtk_editable_get_chars(GTK_EDITABLE(page->socket), 0, -1);

	/* receive_spam */
	config->receive_spam = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->receive_spam));

	/* save_folder */
	g_free(config->save_folder);
	config->save_folder = gtk_editable_get_chars(GTK_EDITABLE(page->save_folder), 0, -1);

	/* max_size */
	config->max_size = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(page->max_size));

	/* timeout */
	config->timeout = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(page->timeout));

	procmsg_spam_set_folder(config->save_folder);
	spamassassin_save_config();
}

static void gtk_message_callback(gchar *message)
{
	statusbar_print_all(message);
}

static struct SpamAssassinPage spamassassin_page;

gint spamassassin_gtk_init(void)
{
	static gchar *path[3];

	path[0] = _("Plugins");
	path[1] = _("SpamAssassin");
	path[2] = NULL;

	spamassassin_page.page.path = path;
	spamassassin_page.page.create_widget = spamassassin_create_widget_func;
	spamassassin_page.page.destroy_widget = spamassassin_destroy_widget_func;
	spamassassin_page.page.save_page = spamassassin_save_func;
	spamassassin_page.page.weight = 35.0;

	prefs_gtk_register_page((PrefsPage *) &spamassassin_page);
	spamassassin_set_message_callback(gtk_message_callback);

	debug_print("SpamAssassin GTK plugin loaded\n");
	return 0;	
}

void spamassassin_gtk_done(void)
{
        prefs_gtk_unregister_page((PrefsPage *) &spamassassin_page);
}
