/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto and the Sylpheed-Claws team
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

/* alfons - all folder item specific settings should migrate into 
 * folderlist.xml!!! the old folderitemrc file will only serve for a few 
 * versions (for compatibility) */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include "folder.h"
#include "prefs_folder_item.h"
#include "folderview.h"
#include "folder.h"
#include "summaryview.h"
#include "menu.h"
#include "account.h"
#include "prefs_gtk.h"
#include "manage_window.h"
#include "utils.h"
#include "addr_compl.h"
#include "prefs_common.h"
#include "gtkutils.h"
#include "filtering.h"
#include "folder_item_prefs.h"
#include "gtk/colorsel.h"

#if USE_ASPELL
#include "gtkaspell.h"
#endif

#define ASSIGN_STRING(string, value) \
	{ \
		g_free(string); \
		string = (value); \
	}

typedef struct _FolderItemGeneralPage FolderItemGeneralPage;
typedef struct _FolderItemComposePage FolderItemComposePage;

struct _FolderItemGeneralPage
{
	PrefsPage page;

	FolderItem *item;

	GtkWidget *table;
	GtkWidget *folder_type;
	GtkWidget *checkbtn_simplify_subject;
	GtkWidget *entry_simplify_subject;
	GtkWidget *checkbtn_folder_chmod;
	GtkWidget *entry_folder_chmod;
	GtkWidget *folder_color_btn;
	GtkWidget *checkbtn_enable_processing;
	GtkWidget *checkbtn_newmailcheck;
	GtkWidget *checkbtn_offlinesync;

	/* apply to sub folders */
	GtkWidget *simplify_subject_rec_checkbtn;
	GtkWidget *folder_chmod_rec_checkbtn;
	GtkWidget *folder_color_rec_checkbtn;
	GtkWidget *enable_processing_rec_checkbtn;
	GtkWidget *newmailcheck_rec_checkbtn;
	GtkWidget *offlinesync_rec_checkbtn;

	gint	   folder_color;
};

struct _FolderItemComposePage
{
	PrefsPage page;

	FolderItem *item;

	GtkWidget *window;
	GtkWidget *table;
	GtkWidget *checkbtn_request_return_receipt;
	GtkWidget *checkbtn_save_copy_to_folder;
	GtkWidget *checkbtn_default_to;
	GtkWidget *entry_default_to;
	GtkWidget *checkbtn_default_reply_to;
	GtkWidget *entry_default_reply_to;
	GtkWidget *checkbtn_enable_default_account;
	GtkWidget *optmenu_default_account;
#if USE_ASPELL
	GtkWidget *checkbtn_enable_default_dictionary;
	GtkWidget *optmenu_default_dictionary;
#endif

	/* apply to sub folders */
	GtkWidget *request_return_receipt_rec_checkbtn;
	GtkWidget *save_copy_to_folder_rec_checkbtn;
	GtkWidget *default_to_rec_checkbtn;
	GtkWidget *default_reply_to_rec_checkbtn;
	GtkWidget *default_account_rec_checkbtn;
#if USE_ASPELL
	GtkWidget *default_dictionary_rec_checkbtn;
#endif

};


static void general_save_folder_prefs(FolderItem *folder, FolderItemGeneralPage *page);
static void compose_save_folder_prefs(FolderItem *folder, FolderItemComposePage *page);

static gboolean general_save_recurse_func(GNode *node, gpointer data);
static gboolean compose_save_recurse_func(GNode *node, gpointer data);

gint prefs_folder_item_chmod_mode		(gchar *folder_chmod);

static void folder_color_set_dialog(GtkWidget *widget, gpointer data);

#define SAFE_STRING(str) \
	(str) ? (str) : ""

