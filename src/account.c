/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2001 Hiroyuki Yamamoto
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
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <errno.h>

#include "intl.h"
#include "main.h"
#include "mainwindow.h"
#include "folderview.h"
#include "folder.h"
#include "account.h"
#include "prefs.h"
#include "prefs_account.h"
#include "compose.h"
#include "manage_window.h"
#include "inc.h"
#include "gtkutils.h"
#include "utils.h"
#include "alertpanel.h"

typedef enum
{
	COL_DEFAULT	= 0,
	COL_NAME	= 1,
	COL_PROTOCOL	= 2,
	COL_SERVER	= 3,
        COL_GETALL      = 4
} EditAccountColumnPos;

# define N_EDIT_ACCOUNT_COLS	5

#define PREFSBUFSIZE		1024

PrefsAccount *cur_account;

static GList *account_list = NULL;

static struct EditAccount {
	GtkWidget *window;
	GtkWidget *clist;
	GtkWidget *close_btn;
} edit_account;

static void account_edit_create		(void);

static void account_edit_prefs		(void);
static void account_delete		(void);

static void account_up			(void);
static void account_down		(void);

static void account_set_default		(void);

static void account_set_recv_at_get_all	(void);

static void account_edit_close		(void);
static gint account_delete_event	(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	 data);
static void account_selected		(GtkCList	*clist,
					 gint		 row,
					 gint		 column,
					 GdkEvent	*event,
					 gpointer	 data);
static void account_key_pressed		(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data);

static gint account_clist_set_row	(PrefsAccount	*ac_prefs,
					 gint		 row);
static void account_clist_set		(void);

static void account_list_set		(void);

void account_read_config_all(void)
{
	GSList *ac_label_list = NULL, *cur;
	gchar *rcpath;
	FILE *fp;
	gchar buf[PREFSBUFSIZE];
	PrefsAccount *ac_prefs;

	debug_print(_("Reading all config for each account...\n"));

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, ACCOUNT_RC, NULL);
	if ((fp = fopen(rcpath, "r")) == NULL) {
		if (ENOENT != errno) FILE_OP_ERROR(rcpath, "fopen");
		g_free(rcpath);
		return;
	}
	g_free(rcpath);

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (!strncmp(buf, "[Account: ", 10)) {
			strretchomp(buf);
			memmove(buf, buf + 1, strlen(buf));
			buf[strlen(buf) - 1] = '\0';
			debug_print(_("Found label: %s\n"), buf);
			ac_label_list = g_slist_append(ac_label_list,
						       g_strdup(buf));
		}
	}
	fclose(fp);

	/* read config data from file */
	cur_account = NULL;
	for (cur = ac_label_list; cur != NULL; cur = cur->next) {
		ac_prefs = g_new0(PrefsAccount, 1);
		prefs_account_read_config(ac_prefs, (gchar *)cur->data);
		account_list = g_list_append(account_list, ac_prefs);
		if (ac_prefs->is_default)
			cur_account = ac_prefs;
	}
	/* if default is not set, assume first account as default */
	if (!cur_account && account_list) {
		ac_prefs = (PrefsAccount *)account_list->data;
		account_set_as_default(ac_prefs);
		cur_account = ac_prefs;
	}

	account_set_menu();
	main_window_reflect_prefs_all();

	while (ac_label_list) {
		g_free(ac_label_list->data);
		ac_label_list = g_slist_remove(ac_label_list,
					       ac_label_list->data);
	}
}

void account_save_config_all(void)
{
	prefs_account_save_config_all(account_list);
}

PrefsAccount *account_find_from_smtp_server(const gchar *address,
					    const gchar *smtp_server)
{
	GList *cur;
	PrefsAccount *ac;

	for (cur = account_list; cur != NULL; cur = cur->next) {
		ac = (PrefsAccount *)cur->data;
		if (!strcmp2(address, ac->address) &&
		    !strcmp2(smtp_server, ac->smtp_server))
			return ac;
	}

	return NULL;
}

