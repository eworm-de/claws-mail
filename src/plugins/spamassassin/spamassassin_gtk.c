/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Hiroyuki Yamamoto and the Claws Mail Team
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

#include "common/claws.h"
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

	GtkWidget *enable_sa_checkbtn;
	GtkWidget *transport_optmenu;
	GtkWidget *transport_label;
	GtkWidget *username;
	GtkWidget *hostname;
	GtkWidget *colon;
	GtkWidget *port;
	GtkWidget *socket;
	GtkWidget *process_emails;
	GtkWidget *receive_spam;
	GtkWidget *save_folder;
	GtkWidget *save_folder_select;
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
	/*{ N_("Disabled"),	SPAMASSASSIN_DISABLED,			PAGE_DISABLED, 0 },*/
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
		/*
	case PAGE_DISABLED:
		gtk_widget_show(page->hostname);
		gtk_widget_show(page->colon);
		gtk_widget_show(page->port);
		gtk_widget_hide(page->socket);
		gtk_widget_set_sensitive(page->username, FALSE);
		gtk_widget_set_sensitive(page->hostname, FALSE);
		gtk_widget_set_sensitive(page->colon, FALSE);
		gtk_widget_set_sensitive(page->port, FALSE);
		gtk_widget_set_sensitive(page->max_size, FALSE);
		gtk_widget_set_sensitive(page->timeout, FALSE);
		gtk_widget_set_sensitive(page->process_emails, FALSE);
		gtk_widget_set_sensitive(page->receive_spam, FALSE);
		gtk_widget_set_sensitive(page->save_folder, FALSE);
		gtk_widget_set_sensitive(page->save_folder_select, FALSE);
		break;
		*/
	case PAGE_UNIX:
		gtk_widget_hide(page->hostname);
		gtk_widget_hide(page->colon);
		gtk_widget_hide(page->port);
		gtk_widget_show(page->socket);
		gtk_widget_set_sensitive(page->username, TRUE);
		gtk_widget_set_sensitive(page->socket, TRUE);
		gtk_widget_set_sensitive(page->max_size, TRUE);
		gtk_widget_set_sensitive(page->timeout, TRUE);
		gtk_widget_set_sensitive(page->process_emails, TRUE);
		gtk_widget_set_sensitive(page->receive_spam, TRUE);
		gtk_widget_set_sensitive(page->save_folder, TRUE);
		gtk_widget_set_sensitive(page->save_folder_select, TRUE);
		break;
	case PAGE_NETWORK:
		gtk_widget_show(page->hostname);
		gtk_widget_show(page->colon);
		gtk_widget_show(page->port);
		gtk_widget_hide(page->socket);
		gtk_widget_set_sensitive(page->username, TRUE);
		gtk_widget_set_sensitive(page->max_size, TRUE);
		gtk_widget_set_sensitive(page->timeout, TRUE);
		gtk_widget_set_sensitive(page->process_emails, TRUE);
		gtk_widget_set_sensitive(page->receive_spam, TRUE);
		gtk_widget_set_sensitive(page->save_folder, TRUE);
		gtk_widget_set_sensitive(page->save_folder_select, TRUE);
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

	GtkWidget *vbox1, *vbox2;
	GtkWidget *frame_transport, *table_transport, *vbox_transport;
	GtkWidget *hbox_spamd, *hbox_max_size, *hbox_timeout;
	GtkWidget *hbox_process_emails, *hbox_save_spam;

	GtkWidget *enable_sa_checkbtn;

	GtkWidget *transport_label;
	GtkWidget *transport_optmenu;
	GtkWidget *transport_menu;

	GtkWidget *user_label;
	GtkWidget *user_entry;

	GtkWidget *spamd_label;
	GtkWidget *spamd_hostname_entry;
	GtkWidget *spamd_colon_label;
	GtkObject *spamd_port_spinbtn_adj;
	GtkWidget *spamd_port_spinbtn;
	GtkWidget *spamd_socket_entry;

	GtkWidget *max_size_label;
	GtkObject *max_size_spinbtn_adj;
	GtkWidget *max_size_spinbtn;
	GtkWidget *max_size_kb_label;

	GtkWidget *timeout_label;
	GtkObject *timeout_spinbtn_adj;
	GtkWidget *timeout_spinbtn;
	GtkWidget *timeout_seconds_label;

	GtkWidget *process_emails_checkbtn;

	GtkWidget *save_spam_checkbtn;
	GtkWidget *save_spam_folder_entry;
	GtkWidget *save_spam_folder_select;

	GtkTooltips *tooltips;

	tooltips = gtk_tooltips_new();

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_vbox_new (FALSE, 4);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	enable_sa_checkbtn = gtk_check_button_new_with_label(_("Enable SpamAssassin plugin"));
	gtk_widget_show(enable_sa_checkbtn);
	gtk_box_pack_start(GTK_BOX(vbox2), enable_sa_checkbtn, TRUE, TRUE, 0);

	vbox_transport = gtkut_get_options_frame(vbox2, &frame_transport, _("Transport"));

	table_transport = gtk_table_new (3, 3, FALSE);
	gtk_widget_show (table_transport);
	gtk_box_pack_start(GTK_BOX(vbox_transport), table_transport, TRUE, TRUE, 0);
	gtk_table_set_row_spacings (GTK_TABLE (table_transport), 4);
	gtk_table_set_col_spacings (GTK_TABLE (table_transport), 8);

	transport_label = gtk_label_new(_("Type of transport"));
	gtk_widget_show(transport_label);
	gtk_table_attach (GTK_TABLE (table_transport), transport_label, 0, 1, 0, 1,
			(GtkAttachOptions) (GTK_FILL),
			(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify(GTK_LABEL(transport_label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC(transport_label), 1, 0.5);

	transport_optmenu = gtk_option_menu_new();
	gtk_widget_show(transport_optmenu);

	gtk_table_attach (GTK_TABLE (table_transport), transport_optmenu, 1, 2, 0, 1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 0, 0);
	transport_menu = gtk_menu_new();

	user_label = gtk_label_new(_("User"));
	gtk_widget_show(user_label);
	gtk_table_attach (GTK_TABLE (table_transport), user_label, 0, 1, 1, 2,
			(GtkAttachOptions) (GTK_FILL),
			(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify(GTK_LABEL(user_label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC(user_label), 1, 0.5);

	user_entry = gtk_entry_new();
	gtk_widget_show(user_entry);
	gtk_table_attach (GTK_TABLE (table_transport), user_entry, 1, 2, 1, 2,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 0, 0);
	gtk_tooltips_set_tip(tooltips, user_entry, _("User to use with spamd server"),
			NULL);

	spamd_label = gtk_label_new(_("spamd"));
	gtk_widget_show(spamd_label);
	gtk_table_attach (GTK_TABLE (table_transport), spamd_label, 0, 1, 2, 3,
			(GtkAttachOptions) (GTK_FILL),
			(GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify(GTK_LABEL(spamd_label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC(spamd_label), 1, 0.5);

	hbox_spamd = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox_spamd);
	gtk_table_attach (GTK_TABLE (table_transport), hbox_spamd, 1, 2, 2, 3,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 0, 0);

	spamd_hostname_entry = gtk_entry_new();
	gtk_widget_show(spamd_hostname_entry);
	gtk_box_pack_start(GTK_BOX(hbox_spamd), spamd_hostname_entry, TRUE, TRUE, 0);
	gtk_tooltips_set_tip(tooltips, spamd_hostname_entry,
			_("Hostname or IP address of spamd server"), NULL);

	spamd_colon_label = gtk_label_new(":");
	gtk_widget_show(spamd_colon_label);
	gtk_box_pack_start(GTK_BOX(hbox_spamd), spamd_colon_label, FALSE, FALSE, 0);

	spamd_port_spinbtn_adj = gtk_adjustment_new(783, 1, 65535, 1, 10, 10);
	spamd_port_spinbtn = gtk_spin_button_new(GTK_ADJUSTMENT(spamd_port_spinbtn_adj), 1, 0);
	gtk_widget_show(spamd_port_spinbtn);
	gtk_box_pack_start(GTK_BOX(hbox_spamd), spamd_port_spinbtn, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(tooltips, spamd_port_spinbtn,
			_("Port of spamd server"), NULL);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spamd_port_spinbtn), TRUE);

	spamd_socket_entry = gtk_entry_new();
	gtk_widget_show(spamd_socket_entry);
	gtk_box_pack_start(GTK_BOX(hbox_spamd), spamd_socket_entry, TRUE, TRUE, 0);
	gtk_tooltips_set_tip(tooltips, spamd_socket_entry, _("Path of Unix socket"),
			NULL);

	hbox_max_size = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox_max_size);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_max_size, TRUE, TRUE, 0);

	max_size_label = gtk_label_new(_("Maximum size"));
	gtk_widget_show(max_size_label);
	gtk_box_pack_start(GTK_BOX(hbox_max_size), max_size_label, FALSE, FALSE, 0);

	max_size_spinbtn_adj = gtk_adjustment_new(250, 0, 10000, 10, 10, 10);
	max_size_spinbtn = gtk_spin_button_new(GTK_ADJUSTMENT(max_size_spinbtn_adj), 1, 0);
	gtk_widget_show(max_size_spinbtn);
	gtk_box_pack_start(GTK_BOX(hbox_max_size), max_size_spinbtn, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(tooltips, max_size_spinbtn,
			_("Messages larger than this will not be checked"), NULL);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(max_size_spinbtn), TRUE);

	max_size_kb_label = gtk_label_new(_("kB"));
	gtk_widget_show(max_size_kb_label);
	gtk_box_pack_start(GTK_BOX(hbox_max_size), max_size_kb_label, FALSE, FALSE, 0);

	hbox_timeout = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox_timeout);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_timeout, TRUE, TRUE, 0);

	timeout_label = gtk_label_new(_("Timeout"));
	gtk_widget_show(timeout_label);
	gtk_box_pack_start(GTK_BOX(hbox_timeout), timeout_label, FALSE, FALSE, 0);

	timeout_spinbtn_adj = gtk_adjustment_new(60, 0, 10000, 10, 10, 10);
	timeout_spinbtn = gtk_spin_button_new(GTK_ADJUSTMENT(timeout_spinbtn_adj), 1, 0);
	gtk_widget_show(timeout_spinbtn);
	gtk_box_pack_start(GTK_BOX(hbox_timeout), timeout_spinbtn, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(tooltips, timeout_spinbtn,
			_("Maximum time allowed for checking. If the check takes longer "
				"it will be aborted."),
			NULL);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(timeout_spinbtn), TRUE);

	timeout_seconds_label = gtk_label_new(_("seconds"));
	gtk_widget_show(timeout_seconds_label);
	gtk_box_pack_start(GTK_BOX(hbox_timeout), timeout_seconds_label, FALSE, FALSE, 0);

	hbox_process_emails = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox_process_emails);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_process_emails, TRUE, TRUE, 0);

	process_emails_checkbtn = gtk_check_button_new_with_label(
			_("Process messages on receiving"));
	gtk_widget_show(process_emails_checkbtn);
	gtk_box_pack_start(GTK_BOX(hbox_process_emails), process_emails_checkbtn, TRUE, TRUE, 0);

	hbox_save_spam = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox_save_spam);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_save_spam, TRUE, TRUE, 0);

	save_spam_checkbtn = gtk_check_button_new_with_label(_("Save spam in"));
	gtk_widget_show(save_spam_checkbtn);
	gtk_box_pack_start(GTK_BOX(hbox_save_spam), save_spam_checkbtn, FALSE, FALSE, 0);

	save_spam_folder_entry = gtk_entry_new();
	gtk_widget_show (save_spam_folder_entry);
	gtk_box_pack_start (GTK_BOX (hbox_save_spam), save_spam_folder_entry, TRUE, TRUE, 0);
	gtk_tooltips_set_tip(tooltips, save_spam_folder_entry,
			_("Folder for storing identified spam. Leave empty to use the trash folder."),
			NULL);

	save_spam_folder_select = gtkut_get_browse_directory_btn(_("_Browse"));
	gtk_widget_show (save_spam_folder_select);
	gtk_box_pack_start (GTK_BOX (hbox_save_spam), save_spam_folder_select, FALSE, FALSE, 0);
	gtk_tooltips_set_tip(tooltips, save_spam_folder_select,
			_("Click this button to select a folder for storing spam"),
			NULL);

	SET_TOGGLE_SENSITIVITY(enable_sa_checkbtn, frame_transport);
	SET_TOGGLE_SENSITIVITY(enable_sa_checkbtn, hbox_max_size);
	SET_TOGGLE_SENSITIVITY(enable_sa_checkbtn, hbox_timeout);
	SET_TOGGLE_SENSITIVITY(enable_sa_checkbtn, hbox_save_spam);
	SET_TOGGLE_SENSITIVITY(save_spam_checkbtn, save_spam_folder_entry);
	SET_TOGGLE_SENSITIVITY(save_spam_checkbtn, save_spam_folder_select);
	SET_TOGGLE_SENSITIVITY(enable_sa_checkbtn, hbox_process_emails);

	config = spamassassin_get_config();

	g_signal_connect(G_OBJECT(save_spam_folder_select), "clicked",
			G_CALLBACK(foldersel_cb), page);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_sa_checkbtn), config->enable);
	if (config->username != NULL)
		gtk_entry_set_text(GTK_ENTRY(user_entry), config->username);
	if (config->hostname != NULL)
		gtk_entry_set_text(GTK_ENTRY(spamd_hostname_entry), config->hostname);
	if (config->socket != NULL)
		gtk_entry_set_text(GTK_ENTRY(spamd_socket_entry), config->socket);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spamd_port_spinbtn), (float) config->port);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(max_size_spinbtn), (float) config->max_size);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(timeout_spinbtn), (float) config->timeout);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(process_emails_checkbtn), config->process_emails);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(save_spam_checkbtn), config->receive_spam);
	if (config->save_folder != NULL)
		gtk_entry_set_text(GTK_ENTRY(save_spam_folder_entry), config->save_folder);

	page->enable_sa_checkbtn = enable_sa_checkbtn;
	page->transport_label = transport_label;
	page->transport_optmenu = transport_optmenu;
	page->username = user_entry;
	page->hostname = spamd_hostname_entry;
	page->colon = spamd_colon_label;
	page->port = spamd_port_spinbtn;
	page->socket = spamd_socket_entry;
	page->max_size = max_size_spinbtn;
	page->timeout = timeout_spinbtn;
	page->process_emails = process_emails_checkbtn;
	page->receive_spam = save_spam_checkbtn;
	page->save_folder = save_spam_folder_entry;
	page->save_folder_select = save_spam_folder_select;

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
		} else if (config->transport == SPAMASSASSIN_DISABLED 
			&& transports[i].transport == SPAMASSASSIN_TRANSPORT_LOCALHOST) {
			show_transport(page, &transports[i]);
			active = i;
			/* and disable via new way */
			config->enable = FALSE;
			gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON(enable_sa_checkbtn), 
				config->enable);
		}
	}
	gtk_option_menu_set_menu(GTK_OPTION_MENU(transport_optmenu), transport_menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(transport_optmenu), active);

	page->page.widget = vbox1;
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
	config->enable = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->enable_sa_checkbtn));
	config->transport = page->trans;

	/* username */
	g_free(config->username);
	config->username = gtk_editable_get_chars(GTK_EDITABLE(page->username), 0, -1);
	spamassassin_check_username();

	/* hostname */
	g_free(config->hostname);
	config->hostname = gtk_editable_get_chars(GTK_EDITABLE(page->hostname), 0, -1);

	/* port */
	config->port = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(page->port));

	/* hostname */
	g_free(config->socket);
	config->socket = gtk_editable_get_chars(GTK_EDITABLE(page->socket), 0, -1);

	/* process_emails */
	config->process_emails = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->process_emails));

	/* receive_spam */
	config->receive_spam = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->receive_spam));

	/* save_folder */
	g_free(config->save_folder);
	config->save_folder = gtk_editable_get_chars(GTK_EDITABLE(page->save_folder), 0, -1);

	/* max_size */
	config->max_size = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(page->max_size));

	/* timeout */
	config->timeout = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(page->timeout));

	if (config->process_emails) {
		spamassassin_register_hook();
	} else {
		spamassassin_unregister_hook();
	}

	if (!config->enable) {
		procmsg_unregister_spam_learner(spamassassin_learn);
		procmsg_spam_set_folder(NULL);
	} else {
		if (config->transport == SPAMASSASSIN_TRANSPORT_TCP)
			debug_print("enabling learner with a remote spamassassin server requires spamc/spamd 3.1.x\n");
		procmsg_register_spam_learner(spamassassin_learn);
		procmsg_spam_set_folder(config->save_folder);
	}

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