void prefs_folder_item_general_create_widget_func(PrefsPage * page_,
						   GtkWindow * window,
                                		   gpointer data)
{
	FolderItemGeneralPage *page = (FolderItemGeneralPage *) page_;
	FolderItem *item = (FolderItem *) data;
	guint rowcount;


	GtkWidget *table;
	GtkWidget *hbox;
	GtkWidget *label;
	
	GtkWidget *folder_type_menu;
	GtkWidget *folder_type;
	GtkWidget *dummy_chkbtn;
	GtkWidget *menuitem;
	SpecialFolderItemType type;
	
	GtkWidget *checkbtn_simplify_subject;
	GtkWidget *entry_simplify_subject;
	GtkWidget *checkbtn_folder_chmod;
	GtkWidget *entry_folder_chmod;
	GtkWidget *folder_color;
	GtkWidget *folder_color_btn;
	GtkWidget *checkbtn_enable_processing;
	GtkWidget *checkbtn_newmailcheck;
	GtkWidget *checkbtn_offlinesync;

	GtkWidget *simplify_subject_rec_checkbtn;
	GtkWidget *folder_chmod_rec_checkbtn;
	GtkWidget *folder_color_rec_checkbtn;
	GtkWidget *enable_processing_rec_checkbtn;
	GtkWidget *newmailcheck_rec_checkbtn;
	GtkWidget *offlinesync_rec_checkbtn;
	GtkTooltips *tooltips;

	page->item	   = item;

	/* Table */
	table = gtk_table_new(6, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 4);
	rowcount = 0;

	/* Apply to subfolders */
	label = gtk_label_new(_("Apply to\nsubfolders"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 2, 3,
			 rowcount, rowcount + 1, GTK_SHRINK, GTK_SHRINK, 0, 0);
	rowcount++;

	/* folder_type */
	folder_type = gtk_option_menu_new ();
	gtk_widget_show (folder_type);

	type = F_NORMAL;
	if (item->stype == F_INBOX)
		type = F_INBOX;
	else if (folder_has_parent_of_type(item, F_OUTBOX))
		type = F_OUTBOX;
	else if (folder_has_parent_of_type(item, F_DRAFT))
		type = F_DRAFT;
	else if (folder_has_parent_of_type(item, F_QUEUE))
		type = F_QUEUE;
	else if (folder_has_parent_of_type(item, F_TRASH))
		type = F_TRASH;

	folder_type_menu = gtk_menu_new ();

	MENUITEM_ADD (folder_type_menu, menuitem, _("Normal"),  F_NORMAL);
	MENUITEM_ADD (folder_type_menu, menuitem, _("Inbox"),  F_INBOX);
	MENUITEM_ADD (folder_type_menu, menuitem, _("Outbox"),  F_OUTBOX);
	MENUITEM_ADD (folder_type_menu, menuitem, _("Drafts"),  F_DRAFT);
	MENUITEM_ADD (folder_type_menu, menuitem, _("Queue"),  F_QUEUE);
	MENUITEM_ADD (folder_type_menu, menuitem, _("Trash"),  F_TRASH);
	gtk_option_menu_set_menu (GTK_OPTION_MENU (folder_type), folder_type_menu);

	gtk_option_menu_set_history(GTK_OPTION_MENU(folder_type), type);

	dummy_chkbtn = gtk_check_button_new();
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dummy_chkbtn), type != F_INBOX);
	gtk_widget_set_sensitive(dummy_chkbtn, FALSE);

	if (type == item->stype && type == F_NORMAL)
		gtk_widget_set_sensitive(folder_type, TRUE);
	else
		gtk_widget_set_sensitive(folder_type, FALSE);

	label = gtk_label_new(_("Folder type:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 
			 rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach(GTK_TABLE(table), folder_type, 1, 2, 
			 rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_FILL, 0, 0);
	gtk_table_attach(GTK_TABLE(table), dummy_chkbtn, 2, 3, 
			 rowcount, rowcount + 1, GTK_SHRINK, GTK_SHRINK, 0, 0);

	rowcount++;

	/* Simplify Subject */
	checkbtn_simplify_subject = gtk_check_button_new_with_label(_("Simplify Subject RegExp: "));
	gtk_table_attach(GTK_TABLE(table), checkbtn_simplify_subject, 0, 1, 
			 rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_FILL, 0, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_simplify_subject), 
				     item->prefs->enable_simplify_subject);

	entry_simplify_subject = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), entry_simplify_subject, 1, 2, 
			 rowcount, rowcount + 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	SET_TOGGLE_SENSITIVITY(checkbtn_simplify_subject, entry_simplify_subject);
	gtk_entry_set_text(GTK_ENTRY(entry_simplify_subject), 
			   SAFE_STRING(item->prefs->simplify_subject_regexp));

	simplify_subject_rec_checkbtn = gtk_check_button_new();
	gtk_table_attach(GTK_TABLE(table), simplify_subject_rec_checkbtn, 2, 3, 
			 rowcount, rowcount + 1, GTK_SHRINK, GTK_SHRINK, 0, 0);

	rowcount++;

	/* Folder chmod */
	checkbtn_folder_chmod = gtk_check_button_new_with_label(_("Folder chmod: "));
	gtk_table_attach(GTK_TABLE(table), checkbtn_folder_chmod, 0, 1, 
			 rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_FILL, 0, 0);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_folder_chmod), 
				     item->prefs->enable_folder_chmod);

	entry_folder_chmod = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), entry_folder_chmod, 1, 2, 
			 rowcount, rowcount + 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	SET_TOGGLE_SENSITIVITY(checkbtn_folder_chmod, entry_folder_chmod);
	if (item->prefs->folder_chmod) {
		gchar *buf;

		buf = g_strdup_printf("%o", item->prefs->folder_chmod);
		gtk_entry_set_text(GTK_ENTRY(entry_folder_chmod), buf);
		g_free(buf);
	}
	
	folder_chmod_rec_checkbtn = gtk_check_button_new();
	gtk_table_attach(GTK_TABLE(table), folder_chmod_rec_checkbtn, 2, 3, 
			 rowcount, rowcount + 1, GTK_SHRINK, GTK_SHRINK, 0, 0);

	rowcount++;
	
	/* Folder color */
	folder_color = gtk_label_new(_("Folder color: "));
	gtk_misc_set_alignment(GTK_MISC(folder_color), 0, 0.5);
	gtk_table_attach(GTK_TABLE(table), folder_color, 0, 1, 
			 rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_FILL, 0, 0);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, 
			 rowcount, rowcount + 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

	folder_color_btn = gtk_button_new_with_label("");
	gtk_widget_set_size_request(folder_color_btn, 36, 26);
  	gtk_box_pack_start (GTK_BOX(hbox), folder_color_btn, FALSE, FALSE, 0);
	tooltips = gtk_tooltips_new();
	gtk_tooltips_set_tip(tooltips, folder_color_btn,
			     _("Pick color for folder"), NULL);

	page->folder_color = item->prefs->color;

	g_signal_connect(G_OBJECT(folder_color_btn), "clicked",
			 G_CALLBACK(folder_color_set_dialog),
			 page);

	gtkut_set_widget_bgcolor_rgb(folder_color_btn, item->prefs->color);

	folder_color_rec_checkbtn = gtk_check_button_new();
	gtk_table_attach(GTK_TABLE(table), folder_color_rec_checkbtn, 2, 3, 
			 rowcount, rowcount + 1, GTK_SHRINK, GTK_SHRINK, 0, 0);

	rowcount++;

	/* Enable processing at startup */
	checkbtn_enable_processing = gtk_check_button_new_with_label(_("Process at startup"));
	gtk_table_attach(GTK_TABLE(table), checkbtn_enable_processing, 0, 2, 
			 rowcount, rowcount + 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_enable_processing), 
				     item->prefs->enable_processing);

	enable_processing_rec_checkbtn = gtk_check_button_new();
	gtk_table_attach(GTK_TABLE(table), enable_processing_rec_checkbtn, 2, 3, 
			 rowcount, rowcount + 1, GTK_SHRINK, GTK_SHRINK, 0, 0);
	
	rowcount++;

	/* Check folder for new mail */
	checkbtn_newmailcheck = gtk_check_button_new_with_label(_("Scan for new mail"));
	gtk_table_attach(GTK_TABLE(table), checkbtn_newmailcheck, 0, 2,
			 rowcount, rowcount+1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_newmailcheck),
								 item->prefs->newmailcheck);
	newmailcheck_rec_checkbtn = gtk_check_button_new();
	gtk_table_attach(GTK_TABLE(table), newmailcheck_rec_checkbtn, 2, 3, 
			 rowcount, rowcount + 1, GTK_SHRINK, GTK_SHRINK, 0, 0);

	rowcount++;

	/* Synchronise folder for offline use */
	checkbtn_offlinesync = gtk_check_button_new_with_label(_("Synchronise for offline use"));
	gtk_table_attach(GTK_TABLE(table), checkbtn_offlinesync, 0, 2,
			 rowcount, rowcount+1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
	
	offlinesync_rec_checkbtn = gtk_check_button_new();
	gtk_table_attach(GTK_TABLE(table), offlinesync_rec_checkbtn, 2, 3, 
			 rowcount, rowcount + 1, GTK_SHRINK, GTK_SHRINK, 0, 0);

	if (item->folder && (item->folder->klass->type != F_IMAP && 
	    item->folder->klass->type != F_NEWS)) {
		 item->prefs->offlinesync = TRUE;
		gtk_widget_set_sensitive(GTK_WIDGET(checkbtn_offlinesync),
								 FALSE);
		gtk_widget_set_sensitive(GTK_WIDGET(offlinesync_rec_checkbtn),
								 FALSE);
	
	}
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_offlinesync),
								 item->prefs->offlinesync);
	rowcount++;

	gtk_widget_show_all(table);

	page->table = table;
	page->folder_type = folder_type;
	page->checkbtn_simplify_subject = checkbtn_simplify_subject;
	page->entry_simplify_subject = entry_simplify_subject;
	page->checkbtn_folder_chmod = checkbtn_folder_chmod;
	page->entry_folder_chmod = entry_folder_chmod;
	page->folder_color_btn = folder_color_btn;
	page->checkbtn_enable_processing = checkbtn_enable_processing;
	page->checkbtn_newmailcheck = checkbtn_newmailcheck;
	page->checkbtn_offlinesync = checkbtn_offlinesync;

	page->simplify_subject_rec_checkbtn  = simplify_subject_rec_checkbtn;
	page->folder_chmod_rec_checkbtn	     = folder_chmod_rec_checkbtn;
	page->folder_color_rec_checkbtn	     = folder_color_rec_checkbtn;
	page->enable_processing_rec_checkbtn = enable_processing_rec_checkbtn;
	page->newmailcheck_rec_checkbtn	     = newmailcheck_rec_checkbtn;
	page->offlinesync_rec_checkbtn	     = offlinesync_rec_checkbtn;

	page->page.widget = table;
}

