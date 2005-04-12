/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto
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
#include "prefs_gtk.h"
#include "prefs_account.h"
#include "folder_item_prefs.h"
#include "compose.h"
#include "manage_window.h"
#include "stock_pixmap.h"
#include "inc.h"
#include "gtkutils.h"
#include "utils.h"
#include "alertpanel.h"
#include "procheader.h"
#include "customheader.h"
#include "remotefolder.h"

typedef enum
{
	COL_DEFAULT	= 0,
	COL_GETALL	= 1,
	COL_NAME	= 2,
	COL_PROTOCOL	= 3,
	COL_SERVER	= 4
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

static GdkPixmap *markxpm;
static GdkBitmap *markxpmmask;
static GdkPixmap *checkboxonxpm;
static GdkPixmap *checkboxonxpmmask;
static GdkPixmap *checkboxoffxpm;
static GdkPixmap *checkboxoffxpmmask;

static void account_edit_create		(void);

static void account_edit_prefs		(GtkWidget *widget, gpointer data);
static void account_delete		(GtkWidget *widget, gpointer data);
static void account_clone		(GtkWidget *widget, gpointer data);

static void account_up			(GtkWidget *widget, gpointer data);
static void account_down		(GtkWidget *widget, gpointer data);

static void account_set_default		(GtkWidget *widget, gpointer data);

static void account_edit_close		(GtkWidget *widget, gpointer data);

static gint account_delete_event	(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	 data);
static void account_selected		(GtkCList	*clist,
					 gint		 row,
					 gint		 column,
					 GdkEvent	*event,
					 gpointer	 data);
static void account_row_moved		(GtkCList	*clist,
					 gint		 source_row,
					 gint		 dest_row);
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

	debug_print("Reading all config for each account...\n");

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, ACCOUNT_RC, NULL);
	if ((fp = fopen(rcpath, "rb")) == NULL) {
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
			debug_print("Found label: %s\n", buf);
			ac_label_list = g_slist_append(ac_label_list,
						       g_strdup(buf));
		}
	}
	fclose(fp);

	/* read config data from file */
	cur_account = NULL;
	for (cur = ac_label_list; cur != NULL; cur = cur->next) {
		ac_prefs = prefs_account_new();
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

void account_write_config_all(void)
{
	prefs_account_write_config_all(account_list);
}

/*
 * account_find_all_from_address:
 * @ac_list: initial list of accounts. NULL to create a new one.
 * Accounts found in the @address will be appended to this list.
 * @address: Email address string.
 *
 * Find all the mail (not news) accounts within the specified address.
 *
 * Return value: the original accounts list with the found accounts appended.
 */
GList *account_find_all_from_address(GList *ac_list, const gchar *address)
{
	GList *cur;
	PrefsAccount *ac;

	if (address == NULL)
		return ac_list;

	for (cur = account_list; cur != NULL; cur = cur->next) {
		ac = (PrefsAccount *)cur->data;
		if (ac->protocol != A_NNTP && ac->address &&
		    strcasestr(address, ac->address) != NULL)
			ac_list = g_list_append(ac_list, ac);
	}
	return ac_list;
}
	
PrefsAccount *account_find_from_smtp_server(const gchar *address,
					    const gchar *smtp_server)
{
	GList *cur;
	PrefsAccount *ac;

	g_return_val_if_fail(address != NULL, NULL);
	g_return_val_if_fail(smtp_server != NULL, NULL);

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

	g_return_val_if_fail(address != NULL, NULL);

	for (cur = account_list; cur != NULL; cur = cur->next) {
		ac = (PrefsAccount *)cur->data;
		if (ac->protocol != A_NNTP && ac->address &&
		    g_strcasecmp(address, ac->address) == 0)
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

PrefsAccount *account_find_from_item(FolderItem *item)
{
	PrefsAccount *ac;

	g_return_val_if_fail(item != NULL, NULL);

	ac = item->account;
	if (!ac) {
		FolderItem *cur_item = folder_item_parent(item);
		while (cur_item != NULL) {
			if (cur_item->account && cur_item->apply_sub) {
				ac = cur_item->account;
				break;
			}				
			cur_item = folder_item_parent(cur_item);
		}
	}
	if (!ac)
		ac = item->folder->account;

	return ac;
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
	inc_lock();

	if (compose_get_compose_list()) {
		alertpanel_error(_("Some composing windows are open.\n"
				   "Please close all the composing "
				   "windows before editing the accounts."));
		inc_unlock();
		return;
	}

	debug_print("Opening account edit window...\n");

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

	if (!ac_prefs) return;

	account_list = g_list_append(account_list, ac_prefs);

	if (ac_prefs->is_default)
		account_set_as_default(ac_prefs);

	account_clist_set();

	if (ac_prefs->protocol == A_IMAP4 || ac_prefs->protocol == A_NNTP) {
		Folder *folder;

		if (ac_prefs->protocol == A_IMAP4) {
			folder = folder_new(folder_get_class_from_string("imap"), ac_prefs->account_name,
					    ac_prefs->recv_server);
		} else {
			folder = folder_new(folder_get_class_from_string("news"), ac_prefs->account_name,
					    ac_prefs->nntp_server);
		}

		folder->account = ac_prefs;
		ac_prefs->folder = folder;
		folder_add(folder);
		if (ac_prefs->protocol == A_IMAP4)
			folder->klass->create_tree(folder);
		folderview_set_all();
	}
}

void account_open(PrefsAccount *ac_prefs)
{
	gboolean prev_default;
	gchar *ac_name;

	g_return_if_fail(ac_prefs != NULL);

	prev_default = ac_prefs->is_default;
	Xstrdup_a(ac_name, ac_prefs->account_name ? ac_prefs->account_name : "",
		  return);

	prefs_account_open(ac_prefs);

	if (!prev_default && ac_prefs->is_default)
		account_set_as_default(ac_prefs);

	if (ac_prefs->folder && strcmp2(ac_name, ac_prefs->account_name) != 0) {
		folder_set_name(FOLDER(ac_prefs->folder),
				ac_prefs->account_name);
		folderview_set_all();
	}

	account_write_config_all();
	account_set_menu();
	main_window_reflect_prefs_all();
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
				folder = folder_new(folder_get_class_from_string("imap"), ap->account_name,
						    ap->recv_server);
			} else {
				folder = folder_new(folder_get_class_from_string("news"), ap->account_name,
						    ap->nntp_server);
			}

			folder->account = ap;
			ap->folder = folder;
			folder_add(folder);
			if (ap->protocol == A_IMAP4)
				folder->klass->create_tree(folder);
		}
	}
}

