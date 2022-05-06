/*
 * mailmbox Plugin -- mbox support for Claws Mail
 * Copyright (C) 2003-2022 the Claws Mail team,
 *			   Christoph Hohmann, 
 *			   Hoa v. Dinh,
 * 			   Alfons Hoogervorst
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#ifdef HAVE_CONFIG_H
#  include "config.h"
#  include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>

#include "plugin.h"
#include "folder.h"
#include "mailmbox_folder.h"
#include "mainwindow.h"
#include "folderview.h"
#include "inputdialog.h"
#include "foldersel.h"
#include "alertpanel.h"
#include "main.h"
#include "menu.h"
#include "account.h"
#include "prefs_actions.h"
#include "summaryview.h"
#include "folder_item_prefs.h"

static void add_mailbox(GtkAction *action, gpointer callback_data);
static void new_folder_cb(GtkAction *action, gpointer data);
static void delete_folder_cb(GtkAction *action, gpointer data);
static void rename_folder_cb(GtkAction *action, gpointer data);
static void move_folder_cb(GtkAction *action, gpointer data);
static void copy_folder_cb(GtkAction *action, gpointer data);
static void update_tree_cb(GtkAction *action, gpointer data);
static void remove_mailbox_cb(GtkAction *action, gpointer data);

static GtkActionEntry claws_mailmbox_popup_entries[] = 
{
	{"FolderViewPopup/CreateNewFolder",	NULL, N_("Create _new folder..."), NULL, NULL, G_CALLBACK(new_folder_cb) },
	{"FolderViewPopup/RenameFolder",	NULL, N_("_Rename folder..."), NULL, NULL, G_CALLBACK(rename_folder_cb) },
	{"FolderViewPopup/MoveFolder",		NULL, N_("M_ove folder..."), NULL, NULL, G_CALLBACK(move_folder_cb) },
	{"FolderViewPopup/CopyFolder",		NULL, N_("Cop_y folder..."), NULL, NULL, G_CALLBACK(copy_folder_cb) },
	{"FolderViewPopup/DeleteFolder",	NULL, N_("_Delete folder..."), NULL, NULL, G_CALLBACK(delete_folder_cb) },
	{"FolderViewPopup/CheckNewMessages",	NULL, N_("_Check for new messages"), NULL, NULL, G_CALLBACK(update_tree_cb) }, /*0*/
	{"FolderViewPopup/CheckNewFolders",	NULL, N_("C_heck for new folders"), NULL, NULL, G_CALLBACK(update_tree_cb) }, /*1*/
	{"FolderViewPopup/RebuildTree",		NULL, N_("R_ebuild folder tree"), NULL, NULL, G_CALLBACK(update_tree_cb) }, /*2*/
	{"FolderViewPopup/RemoveMailbox",	NULL, N_("Remove _mailbox..."), NULL, NULL, G_CALLBACK(remove_mailbox_cb) },
};			
static void set_sensitivity(GtkUIManager *ui_manager, FolderItem *item);
static void add_menuitems(GtkUIManager *ui_manager, FolderItem *item);

static FolderViewPopup claws_mailmbox_popup =
{
	"mailmbox",
	"<MailmboxFolder>",
	claws_mailmbox_popup_entries,
	G_N_ELEMENTS(claws_mailmbox_popup_entries),
	NULL, 0,
	NULL, 0, 0, NULL,
	add_menuitems,
	set_sensitivity
};

static GtkActionEntry mainwindow_add_mailbox[] = {{
	"File/AddMailbox/Mbox",
	NULL, "mbox...", NULL, NULL, G_CALLBACK(add_mailbox)
}};

static guint main_menu_id = 0;

gint plugin_gtk_init(gchar **error)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();

	folderview_register_popup(&claws_mailmbox_popup);

	gtk_action_group_add_actions(mainwin->action_group, mainwindow_add_mailbox,
			1, (gpointer)mainwin);
	MENUITEM_ADDUI_ID_MANAGER(mainwin->ui_manager, "/Menu/File/AddMailbox", "Mbox", 
			  "File/AddMailbox/Mbox", GTK_UI_MANAGER_MENUITEM,
			  main_menu_id)

	return 0;
}