void prefs_folder_item_general_destroy_widget_func(PrefsPage *page_) 
{
	/* FolderItemGeneralPage *page = (FolderItemGeneralPage *) page_; */
}

/** \brief  Save the prefs in page to folder.
 *
 *  If the folder is not the one  specified in page->item, then only those properties 
 *  that have the relevant 'apply to sub folders' button checked are saved
 */
static void general_save_folder_prefs(FolderItem *folder, FolderItemGeneralPage *page)
{
	FolderItemPrefs *prefs = folder->prefs;
	gchar *buf;
	gboolean all = FALSE;
	SpecialFolderItemType type = F_NORMAL;
	GtkWidget *menu;
	GtkWidget *menuitem;

	if (folder->path == NULL)
		return;

	g_return_if_fail(prefs != NULL);

	if (page->item == folder) 
		all = TRUE;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(page->folder_type));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	type = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));
	if (all && folder->stype != type) {
		folder_item_change_type(folder, type);
	}

	if (all || gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->simplify_subject_rec_checkbtn))) {
		prefs->enable_simplify_subject =
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_simplify_subject));
		ASSIGN_STRING(prefs->simplify_subject_regexp,
			      gtk_editable_get_chars(GTK_EDITABLE(page->entry_simplify_subject), 0, -1));
	}
	
	if (all || gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->folder_chmod_rec_checkbtn))) {
		prefs->enable_folder_chmod = 
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_folder_chmod));
		buf = gtk_editable_get_chars(GTK_EDITABLE(page->entry_folder_chmod), 0, -1);
		prefs->folder_chmod = prefs_folder_item_chmod_mode(buf);
		g_free(buf);
	}

	if (all || gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->folder_color_rec_checkbtn))) {
		int old_color = prefs->color;
		prefs->color = page->folder_color;
	
		/* update folder view */
		if (prefs->color != old_color)
			folder_item_update(folder, F_ITEM_UPDATE_MSGCNT);
	}

	if (all || gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->enable_processing_rec_checkbtn))) {
		prefs->enable_processing = 
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_enable_processing));
	}

	if (all ||  gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->newmailcheck_rec_checkbtn))) {
		prefs->newmailcheck = 
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_newmailcheck));
	}

	if (all ||  gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->offlinesync_rec_checkbtn))) {
		prefs->offlinesync = 
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_offlinesync));
	}

	folder_item_prefs_save_config(folder);
}	