FolderItem *account_get_special_folder(PrefsAccount *ac_prefs,
				       SpecialFolderItemType type)
{
	FolderItem *item = NULL;

	g_return_val_if_fail(ac_prefs != NULL, NULL);

	switch (type) {
	case F_INBOX:
		if (ac_prefs->folder)
			item = FOLDER(ac_prefs->folder)->inbox;
		if (!item)
			item = folder_get_default_inbox();
		break;
	case F_OUTBOX:
		if (ac_prefs->set_sent_folder && ac_prefs->sent_folder) {
			item = folder_find_item_from_identifier
				(ac_prefs->sent_folder);
		}
		if (!item) {
			if (ac_prefs->folder)
				item = FOLDER(ac_prefs->folder)->outbox;
			if (!item)
				item = folder_get_default_outbox();
		}
		break;
	case F_DRAFT:
		if (ac_prefs->set_draft_folder && ac_prefs->draft_folder) {
			item = folder_find_item_from_identifier
				(ac_prefs->draft_folder);
		}
		if (!item) {
			if (ac_prefs->folder)
				item = FOLDER(ac_prefs->folder)->draft;
			if (!item)
				item = folder_get_default_draft();
		}
		break;
	case F_QUEUE:
		if (ac_prefs->set_queue_folder && ac_prefs->queue_folder) {
			item = folder_find_item_from_identifier
				(ac_prefs->queue_folder);
		}
		if (!item) {
			if (ac_prefs->folder)
				item = FOLDER(ac_prefs->folder)->queue;
			if (!item)
				item = folder_get_default_queue();
		}
		break;
	case F_TRASH:
		if (ac_prefs->set_trash_folder && ac_prefs->trash_folder) {
			item = folder_find_item_from_identifier
				(ac_prefs->trash_folder);
		}
		if (!item) {
			if (ac_prefs->folder)
				item = FOLDER(ac_prefs->folder)->trash;
			if (!item)
				item = folder_get_default_trash();
		}
		break;
	default:
		break;
	}

	return item;
}