void plugin_gtk_done(void)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();
	
	if (mainwin == NULL || claws_is_exiting())
		return;

	folderview_unregister_popup(&claws_mailmbox_popup);

	MENUITEM_REMUI_MANAGER(mainwin->ui_manager,mainwin->action_group, "File/AddMailbox/Mbox", main_menu_id);
	main_menu_id = 0;
}

static void add_menuitems(GtkUIManager *ui_manager, FolderItem *item)
{
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "CreateNewFolder", "FolderViewPopup/CreateNewFolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SeparatorMbox1", "FolderViewPopup/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "RenameFolder", "FolderViewPopup/RenameFolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "MoveFolder", "FolderViewPopup/MoveFolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "CopyFolder", "FolderViewPopup/CopyFolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SeparatorMbox2", "FolderViewPopup/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "DeleteFolder", "FolderViewPopup/DeleteFolder", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SeparatorMbox3", "FolderViewPopup/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "CheckNewMessages", "FolderViewPopup/CheckNewMessages", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "CheckNewFolders", "FolderViewPopup/CheckNewFolders", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "RebuildTree", "FolderViewPopup/RebuildTree", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SeparatorMbox4", "FolderViewPopup/---", GTK_UI_MANAGER_SEPARATOR)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "RemoveMailbox", "FolderViewPopup/RemoveMailbox", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Popup/FolderViewPopup", "SeparatorMbox5", "FolderViewPopup/---", GTK_UI_MANAGER_SEPARATOR)
}

static void set_sensitivity(GtkUIManager *ui_manager, FolderItem *item)
{
	gboolean folder_is_normal = 
			item != NULL &&
			item->stype == F_NORMAL &&
			!folder_has_parent_of_type(item, F_OUTBOX) &&
			!folder_has_parent_of_type(item, F_DRAFT) &&
			!folder_has_parent_of_type(item, F_QUEUE) &&
			!folder_has_parent_of_type(item, F_TRASH);
#define SET_SENS(name, sens) \
	cm_menu_set_sensitive_full(ui_manager, "Popup/"name, sens)

	SET_SENS("FolderViewPopup/CreateNewFolder",	item && item->stype != F_INBOX);
	SET_SENS("FolderViewPopup/RenameFolder",	item && item->stype == F_NORMAL && folder_item_parent(item) != NULL);
	SET_SENS("FolderViewPopup/MoveFolder",		folder_is_normal && folder_item_parent(item) != NULL);
	SET_SENS("FolderViewPopup/DeleteFolder",	item && item->stype == F_NORMAL && folder_item_parent(item) != NULL);

	SET_SENS("FolderViewPopup/CheckNewMessages",	folder_item_parent(item) == NULL);
	SET_SENS("FolderViewPopup/CheckNewFolders", 	folder_item_parent(item) == NULL);
	SET_SENS("FolderViewPopup/RebuildTree",		folder_item_parent(item) == NULL);

	SET_SENS("FolderViewPopup/RemoveMailbox",	folder_item_parent(item) == NULL);

#undef SET_SENS
}

#define DO_ACTION(name, act)	{ if (!strcmp(a_name, name)) act; }

static void update_tree_cb(GtkAction *action, gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	FolderItem *item;
	const gchar *a_name = gtk_action_get_name(action);

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);

	summary_show(folderview->summaryview, NULL, FALSE);

	g_return_if_fail(item->folder != NULL);

	DO_ACTION("FolderViewPopup/CheckNewMessages", folderview_check_new(item->folder));
	DO_ACTION("FolderViewPopup/CheckNewFolders", folderview_rescan_tree(item->folder, FALSE));
	DO_ACTION("FolderViewPopup/RebuildTree", folderview_rescan_tree(item->folder, TRUE));
}