static gboolean general_save_recurse_func(GNode *node, gpointer data)
{
	FolderItem *item = (FolderItem *) node->data;
	FolderItemGeneralPage *page = (FolderItemGeneralPage *) data;

	g_return_val_if_fail(item != NULL, TRUE);
	g_return_val_if_fail(page != NULL, TRUE);

	general_save_folder_prefs(item, page);

	/* optimise by not continuing if none of the 'apply to sub folders'
	   check boxes are selected - and optimise the checking by only doing
	   it once */
	if ((node == page->item->node) &&
	    !(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->simplify_subject_rec_checkbtn)) ||
	      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->folder_chmod_rec_checkbtn)) ||
	      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->folder_color_rec_checkbtn)) ||
	      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->enable_processing_rec_checkbtn)) ||
	      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->newmailcheck_rec_checkbtn)) ||
	      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->offlinesync_rec_checkbtn))))
		return TRUE;
	else 
		return FALSE;
}

void prefs_folder_item_general_save_func(PrefsPage *page_) 
{
	FolderItemGeneralPage *page = (FolderItemGeneralPage *) page_;

	g_node_traverse(page->item->node, G_PRE_ORDER, G_TRAVERSE_ALL,
			-1, general_save_recurse_func, page);

}

static RecvProtocol item_protocol(FolderItem *item)
{
	if (!item)
		return A_NONE;
	if (!item->folder)
		return A_NONE;
	if (!item->folder->account)
		return A_NONE;
	return item->folder->account->protocol;
}
void prefs_folder_item_compose_create_widget_func(PrefsPage * page_,
						   GtkWindow * window,
                                		   gpointer data)
{
	FolderItemComposePage *page = (FolderItemComposePage *) page_;
	FolderItem *item = (FolderItem *) data;
	guint rowcount;

	GtkWidget *table;
	GtkWidget *label;
	
	GtkWidget *checkbtn_request_return_receipt = NULL;
	GtkWidget *checkbtn_save_copy_to_folder = NULL;
	GtkWidget *checkbtn_default_to = NULL;
	GtkWidget *entry_default_to = NULL;
	GtkWidget *checkbtn_default_reply_to = NULL;
	GtkWidget *entry_default_reply_to = NULL;
	GtkWidget *checkbtn_enable_default_account = NULL;
	GtkWidget *optmenu_default_account = NULL;
	GtkWidget *optmenu_default_account_menu = NULL;
	GtkWidget *optmenu_default_account_menuitem = NULL;
#if USE_ASPELL
	GtkWidget *checkbtn_enable_default_dictionary = NULL;
	GtkWidget *optmenu_default_dictionary = NULL;
#endif
	GtkWidget *request_return_receipt_rec_checkbtn = NULL;
	GtkWidget *save_copy_to_folder_rec_checkbtn = NULL;
	GtkWidget *default_to_rec_checkbtn = NULL;
	GtkWidget *default_reply_to_rec_checkbtn = NULL;
	GtkWidget *default_account_rec_checkbtn = NULL;
#if USE_ASPELL
	GtkWidget *default_dictionary_rec_checkbtn = NULL;
#endif

	GList *cur_ac;
	GList *account_list;
#if USE_ASPELL
	gchar *dictionary;
#endif
	PrefsAccount *ac_prefs;
	GtkOptionMenu *optmenu;
	GtkWidget *menu;
	GtkWidget *menuitem;
	gint account_index, index;

	page->item	   = item;

	/* Table */
#if USE_ASPELL
# define TABLEHEIGHT 6
#else
# define TABLEHEIGHT 5
#endif
	table = gtk_table_new(TABLEHEIGHT, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 4);
	rowcount = 0;

	/* Apply to subfolders */
	label = gtk_label_new(_("Apply to\nsubfolders"));
	gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 2, 3,
			 rowcount, rowcount + 1, GTK_SHRINK, GTK_SHRINK, 0, 0);
	rowcount++;

	if (item_protocol(item) != A_NNTP) {
		/* Request Return Receipt */
		checkbtn_request_return_receipt = gtk_check_button_new_with_label
			(_("Request Return Receipt"));
		gtk_table_attach(GTK_TABLE(table), checkbtn_request_return_receipt, 
				 0, 2, rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, 
				 GTK_FILL, 0, 0);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_request_return_receipt),
					     item->ret_rcpt ? TRUE : FALSE);

		request_return_receipt_rec_checkbtn = gtk_check_button_new();
		gtk_table_attach(GTK_TABLE(table), request_return_receipt_rec_checkbtn, 2, 3, 
				 rowcount, rowcount + 1, GTK_SHRINK, GTK_SHRINK, 0, 0);

		rowcount++;

		/* Save Copy to Folder */
		checkbtn_save_copy_to_folder = gtk_check_button_new_with_label
			(_("Save copy of outgoing messages to this folder instead of Sent"));
		gtk_table_attach(GTK_TABLE(table), checkbtn_save_copy_to_folder, 0, 2, 
				 rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_FILL, 0, 0);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_save_copy_to_folder),
					     item->prefs->save_copy_to_folder ? TRUE : FALSE);

		save_copy_to_folder_rec_checkbtn = gtk_check_button_new();
		gtk_table_attach(GTK_TABLE(table), save_copy_to_folder_rec_checkbtn, 2, 3, 
				 rowcount, rowcount + 1, GTK_SHRINK, GTK_SHRINK, 0, 0);

		rowcount++;

		/* Default To */
		checkbtn_default_to = gtk_check_button_new_with_label(_("Default To: "));
		gtk_table_attach(GTK_TABLE(table), checkbtn_default_to, 0, 1, 
				 rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_SHRINK, 0, 0);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_default_to), 
					     item->prefs->enable_default_to);

		entry_default_to = gtk_entry_new();
		gtk_table_attach(GTK_TABLE(table), entry_default_to, 1, 2,
				 rowcount, rowcount + 1, GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);
		SET_TOGGLE_SENSITIVITY(checkbtn_default_to, entry_default_to);
		gtk_entry_set_text(GTK_ENTRY(entry_default_to), SAFE_STRING(item->prefs->default_to));
		address_completion_register_entry(GTK_ENTRY(entry_default_to));

		default_to_rec_checkbtn = gtk_check_button_new();
		gtk_table_attach(GTK_TABLE(table), default_to_rec_checkbtn, 2, 3, 
				 rowcount, rowcount + 1, GTK_SHRINK, GTK_SHRINK, 0, 0);

		rowcount++;

		/* Default address to reply to */
		checkbtn_default_reply_to = gtk_check_button_new_with_label(_("Default To for replies: "));
		gtk_table_attach(GTK_TABLE(table), checkbtn_default_reply_to, 0, 1, 
				 rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_SHRINK, 0, 0);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_default_reply_to), 
					     item->prefs->enable_default_reply_to);

		entry_default_reply_to = gtk_entry_new();
		gtk_table_attach(GTK_TABLE(table), entry_default_reply_to, 1, 2,
				 rowcount, rowcount + 1, GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);
		SET_TOGGLE_SENSITIVITY(checkbtn_default_reply_to, entry_default_reply_to);
		gtk_entry_set_text(GTK_ENTRY(entry_default_reply_to), SAFE_STRING(item->prefs->default_reply_to));
		address_completion_register_entry(GTK_ENTRY(entry_default_reply_to));

		default_reply_to_rec_checkbtn = gtk_check_button_new();
		gtk_table_attach(GTK_TABLE(table), default_reply_to_rec_checkbtn, 2, 3, 
				 rowcount, rowcount + 1, GTK_SHRINK, GTK_SHRINK, 0, 0);

		rowcount++;
	}
	/* Default account */
	checkbtn_enable_default_account = gtk_check_button_new_with_label(_("Default account: "));
	gtk_table_attach(GTK_TABLE(table), checkbtn_enable_default_account, 0, 1, 
			 rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_SHRINK, 0, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_enable_default_account), 
				     item->prefs->enable_default_account);

 	optmenu_default_account = gtk_option_menu_new ();
	gtk_table_attach(GTK_TABLE(table), optmenu_default_account, 1, 2, 
			 rowcount, rowcount + 1, GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);
 	optmenu_default_account_menu = gtk_menu_new ();

	account_list = account_get_list();
	account_index = 0;
	index = 0;
	for (cur_ac = account_list; cur_ac != NULL; cur_ac = cur_ac->next) {
		ac_prefs = (PrefsAccount *)cur_ac->data;
		if (item->folder->account &&
	    	    ( (item_protocol(item) == A_NNTP && ac_prefs->protocol != A_NNTP)
		    ||(item_protocol(item) != A_NNTP && ac_prefs->protocol == A_NNTP))) 
			continue;

		if (item->folder->klass->type != F_NEWS && ac_prefs->protocol == A_NNTP)
			continue;

	 	MENUITEM_ADD (optmenu_default_account_menu, optmenu_default_account_menuitem,
					ac_prefs->account_name?ac_prefs->account_name : _("Untitled"),
					ac_prefs->account_id);
		/* get the index for menu's set_history (sad method?) */
		if (ac_prefs->account_id == item->prefs->default_account)
			account_index = index;
		index++;			
	}

	optmenu = GTK_OPTION_MENU(optmenu_default_account);
 	gtk_option_menu_set_menu(optmenu, optmenu_default_account_menu);

	gtk_option_menu_set_history(optmenu, account_index);

	menu = gtk_option_menu_get_menu(optmenu);
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	gtk_menu_item_activate(GTK_MENU_ITEM(menuitem));

	SET_TOGGLE_SENSITIVITY(checkbtn_enable_default_account, optmenu_default_account);

	default_account_rec_checkbtn = gtk_check_button_new();
	gtk_table_attach(GTK_TABLE(table), default_account_rec_checkbtn, 2, 3, 
			 rowcount, rowcount + 1, GTK_SHRINK, GTK_SHRINK, 0, 0);
	rowcount++;