void account_destroy(PrefsAccount *ac_prefs)
{
	g_return_if_fail(ac_prefs != NULL);

	folder_unref_account_all(ac_prefs);

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
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *scrolledwin;
	GtkWidget *clist;
	gchar *titles[N_EDIT_ACCOUNT_COLS];
	gint i;

	GtkWidget *vbox2;
	GtkWidget *add_btn;
	GtkWidget *edit_btn;
	GtkWidget *del_btn;
	GtkWidget *clone_btn;
	GtkWidget *up_btn;
	GtkWidget *down_btn;

	GtkWidget *default_btn;

	GtkWidget *hbbox;
	GtkWidget *close_btn;

	debug_print("Creating account edit window...\n");

	window = gtk_window_new (GTK_WINDOW_DIALOG);
	gtk_widget_set_usize (window, 500, 350);
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_window_set_title (GTK_WINDOW (window), _("Edit accounts"));
	gtk_window_set_modal (GTK_WINDOW (window), TRUE);
	gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			    GTK_SIGNAL_FUNC (account_delete_event), NULL);
	gtk_signal_connect (GTK_OBJECT (window), "key_press_event",
			    GTK_SIGNAL_FUNC (account_key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT (window);
	gtk_widget_realize(window);

	vbox = gtk_vbox_new (FALSE, 10);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new
		(_("New messages will be checked in this order. Check the boxes\n"
		   "on the `G' column to enable message retrieval by `Get all'."));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 4);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);

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

	titles[COL_DEFAULT]  = "D";
	titles[COL_GETALL]   = "G";
	titles[COL_NAME]     = _("Name");
	titles[COL_PROTOCOL] = _("Protocol");
	titles[COL_SERVER]   = _("Server");

	clist = gtk_clist_new_with_titles (N_EDIT_ACCOUNT_COLS, titles);
	gtk_widget_show (clist);
	gtk_container_add (GTK_CONTAINER (scrolledwin), clist);
	gtk_clist_set_column_width (GTK_CLIST(clist), COL_DEFAULT , 10);
	gtk_clist_set_column_width (GTK_CLIST(clist), COL_GETALL  , 11);
	gtk_clist_set_column_width (GTK_CLIST(clist), COL_NAME    , 100);
	gtk_clist_set_column_width (GTK_CLIST(clist), COL_PROTOCOL, 100);
	gtk_clist_set_column_width (GTK_CLIST(clist), COL_SERVER  , 100);
	gtk_clist_set_column_justification (GTK_CLIST(clist), COL_DEFAULT,
					    GTK_JUSTIFY_CENTER);
	gtk_clist_set_column_justification (GTK_CLIST(clist), COL_GETALL,
					    GTK_JUSTIFY_CENTER);
	gtk_clist_set_selection_mode (GTK_CLIST(clist), GTK_SELECTION_BROWSE);

	for (i = 0; i < N_EDIT_ACCOUNT_COLS; i++)
		GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(clist)->column[i].button,
				       GTK_CAN_FOCUS);

	gtk_signal_connect (GTK_OBJECT (clist), "select_row",
			    GTK_SIGNAL_FUNC (account_selected), NULL);
	gtk_signal_connect_after (GTK_OBJECT (clist), "row_move",
				  GTK_SIGNAL_FUNC (account_row_moved), NULL);

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

	clone_btn = gtk_button_new_with_label (_(" Clone "));
	gtk_widget_show (clone_btn);
	gtk_box_pack_start (GTK_BOX (vbox2), clone_btn, FALSE, FALSE, 4);
	gtk_signal_connect (GTK_OBJECT(clone_btn), "clicked",
			    GTK_SIGNAL_FUNC (account_clone), NULL);
	
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

	gtkut_button_set_create(&hbbox, &close_btn, _("Close"),
				NULL, NULL, NULL, NULL);
	gtk_widget_show(hbbox);
	gtk_box_pack_end (GTK_BOX (hbox), hbbox, FALSE, FALSE, 0);
	gtk_widget_grab_default (close_btn);

	gtk_signal_connect (GTK_OBJECT (close_btn), "clicked",
			    GTK_SIGNAL_FUNC (account_edit_close),
			    NULL);

	stock_pixmap_gdk(clist, STOCK_PIXMAP_MARK, &markxpm, &markxpmmask);
	stock_pixmap_gdk(clist, STOCK_PIXMAP_CHECKBOX_ON,
			 &checkboxonxpm, &checkboxonxpmmask);
	stock_pixmap_gdk(clist, STOCK_PIXMAP_CHECKBOX_OFF,
			 &checkboxoffxpm, &checkboxoffxpmmask);

	edit_account.window    = window;
	edit_account.clist     = clist;
	edit_account.close_btn = close_btn;
}