/*
 * account_find_from_address:
 * @address: Email address string.
 *
 * Find a mail (not news) account with the specified email address.
 *
 * Return value: The found account, or NULL if not found.
 */
PrefsAccount *account_find_from_address(const gchar *address)
{
	GList *cur;
	PrefsAccount *ac;

	for (cur = account_list; cur != NULL; cur = cur->next) {
		ac = (PrefsAccount *)cur->data;
		if (ac->protocol != A_NNTP && !strcmp2(address, ac->address))
			return ac;
	}

	return NULL;
}

PrefsAccount *account_find_from_id(gint id)
{
	GList *cur;
	PrefsAccount *ac;

	for (cur = account_list; cur != NULL; cur = cur->next) {
		ac = (PrefsAccount *)cur->data;
		if (id == ac->account_id)
			return ac;
	}

	return NULL;
}

void account_set_menu(void)
{
	main_window_set_account_menu(account_list);
}

void account_foreach(AccountFunc func, gpointer user_data)
{
	GList *cur;

	for (cur = account_list; cur != NULL; cur = cur->next)
		if (func((PrefsAccount *)cur->data, user_data) != 0)
			return;
}

GList *account_get_list(void)
{
	return account_list;
}

void account_edit_open(void)
{
	inc_autocheck_timer_remove();

	if (compose_get_compose_list()) {
		alertpanel_notice(_("Some composing windows are open.\n"
				    "Please close all the composing windows before editing the accounts."));
		inc_autocheck_timer_set();
		return;
	}

	debug_print(_("Opening account edit window...\n"));

	if (!edit_account.window)
		account_edit_create();

	account_clist_set();

	manage_window_set_transient(GTK_WINDOW(edit_account.window));
	gtk_widget_grab_focus(edit_account.close_btn);
	gtk_widget_show(edit_account.window);

	manage_window_focus_in(edit_account.window, NULL, NULL);
}

void account_add(void)
{
	PrefsAccount *ac_prefs;

	ac_prefs = prefs_account_open(NULL);
	inc_autocheck_timer_remove();

	if (!ac_prefs) return;

	account_list = g_list_append(account_list, ac_prefs);

	if (ac_prefs->is_default)
		account_set_as_default(ac_prefs);

	if (ac_prefs->recv_at_getall)
		account_set_as_recv_at_get_all(ac_prefs);

	account_clist_set();

	if (ac_prefs->protocol == A_IMAP4 || ac_prefs->protocol == A_NNTP) {
		Folder *folder;

		if (ac_prefs->protocol == A_IMAP4) {
			folder = folder_new(F_IMAP, ac_prefs->account_name,
					    ac_prefs->recv_server);
		} else {
			folder = folder_new(F_NEWS, ac_prefs->account_name,
					    ac_prefs->nntp_server);
		}

		folder->account = ac_prefs;
		ac_prefs->folder = REMOTE_FOLDER(folder);
		folder_add(folder);
		if (ac_prefs->protocol == A_IMAP4)
			folder->create_tree(folder);
		folderview_set_all();
	}
}

void account_set_as_default(PrefsAccount *ac_prefs)
{
	PrefsAccount *ap;
	GList *cur;

	for (cur = account_list; cur != NULL; cur = cur->next) {
		ap = (PrefsAccount *)cur->data;
		if (ap->is_default)
			ap->is_default = FALSE;
	}

	ac_prefs->is_default = TRUE;
}

PrefsAccount *account_get_default(void)
{
	PrefsAccount *ap;
	GList *cur;

	for (cur = account_list; cur != NULL; cur = cur->next) {
		ap = (PrefsAccount *)cur->data;
		if (ap->is_default)
			return ap;
	}

	return NULL;
}

