/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto
 * This file (C) 2004 Colin Leroy
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
#include "defs.h"

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtkentry.h>
#include <gtk/gtklabel.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtk.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "intl.h"
#include "utils.h"
#include "gtk/menu.h"
#include "prefs_account.h"
#include "mainwindow.h"
#include "stock_pixmap.h"
#include "setup.h"

typedef enum
{
	GO_BACK,
	GO_FORWARD,
	CANCEL,
	FINISHED
} PageNavigation;

typedef struct
{
	GtkWidget *window;
	GSList    *pages;
	GtkWidget *notebook;

	MainWindow *mainwin;
	
	GtkWidget *email;
	GtkWidget *full_name;
	GtkWidget *organization;

	GtkWidget *mailbox_name;
	
	GtkWidget *smtp_server;
	GtkWidget *smtp_port;
	
	GtkWidget *recv_type;
	GtkWidget *recv_server;
	GtkWidget *recv_port;
	GtkWidget *recv_username;
	GtkWidget *recv_password;
	
	gboolean create_mailbox;
	gboolean finished;
} WizardWindow;

static void wizard_write_config(WizardWindow *wizard)
{
	gboolean mailbox_ok = FALSE;
	PrefsAccount *prefs_account = prefs_account_new();
	GList *account_list = NULL;
	
	if (wizard->create_mailbox) {
		mailbox_ok = setup_write_mailbox_path(wizard->mainwin, 
				gtk_entry_get_text(GTK_ENTRY(wizard->mailbox_name)));
	}

	prefs_account->account_name = g_strdup(
				gtk_entry_get_text(GTK_ENTRY(wizard->recv_server)));
	prefs_account->name = g_strdup(
				gtk_entry_get_text(GTK_ENTRY(wizard->full_name)));
	prefs_account->address = g_strdup(
				gtk_entry_get_text(GTK_ENTRY(wizard->email)));
	prefs_account->organization = g_strdup(
				gtk_entry_get_text(GTK_ENTRY(wizard->organization)));
	prefs_account->smtp_server = g_strdup(
				gtk_entry_get_text(GTK_ENTRY(wizard->smtp_server)));
	prefs_account->recv_server = g_strdup(
				gtk_entry_get_text(GTK_ENTRY(wizard->recv_server)));
	prefs_account->userid = g_strdup(
				gtk_entry_get_text(GTK_ENTRY(wizard->recv_username)));
	prefs_account->passwd = g_strdup(
				gtk_entry_get_text(GTK_ENTRY(wizard->recv_password)));
	prefs_account->protocol = GPOINTER_TO_INT
			(g_object_get_data(G_OBJECT(wizard->recv_type), 
					   MENU_VAL_ID));
	
	account_list = g_list_append(account_list, prefs_account);
	prefs_account_write_config_all(account_list);
	prefs_account_free(prefs_account);
}

static GtkWidget* create_page (WizardWindow *wizard, const char * title)
{
	GtkWidget *w;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *image;
	char *title_string;

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_container_set_border_width  (GTK_CONTAINER(vbox), 10);

	/* create the titlebar */
	hbox = gtk_hbox_new (FALSE, 12);
	image = stock_pixmap_widget(wizard->window, 
			  	STOCK_PIXMAP_SYLPHEED_ICON);
	gtk_box_pack_start (GTK_BOX(hbox), image, FALSE, FALSE, 0);
     	title_string = g_strconcat ("<span size=\"xx-large\" weight=\"ultrabold\">", title ? title : "", "</span>", NULL);
	w = gtk_label_new (title_string);
	gtk_label_set_use_markup (GTK_LABEL(w), TRUE);
	g_free (title_string);
	gtk_box_pack_start (GTK_BOX(hbox), w, FALSE, FALSE, 0);

	/* pack the titlebar */
	gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	/* pack the separator */
	gtk_box_pack_start (GTK_BOX(vbox), gtk_hseparator_new(), FALSE, FALSE, 0);

	/* pack space */
	w = gtk_alignment_new (0, 0, 0, 0);
	gtk_widget_set_usize (w, 0, 6);
	gtk_box_pack_start (GTK_BOX(vbox), w, FALSE, FALSE, 0);

	return vbox;
}

