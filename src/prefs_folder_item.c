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

/* alfons - all folder item specific settings should migrate into 
 * folderlist.xml!!! the old folderitemrc file will only serve for a few 
 * versions (for compatibility) */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "intl.h"
#include "defs.h"
#include "folder.h"
#include "prefs_folder_item.h"
#include "folderview.h"
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

struct FolderItemGeneralPage
{
	PrefsPage page;

	FolderItem *item;

	GtkWidget *table;
	GtkWidget *checkbtn_simplify_subject;
	GtkWidget *entry_simplify_subject;
	GtkWidget *checkbtn_folder_chmod;
	GtkWidget *entry_folder_chmod;
	GtkWidget *folder_color_btn;
	GtkWidget *checkbtn_enable_processing;
	GtkWidget *checkbtn_newmailcheck;

	gint	   folder_color;
};

struct FolderItemComposePage
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
};


gint prefs_folder_item_chmod_mode		(gchar *folder_chmod);

static void folder_color_set_dialog(GtkWidget *widget, gpointer data);

#define SAFE_STRING(str) \
	(str) ? (str) : ""

void prefs_folder_item_general_create_widget_func(PrefsPage * page_,
						   GtkWindow * window,
                                		   gpointer data)
{
	struct FolderItemGeneralPage *page = (struct FolderItemGeneralPage *) page_;
	FolderItem *item = (FolderItem *) data;
	guint rowcount;

	GtkWidget *table;
	GtkWidget *hbox;
	
	GtkWidget *checkbtn_simplify_subject;
	GtkWidget *entry_simplify_subject;
	GtkWidget *checkbtn_folder_chmod;
	GtkWidget *entry_folder_chmod;
	GtkWidget *folder_color;
	GtkWidget *folder_color_btn;
	GtkWidget *checkbtn_enable_processing;
	GtkWidget *checkbtn_newmailcheck;

	page->item	   = item;

	/* Table */
	table = gtk_table_new(4, 2, FALSE);
	gtk_widget_show(table);
	gtk_table_set_row_spacings(GTK_TABLE(table), -1);
	rowcount = 0;

	/* Simplify Subject */
	checkbtn_simplify_subject = gtk_check_button_new_with_label(_("Simplify Subject RegExp: "));
	gtk_widget_show(checkbtn_simplify_subject);
	gtk_table_attach(GTK_TABLE(table), checkbtn_simplify_subject, 0, 1, 
			 rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_SHRINK, 0, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_simplify_subject), 
				     item->prefs->enable_simplify_subject);

	entry_simplify_subject = gtk_entry_new();
	gtk_widget_show(entry_simplify_subject);
	gtk_table_attach_defaults(GTK_TABLE(table), entry_simplify_subject, 1, 2, 
				  rowcount, rowcount + 1);
	SET_TOGGLE_SENSITIVITY(checkbtn_simplify_subject, entry_simplify_subject);
	gtk_entry_set_text(GTK_ENTRY(entry_simplify_subject), 
			   SAFE_STRING(item->prefs->simplify_subject_regexp));

	rowcount++;

	/* Folder chmod */
	checkbtn_folder_chmod = gtk_check_button_new_with_label(_("Folder chmod: "));
	gtk_widget_show(checkbtn_folder_chmod);
	gtk_table_attach(GTK_TABLE(table), checkbtn_folder_chmod, 0, 1, 
			 rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_SHRINK, 0, 0);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_folder_chmod), 
				     item->prefs->enable_folder_chmod);

	entry_folder_chmod = gtk_entry_new();
	gtk_widget_show(entry_folder_chmod);
	gtk_table_attach_defaults(GTK_TABLE(table), entry_folder_chmod, 1, 2, 
				  rowcount, rowcount + 1);
	SET_TOGGLE_SENSITIVITY(checkbtn_folder_chmod, entry_folder_chmod);
	if (item->prefs->folder_chmod) {
		gchar *buf;

		buf = g_strdup_printf("%o", item->prefs->folder_chmod);
		gtk_entry_set_text(GTK_ENTRY(entry_folder_chmod), buf);
		g_free(buf);
	}
	
	rowcount++;
	
	/* Folder color */
	folder_color = gtk_label_new(_("Folder color: "));
	gtk_misc_set_alignment(GTK_MISC(folder_color), 0, 0.5);
	gtk_widget_show(folder_color);
	gtk_table_attach_defaults(GTK_TABLE(table), folder_color, 0, 1, 
			 rowcount, rowcount + 1);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox);
	gtk_table_attach_defaults(GTK_TABLE(table), hbox, 1, 2, 
				  rowcount, rowcount + 1);

	folder_color_btn = gtk_button_new_with_label("");
	gtk_widget_set_size_request(folder_color_btn, 36, 26);
  	gtk_box_pack_start (GTK_BOX(hbox), folder_color_btn, FALSE, FALSE, 0);

	page->folder_color = item->prefs->color;

	g_signal_connect(G_OBJECT(folder_color_btn), "clicked",
			 G_CALLBACK(folder_color_set_dialog),
			 page);

	gtkut_set_widget_bgcolor_rgb(folder_color_btn, item->prefs->color);

	rowcount++;

	/* Enable processing at startup */
	checkbtn_enable_processing = gtk_check_button_new_with_label(_("Process at startup"));
	gtk_widget_show(checkbtn_enable_processing);
	gtk_table_attach(GTK_TABLE(table), checkbtn_enable_processing, 0, 2, 
			 rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_FILL, 0, 0);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_enable_processing), 
				     item->prefs->enable_processing);

	rowcount++;

	/* Check folder for new mail */
	checkbtn_newmailcheck = gtk_check_button_new_with_label(_("Scan for new mail"));
	gtk_widget_show(checkbtn_newmailcheck);
	gtk_table_attach(GTK_TABLE(table), checkbtn_newmailcheck, 0, 2,
					 rowcount, rowcount+1, GTK_SHRINK | GTK_FILL, GTK_FILL, 0, 0);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_newmailcheck),
								 item->prefs->newmailcheck);
	
	rowcount++;

	page->table = table;
	page->checkbtn_simplify_subject = checkbtn_simplify_subject;
	page->entry_simplify_subject = entry_simplify_subject;
	page->checkbtn_folder_chmod = checkbtn_folder_chmod;
	page->entry_folder_chmod = entry_folder_chmod;
	page->folder_color_btn = folder_color_btn;
	page->checkbtn_enable_processing = checkbtn_enable_processing;
	page->checkbtn_newmailcheck = checkbtn_newmailcheck;

	page->page.widget = table;
}

