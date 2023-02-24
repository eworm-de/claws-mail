/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2005-2023 the Claws Mail Team and Andrej Kacian <andrej@kacian.sk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

/* Global includes */
#include <gtk/gtk.h>
#include <glib/gi18n.h>

/* Claws Mail includes */
#include <gtk/menu.h>
#include <mainwindow.h>
#include <inputdialog.h>
#include <folderview.h>
#include <alertpanel.h>
#include <main.h>
#include <summaryview.h>

/* Local includes */
#include "rssyl.h"
#include "rssyl_prefs.h"
#include "rssyl_gtk.h"
#include "rssyl_cb_menu.h"

static char *rssyl_popup_menu_labels[] =
{
	N_("_Refresh feed"),
	N_("Feed pr_operties"),
	N_("Rena_me..."),
	N_("R_efresh recursively"),
	N_("Subscribe _new feed..."),
	N_("Create new _folder..."),
	N_("Import feed list..."),
	N_("_Delete folder..."),
	N_("Remove tree"),
	NULL
};

static GtkActionEntry rssyl_popup_entries[] = 
{
	{"FolderViewPopup/RefreshFeed", NULL, NULL, NULL, NULL, G_CALLBACK(rssyl_refresh_feed_cb) },
	{"FolderViewPopup/FeedProperties", NULL, NULL, NULL, NULL, G_CALLBACK(rssyl_prop_cb) },
	{"FolderViewPopup/RenameFolder", NULL, NULL, NULL, NULL, G_CALLBACK(rssyl_rename_cb) },
	{"FolderViewPopup/RefreshAllFeeds", NULL, NULL, NULL, NULL, G_CALLBACK(rssyl_update_all_cb) },
	{"FolderViewPopup/NewFeed", NULL, NULL, NULL, NULL, G_CALLBACK(rssyl_new_feed_cb) },
	{"FolderViewPopup/NewFolder", NULL, NULL, NULL, NULL, G_CALLBACK(rssyl_new_folder_cb) },
	{"FolderViewPopup/ImportFeedList", NULL, NULL, NULL, NULL, G_CALLBACK(rssyl_import_feed_list_cb) },
	{"FolderViewPopup/RemoveFolder", NULL, NULL, NULL, NULL, G_CALLBACK(rssyl_remove_folder_cb) },
	{"FolderViewPopup/RemoveMailbox", NULL, NULL, NULL, NULL, G_CALLBACK(rssyl_remove_mailbox_cb) }
};

static void rssyl_add_menuitems(GtkUIManager *ui_manager, FolderItem *item)
{
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "RefreshFeed", "FolderViewPopup/RefreshFeed", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "FeedProperties", "FolderViewPopup/FeedProperties", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "RenameFolder", "FolderViewPopup/RenameFolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SeparatorRSS1", "FolderViewPopup/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "RefreshAllFeeds", "FolderViewPopup/RefreshAllFeeds", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SeparatorRSS2", "FolderViewPopup/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "NewFeed", "FolderViewPopup/NewFeed", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "NewFolder", "FolderViewPopup/NewFolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "ImportFeedList", "FolderViewPopup/ImportFeedList", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SeparatorRSS3", "FolderViewPopup/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "RemoveFolder", "FolderViewPopup/RemoveFolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SeparatorRSS4", "FolderView/Popup/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "RemoveMailbox", "FolderViewPopup/RemoveMailbox", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SeparatorRSS5", "FolderView/Popup/---", GTK_UI_MANAGER_SEPARATOR)
}