static GtkWidget*
create_page_with_text (WizardWindow *wizard, const char * title, 
			const char * text)
{
	GtkWidget *label;
	GtkWidget *page;
       
	page = create_page (wizard, title);
	label = gtk_label_new (text);
	gtk_box_pack_start (GTK_BOX(page), label, TRUE, TRUE, 0);

	return page;
}

#define GTK_TABLE_ADD_ROW_AT(table,text,entry,i) { 			      \
	GtkWidget *label = gtk_label_new(text);				      \
	gtk_table_attach(GTK_TABLE(table), label, 			      \
			 0,1,i,i+1, GTK_EXPAND|GTK_FILL, 0, 0, 0);	      \
	gtk_misc_set_alignment(GTK_MISC(label), 1, 0.5);		      \
	gtk_table_attach(GTK_TABLE(table), entry, 			      \
			 1,2,i,i+1, GTK_EXPAND|GTK_FILL, 0, 0, 0);	      \
}

static gchar *get_default_email_addr(void)
{
	gchar *domain_name = g_strdup(get_domain_name());
	gchar *result;
	if (strchr(domain_name, '.') != strrchr(domain_name, '.')
	&& strlen(strchr(domain_name, '.')) > 6) {
		gchar *tmp = g_strdup(strchr(domain_name, '.')+1);
		g_free(domain_name);
		domain_name = tmp;
	}
	result = g_strdup_printf("%s@%s",
				g_get_user_name(),
				domain_name);
	g_free(domain_name);
	return result;
}

static GtkWidget* user_page (WizardWindow * wizard)
{
	GtkWidget *table = gtk_table_new(3,2, FALSE);
	gchar *text;
	gint i = 0;
	
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	wizard->full_name = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(wizard->full_name), g_get_real_name());
	GTK_TABLE_ADD_ROW_AT(table, _("Your name:"), 
			     wizard->full_name, i); i++;
	
	wizard->email = gtk_entry_new();
	text = get_default_email_addr();
	gtk_entry_set_text(GTK_ENTRY(wizard->email), text);
	g_free(text);
	GTK_TABLE_ADD_ROW_AT(table, _("Your email address:"), 
			     wizard->email, i); i++;
	
	wizard->organization = gtk_entry_new();
	GTK_TABLE_ADD_ROW_AT(table, _("Your organization:"), 
			     wizard->organization, i); i++;
	
	return table;
}

static GtkWidget* mailbox_page (WizardWindow * wizard)
{
	GtkWidget *table = gtk_table_new(1,2, FALSE);
	gchar *text;
	gint i = 0;
	
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	wizard->full_name = gtk_entry_new();
	wizard->mailbox_name = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(wizard->mailbox_name), "Mail");
	GTK_TABLE_ADD_ROW_AT(table, _("Mailbox name:"), 
			     wizard->mailbox_name, i); i++;
	
	return table;
}

static gchar *get_default_server(const gchar *type)
{
	gchar *domain_name = g_strdup(get_domain_name());
	gchar *result;
	if (strchr(domain_name, '.') != strrchr(domain_name, '.')
	&& strlen(strchr(domain_name, '.')) > 6) {
		gchar *tmp = g_strdup(strchr(domain_name, '.')+1);
		g_free(domain_name);
		domain_name = tmp;
	}
	result = g_strdup_printf("%s.%s",
				type, domain_name);
	g_free(domain_name);
	return result;
}