static void account_edit_prefs(GtkWidget *widget, gpointer data)
{
	GtkCList *clist = GTK_CLIST(edit_account.clist);
	PrefsAccount *ac_prefs;
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	ac_prefs = gtk_clist_get_row_data(clist, row);
	account_open(ac_prefs);
	
	account_clist_set();
}

static gboolean account_delete_references_func(GNode *node, gpointer data)
{
	FolderItem *item;
	gint account;

	g_return_val_if_fail(node->data != NULL, FALSE);

	item = FOLDER_ITEM(node->data);
	account = GPOINTER_TO_INT(data);

	if(!item->prefs) /* && item->prefs->stype == F_NORMAL */
		return FALSE;
	if(item->prefs->default_account != account)
		return FALSE;
	
	item->prefs->enable_default_account = FALSE;
	item->prefs->default_account = 0;
	folder_item_prefs_save_config(item);

	return FALSE;
}

	
#define ACP_FDUP(fld) ac_clon->fld = ((ac_prefs->fld) != NULL)?\
				     g_strdup(ac_prefs->fld): NULL
#define ACP_FASSIGN(fld) ac_clon->fld = ac_prefs->fld
static void account_clone(GtkWidget *widget, gpointer data)
{
	GtkCList *clist = GTK_CLIST(edit_account.clist);
	gint row;
	PrefsAccount *ac_prefs, *ac_clon;
	GSList *hdrs = NULL;
	CustomHeader *cch = NULL, *ch = NULL;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	ac_prefs = gtk_clist_get_row_data(clist, row);

	if (ac_prefs->protocol == A_IMAP4 || ac_prefs->protocol == A_NNTP) {
		alertpanel_error(_("Accounts with remote folders cannot be cloned"));
		return;
	}
	
	ac_clon = prefs_account_new();
	/* copy fields */
	ac_clon->account_name = g_strdup_printf(_("Cloned %s"), 
						ac_prefs->account_name);
	/* personal */
	ACP_FDUP(name);
	ACP_FDUP(address);
	ACP_FDUP(organization);

	/* server */
	ACP_FASSIGN(protocol);
	ACP_FDUP(recv_server);
	ACP_FDUP(smtp_server);
	ACP_FDUP(nntp_server);
	ACP_FASSIGN(use_nntp_auth);
	ACP_FASSIGN(use_nntp_auth_onconnect);
	ACP_FDUP(userid);
	ACP_FDUP(passwd);

	ACP_FDUP(local_mbox);
	ACP_FASSIGN(use_mail_command);
	ACP_FDUP(mail_command);
	
#if USE_OPENSSL
	ACP_FASSIGN(ssl_pop);
	ACP_FASSIGN(ssl_imap);
	ACP_FASSIGN(ssl_nntp);
	ACP_FASSIGN(ssl_smtp);
	ACP_FASSIGN(use_nonblocking_ssl);
#endif /* USE_OPENSSL */
	
	ac_clon->tmp_pass = NULL;

	/* receive */
	ACP_FASSIGN(rmmail);
	ACP_FASSIGN(msg_leave_time);
	ACP_FASSIGN(getall);
	ACP_FASSIGN(recv_at_getall);
	ACP_FASSIGN(sd_rmmail_on_download);
	ACP_FASSIGN(sd_filter_on_recv);
	ACP_FASSIGN(enable_size_limit);
	ACP_FASSIGN(size_limit);
	ACP_FASSIGN(filter_on_recv);
	ACP_FDUP(inbox);
	ACP_FASSIGN(max_articles);

	ACP_FASSIGN(imap_auth_type);

	/* send */
	ACP_FASSIGN(add_date);
	ACP_FASSIGN(gen_msgid);
	ACP_FASSIGN(add_customhdr);
	ACP_FASSIGN(use_smtp_auth);
	ACP_FASSIGN(smtp_auth_type);
	ACP_FDUP(smtp_userid);
	ACP_FDUP(smtp_passwd);

	ac_clon->tmp_smtp_pass = NULL;

	ACP_FASSIGN(pop_before_smtp);
	ACP_FASSIGN(pop_before_smtp_timeout);
	ACP_FASSIGN(last_pop_login_time);

	ac_clon->customhdr_list = NULL;
	hdrs = ac_prefs->customhdr_list;
	while (hdrs != NULL) {
		ch = (CustomHeader *)hdrs->data;

		cch = g_new0(CustomHeader, 1);
		cch->account_id = ac_clon->account_id;	
		cch->name = (ch->name != NULL) ? g_strdup(ch->name) : NULL;
		cch->value = (ch->value != NULL) ? g_strdup(ch->value) : NULL;
		
		ac_clon->customhdr_list = g_slist_append(ac_clon->customhdr_list, cch);
		
		hdrs = g_slist_next(hdrs);
	}

	/* compose */
        ACP_FASSIGN(sig_type);
        ACP_FDUP(sig_path);
        ACP_FASSIGN(auto_sig);
        ACP_FDUP(sig_sep);
        ACP_FASSIGN(set_autocc);
        ACP_FDUP(auto_cc);
        ACP_FASSIGN(set_autobcc);
        ACP_FDUP(auto_bcc);
        ACP_FASSIGN(set_autoreplyto);
        ACP_FDUP(auto_replyto);

        /* privacy */
        ACP_FASSIGN(default_encrypt);
        ACP_FASSIGN(default_sign);
	
        /* advanced */
        ACP_FASSIGN(set_smtpport);
        ACP_FASSIGN(smtpport);
        ACP_FASSIGN(set_popport);
        ACP_FASSIGN(popport);
        ACP_FASSIGN(set_imapport);
        ACP_FASSIGN(imapport);
        ACP_FASSIGN(set_nntpport);
        ACP_FASSIGN(nntpport);
        ACP_FASSIGN(set_domain);
        ACP_FDUP(domain);
        ACP_FASSIGN(mark_crosspost_read);
        ACP_FASSIGN(crosspost_col);

        ACP_FASSIGN(set_tunnelcmd);
        ACP_FDUP(tunnelcmd);

        ACP_FDUP(imap_dir);

        ACP_FASSIGN(set_sent_folder);
        ACP_FDUP(sent_folder);
        ACP_FASSIGN(set_draft_folder);
        ACP_FDUP(draft_folder);
        ACP_FASSIGN(set_trash_folder);
        ACP_FDUP(trash_folder);
	/* don't want two default accounts */
	ac_clon->is_default = FALSE;
	ACP_FASSIGN(folder);

	account_list = g_list_append(account_list, ac_clon);
	account_clist_set();
}
#undef ACP_FDUP
#undef ACP_FASSIGN