void account_set_as_recv_at_get_all(PrefsAccount *ac_prefs)
{
	PrefsAccount *ap;
	GList *cur;

	for (cur = account_list; cur != NULL; cur = cur->next) {
 		ap = (PrefsAccount *)cur->data;
 		if (ap->account_name == ac_prefs->account_name) {
			if (ap->recv_at_getall == 0)
				ap->recv_at_getall = 1;
			else
				ap->recv_at_getall = 0;
		}
        }

}


void account_set_missing_folder(void)
{
	PrefsAccount *ap;
	GList *cur;

	for (cur = account_list; cur != NULL; cur = cur->next) {
		ap = (PrefsAccount *)cur->data;
		if ((ap->protocol == A_IMAP4 || ap->protocol == A_NNTP) &&
		    !ap->folder) {
			Folder *folder;

			if (ap->protocol == A_IMAP4) {
				folder = folder_new(F_IMAP, ap->account_name,
						    ap->recv_server);
			} else {
				folder = folder_new(F_NEWS, ap->account_name,
						    ap->nntp_server);
			}

			folder->account = ap;
			ap->folder = REMOTE_FOLDER(folder);
			folder_add(folder);
			if (ap->protocol == A_IMAP4)
				folder->create_tree(folder);
		}
	}
}

void account_destroy(PrefsAccount *ac_prefs)
{
	g_return_if_fail(ac_prefs != NULL);

	prefs_account_free(ac_prefs);
	account_list = g_list_remove(account_list, ac_prefs);

	if (cur_account == ac_prefs) cur_account = NULL;
	if (!cur_account && account_list) {
		cur_account = account_get_default();
		if (!cur_account) {
			ac_prefs = (PrefsAccount *)account_list->data;
			account_set_as_default(ac_prefs);
			cur_account = ac_prefs;
		}
	}
}


