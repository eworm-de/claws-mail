/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto & the Sylpheed-Claws Team
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
#include <glib/gi18n.h>

#include <gtk/gtk.h>

#include "utils.h"
#include "folder.h"
#include "folderview.h"
#include "menu.h"
#include "account.h"
#include "alertpanel.h"
#include "grouplistdialog.h"
#include "common/hooks.h"
#include "inc.h"

static void subscribe_newsgroup_cb(FolderView *folderview, guint action, GtkWidget *widget);
static void unsubscribe_newsgroup_cb(FolderView *folderview, guint action, GtkWidget *widget);
static void news_settings_cb(FolderView *folderview, guint action, GtkWidget *widget);
static void remove_news_server_cb(FolderView *folderview, guint action, GtkWidget *widget);
static void update_tree_cb(FolderView *folderview, guint action, GtkWidget *widget);
static void download_cb(FolderView *folderview, guint action, GtkWidget *widget);

static GtkItemFactoryEntry news_popup_entries[] =
{
	{N_("/_Subscribe to newsgroup..."),	NULL, subscribe_newsgroup_cb,    0, NULL},
	{N_("/_Unsubscribe newsgroup"),		NULL, unsubscribe_newsgroup_cb,  0, NULL},
	{N_("/---"),				NULL, NULL,                      0, "<Separator>"},
	{N_("/Down_load"),			NULL, download_cb,               0, NULL},
	{N_("/---"),				NULL, NULL,                      0, "<Separator>"},
	{N_("/_Check for new messages"),	NULL, update_tree_cb,            0, NULL},
	{N_("/---"),				NULL, NULL,                      0, "<Separator>"},
	{N_("/News _account settings"),		NULL, news_settings_cb,     	 0, NULL},
	{N_("/Remove _news account"),		NULL, remove_news_server_cb,     0, NULL},
	{N_("/---"),				NULL, NULL, 		         0, "<Separator>"},
};

static void set_sensitivity(GtkItemFactory *factory, FolderItem *item);

static FolderViewPopup news_popup =
{
	"news",
	"<NewsFolder>",
	NULL,
	set_sensitivity
};

void news_gtk_init(void)
{
	guint i, n_entries;

	n_entries = sizeof(news_popup_entries) /
		sizeof(news_popup_entries[0]);
	for (i = 0; i < n_entries; i++)
		news_popup.entries = g_slist_append(news_popup.entries, &news_popup_entries[i]);

	folderview_register_popup(&news_popup);
}

static void set_sensitivity(GtkItemFactory *factory, FolderItem *item)
{
#define SET_SENS(name, sens) \
	menu_set_sensitive(factory, name, sens)

	SET_SENS("/Subscribe to newsgroup...", folder_item_parent(item) == NULL);
	SET_SENS("/Unsubscribe newsgroup",     folder_item_parent(item) != NULL);
	SET_SENS("/Remove news account",       folder_item_parent(item) == NULL);

	SET_SENS("/Check for new messages",    folder_item_parent(item) == NULL);

#undef SET_SENS
}

static void subscribe_newsgroup_cb(FolderView *folderview, guint action, GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	GtkCTreeNode *servernode, *node;
	Folder *folder;
	FolderItem *item;
	FolderItem *rootitem;
	FolderItem *newitem;
	GSList *new_subscr;
	GSList *cur;
	GNode *gnode;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	folder = item->folder;
	g_return_if_fail(folder != NULL);
	g_return_if_fail(FOLDER_TYPE(folder) == F_NEWS);
	g_return_if_fail(folder->account != NULL);

	if (GTK_CTREE_ROW(folderview->selected)->parent != NULL)
		servernode = GTK_CTREE_ROW(folderview->selected)->parent;
	else
		servernode = folderview->selected;

	rootitem = gtk_ctree_node_get_row_data(ctree, servernode);

	new_subscr = grouplist_dialog(folder);

	/* remove unsubscribed newsgroups */
	for (gnode = folder->node->children; gnode != NULL; ) {
		GNode *next = gnode->next;

		item = FOLDER_ITEM(gnode->data);
		if (g_slist_find_custom(new_subscr, item->path,
					(GCompareFunc)g_ascii_strcasecmp) != NULL) {
			gnode = next;
			continue;
		}

		node = gtk_ctree_find_by_row_data(ctree, servernode, item);
		if (!node) {
			gnode = next;
			continue;
		}

		if (folderview->opened == node) {
			summary_clear_all(folderview->summaryview);
			folderview->opened = NULL;
		}

		gtk_ctree_remove_node(ctree, node);
		folder_item_remove(item);

		gnode = next;
	}

	gtk_clist_freeze(GTK_CLIST(ctree));

	/* add subscribed newsgroups */
	for (cur = new_subscr; cur != NULL; cur = cur->next) {
		gchar *name = (gchar *)cur->data;
		FolderUpdateData hookdata;

		if (folder_find_child_item_by_name(rootitem, name) != NULL)
			continue;

		newitem = folder_item_new(folder, name, name);
		folder_item_append(rootitem, newitem);

		hookdata.folder = newitem->folder;
		hookdata.update_flags = FOLDER_TREE_CHANGED | FOLDER_ADD_FOLDERITEM;
		hookdata.item = newitem;
		hooks_invoke(FOLDER_UPDATE_HOOKLIST, &hookdata);
	}

	gtk_clist_thaw(GTK_CLIST(ctree));

	slist_free_strings(new_subscr);
	g_slist_free(new_subscr);

	folder_write_list();
}