static void account_delete(GtkWidget *widget, gpointer data)
{
	GtkCList *clist = GTK_CLIST(edit_account.clist);
	PrefsAccount *ac_prefs;
	gint row;
	GList *list;
	Folder *folder;
	
	if (!clist->selection) return;

	if (alertpanel(_("Delete account"),
		       _("Do you really want to delete this account?"),
		       _("Yes"), _("+No"), NULL) != G_ALERTDEFAULT)
		return;

	row = GPOINTER_TO_INT(clist->selection->data);
	ac_prefs = gtk_clist_get_row_data(clist, row);
	if (ac_prefs->folder) {
		FolderItem *item;

		item = mainwindow_get_mainwindow()->summaryview->folder_item;
		if (item && item->folder == FOLDER(ac_prefs->folder))
			summary_clear_all(mainwindow_get_mainwindow()->summaryview);
		folder_destroy(FOLDER(ac_prefs->folder));
		folderview_set_all();
	}
	account_destroy(ac_prefs);
	account_clist_set();

	debug_print("Removing deleted account references for all the folders...\n");
	list = folder_get_list();
	for (; list != NULL; list = list->next) {
		folder = FOLDER(list->data);
		if (folder->node)  /* && folder->type == F_? */
			g_node_traverse(folder->node, G_PRE_ORDER,
				G_TRAVERSE_ALL, -1,
				account_delete_references_func,
				GINT_TO_POINTER(ac_prefs->account_id));
	}
}