#if USE_ASPELL
	/* Default dictionary */
	checkbtn_enable_default_dictionary = gtk_check_button_new_with_label(_("Default dictionary: "));
	gtk_table_attach(GTK_TABLE(table), checkbtn_enable_default_dictionary, 0, 1,
	    		 rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_SHRINK, 0, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_enable_default_dictionary),
	    			     item->prefs->enable_default_dictionary);

	optmenu_default_dictionary = gtk_option_menu_new();
	gtk_table_attach(GTK_TABLE(table), optmenu_default_dictionary, 1, 2,
	    		 rowcount, rowcount + 1, GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu_default_dictionary), 
				 gtkaspell_dictionary_option_menu_new(
					 prefs_common.aspell_path));

	dictionary = item->prefs->default_dictionary;

	optmenu = GTK_OPTION_MENU(optmenu_default_dictionary);

	menu = gtk_option_menu_get_menu(optmenu);
	if (dictionary)
		gtkaspell_set_dictionary_menu_active_item(optmenu_default_dictionary, dictionary);
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	gtk_menu_item_activate(GTK_MENU_ITEM(menuitem));

	SET_TOGGLE_SENSITIVITY(checkbtn_enable_default_dictionary, optmenu_default_dictionary);

	default_dictionary_rec_checkbtn = gtk_check_button_new();
	gtk_table_attach(GTK_TABLE(table), default_dictionary_rec_checkbtn, 2, 3, 
			 rowcount, rowcount + 1, GTK_SHRINK, GTK_SHRINK, 0, 0);
	
	rowcount++;