static GtkWidget* smtp_page (WizardWindow * wizard)
{
	GtkWidget *table = gtk_table_new(2,2, FALSE);
	gchar *text;
	gint i = 0;
	
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	wizard->full_name = gtk_entry_new();
	wizard->smtp_server = gtk_entry_new();
	text = get_default_server("smtp");
	gtk_entry_set_text(GTK_ENTRY(wizard->smtp_server), text);
	g_free(text);
	GTK_TABLE_ADD_ROW_AT(table, _("SMTP server address:"), 
			     wizard->smtp_server, i); i++;
	
	wizard->smtp_port = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(wizard->smtp_port), "25");
	GTK_TABLE_ADD_ROW_AT(table, _("SMTP port:"), 
			     wizard->smtp_port, i); i++;
	
	return table;
}

static GtkWidget* recv_page (WizardWindow * wizard)
{
	GtkWidget *table = gtk_table_new(5,2, FALSE);
	GtkWidget *menu = gtk_menu_new();
	GtkWidget *menuitem;
	gchar *text;
	gint i = 0;
	
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	wizard->full_name = gtk_entry_new();
	wizard->recv_type = gtk_option_menu_new();
	
	MENUITEM_ADD (menu, menuitem, _("POP3"), A_POP3);
	MENUITEM_ADD (menu, menuitem, _("IMAP"), A_IMAP4);
	gtk_option_menu_set_menu (GTK_OPTION_MENU (wizard->recv_type), menu);
	GTK_TABLE_ADD_ROW_AT(table, _("Server type:"), 
			     wizard->recv_type, i); i++;

	wizard->recv_server = gtk_entry_new();
	text = get_default_server("pop");
	gtk_entry_set_text(GTK_ENTRY(wizard->recv_server), text);
	g_free(text);
	GTK_TABLE_ADD_ROW_AT(table, _("Server address:"), 
			     wizard->recv_server, i); i++;
	
	wizard->recv_port = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(wizard->recv_port), "110");
	GTK_TABLE_ADD_ROW_AT(table, _("Port:"), 
			     wizard->recv_port, i); i++;
	
	wizard->recv_username = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(wizard->recv_username), g_get_user_name());
	GTK_TABLE_ADD_ROW_AT(table, _("Username:"), 
			     wizard->recv_username, i); i++;
	
	wizard->recv_password = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(wizard->recv_password), FALSE);
	GTK_TABLE_ADD_ROW_AT(table, _("Password:"), 
			     wizard->recv_password, i); i++;
	
	return table;
}

static void
wizard_response_cb (GtkDialog * dialog, int response, gpointer data)
{
	WizardWindow * wizard = (WizardWindow *)data;
	int current_page, num_pages;
	
	num_pages = g_slist_length(wizard->pages);
 	current_page = gtk_notebook_get_current_page (
				GTK_NOTEBOOK(wizard->notebook));
	if (response == CANCEL)
	{
		wizard->finished = TRUE;
		gtk_widget_destroy (GTK_WIDGET(dialog));
	}
	else if (response == FINISHED)
	{
		wizard_write_config(wizard);
		wizard->finished = TRUE;
		gtk_widget_destroy (GTK_WIDGET(dialog));
	}
	else
	{
		if (response == GO_BACK)
		{
			if (current_page > 0)
				gtk_notebook_set_current_page (
					GTK_NOTEBOOK(wizard->notebook), 
					--current_page);
		}
		else if (response == GO_FORWARD)
		{
			if (current_page < (num_pages-1))
				gtk_notebook_set_current_page (
					GTK_NOTEBOOK(wizard->notebook), 
					++current_page);
		}

		gtk_dialog_set_response_sensitive (dialog, GO_BACK, 
				current_page > 0);
		gtk_dialog_set_response_sensitive (dialog, GO_FORWARD, 
				current_page < (num_pages - 1));
		gtk_dialog_set_response_sensitive (dialog, FINISHED, 
				current_page == (num_pages - 1));
	}
}