static void account_up(GtkWidget *widget, gpointer data)
{
	GtkCList *clist = GTK_CLIST(edit_account.clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row > 0)
		gtk_clist_row_move(clist, row, row - 1);
}

static void account_down(GtkWidget *widget, gpointer data)
{
	GtkCList *clist = GTK_CLIST(edit_account.clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row < clist->rows - 1)
		gtk_clist_row_move(clist, row, row + 1);
}

static void account_set_default(GtkWidget *widget, gpointer data)
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

static void account_edit_close(GtkWidget *widget, gpointer data)
{
	account_list_set();
	account_write_config_all();

	if (!cur_account && account_list) {
		PrefsAccount *ac_prefs = (PrefsAccount *)account_list->data;
		account_set_as_default(ac_prefs);
		cur_account = ac_prefs;
	}

	account_set_menu();
	main_window_reflect_prefs_all();

	gtk_widget_hide(edit_account.window);

	inc_unlock();
}

static gint account_delete_event(GtkWidget *widget, GdkEventAny *event,
				 gpointer data)
{
	account_edit_close(NULL, NULL);
	return TRUE;
}

static void account_selected(GtkCList *clist, gint row, gint column,
			     GdkEvent *event, gpointer data)
{
	if (event && event->type == GDK_2BUTTON_PRESS) {
		account_edit_prefs(NULL, NULL);
		return;
	}

	if (column == COL_GETALL) {
		PrefsAccount *ac;

		ac = gtk_clist_get_row_data(clist, row);
		if (ac->protocol == A_POP3 || ac->protocol == A_IMAP4 ||
		    ac->protocol == A_NNTP || ac->protocol == A_LOCAL) {
			ac->recv_at_getall ^= TRUE;
			account_clist_set_row(ac, row);
		}
	}
}

static void account_row_moved(GtkCList *clist, gint source_row, gint dest_row)
{
	account_list_set();
	if (gtk_clist_row_is_visible(clist, dest_row) != GTK_VISIBILITY_FULL)
		gtk_clist_moveto(clist, dest_row, -1, 0.5, 0.0);
}

static void account_key_pressed(GtkWidget *widget, GdkEventKey *event,
				gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		account_edit_close(NULL, NULL);
}

/* set one CList row or add new row */
static gint account_clist_set_row(PrefsAccount *ac_prefs, gint row)
{
	GtkCList *clist = GTK_CLIST(edit_account.clist);
	gchar *text[N_EDIT_ACCOUNT_COLS];
	gboolean has_getallbox;
	gboolean getall;

	text[COL_DEFAULT] = "";
	text[COL_GETALL]  = "";
	text[COL_NAME]    = ac_prefs->account_name;
#if USE_OPENSSL
	text[COL_PROTOCOL] = ac_prefs->protocol == A_POP3 ?
			     (ac_prefs->ssl_pop == SSL_TUNNEL ?
			      "POP3 (SSL)" :
			      ac_prefs->ssl_pop == SSL_STARTTLS ?
			      "POP3 (TLS)" : "POP3") :
			     ac_prefs->protocol == A_IMAP4 ?
			     (ac_prefs->ssl_imap == SSL_TUNNEL ?
			      "IMAP4 (SSL)" :
			      ac_prefs->ssl_imap == SSL_STARTTLS ?
			      "IMAP4 (TLS)" : "IMAP4") :
			     ac_prefs->protocol == A_NNTP ?
			     (ac_prefs->ssl_nntp == SSL_TUNNEL ?
			      "NNTP (SSL)" : "NNTP") :
			     "";
#else
	text[COL_PROTOCOL] = ac_prefs->protocol == A_POP3  ? "POP3" :
			     ac_prefs->protocol == A_IMAP4 ? "IMAP4" :
			     ac_prefs->protocol == A_LOCAL ? "Local" :
			     ac_prefs->protocol == A_NNTP  ? "NNTP" : "";
#endif
	text[COL_SERVER] = ac_prefs->protocol == A_NNTP
		? ac_prefs->nntp_server : ac_prefs->recv_server;

	if (row < 0)
		row = gtk_clist_append(clist, text);
	else {
		gtk_clist_set_text(clist, row, COL_DEFAULT, text[COL_DEFAULT]);
		gtk_clist_set_text(clist, row, COL_GETALL, text[COL_GETALL]);
		gtk_clist_set_text(clist, row, COL_NAME, text[COL_NAME]);
		gtk_clist_set_text(clist, row, COL_PROTOCOL, text[COL_PROTOCOL]);
		gtk_clist_set_text(clist, row, COL_SERVER, text[COL_SERVER]);
	}

	has_getallbox = (ac_prefs->protocol == A_POP3  ||
			 ac_prefs->protocol == A_IMAP4 ||
			 ac_prefs->protocol == A_NNTP ||
			 ac_prefs->protocol == A_LOCAL);
	getall = has_getallbox && ac_prefs->recv_at_getall;

	if (ac_prefs->is_default)
		gtk_clist_set_pixmap(clist, row, COL_DEFAULT,
				     markxpm, markxpmmask);
	if (getall)
		gtk_clist_set_pixmap(clist, row, COL_GETALL,
				     checkboxonxpm, checkboxonxpmmask);
	else if (has_getallbox)
		gtk_clist_set_pixmap(clist, row, COL_GETALL,
				     checkboxoffxpm, checkboxoffxpmmask);

	gtk_clist_set_row_data(clist, row, ac_prefs);

	return row;
}