#endif

	gtk_widget_show_all(table);

	page->window = GTK_WIDGET(window);
	page->table = table;
	page->checkbtn_request_return_receipt = checkbtn_request_return_receipt;
	page->checkbtn_save_copy_to_folder = checkbtn_save_copy_to_folder;
	page->checkbtn_default_to = checkbtn_default_to;
	page->entry_default_to = entry_default_to;
	page->checkbtn_default_reply_to = checkbtn_default_reply_to;
	page->entry_default_reply_to = entry_default_reply_to;
	page->checkbtn_enable_default_account = checkbtn_enable_default_account;
	page->optmenu_default_account = optmenu_default_account;
#ifdef USE_ASPELL
	page->checkbtn_enable_default_dictionary = checkbtn_enable_default_dictionary;
	page->optmenu_default_dictionary = optmenu_default_dictionary;
#endif

	page->request_return_receipt_rec_checkbtn = request_return_receipt_rec_checkbtn;
	page->save_copy_to_folder_rec_checkbtn	  = save_copy_to_folder_rec_checkbtn;
	page->default_to_rec_checkbtn		  = default_to_rec_checkbtn;
	page->default_reply_to_rec_checkbtn	  = default_reply_to_rec_checkbtn;
	page->default_account_rec_checkbtn	  = default_account_rec_checkbtn;