void prefs_folder_item_general_destroy_widget_func(PrefsPage *page_) 
{
	/* struct FolderItemGeneralPage *page = (struct FolderItemGeneralPage *) page_; */
}

void prefs_folder_item_general_save_func(PrefsPage *page_) 
{
	gchar *buf;
	struct FolderItemGeneralPage *page = (struct FolderItemGeneralPage *) page_;
	FolderItemPrefs *prefs = page->item->prefs;

	g_return_if_fail(prefs != NULL);

	prefs->enable_simplify_subject =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_simplify_subject));
	ASSIGN_STRING(prefs->simplify_subject_regexp,
	    gtk_editable_get_chars(GTK_EDITABLE(page->entry_simplify_subject), 0, -1));
	
/*
	if (page->item == page->folderview->summaryview->folder_item &&
	    (prefs->enable_simplify_subject != old_simplify_val ||  
	    0 != strcmp2(prefs->simplify_subject_regexp, old_simplify_str))) {
		summary_clear_all(page->folderview->summaryview);
		page->folderview->opened = NULL;
		page->folderview->selected = NULL;
		folderview_select(page->folderview, page->item);
	}
*/
	prefs->enable_folder_chmod = 
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_folder_chmod));
	buf = gtk_editable_get_chars(GTK_EDITABLE(page->entry_folder_chmod), 0, -1);
	prefs->folder_chmod = prefs_folder_item_chmod_mode(buf);
	g_free(buf);

	prefs->color = page->folder_color;
	/* update folder view */
	if (prefs->color > 0)
		folder_item_update(page->item, F_ITEM_UPDATE_MSGCNT);

	prefs->enable_processing = 
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_enable_processing));

	prefs->newmailcheck = 
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_newmailcheck));

	folder_item_prefs_save_config(page->item);
}