static void add_mailbox(GtkAction *action, gpointer callback_data)
{
	MainWindow *mainwin = (MainWindow *) callback_data;
	gchar *path, *basename;
	Folder *folder;

	path = input_dialog(_("Add mailbox"),
			    _("Input the location of mailbox.\n"
			      "If the existing mailbox is specified, it will be\n"
			      "scanned automatically."),
			    "Mail");
	if (!path) return;
	if (folder_find_from_path(path)) {
		alertpanel_error(_("The mailbox '%s' already exists."), path);
		g_free(path);
		return;
	}
	basename = g_path_get_basename(path);

	if (!folder_local_name_ok(basename)) {
		g_free(path);
		g_free(basename);
		return;
	}

	folder = folder_new(folder_get_class_from_string("mailmbox"), 
			    !strcmp(path, "Mail") ? _("Mailbox") : basename,
			    path);
	g_free(basename);
	g_free(path);

	if (folder->klass->create_tree(folder) < 0) {
		alertpanel_error(_("Creation of the mailbox failed.\n"
				   "Maybe some files already exist, or you don't have the permission to write there."));
		folder_destroy(folder);
		return;
	}

	folder_add(folder);
	folder_scan_tree(folder, TRUE);

	folderview_set(mainwin->folderview);

	return;
}

static void new_folder_cb(GtkAction *action, gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	FolderItem *item;
	FolderItem *new_item;
	gchar *new_folder;
	gchar *name;
	gchar *p;

	if (!folderview->selected) return;

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	new_folder = input_dialog(_("New folder"),
				  _("Input the name of new folder:"),
				  _("NewFolder"));
	if (!new_folder) return;
	AUTORELEASE_STR(new_folder, {g_free(new_folder); return;});

	p = strchr(new_folder, G_DIR_SEPARATOR);
	if (p == NULL)
		p = strchr(new_folder, '.');
	if (p) {
		alertpanel_error(_("'%c' can't be included in folder name."),
				 p[0]);
		return;
	}

	if (!folder_local_name_ok(new_folder))
		return;

	name = trim_string(new_folder, 32);
	AUTORELEASE_STR(name, {g_free(name); return;});

	/* find whether the directory already exists */
	p = g_strconcat(item->path ? item->path : "", ".", new_folder, NULL);
	if (folder_find_child_item_by_name(item, p)) {
		g_free(p);
		alertpanel_error(_("The folder '%s' already exists."), name);
		return;
	}
	g_free(p);

	new_item = folder_create_folder(item, new_folder);
	if (!new_item) {
		alertpanel_error(_("Can't create the folder '%s'."), name);
		return;
	}

	folder_write_list();
}

static void remove_mailbox_cb(GtkAction *action, gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	FolderItem *item;
	gchar *name;
	gchar *message;
	AlertValue avalue;

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	if (folder_item_parent(item)) return;

	name = trim_string(item->folder->name, 32);
	message = g_strdup_printf
		(_("Really remove the mailbox '%s'?\n"
		   "(The messages are NOT deleted from the disk)"), name);
	avalue = alertpanel_full(_("Remove mailbox"), message,
				 NULL, _("_Cancel"), "list-remove", _("_Remove"),
				 NULL, NULL, ALERTFOCUS_FIRST, FALSE,
				 NULL, ALERT_WARNING);
	g_free(message);
	g_free(name);
	if (avalue != G_ALERTALTERNATE) return;

	folderview_unselect(folderview);
	summary_clear_all(folderview->summaryview);

	folder_destroy(item->folder);
}