static void account_edit_create(void)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *scrolledwin;
	GtkWidget *clist;
	gchar *titles[N_EDIT_ACCOUNT_COLS];
	gint i;

	GtkWidget *vbox2;
	GtkWidget *add_btn;
	GtkWidget *edit_btn;
	GtkWidget *del_btn;
	GtkWidget *up_btn;
	GtkWidget *down_btn;

	GtkWidget *default_btn;

        GtkWidget *recvatgetall_btn;

	GtkWidget *hbbox;
	GtkWidget *close_btn;

	debug_print(_("Creating account edit window...\n"));

	window = gtk_window_new (GTK_WINDOW_DIALOG);
	gtk_widget_set_usize (window, 500, 320);
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_window_set_title (GTK_WINDOW (window), _("Edit accounts"));
	gtk_window_set_modal (GTK_WINDOW (window), TRUE);
	gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			    GTK_SIGNAL_FUNC (account_delete_event), NULL);
	gtk_signal_connect (GTK_OBJECT (window), "key_press_event",
			    GTK_SIGNAL_FUNC (account_key_pressed), NULL);
	gtk_signal_connect (GTK_OBJECT (window), "focus_in_event",
			    GTK_SIGNAL_FUNC (manage_window_focus_in), NULL);
	gtk_signal_connect (GTK_OBJECT (window), "focus_out_event",
			    GTK_SIGNAL_FUNC (manage_window_focus_out), NULL);

	vbox = gtk_vbox_new (FALSE, 12);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);

	scrolledwin = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolledwin);
	gtk_box_pack_start (GTK_BOX (hbox), scrolledwin, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwin),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	titles[COL_DEFAULT]  = "";
	titles[COL_NAME]     = _("Name");
	titles[COL_PROTOCOL] = _("Protocol");
	titles[COL_SERVER]   = _("Server");
        titles[COL_GETALL]   = _("Get all");

	clist = gtk_clist_new_with_titles (N_EDIT_ACCOUNT_COLS, titles);
	gtk_widget_show (clist);
	gtk_container_add (GTK_CONTAINER (scrolledwin), clist);
	gtk_clist_set_column_width (GTK_CLIST(clist), COL_DEFAULT , 16);
	gtk_clist_set_column_width (GTK_CLIST(clist), COL_NAME    , 100);
	gtk_clist_set_column_width (GTK_CLIST(clist), COL_PROTOCOL, 70);
	gtk_clist_set_column_width (GTK_CLIST(clist), COL_SERVER  , 100);
	gtk_clist_set_column_width (GTK_CLIST(clist), COL_GETALL  , 20);
	gtk_clist_set_selection_mode (GTK_CLIST(clist), GTK_SELECTION_BROWSE);

	for (i = 0; i < N_EDIT_ACCOUNT_COLS; i++)
		GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(clist)->column[i].button,
				       GTK_CAN_FOCUS);

	gtk_signal_connect (GTK_OBJECT (clist), "select_row",
			    GTK_SIGNAL_FUNC (account_selected), NULL);

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (hbox), vbox2, FALSE, FALSE, 0);

	add_btn = gtk_button_new_with_label (_("Add"));
	gtk_widget_show (add_btn);
	gtk_box_pack_start (GTK_BOX (vbox2), add_btn, FALSE, FALSE, 4);
	gtk_signal_connect (GTK_OBJECT(add_btn), "clicked",
			    GTK_SIGNAL_FUNC (account_add), NULL);

	edit_btn = gtk_button_new_with_label (_("Edit"));
	gtk_widget_show (edit_btn);
	gtk_box_pack_start (GTK_BOX (vbox2), edit_btn, FALSE, FALSE, 4);
	gtk_signal_connect (GTK_OBJECT(edit_btn), "clicked",
			    GTK_SIGNAL_FUNC (account_edit_prefs), NULL);

	del_btn = gtk_button_new_with_label (_(" Delete "));
	gtk_widget_show (del_btn);
	gtk_box_pack_start (GTK_BOX (vbox2), del_btn, FALSE, FALSE, 4);
	gtk_signal_connect (GTK_OBJECT(del_btn), "clicked",
			    GTK_SIGNAL_FUNC (account_delete), NULL);

	down_btn = gtk_button_new_with_label (_("Down"));
	gtk_widget_show (down_btn);
	gtk_box_pack_end (GTK_BOX (vbox2), down_btn, FALSE, FALSE, 4);
	gtk_signal_connect (GTK_OBJECT(down_btn), "clicked",
			    GTK_SIGNAL_FUNC (account_down), NULL);

	up_btn = gtk_button_new_with_label (_("Up"));
	gtk_widget_show (up_btn);
	gtk_box_pack_end (GTK_BOX (vbox2), up_btn, FALSE, FALSE, 4);
	gtk_signal_connect (GTK_OBJECT(up_btn), "clicked",
			    GTK_SIGNAL_FUNC (account_up), NULL);

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	vbox2 = gtk_vbox_new(FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (hbox), vbox2, FALSE, FALSE, 0);

	default_btn = gtk_button_new_with_label (_(" Set as default account "));
	gtk_widget_show (default_btn);
	gtk_box_pack_start (GTK_BOX (vbox2), default_btn, TRUE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT(default_btn), "clicked",
			    GTK_SIGNAL_FUNC (account_set_default), NULL);

	recvatgetall_btn = gtk_button_new_with_label (_(" Enable/Disable 'Receive at Get all' "));
	gtk_widget_show (recvatgetall_btn);
	gtk_box_pack_start (GTK_BOX (vbox2), recvatgetall_btn, TRUE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT(recvatgetall_btn), "clicked",
			    GTK_SIGNAL_FUNC (account_set_recv_at_get_all), NULL);

        gtkut_button_set_create(&hbbox, &close_btn, _("Close"),
				NULL, NULL, NULL, NULL);
	gtk_widget_show(hbbox);
	gtk_box_pack_end (GTK_BOX (hbox), hbbox, FALSE, FALSE, 0);
	gtk_widget_grab_default (close_btn);

	gtk_signal_connect (GTK_OBJECT (close_btn), "clicked",
			    GTK_SIGNAL_FUNC (account_edit_close),
			    NULL);

	edit_account.window    = window;
	edit_account.clist     = clist;
	edit_account.close_btn = close_btn;
}