void prefs_folder_item_compose_create_widget_func(PrefsPage * page_,
						   GtkWindow * window,
                                		   gpointer data)
{
	struct FolderItemComposePage *page = (struct FolderItemComposePage *) page_;
	FolderItem *item = (FolderItem *) data;
	guint rowcount;

	GtkWidget *table;
	
	GtkWidget *checkbtn_request_return_receipt;
	GtkWidget *checkbtn_save_copy_to_folder;
	GtkWidget *checkbtn_default_to;
	GtkWidget *entry_default_to;
	GtkWidget *checkbtn_default_reply_to;
	GtkWidget *entry_default_reply_to;
	GtkWidget *checkbtn_enable_default_account;
	GtkWidget *optmenu_default_account;
	GtkWidget *optmenu_default_account_menu;
	GtkWidget *optmenu_default_account_menuitem;
#if USE_ASPELL
	GtkWidget *checkbtn_enable_default_dictionary;
	GtkWidget *optmenu_default_dictionary;
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
	gtk_widget_show(table);
	gtk_table_set_row_spacings(GTK_TABLE(table), -1);
	rowcount = 0;

	/* Request Return Receipt */
	checkbtn_request_return_receipt = gtk_check_button_new_with_label
		(_("Request Return Receipt"));
	gtk_widget_show(checkbtn_request_return_receipt);
	gtk_table_attach(GTK_TABLE(table), checkbtn_request_return_receipt, 
			 0, 2, rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, 
			 GTK_FILL, 0, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_request_return_receipt),
				     item->ret_rcpt ? TRUE : FALSE);

	rowcount++;

	/* Save Copy to Folder */
	checkbtn_save_copy_to_folder = gtk_check_button_new_with_label
		(_("Save copy of outgoing messages to this folder instead of Sent"));
	gtk_widget_show(checkbtn_save_copy_to_folder);
	gtk_table_attach(GTK_TABLE(table), checkbtn_save_copy_to_folder, 0, 2, 
			 rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_FILL, 0, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_save_copy_to_folder),
				     item->prefs->save_copy_to_folder ? TRUE : FALSE);

	rowcount++;

	/* Default To */
	checkbtn_default_to = gtk_check_button_new_with_label(_("Default To: "));
	gtk_widget_show(checkbtn_default_to);
	gtk_table_attach(GTK_TABLE(table), checkbtn_default_to, 0, 1, 
			 rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_SHRINK, 0, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_default_to), 
				     item->prefs->enable_default_to);

	entry_default_to = gtk_entry_new();
	gtk_widget_show(entry_default_to);
	gtk_table_attach_defaults(GTK_TABLE(table), entry_default_to, 1, 2, rowcount, rowcount + 1);
	SET_TOGGLE_SENSITIVITY(checkbtn_default_to, entry_default_to);
	gtk_entry_set_text(GTK_ENTRY(entry_default_to), SAFE_STRING(item->prefs->default_to));
	address_completion_register_entry(GTK_ENTRY(entry_default_to));

	rowcount++;

	/* Default address to reply to */
	checkbtn_default_reply_to = gtk_check_button_new_with_label(_("Send replies to: "));
	gtk_widget_show(checkbtn_default_reply_to);
	gtk_table_attach(GTK_TABLE(table), checkbtn_default_reply_to, 0, 1, 
			 rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_SHRINK, 0, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_default_reply_to), 
				     item->prefs->enable_default_reply_to);

	entry_default_reply_to = gtk_entry_new();
	gtk_widget_show(entry_default_reply_to);
	gtk_table_attach_defaults(GTK_TABLE(table), entry_default_reply_to, 1, 2, rowcount, rowcount + 1);
	SET_TOGGLE_SENSITIVITY(checkbtn_default_reply_to, entry_default_reply_to);
	gtk_entry_set_text(GTK_ENTRY(entry_default_reply_to), SAFE_STRING(item->prefs->default_reply_to));
	address_completion_register_entry(GTK_ENTRY(entry_default_reply_to));

	rowcount++;

	/* Default account */
	checkbtn_enable_default_account = gtk_check_button_new_with_label(_("Default account: "));
	gtk_widget_show(checkbtn_enable_default_account);
	gtk_table_attach(GTK_TABLE(table), checkbtn_enable_default_account, 0, 1, 
			 rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_SHRINK, 0, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_enable_default_account), 
				     item->prefs->enable_default_account);

 	optmenu_default_account = gtk_option_menu_new ();
 	gtk_widget_show (optmenu_default_account);
	gtk_table_attach_defaults(GTK_TABLE(table), optmenu_default_account, 1, 2, 
				  rowcount, rowcount + 1);
 	optmenu_default_account_menu = gtk_menu_new ();

	account_list = account_get_list();
	account_index = 0;
	index = 0;
	for (cur_ac = account_list; cur_ac != NULL; cur_ac = cur_ac->next) {
		ac_prefs = (PrefsAccount *)cur_ac->data;
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

	rowcount++;

#if USE_ASPELL
	/* Default dictionary */
	checkbtn_enable_default_dictionary = gtk_check_button_new_with_label(_("Default dictionary: "));
	gtk_widget_show(checkbtn_enable_default_dictionary);
	gtk_table_attach(GTK_TABLE(table), checkbtn_enable_default_dictionary, 0, 1,
	    		 rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_SHRINK, 0, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_enable_default_dictionary),
	    			     item->prefs->enable_default_dictionary);

	optmenu_default_dictionary = gtk_option_menu_new();
	gtk_widget_show(optmenu_default_dictionary);
	gtk_table_attach_defaults(GTK_TABLE(table), optmenu_default_dictionary, 1, 2,
	    			rowcount, rowcount + 1);

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

	rowcount++;