static void rssyl_set_sensitivity(GtkUIManager *ui_manager, FolderItem *item)
{
#define SET_SENS(name, sens) \
	cm_menu_set_sensitive_full(ui_manager, "Popup/"name, sens)
	RFolderItem *ritem = (RFolderItem *)item;
	SET_SENS("FolderViewPopup/RefreshFeed", folder_item_parent(item) != NULL && ritem->url );
	SET_SENS("FolderViewPopup/FeedProperties", folder_item_parent(item) != NULL && ritem->url );
	SET_SENS("FolderViewPopup/RenameFolder", folder_item_parent(item) != NULL );
	SET_SENS("FolderViewPopup/RefreshAllFeeds", TRUE );
	SET_SENS("FolderViewPopup/NewFeed", TRUE);
	SET_SENS("FolderViewPopup/NewFolder", TRUE );
	SET_SENS("FolderViewPopup/RemoveFolder", folder_item_parent(item) != NULL);
	SET_SENS("FolderViewPopup/RemoveMailbox", folder_item_parent(item) == NULL);

#undef SET_SENS
}

static FolderViewPopup rssyl_popup =
{
	"rssyl",
	"<rssyl>",
	rssyl_popup_entries,
	G_N_ELEMENTS(rssyl_popup_entries),
	NULL, 0,
	NULL, 0, 0, NULL,
	rssyl_add_menuitems,
	rssyl_set_sensitivity
};

static void rssyl_add_mailbox(GtkAction *action, gpointer callback_data)
{
	MainWindow *mainwin = (MainWindow *) callback_data;
	gchar *path = NULL, *tmp = NULL;
	Folder *folder;

	path = input_dialog(_("Add RSS folder tree"),
			_("Enter name for a new RSS folder tree."),
			RSSYL_DEFAULT_MAILBOX);
	if( !path ) return;

	if( folder_find_from_path(path) ) {
		alertpanel_error(_("The mailbox '%s' already exists."), path);
		g_free(path);
		return;
	}

	tmp = g_path_get_basename(path);
	folder = folder_new(folder_get_class_from_string("rssyl"), tmp, path);
	g_free(tmp);
	g_free(path);

	if( folder->klass->create_tree(folder) < 0 ) {
		alertpanel_error(_("Creation of folder tree failed.\n"
				"Maybe some files already exist, or you don't have the permission "
				"to write there?"));
		folder_destroy(folder);
		return;
	}

	folder_add(folder);
	folder_scan_tree(folder, TRUE);

	folderview_set(mainwin->folderview);
}


static GtkActionEntry mainwindow_add_mailbox[] = {{
	"File/AddMailbox/RSSyl",
	NULL, "RSSyl...", NULL, NULL, G_CALLBACK(rssyl_add_mailbox)
}};

static void rssyl_fill_popup_menu_labels(void) {
	gint i;

	for (i = 0; rssyl_popup_menu_labels[i] != NULL; i++) { 
		(rssyl_popup_entries[i].label = _(rssyl_popup_menu_labels[i]));
	}
}

static guint main_menu_id = 0;

void rssyl_gtk_init(void)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();
	gtk_action_group_add_actions(mainwin->action_group, mainwindow_add_mailbox,
			1, (gpointer)mainwin);
	MENUITEM_ADDUI_ID_MANAGER(mainwin->ui_manager, "/Menu/File/AddMailbox", PLUGIN_NAME,
			  "File/AddMailbox/"PLUGIN_NAME, GTK_UI_MANAGER_MENUITEM,
			  main_menu_id);
	rssyl_fill_popup_menu_labels();
	folderview_register_popup(&rssyl_popup);
}

void rssyl_gtk_done(void)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();
	FolderView *folderview = NULL;
	FolderItem *fitem = NULL;

	if (mainwin == NULL || claws_is_exiting())
		return;

	folderview = mainwin->folderview;
	fitem = folderview->summaryview->folder_item;

	if( fitem && IS_RSSYL_FOLDER_ITEM(fitem) ) {
		folderview_unselect(folderview);
		summary_clear_all(folderview->summaryview);
	}

	folderview_unregister_popup(&rssyl_popup);

	MENUITEM_REMUI_MANAGER(mainwin->ui_manager,mainwin->action_group, "File/AddMailbox/RSSyl", main_menu_id);
	main_menu_id = 0;

}