gboolean run_wizard(MainWindow *mainwin, gboolean create_mailbox) {
	WizardWindow *wizard = g_new0(WizardWindow, 1);
	GtkWidget *page;
	GtkWidget *widget;
	gchar     *text;
	GSList     *cur;
	
	wizard->mainwin = mainwin;
	wizard->create_mailbox = create_mailbox;
	
	gtk_widget_hide(mainwin->window);
	
	wizard->window = gtk_dialog_new_with_buttons (_("New User"),
			NULL, 0, 
			GTK_STOCK_GO_BACK, GO_BACK,
			GTK_STOCK_GO_FORWARD, GO_FORWARD,
			GTK_STOCK_SAVE, FINISHED,
			GTK_STOCK_QUIT, CANCEL,
			NULL);

	g_signal_connect(wizard->window, "response", 
			  G_CALLBACK(wizard_response_cb), wizard);
	gtk_widget_realize(wizard->window);
	gtk_dialog_set_default_response(GTK_DIALOG(wizard->window), 
			GO_FORWARD);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(wizard->window), 
			GO_BACK, FALSE);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(wizard->window), 
			GO_FORWARD, TRUE);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(wizard->window), 
			FINISHED, FALSE);
	gtk_dialog_set_response_sensitive(GTK_DIALOG(wizard->window), 
			CANCEL, TRUE);
	
	wizard->notebook = gtk_notebook_new();
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(wizard->notebook), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(wizard->notebook), FALSE);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(wizard->window)->vbox), 
			    wizard->notebook, TRUE, TRUE, 0);
	
	wizard->pages = NULL;
	
/*welcome page */
	page = create_page(wizard, _("Welcome to Sylpheed-Claws."));
	
	wizard->pages = g_slist_append(wizard->pages, page);
	widget = stock_pixmap_widget(wizard->window, 
			  	STOCK_PIXMAP_SYLPHEED_LOGO);

	gtk_box_pack_start (GTK_BOX(page), widget, FALSE, FALSE, 0);
	
	text = g_strdup(_("Welcome to Sylpheed-Claws.\n\n"
			  "It looks like it's the first time you use \n"
			  "Sylpheed-Claws. So, we'll now define some basic\n"
			  "information about yourself and your most common\n"
			  "mail parameters; so that you can begin to use\n"
			  "Sylpheed-Claws in less than five minutes."));
	widget = gtk_label_new(text);
	gtk_box_pack_start (GTK_BOX(page), widget, FALSE, FALSE, 0);
	g_free(text);

/* user page */
	widget = create_page (wizard, _("About You"));
	gtk_box_pack_start (GTK_BOX(widget), user_page(wizard), FALSE, FALSE, 0);
	wizard->pages = g_slist_append(wizard->pages, widget);

/* mailbox page */
	if (create_mailbox) {
		widget = create_page (wizard, _("Saving mail on disk"));
		gtk_box_pack_start (GTK_BOX(widget), mailbox_page(wizard), FALSE, FALSE, 0);
		wizard->pages = g_slist_append(wizard->pages, widget);
	}
/*smtp page */
	widget = create_page (wizard, _("Sending mail"));
	gtk_box_pack_start (GTK_BOX(widget), smtp_page(wizard), FALSE, FALSE, 0);
	wizard->pages = g_slist_append(wizard->pages, widget);

/* recv+auth page */
	widget = create_page (wizard, _("Receiving mail"));
	gtk_box_pack_start (GTK_BOX(widget), recv_page(wizard), FALSE, FALSE, 0);
	wizard->pages = g_slist_append(wizard->pages, widget);

	for (cur = wizard->pages; cur && cur->data; cur = cur->next) {
		gtk_notebook_append_page (GTK_NOTEBOOK(wizard->notebook), 
					  GTK_WIDGET(cur->data), NULL);
	}
	
	gtk_widget_show_all (wizard->window);

	while (!wizard->finished)
		gtk_main_iteration();

	GTK_EVENTS_FLUSH();

	gtk_widget_show(mainwin->window);
	g_free(wizard);

	return TRUE;
}