#endif

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

	address_completion_start(page->window);

	page->page.widget = table;
}

void prefs_folder_item_compose_destroy_widget_func(PrefsPage *page_) 
{
	struct FolderItemComposePage *page = (struct FolderItemComposePage *) page_;

	address_completion_unregister_entry(GTK_ENTRY(page->entry_default_to));
	address_completion_unregister_entry(GTK_ENTRY(page->entry_default_reply_to));
	address_completion_end(page->window);
}

void prefs_folder_item_compose_save_func(PrefsPage *page_) 
{
	struct FolderItemComposePage *page = (struct FolderItemComposePage *) page_;
	FolderItemPrefs *prefs = page->item->prefs;
	GtkWidget *menu;
	GtkWidget *menuitem;

	g_return_if_fail(prefs != NULL);

	prefs->request_return_receipt = 
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_request_return_receipt));
	/* MIGRATION */    
	page->item->ret_rcpt = prefs->request_return_receipt;

	prefs->save_copy_to_folder = 
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_save_copy_to_folder));

	prefs->enable_default_to = 
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_default_to));
	ASSIGN_STRING(prefs->default_to,
	    gtk_editable_get_chars(GTK_EDITABLE(page->entry_default_to), 0, -1));

	prefs->enable_default_reply_to = 
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_default_reply_to));
	ASSIGN_STRING(prefs->default_reply_to,
	    gtk_editable_get_chars(GTK_EDITABLE(page->entry_default_reply_to), 0, -1));

	prefs->enable_default_account = 
 	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_enable_default_account));
 	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(page->optmenu_default_account));
 	menuitem = gtk_menu_get_active(GTK_MENU(menu));
 	prefs->default_account = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));

#if USE_ASPELL
	prefs->enable_default_dictionary =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_enable_default_dictionary));
	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(page->optmenu_default_dictionary));
	ASSIGN_STRING(prefs->default_dictionary,
	    gtkaspell_get_dictionary_menu_active_item(menu));
#endif

	folder_item_prefs_save_config(page->item);
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
	struct FolderItemGeneralPage *page = (struct FolderItemGeneralPage *) data;
	gint rgbcolor;

	rgbcolor = colorsel_select_color_rgb(_("Pick color for folder"), 
					     page->folder_color);
	gtkut_set_widget_bgcolor_rgb(page->folder_color_btn, rgbcolor);
	page->folder_color = rgbcolor;
}

struct FolderItemGeneralPage folder_item_general_page;

static void register_general_page()
{
        folder_item_general_page.page.path = _("General");
        folder_item_general_page.page.create_widget = prefs_folder_item_general_create_widget_func;
        folder_item_general_page.page.destroy_widget = prefs_folder_item_general_destroy_widget_func;
        folder_item_general_page.page.save_page = prefs_folder_item_general_save_func;
        
	prefs_folder_item_register_page((PrefsPage *) &folder_item_general_page);
}

struct FolderItemComposePage folder_item_compose_page;

static void register_compose_page(void)
{
        folder_item_compose_page.page.path = _("Compose");
        folder_item_compose_page.page.create_widget = prefs_folder_item_compose_create_widget_func;
        folder_item_compose_page.page.destroy_widget = prefs_folder_item_compose_destroy_widget_func;
        folder_item_compose_page.page.save_page = prefs_folder_item_compose_save_func;
        
	prefs_folder_item_register_page((PrefsPage *) &folder_item_compose_page);
}

static GSList *prefs_pages = NULL;

void prefs_folder_item_open(FolderItem *item)
{
	if (prefs_pages == NULL) {
		register_general_page();
		register_compose_page();
	}

	prefswindow_open(_("Settings for folder"), prefs_pages, item);
}

void prefs_folder_item_register_page(PrefsPage *page)
{
	prefs_pages = g_slist_append(prefs_pages, page);
}

void prefs_folder_item_unregister_page(PrefsPage *page)
{
	prefs_pages = g_slist_remove(prefs_pages, page);
}