static void account_edit_prefs(void)
{
	GtkCList *clist = GTK_CLIST(edit_account.clist);
	PrefsAccount *ac_prefs;
	gint row;
	gboolean prev_default;
	gchar *ac_name;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	ac_prefs = gtk_clist_get_row_data(clist, row);
	prev_default = ac_prefs->is_default;
	Xstrdup_a(ac_name, ac_prefs->account_name, return);

	prefs_account_open(ac_prefs);
	inc_autocheck_timer_remove();

	if (!prev_default && ac_prefs->is_default)
		account_set_as_default(ac_prefs);

	if ((ac_prefs->protocol == A_IMAP4 || ac_prefs->protocol == A_NNTP) &&
	    ac_prefs->folder && strcmp(ac_name, ac_prefs->account_name) != 0) {
		folder_set_name(FOLDER(ac_prefs->folder),
				ac_prefs->account_name);
		folderview_update_all();
	}

	account_clist_set();
}

static void account_delete(void)
{
	GtkCList *clist = GTK_CLIST(edit_account.clist);
	PrefsAccount *ac_prefs;
	gint row;

	if (!clist->selection) return;

	if (alertpanel(_("Delete account"),
		       _("Do you really want to delete this account?"),
		       _("Yes"), _("+No"), NULL) != G_ALERTDEFAULT)
		return;

	row = GPOINTER_TO_INT(clist->selection->data);
	ac_prefs = gtk_clist_get_row_data(clist, row);
	if (ac_prefs->folder) {
		folder_destroy(FOLDER(ac_prefs->folder));
		folderview_update_all();
	}
	account_destroy(ac_prefs);
	account_clist_set();
}

static void account_up(void)
{
	GtkCList *clist = GTK_CLIST(edit_account.clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row > 0) {
		gtk_clist_row_move(clist, row, row - 1);
		account_list_set();
	}
}

static void account_down(void)
{
	GtkCList *clist = GTK_CLIST(edit_account.clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row < clist->rows - 1) {
		gtk_clist_row_move(clist, row, row + 1);
		account_list_set();
	}
}

static void account_set_default(void)
{
	GtkCList *clist = GTK_CLIST(edit_account.clist);
	gint row;
	PrefsAccount *ac_prefs;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	ac_prefs = gtk_clist_get_row_data(clist, row);
	account_set_as_default(ac_prefs);
	account_clist_set();

	cur_account = ac_prefs;
	account_set_menu();
	main_window_reflect_prefs_all();
}

static void account_set_recv_at_get_all(void)
{
	GtkCList *clist = GTK_CLIST(edit_account.clist);
	gint row;
	PrefsAccount *ac_prefs;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	ac_prefs = gtk_clist_get_row_data(clist, row);
	account_set_as_recv_at_get_all(ac_prefs);
	account_clist_set();
}

static void account_edit_close(void)
{
	account_list_set();
	account_save_config_all();

	if (!cur_account && account_list) {
		PrefsAccount *ac_prefs = (PrefsAccount *)account_list->data;
		account_set_as_default(ac_prefs);
		cur_account = ac_prefs;
	}

	account_set_menu();
	main_window_reflect_prefs_all();

	gtk_widget_hide(edit_account.window);

	inc_autocheck_timer_set();
}

static gint account_delete_event(GtkWidget *widget, GdkEventAny *event,
				 gpointer data)
{
	account_edit_close();
	return TRUE;
}

static void account_selected(GtkCList *clist, gint row, gint column,
			     GdkEvent *event, gpointer data)
{
	if (event && event->type == GDK_2BUTTON_PRESS)
		account_edit_prefs();
}

static void account_key_pressed(GtkWidget *widget, GdkEventKey *event,
				gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		account_edit_close();
}

