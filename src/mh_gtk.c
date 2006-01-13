/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto & the Sylpheed-Claws Team
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

#include "utils.h"
#include "folder.h"
#include "folderview.h"
#include "menu.h"
#include "account.h"
#include "alertpanel.h"
#include "inputdialog.h"
#include "mh.h"
#include "foldersel.h"

static void new_folder_cb(FolderView *folderview, guint action, GtkWidget *widget);
static void delete_folder_cb(FolderView *folderview, guint action, GtkWidget *widget);
static void rename_folder_cb(FolderView *folderview, guint action, GtkWidget *widget);
static void move_folder_cb(FolderView *folderview, guint action, GtkWidget *widget);
static void update_tree_cb(FolderView *folderview, guint action, GtkWidget *widget);
static void remove_mailbox_cb(FolderView *folderview, guint action, GtkWidget *widget);

static GtkItemFactoryEntry mh_popup_entries[] =
{
	{N_("/Create _new folder..."),	 NULL, new_folder_cb,     0, NULL},
	{N_("/_Rename folder..."),	 NULL, rename_folder_cb,  0, NULL},
	{N_("/M_ove folder..."), 	 NULL, move_folder_cb,    0, NULL},
	{N_("/_Delete folder..."),	 NULL, delete_folder_cb,  0, NULL},
	{N_("/---"),			 NULL, NULL,              0, "<Separator>"},
	{N_("/_Check for new messages"), NULL, update_tree_cb,    0, NULL},
	{N_("/C_heck for new folders"),	 NULL, update_tree_cb,    1, NULL},
	{N_("/R_ebuild folder tree"),	 NULL, update_tree_cb,    2, NULL},
	{N_("/---"),			 NULL, NULL, 		  0, "<Separator>"},
	{N_("/Remove _mailbox..."),	 NULL, remove_mailbox_cb, 0, NULL},
	{N_("/---"),			 NULL, NULL, 		  0, "<Separator>"},
};

static void set_sensitivity(GtkItemFactory *factory, FolderItem *item);

static FolderViewPopup mh_popup =
{
	"mh",
	"<MHFolder>",
	NULL,
	set_sensitivity
};

void mh_gtk_init(void)
{
	guint i, n_entries;

	n_entries = sizeof(mh_popup_entries) /
		sizeof(mh_popup_entries[0]);
	for (i = 0; i < n_entries; i++)
		mh_popup.entries = g_slist_append(mh_popup.entries, &mh_popup_entries[i]);

	folderview_register_popup(&mh_popup);
}

static void set_sensitivity(GtkItemFactory *factory, FolderItem *item)
{
	gboolean folder_is_normal = 
			item != NULL &&
			item->stype == F_NORMAL &&
			!folder_has_parent_of_type(item, F_OUTBOX) &&
			!folder_has_parent_of_type(item, F_DRAFT) &&
			!folder_has_parent_of_type(item, F_QUEUE) &&
			!folder_has_parent_of_type(item, F_TRASH);
#define SET_SENS(name, sens) \
	menu_set_sensitive(factory, name, sens)

	SET_SENS("/Create new folder...",   TRUE);
	SET_SENS("/Rename folder...",       item->stype == F_NORMAL && folder_item_parent(item) != NULL);
	SET_SENS("/Move folder...", 	    folder_is_normal && folder_item_parent(item) != NULL);
	SET_SENS("/Delete folder...", 	    item->stype == F_NORMAL && folder_item_parent(item) != NULL);

	SET_SENS("/Check for new messages", folder_item_parent(item) == NULL);
	SET_SENS("/Check for new folders",  folder_item_parent(item) == NULL);
	SET_SENS("/Rebuild folder tree",    folder_item_parent(item) == NULL);

	SET_SENS("/Remove mailbox...",         folder_item_parent(item) == NULL);

#undef SET_SENS
}

static void new_folder_cb(FolderView *folderview, guint action,
		          GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	FolderItem *new_item;
	gchar *new_folder;
	gchar *name;
	gchar *p;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	new_folder = input_dialog(_("New folder"),
				  _("Input the name of new folder:"),
				  _("NewFolder"));
	if (!new_folder) return;
	AUTORELEASE_STR(new_folder, {g_free(new_folder); return;});

	p = strchr(new_folder, G_DIR_SEPARATOR);
	if (p) {
		alertpanel_error(_("'%c' can't be included in folder name."),
				 G_DIR_SEPARATOR);
		return;
	}

	name = trim_string(new_folder, 32);
	AUTORELEASE_STR(name, {g_free(name); return;});

	/* find whether the directory already exists */
	if (folder_find_child_item_by_name(item, new_folder)) {
		alertpanel_error(_("The folder '%s' already exists."), name);
		return;
	}

	new_item = folder_create_folder(item, new_folder);
	if (!new_item) {
		alertpanel_error(_("Can't create the folder '%s'."), name);
		return;
	}

	folder_write_list();
}