#if USE_ASPELL
	page->default_dictionary_rec_checkbtn = default_dictionary_rec_checkbtn;
#endif

	address_completion_start(page->window);

	page->page.widget = table;
}

void prefs_folder_item_compose_destroy_widget_func(PrefsPage *page_) 
{
	FolderItemComposePage *page = (FolderItemComposePage *) page_;

	if (page->entry_default_to)
		address_completion_unregister_entry(GTK_ENTRY(page->entry_default_to));
	if (page->entry_default_reply_to)
		address_completion_unregister_entry(GTK_ENTRY(page->entry_default_reply_to));
	address_completion_end(page->window);
}

/** \brief  Save the prefs in page to folder.
 *
 *  If the folder is not the one  specified in page->item, then only those properties 
 *  that have the relevant 'apply to sub folders' button checked are saved
 */
static void compose_save_folder_prefs(FolderItem *folder, FolderItemComposePage *page)
{
	FolderItemPrefs *prefs = folder->prefs;
	GtkWidget *menu;
	GtkWidget *menuitem;
	gboolean all = FALSE;

	if (folder->path == NULL)
		return;

	if (page->item == folder) 
		all = TRUE;

	g_return_if_fail(prefs != NULL);

	if (item_protocol(folder) != A_NNTP) {
		if (all || gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->request_return_receipt_rec_checkbtn))) {
			prefs->request_return_receipt = 
				gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_request_return_receipt));
			/* MIGRATION */    
			folder->ret_rcpt = prefs->request_return_receipt;
		}

		if (all || gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->save_copy_to_folder_rec_checkbtn))) {
			prefs->save_copy_to_folder = 
				gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_save_copy_to_folder));
		}

		if (all || gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->default_to_rec_checkbtn))) {

			prefs->enable_default_to = 
				gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_default_to));
			ASSIGN_STRING(prefs->default_to,
				      gtk_editable_get_chars(GTK_EDITABLE(page->entry_default_to), 0, -1));
		}

		if (all || gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->default_reply_to_rec_checkbtn))) {
			prefs->enable_default_reply_to = 
				gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_default_reply_to));
			ASSIGN_STRING(prefs->default_reply_to,
				      gtk_editable_get_chars(GTK_EDITABLE(page->entry_default_reply_to), 0, -1));
		}
	} else {
		prefs->request_return_receipt = FALSE;
		prefs->save_copy_to_folder = FALSE;
		prefs->enable_default_to = FALSE;
		prefs->enable_default_reply_to = FALSE;
	}
	if (all || gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->default_account_rec_checkbtn))) {
		prefs->enable_default_account = 
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_enable_default_account));
		menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(page->optmenu_default_account));
		menuitem = gtk_menu_get_active(GTK_MENU(menu));
		prefs->default_account = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));
	}

#if USE_ASPELL
	if (all || gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->default_dictionary_rec_checkbtn))) {
		prefs->enable_default_dictionary =
			gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_enable_default_dictionary));
		menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(page->optmenu_default_dictionary));
		ASSIGN_STRING(prefs->default_dictionary,
			      gtkaspell_get_dictionary_menu_active_item(menu));
	}