static void delete_folder_cb(GtkAction *action, gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	FolderItem *item, *opened;
	gchar *message, *name;
	AlertValue avalue;
	gchar *old_id;

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);
	opened = folderview_get_opened_item(folderview);

	name = trim_string(item->name, 32);
	AUTORELEASE_STR(name, {g_free(name); return;});
	message = g_strdup_printf
		(_("All folders and messages under '%s' will be permanently deleted.\n"
		   "Recovery will not be possible.\n\n"
		   "Do you really want to delete?"), name);
	avalue = alertpanel_full(_("Delete folder"), message,
				 NULL, _("_Cancel"), "edit-delete", _("_Delete"),
				 NULL, NULL, ALERTFOCUS_FIRST, FALSE,
				 NULL, ALERT_NOTICE);
	g_free(message);
	if (avalue != G_ALERTALTERNATE) return;

	old_id = folder_item_get_identifier(item);

	if (item == opened ||
			folder_is_child_of(item, opened)) {
		summary_clear_all(folderview->summaryview);
		folderview_close_opened(folderview, TRUE);
	}

	if (item->folder->klass->remove_folder(item->folder, item) < 0) {
		alertpanel_error(_("Can't remove the folder '%s'."), name);
		if (item == opened)
			summary_show(folderview->summaryview,
				     folderview->summaryview->folder_item, FALSE);
		g_free(old_id);
		return;
	}

	folder_write_list();

	prefs_filtering_delete_path(old_id);
	g_free(old_id);

}

static void move_folder_cb(GtkAction *action, gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	FolderItem *from_folder = NULL, *to_folder = NULL;
	gchar *msg;

	from_folder = folderview_get_selected_item(folderview);
	if (!from_folder || from_folder->folder->klass != claws_mailmbox_get_class())
		return;

	msg = g_strdup_printf(_("Select folder to move folder '%s' to"),
		from_folder->name);
	to_folder = foldersel_folder_sel(NULL, FOLDER_SEL_MOVE, NULL, FALSE, msg);
	g_free(msg);
	if (!to_folder)
		return;

	folderview_move_folder(folderview, from_folder, to_folder, 0);
}

static void copy_folder_cb(GtkAction *action, gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	FolderItem *from_folder = NULL, *to_folder = NULL;
	gchar *msg;

	from_folder = folderview_get_selected_item(folderview);
	if (!from_folder || from_folder->folder->klass != claws_mailmbox_get_class())
		return;

	msg = g_strdup_printf(_("Select folder to copy folder '%s' to"),
		from_folder->name);
	to_folder = foldersel_folder_sel(NULL, FOLDER_SEL_MOVE, NULL, FALSE, msg);
	g_free(msg);
	if (!to_folder)
		return;

	folderview_move_folder(folderview, from_folder, to_folder, 1);
}

static void rename_folder_cb(GtkAction *action, gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	FolderItem *item, *parent;
	gchar *new_folder;
	gchar *name;
	gchar *message;
	gchar *old_id;
	gchar *new_id;
	gchar *p;

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	name = trim_string(item->name, 32);
	message = g_strdup_printf(_("Input new name for '%s':"), name);
	new_folder = input_dialog(_("Rename folder"), message, item->name);
	g_free(message);
	g_free(name);
	if (!new_folder) return;
	AUTORELEASE_STR(new_folder, {g_free(new_folder); return;});

	p = strchr(new_folder, G_DIR_SEPARATOR);
	if (p == NULL)
		p = strchr(new_folder, '.');
	if (p) {
		alertpanel_error(_("'%c' can't be included in folder name."),
				 p[0]);
		return;
	}

	if (!folder_local_name_ok(new_folder))
		return;

	parent = folder_item_parent(item);
	p = g_strconcat(parent->path ? parent->path : "", ".", new_folder, NULL);
	if (folder_find_child_item_by_name(parent, p)) {
		name = trim_string(new_folder, 32);
		alertpanel_error(_("The folder '%s' already exists."), name);
		g_free(name);
		return;
	}

	old_id = folder_item_get_identifier(item);

	if (folder_item_rename(item, new_folder) < 0) {
		alertpanel_error(_("The folder could not be renamed.\n"
				   "The new folder name is not allowed."));
		g_free(old_id);
		return;
	}

	new_id = folder_item_get_identifier(item);
	prefs_filtering_rename_path(old_id, new_id);
	account_rename_path(old_id, new_id);
	prefs_actions_rename_path(old_id, new_id);

	g_free(old_id);
	g_free(new_id);

	folder_item_prefs_save_config_recursive(item);
	folder_write_list();
}