static void delete_folder_cb(FolderView *folderview, guint action,
			     GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	gchar *message, *name;
	AlertValue avalue;
	gchar *old_path;
	gchar *old_id;

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	name = trim_string(item->name, 32);
	AUTORELEASE_STR(name, {g_free(name); return;});
	message = g_strdup_printf
		(_("All folders and messages under '%s' will be permanently deleted. "
		   "Recovery will not be possible.\n\n"
		   "Do you really want to delete?"), name);
	avalue = alertpanel_full(_("Delete folder"), message,
				 GTK_STOCK_YES, GTK_STOCK_NO, NULL, FALSE,
				 NULL, ALERT_WARNING, G_ALERTALTERNATE);
	g_free(message);
	if (avalue != G_ALERTDEFAULT) return;

	Xstrdup_a(old_path, item->path, return);
	old_id = folder_item_get_identifier(item);

	if (folderview->opened == folderview->selected ||
	    gtk_ctree_is_ancestor(ctree,
				  folderview->selected,
				  folderview->opened)) {
		summary_clear_all(folderview->summaryview);
		folderview->opened = NULL;
	}

	if (item->folder->klass->remove_folder(item->folder, item) < 0) {
		folder_item_scan(item);
		alertpanel_error(_("Can't remove the folder '%s'."), name);
		g_free(old_id);
		return;
	}

	folder_write_list();

	prefs_filtering_delete_path(old_id);
	g_free(old_id);

}

static void rename_folder_cb(FolderView *folderview, guint action,
			     GtkWidget *widget)
{
	FolderItem *item;
	gchar *new_folder;
	gchar *name;
	gchar *message;
	gchar *old_path;
	gchar *old_id;
	gchar *new_id;
	gchar *base;

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->path != NULL);
	g_return_if_fail(item->folder != NULL);

	name = trim_string(item->name, 32);
	message = g_strdup_printf(_("Input new name for '%s':"), name);
	base = g_path_get_basename(item->path);
	new_folder = input_dialog(_("Rename folder"), message, base);
	g_free(message);
	g_free(name);
	g_free(base);
	if (!new_folder) return;
	AUTORELEASE_STR(new_folder, {g_free(new_folder); return;});

	if (strchr(new_folder, G_DIR_SEPARATOR) != NULL) {
		alertpanel_error(_("'%c' can't be included in folder name."),
				 G_DIR_SEPARATOR);
		return;
	}

	if (folder_find_child_item_by_name(folder_item_parent(item), new_folder)) {
		name = trim_string(new_folder, 32);
		alertpanel_error(_("The folder '%s' already exists."), name);
		g_free(name);
		return;
	}

	Xstrdup_a(old_path, item->path, {g_free(new_folder); return;});

	old_id = folder_item_get_identifier(item);
	
	if (folder_item_rename(item, new_folder) < 0) {
		alertpanel_error(_("The folder could not be renamed.\n"
				   "The new folder name is not allowed."));
		g_free(old_id);
		return;
	}

	/* if (FOLDER_TYPE(item->folder) == F_MH)
		prefs_filtering_rename_path(old_path, item->path); */
	new_id = folder_item_get_identifier(item);
	prefs_filtering_rename_path(old_id, new_id);

	g_free(old_id);
	g_free(new_id);

	folder_item_prefs_save_config(item);
	folder_write_list();
}

static void move_folder_cb(FolderView *folderview, guint action, GtkWidget *widget)
{
	FolderItem *from_folder = NULL, *to_folder = NULL;

	from_folder = folderview_get_selected_item(folderview);
	if (!from_folder || from_folder->folder->klass != mh_get_class())
		return;

	to_folder = foldersel_folder_sel(from_folder->folder, FOLDER_SEL_MOVE, NULL);
	if (!to_folder)
		return;
	
	folderview_move_folder(folderview, from_folder, to_folder);
}

static void update_tree_cb(FolderView *folderview, guint action,
			   GtkWidget *widget)
{
	FolderItem *item;

	item = folderview_get_selected_item(folderview);
	g_return_if_fail(item != NULL);

	summary_show(folderview->summaryview, NULL);

	g_return_if_fail(item->folder != NULL);

	if (action == 0)
		folderview_check_new(item->folder);
	else if (action == 1)
		folderview_rescan_tree(item->folder, FALSE);
	else if (action == 2)
		folderview_rescan_tree(item->folder, TRUE);
}

static void remove_mailbox_cb(FolderView *folderview, guint action,
			      GtkWidget *widget)
{
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
		(_("Really remove the mailbox '%s' ?\n"
		   "(The messages are NOT deleted from the disk)"), name);
	avalue = alertpanel_full(_("Remove mailbox"), message,
		 		 GTK_STOCK_YES, GTK_STOCK_NO, NULL, FALSE,
				 NULL, ALERT_WARNING, G_ALERTALTERNATE);
			    
	g_free(message);
	g_free(name);
	if (avalue != G_ALERTDEFAULT) return;

	folderview_unselect(folderview);
	summary_clear_all(folderview->summaryview);

	folder_destroy(item->folder);
}