#endif

	folder_item_prefs_save_config(folder);
}	

static gboolean compose_save_recurse_func(GNode *node, gpointer data)
{
	FolderItem *item = (FolderItem *) node->data;
	FolderItemComposePage *page = (FolderItemComposePage *) data;

	g_return_val_if_fail(item != NULL, TRUE);
	g_return_val_if_fail(page != NULL, TRUE);

	compose_save_folder_prefs(item, page);

	/* optimise by not continuing if none of the 'apply to sub folders'
	   check boxes are selected - and optimise the checking by only doing
	   it once */
	if ((node == page->item->node) && item_protocol(item) != A_NNTP &&
	    !(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->request_return_receipt_rec_checkbtn)) ||
	      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->save_copy_to_folder_rec_checkbtn)) ||
	      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->default_to_rec_checkbtn)) ||
	      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->default_account_rec_checkbtn)) ||
#if USE_ASPELL
	      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->default_dictionary_rec_checkbtn)) ||
#endif
	      gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->default_reply_to_rec_checkbtn))))
		return TRUE;

	if ((node == page->item->node) &&
	    !(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->default_account_rec_checkbtn)) 
#if USE_ASPELL
	      || gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->default_dictionary_rec_checkbtn))
#endif
		    ))
		return TRUE;
	else 
		return FALSE;
}

void prefs_folder_item_compose_save_func(PrefsPage *page_) 
{
	FolderItemComposePage *page = (FolderItemComposePage *) page_;

	g_node_traverse(page->item->node, G_PRE_ORDER, G_TRAVERSE_ALL,
			-1, compose_save_recurse_func, page);

}

gint prefs_folder_item_chmod_mode(gchar *folder_chmod) 
{
	gint newmode = 0;
	gchar *tmp;

	if (folder_chmod) {
		newmode = strtol(folder_chmod, &tmp, 8);
		if (!(*(folder_chmod) && !(*tmp)))
			newmode = 0;
	}

	return newmode;
}

static void folder_color_set_dialog(GtkWidget *widget, gpointer data)
{
	FolderItemGeneralPage *page = (FolderItemGeneralPage *) data;
	gint rgbcolor;

	rgbcolor = colorsel_select_color_rgb(_("Pick color for folder"), 
					     page->folder_color);
	gtkut_set_widget_bgcolor_rgb(page->folder_color_btn, rgbcolor);
	page->folder_color = rgbcolor;
}


static void register_general_page()
{
	static gchar *pfi_general_path[2];
	static FolderItemGeneralPage folder_item_general_page;

	pfi_general_path[0] = _("General");
	pfi_general_path[1] = NULL;

        folder_item_general_page.page.path = pfi_general_path;
        folder_item_general_page.page.create_widget = prefs_folder_item_general_create_widget_func;
        folder_item_general_page.page.destroy_widget = prefs_folder_item_general_destroy_widget_func;
        folder_item_general_page.page.save_page = prefs_folder_item_general_save_func;
        
	prefs_folder_item_register_page((PrefsPage *) &folder_item_general_page);
}


static void register_compose_page(void)
{
	static gchar *pfi_compose_path[2];
	static FolderItemComposePage folder_item_compose_page;

	pfi_compose_path[0] = _("Compose");
	pfi_compose_path[1] = NULL;

        folder_item_compose_page.page.path = pfi_compose_path;
        folder_item_compose_page.page.create_widget = prefs_folder_item_compose_create_widget_func;
        folder_item_compose_page.page.destroy_widget = prefs_folder_item_compose_destroy_widget_func;
        folder_item_compose_page.page.save_page = prefs_folder_item_compose_save_func;
        
	prefs_folder_item_register_page((PrefsPage *) &folder_item_compose_page);
}

static GSList *prefs_pages = NULL;

void prefs_folder_item_open(FolderItem *item)
{
	gchar *id, *title;

	if (prefs_pages == NULL) {
		register_general_page();
		register_compose_page();
	}

	if (item->path)
		id = folder_item_get_identifier (item);
	else 
		id = g_strdup(item->name);
	title = g_strdup_printf (_("Properties for folder %s"), id);
	g_free (id);
	prefswindow_open(title, prefs_pages, item,
			&prefs_common.folderitemwin_width, &prefs_common.folderitemwin_height);
	g_free (title);
}

void prefs_folder_item_register_page(PrefsPage *page)
{
	prefs_pages = g_slist_append(prefs_pages, page);
}

void prefs_folder_item_unregister_page(PrefsPage *page)
{
	prefs_pages = g_slist_remove(prefs_pages, page);
}