static void unsubscribe_newsgroup_cb(FolderView *folderview, guint action,
				     GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	gchar *name;
	gchar *message;
	gchar *old_id;
	AlertValue avalue;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(FOLDER_TYPE(item->folder) == F_NEWS);
	g_return_if_fail(item->folder->account != NULL);

	old_id = folder_item_get_identifier(item);

	name = trim_string(item->path, 32);
	message = g_strdup_printf(_("Really unsubscribe newsgroup '%s'?"), name);
	avalue = alertpanel(_("Unsubscribe newsgroup"), message,
			    _("Yes"), _("+No"), NULL);
	g_free(message);
	g_free(name);
	if (avalue != G_ALERTDEFAULT) return;

	if (folderview->opened == folderview->selected) {
		summary_clear_all(folderview->summaryview);
		folderview->opened = NULL;
	}

	folder_item_remove(item);
	folder_write_list();
	
	prefs_filtering_delete_path(old_id);
	g_free(old_id);
}

static void news_settings_cb(FolderView *folderview, guint action, GtkWidget *widget)
{
	FolderItem *item;

	item = folderview_get_selected_item(folderview);
	if (item == NULL)
		return;

	account_open(item->folder->account);
}

static void remove_news_server_cb(FolderView *folderview, guint action,
				  GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	PrefsAccount *account;
	gchar *name;
	gchar *message;
	AlertValue avalue;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	g_return_if_fail(FOLDER_TYPE(item->folder) == F_NEWS);
	g_return_if_fail(item->folder->account != NULL);

	name = trim_string(item->folder->name, 32);
	message = g_strdup_printf(_("Really delete news account '%s'?"), name);
	avalue = alertpanel(_("Delete news account"), message,
			    _("Yes"), _("+No"), NULL);
	g_free(message);
	g_free(name);

	if (avalue != G_ALERTDEFAULT) return;

	if (folderview->opened == folderview->selected ||
	    gtk_ctree_is_ancestor(ctree,
				  folderview->selected,
				  folderview->opened)) {
		summary_clear_all(folderview->summaryview);
		folderview->opened = NULL;
	}

	account = item->folder->account;
	folderview_unselect(folderview);
	summary_clear_all(folderview->summaryview);
	folder_destroy(item->folder);
	account_destroy(account);
	account_set_menu();
	main_window_reflect_prefs_all();
	folder_write_list();
}

static void update_tree_cb(FolderView *folderview, guint action,
			   GtkWidget *widget)
{
	FolderItem *item;

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);

	summary_show(folderview->summaryview, NULL);

	g_return_if_fail(item->folder != NULL);

	folderview_check_new(item->folder);
}

static void download_cb(FolderView *folderview, guint action,
			GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	MainWindow *mainwin = folderview->mainwin;
	FolderItem *item;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
#if 0
	if (!prefs_common.online_mode) {
		if (alertpanel(_("Offline"),
			       _("You are offline. Go online?"),
			       _("Yes"), _("No"), NULL) == G_ALERTDEFAULT)
			main_window_toggle_online(folderview->mainwin, TRUE);
		else
			return;
	}
#endif
	main_window_cursor_wait(mainwin);
	inc_lock();
	main_window_lock(mainwin);
	gtk_widget_set_sensitive(folderview->ctree, FALSE);
	main_window_progress_on(mainwin);
	GTK_EVENTS_FLUSH();
	if (folder_item_fetch_all_msg(item) < 0) {
		gchar *name;

		name = trim_string(item->name, 32);
		alertpanel_error(_("Error occurred while downloading messages in '%s'."), name);
		g_free(name);
	}
	folder_set_ui_func(item->folder, NULL, NULL);
	main_window_progress_off(mainwin);
	gtk_widget_set_sensitive(folderview->ctree, TRUE);
	main_window_unlock(mainwin);
	inc_unlock();
	main_window_cursor_normal(mainwin);
}