/* set one CList row or add new row */
static gint account_clist_set_row(PrefsAccount *ac_prefs, gint row)
{
	GtkCList *clist = GTK_CLIST(edit_account.clist);
	gchar *text[N_EDIT_ACCOUNT_COLS];

	text[COL_DEFAULT] = ac_prefs->is_default ? "*" : "";
	text[COL_NAME] = ac_prefs->account_name;
#if !USE_SSL
	text[COL_PROTOCOL] = ac_prefs->protocol == A_POP3  ? "POP3"  :
			     ac_prefs->protocol == A_APOP  ? "APOP"  :
			     ac_prefs->protocol == A_IMAP4 ? "IMAP4" :
			     ac_prefs->protocol == A_LOCAL ? "Local" :
			     ac_prefs->protocol == A_NNTP  ? "NNTP"  :  "";
#else
	text[COL_PROTOCOL] = ac_prefs->protocol == A_POP3  ? (!ac_prefs->ssl_pop ? "POP3" : "POP3 (SSL)") :
			     ac_prefs->protocol == A_APOP  ? (!ac_prefs->ssl_pop ? "APOP" : "APOP (SSL)") :
			     ac_prefs->protocol == A_IMAP4 ? (!ac_prefs->ssl_imap ? "IMAP4" : "IMAP4 (SSL)") :
			     ac_prefs->protocol == A_LOCAL ? "Local" :
			     ac_prefs->protocol == A_NNTP  ? "NNTP"  :  "";
#endif
	text[COL_SERVER] = ac_prefs->protocol == A_NNTP
		? ac_prefs->nntp_server : ac_prefs->recv_server;

  	if (!ac_prefs->protocol == A_POP3) {
	text[COL_GETALL] = ac_prefs->recv_at_getall == 1  ? _("No")  :
	                   ac_prefs->recv_at_getall == 0  ? _("No")   : "";
        }
        else {
	text[COL_GETALL] = ac_prefs->recv_at_getall == 1  ? _("Yes")  :
	                   ac_prefs->recv_at_getall == 0  ? _("No")   : "";
	}

	if (row < 0)
		row = gtk_clist_append(clist, text);
	else {
		gtk_clist_set_text(clist, row, COL_DEFAULT, text[COL_DEFAULT]);
		gtk_clist_set_text(clist, row, COL_NAME, text[COL_NAME]);
		gtk_clist_set_text(clist, row, COL_PROTOCOL, text[COL_PROTOCOL]);
		gtk_clist_set_text(clist, row, COL_SERVER, text[COL_SERVER]);
		gtk_clist_set_text(clist, row, COL_GETALL, text[COL_GETALL]);
	}

	gtk_clist_set_row_data(clist, row, ac_prefs);

	return row;
}

/* set CList from account list */
static void account_clist_set(void)
{
	GtkCList *clist = GTK_CLIST(edit_account.clist);
	GList *cur;
	gint prev_row;

	if (clist->selection)
		prev_row = GPOINTER_TO_INT(clist->selection->data);
	else
		prev_row = -1;

	gtk_clist_freeze(clist);
	gtk_clist_clear(clist);

	for (cur = account_list; cur != NULL; cur = cur->next) {
		gint row;

		row = account_clist_set_row((PrefsAccount *)cur->data, -1);
		if ((PrefsAccount *)cur->data == cur_account) {
			gtk_clist_select_row(clist, row, -1);
			gtkut_clist_set_focus_row(clist, row);
		}
	}

	if (prev_row >= 0) {
		gtk_clist_select_row(clist, prev_row, -1);
		gtkut_clist_set_focus_row(clist, prev_row);
	}

	gtk_clist_thaw(clist);
}

/* set account list from CList */
static void account_list_set(void)
{
	GtkCList *clist = GTK_CLIST(edit_account.clist);
	gint row;
	PrefsAccount *ac_prefs;

	while (account_list)
		account_list = g_list_remove(account_list, account_list->data);

	for (row = 0; (ac_prefs = gtk_clist_get_row_data(clist, row)) != NULL;
	     row++)
		account_list = g_list_append(account_list, ac_prefs);
}