/* set CList from account list */
static void account_clist_set(void)
{
	GtkCList *clist = GTK_CLIST(edit_account.clist);
	GList *cur;
	gint row = -1, prev_row = -1;

	if (clist->selection)
		prev_row = GPOINTER_TO_INT(clist->selection->data);

	gtk_clist_freeze(clist);
	gtk_clist_clear(clist);

	for (cur = account_list; cur != NULL; cur = cur->next) {
		row = account_clist_set_row((PrefsAccount *)cur->data, -1);
		if ((PrefsAccount *)cur->data == cur_account) {
			gtk_clist_select_row(clist, row, -1);
			gtkut_clist_set_focus_row(clist, row);
		}
	}

	if (prev_row >= 0) {
		row = prev_row;
		gtk_clist_select_row(clist, row, -1);
		gtkut_clist_set_focus_row(clist, row);
	}

	if (row >= 0 &&
	    gtk_clist_row_is_visible(clist, row) != GTK_VISIBILITY_FULL)
		gtk_clist_moveto(clist, row, -1, 0.5, 0);

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

/*!
 *\brief	finds the PrefsAccounts which should be used to answer a mail
 *
 *\param	msginfo The message to be answered
 *\param	reply_autosel Indicates whether reply account autoselection is on
 *
 *\return	PrefsAccount * the correct account, NULL if not found
 */
PrefsAccount *account_get_reply_account(MsgInfo *msginfo, gboolean reply_autosel)
{
	PrefsAccount *account = NULL;
	/* select the account set in folderitem's property (if enabled) */
	if (msginfo->folder->prefs && msginfo->folder->prefs->enable_default_account)
		account = account_find_from_id(msginfo->folder->prefs->default_account);
	
	/* select account by to: and cc: header if enabled */
	if (reply_autosel) {
		gchar * field = NULL;
		int fieldno = 0;
		for (field = msginfo->to; fieldno++ < 2; field = msginfo->cc) {
			if (!account && field) {
				gchar *to = NULL;
				if (!strchr(field, ',')) {
					Xstrdup_a(to, field, return NULL);
					extract_address(to);
					account = account_find_from_address(to);
				} else {
					gchar **split = g_strsplit(field, ",", -1);
					int i = -1;
					do {
						i++;
						if (!split[i])
							break;
						Xstrdup_a(to, split[i], return NULL);
						extract_address(to);
						account = account_find_from_address(to);
					} while (!account);
					g_strfreev(split);
				}
			}
		}
	}

	/* select the account for the whole folder (IMAP / NNTP) */
	if (!account) 
		/* FIXME: this is not right, because folder may be nested. we should
		 * ascend the tree until we find a parent with proper account 
		 * information */
		account = msginfo->folder->folder->account;

	/* select current account */
	if (!account) account = cur_account;
	
	return account;
}
