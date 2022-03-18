/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2004-2022 the Claws Mail team
 * Copyright (C) 2014-2015 Charles Lehner
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; withouNULL, t even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#include "claws-features.h"
#endif

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "defs.h"
#include "gtk/gtkutils.h"
#include "gtk/combobox.h"
#include "gtk/inputdialog.h"
#include "gtk/manage_window.h"
#include "alertpanel.h"
#include "utils.h"
#include "prefs.h"
#include "account.h"
#include "mainwindow.h"
#include "managesieve.h"
#include "sieve_editor.h"
#include "sieve_prefs.h"
#include "sieve_manager.h"

enum {
	FILTER_NAME,
	FILTER_ACTIVE,
	N_FILTER_COLUMNS
};

typedef struct {
	SieveManagerPage *page;
	gchar *name_old;
	gchar *name_new;
} CommandDataRename;

typedef struct {
	SieveManagerPage *page;
	gchar *filter_name;
} CommandDataName;

static void filter_got_load_error(SieveSession *session, gpointer data);
static void account_changed(GtkWidget *widget, SieveManagerPage *page);
static void sieve_manager_close(GtkWidget *widget, SieveManagerPage *page);
static gboolean sieve_manager_deleted(GtkWidget *widget, GdkEvent *event,
		SieveManagerPage *page);
static void filter_set_active(SieveManagerPage *page, gchar *filter_name);
gboolean filter_find_by_name (GtkTreeModel *model, GtkTreeIter *iter,
		gchar *filter_name);
static void got_session_error(SieveSession *session, const gchar *msg,
		SieveManagerPage *page);

static GSList *manager_pages = NULL;

/*
 * Perform a command on all manager pages for a given session
 */
#define manager_sessions_foreach(cur, session, page) \
	for(cur = manager_pages; cur != NULL; cur = cur->next) \
		if ((page = (SieveManagerPage *)cur->data) && \
			page->active_session == session)

void sieve_managers_done()
{
	GSList *list = manager_pages;
	manager_pages = NULL;
	g_slist_free_full(list, (GDestroyNotify)sieve_manager_done);
}

static void filters_list_clear(SieveManagerPage *page)
{
	GtkListStore *list_store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(page->filters_list)));
	gtk_list_store_clear(list_store);
	page->got_list = FALSE;
}

static void filters_list_delete_filter(SieveManagerPage *page, gchar *name)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(page->filters_list));

	if (!filter_find_by_name(model, &iter, name))
		return;

	gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
}


static void filters_list_rename_filter(SieveManagerPage *page,
		gchar *name_old, char *name_new)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(page->filters_list));

	if (!filter_find_by_name(model, &iter, name_old))
		return;

	gtk_list_store_set(GTK_LIST_STORE(model), &iter,
			FILTER_NAME, name_new,
			-1);
}

static void filters_list_insert_filter(SieveManagerPage *page,
		SieveScript *filter)
{
	GtkListStore *list_store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(page->filters_list)));
	GtkTreeIter iter;

	gtk_list_store_append(list_store, &iter);
	gtk_list_store_set(list_store, &iter,
			FILTER_NAME, filter->name,
			FILTER_ACTIVE, filter->active,
			-1);
}

static gchar *filters_list_get_selected_filter(GtkWidget *list_view)
{
	GtkTreeSelection *selector;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *res = NULL;

	selector = gtk_tree_view_get_selection(GTK_TREE_VIEW(list_view));

	if (!gtk_tree_selection_get_selected(selector, &model, &iter))
		return NULL;

	gtk_tree_model_get(model, &iter, FILTER_NAME, &res, -1);

	return res;
}

static void filter_add(GtkWidget *widget, SieveManagerPage *page)
{
	SieveSession *session = page->active_session;
	SieveEditorPage *editor;
	if (!session)
		return;
	gchar *filter_name = input_dialog(_("Add Sieve script"),
			_("Enter name for a new Sieve filter script."), "");
	if (!filter_name || !filter_name[0])
		return;

	editor = sieve_editor_get(session, filter_name);
	if (editor) {
		/* TODO: show error that filter already exists */
		sieve_editor_present(editor);
		g_free(filter_name);
		sieve_editor_load(editor,
			(sieve_session_cb_fn)filter_got_load_error, page);
	} else {
		editor = sieve_editor_new(session, filter_name);
		editor->is_new = TRUE;
		sieve_editor_show(editor);
	}
}

static void filter_got_load_error(SieveSession *session, gpointer data)
{
	SieveManagerPage *page = data;

	got_session_error(session, _("Unable to get script contents"), page);
}

static void filter_edit(GtkWidget *widget, SieveManagerPage *page)
{
	SieveEditorPage *editor;
	SieveSession *session = page->active_session;
	gchar *filter_name;

	if (!session)
		return;

	filter_name = filters_list_get_selected_filter(page->filters_list);
	if (!filter_name)
		return;

	editor = sieve_editor_get(session, filter_name);
	if (editor) {
		sieve_editor_present(editor);
		g_free(filter_name);
	} else {
		editor = sieve_editor_new(session, filter_name);
		/* filter_name becomes ownership of newly created
		 * SieveEditorPage, so we do not need to free it here. */
		sieve_editor_load(editor,
			(sieve_session_cb_fn)filter_got_load_error, page);
	}
}

static void filter_renamed(SieveSession *session, gboolean abort,
		gboolean success, CommandDataRename *data)
{
	SieveManagerPage *page = data->page;
	GSList *cur;

	if (abort) {
	} else if (!success) {
		got_session_error(session, "Unable to rename script", page);
	} else {
		manager_sessions_foreach(cur, session, page) {
			filters_list_rename_filter(page, data->name_old,
					data->name_new);
		}
	}
	g_free(data->name_old);
	g_free(data->name_new);
	g_free(data);
}

static void filter_rename(GtkWidget *widget, SieveManagerPage *page)
{
	CommandDataRename *cmd_data;
	gchar *name_old, *name_new;
	SieveSession *session;

	name_old = filters_list_get_selected_filter(page->filters_list);
	if (!name_old)
		return;

	session = page->active_session;
	if (!session)
		return;

	name_new = input_dialog(_("Add Sieve script"),
			_("Enter new name for the script."), name_old);
	if (!name_new)
		return;

	cmd_data = g_new(CommandDataRename, 1);
	cmd_data->name_new = name_new;
	cmd_data->name_old = name_old;
	cmd_data->page = page;
	sieve_session_rename_script(session, name_old, name_new,
			(sieve_session_data_cb_fn)filter_renamed, (gpointer)cmd_data);
}

static void filter_activated(SieveSession *session, gboolean abort,
		const gchar *err, CommandDataName *cmd_data)
{
	SieveManagerPage *page = cmd_data->page;
	GSList *cur;

	if (abort) {
	} else if (err) {
		got_session_error(session, err, page);
	} else {
		manager_sessions_foreach(cur, session, page) {
			filter_set_active(page, cmd_data->filter_name);
		}
	}
	g_free(cmd_data->filter_name);
	g_free(cmd_data);
}

static void sieve_set_active_filter(SieveManagerPage *page, gchar *filter_name)
{
	SieveSession *session;
	CommandDataName *cmd_data;

	session = page->active_session;
	cmd_data = g_new(CommandDataName, 1);
	cmd_data->filter_name = filter_name;
	cmd_data->page = page;

	sieve_session_set_active_script(session, filter_name,
			(sieve_session_data_cb_fn)filter_activated, cmd_data);
}

static void filter_deleted(SieveSession *session, gboolean abort,
		const gchar *err_msg,
		CommandDataName *cmd_data)
{
	SieveManagerPage *page = cmd_data->page;
	GSList *cur;

	if (abort) {
	} else if (err_msg) {
		got_session_error(session, err_msg, page);
	} else {
		manager_sessions_foreach(cur, session, page) {
			filters_list_delete_filter(page,
					cmd_data->filter_name);
		}
	}
	g_free(cmd_data->filter_name);
	g_free(cmd_data);
}


static void filter_delete(GtkWidget *widget, SieveManagerPage *page)
{
	gchar buf[256];
	gchar *filter_name;
	SieveSession *session;
	CommandDataName *cmd_data;

	filter_name = filters_list_get_selected_filter(page->filters_list);
	if (filter_name == NULL)
		return;

	session = page->active_session;
	if (!session)
		return;

	g_snprintf(buf, sizeof(buf),
		   _("Do you really want to delete the filter '%s'?"), filter_name);
	if (alertpanel_full(_("Delete filter"), buf,
				NULL, _("_Cancel"), "edit-delete", _("_Delete"), NULL, NULL,
				ALERTFOCUS_FIRST, FALSE, NULL, ALERT_WARNING) != G_ALERTALTERNATE)
		return;

	cmd_data = g_new(CommandDataName, 1);
	cmd_data->filter_name = filter_name;
	cmd_data->page = page;

	sieve_session_delete_script(session, filter_name,
			(sieve_session_data_cb_fn)filter_deleted, cmd_data);
}

/*
 * select a filter in the list
 *
 * return TRUE is successfully selected, FALSE otherwise
 */

static gboolean filter_select (GtkWidget *list_view, GtkTreeModel *model,
		GtkTreeIter *iter)
{
	GtkTreeSelection *selection;
	GtkTreePath* path;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list_view));
	gtk_tree_selection_select_iter(selection, iter);
	path = gtk_tree_model_get_path(model, iter);
	if (path == NULL) return FALSE;
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(list_view), path, NULL, FALSE);
	gtk_tree_path_free(path);
	return TRUE;
}

/*
 * find matching filter. return FALSE on match
 */
static gboolean filter_search_equal_fn (GtkTreeModel *model, gint column,
		const gchar *key, GtkTreeIter *iter, gpointer search_data)
{
	SieveManagerPage *page = (SieveManagerPage *)search_data;
	gchar *filter_name;

	if (!key) return TRUE;

	gtk_tree_model_get (model, iter, FILTER_NAME, &filter_name, -1);

	if (strncmp (key, filter_name, strlen(key)) != 0) return TRUE;
	return !filter_select(page->filters_list, model, iter);
}

/*
 * search for a filter row by its name. return true if found.
 */
gboolean filter_find_by_name (GtkTreeModel *model, GtkTreeIter *iter,
		gchar *filter_name)
{
	gchar *name;

	if (!gtk_tree_model_get_iter_first (model, iter))
		return FALSE;

	do {
		gtk_tree_model_get (model, iter, FILTER_NAME, &name, -1);
		if (strcmp(filter_name, name) == 0) {
			return TRUE;
		}
	} while (gtk_tree_model_iter_next (model, iter));
	return FALSE;
}

static gboolean filter_set_inactive(GtkTreeModel *model,
		GtkTreePath *path, GtkTreeIter *iter, gpointer data)
{
	gtk_list_store_set(GTK_LIST_STORE(model), iter,
			FILTER_ACTIVE, FALSE,
			-1);
	return FALSE;
}

/*
 * Set the active filter in the table.
 * @param filter_name The filter to make active (may be null)
 */
static void filter_set_active(SieveManagerPage *page, gchar *filter_name)
{
	GtkTreeIter iter;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(page->filters_list));

	/* Deactivate all filters */
	gtk_tree_model_foreach(model, filter_set_inactive, NULL);

	/* Set active filter */
	if (filter_name) {
		if (!filter_find_by_name (model, &iter, filter_name))
			return;

		gtk_list_store_set(GTK_LIST_STORE(model), &iter,
				FILTER_ACTIVE, TRUE,
				-1);
	}
}

static void filter_active_toggled(GtkCellRendererToggle *widget,
				    gchar *path,
				    SieveManagerPage *page)
{
	GtkTreeIter iter;
	gchar *filter_name;
	gboolean active;
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(page->filters_list));

	if (!gtk_tree_model_get_iter_from_string(model, &iter, path))
		return;

	/* get existing value */
	gtk_tree_model_get(model, &iter,
			FILTER_NAME, &filter_name,
			FILTER_ACTIVE, &active,
			-1);

	sieve_set_active_filter(page, active ? NULL : filter_name);
}

static void filter_double_clicked(GtkTreeView *list_view,
				   GtkTreePath *path, GtkTreeViewColumn *column,
				   SieveManagerPage *page)
{
	filter_edit(GTK_WIDGET(list_view), page);
}

static void filters_create_list_view_columns(SieveManagerPage *page,
		GtkWidget *list_view)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkWidget *label;

	/* Name */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
		(_("Name"), renderer,
		 "text", FILTER_NAME,
		 NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);
	gtk_tree_view_column_set_expand(column, TRUE);

	/* Active */
	renderer = gtk_cell_renderer_toggle_new();
	g_object_set(renderer,
		     "radio", TRUE,
		     "activatable", TRUE,
		      NULL);
	column = gtk_tree_view_column_new_with_attributes
		(_("Active"), renderer,
		 "active", FILTER_ACTIVE,
		 NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);
	gtk_tree_view_column_set_alignment (column, 0.5);

	/* the column header needs a widget to have a tooltip */
	label = gtk_label_new(gtk_tree_view_column_get_title(column));
	gtk_widget_show(label);
	gtk_tree_view_column_set_widget(column, label);
	CLAWS_SET_TIP(label,
			_("An account can only have one active script at a time."));

	g_signal_connect(G_OBJECT(renderer), "toggled",
			 G_CALLBACK(filter_active_toggled), page);

	gtk_tree_view_set_search_column(GTK_TREE_VIEW(list_view), FILTER_NAME);
	gtk_tree_view_set_search_equal_func(GTK_TREE_VIEW(list_view),
			filter_search_equal_fn, page, NULL);
}


static GtkListStore* filters_create_data_store(void)
{
	return gtk_list_store_new(N_FILTER_COLUMNS,
			G_TYPE_STRING,	/* FILTER_NAME */
			G_TYPE_BOOLEAN,	/* FILTER_ACTIVE */
			-1);
}

static GtkWidget *filters_list_view_create(SieveManagerPage *page)
{
	GtkTreeView *list_view;
	GtkTreeSelection *selector;
	GtkListStore *store = filters_create_data_store();

	list_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(store)));
	g_object_unref(G_OBJECT(store));

	selector = gtk_tree_view_get_selection(list_view);
	gtk_tree_selection_set_mode(selector, GTK_SELECTION_BROWSE);

	/* create the columns */
	filters_create_list_view_columns(page, GTK_WIDGET(list_view));

	/* set a double click listener */
	g_signal_connect(G_OBJECT(list_view), "row_activated",
			G_CALLBACK(filter_double_clicked),
			page);

	return GTK_WIDGET(list_view);
}

static gboolean manager_key_pressed(GtkWidget *widget, GdkEventKey *event,
				    gpointer data)
{
	SieveManagerPage* page = (SieveManagerPage *) data;

	if (event && event->keyval == GDK_KEY_Escape)
		sieve_manager_done(page);

	return FALSE;
}

static void size_allocate_cb(GtkWidget *widget, GtkAllocation *allocation)
{
	cm_return_if_fail(allocation != NULL);

	gtk_window_get_size(GTK_WINDOW(widget),
		&sieve_config.manager_win_width, &sieve_config.manager_win_height);
}

static void got_session_error(SieveSession *session, const gchar *msg,
		SieveManagerPage *page)
{
	if (!g_slist_find(manager_pages, page))
		return;
	if (page->active_session != session)
		return;
	gtk_label_set_text(GTK_LABEL(page->status_text), msg);
}

static void sieve_manager_on_error(SieveSession *session,
		const gchar *msg, gpointer user_data)
{
	SieveManagerPage *page = (SieveManagerPage *)user_data;
	got_session_error(session, msg, page);
}

static void sieve_manager_on_connected(SieveSession *session,
		gboolean connected, gpointer user_data)
{
	SieveManagerPage *page = (SieveManagerPage *)user_data;
	if (page->active_session != session)
		return;
	if (!connected) {
		gtk_widget_set_sensitive(GTK_WIDGET(page->vbox_buttons), FALSE);
		gtk_label_set_text(GTK_LABEL(page->status_text),
				_("Unable to connect"));
	}
}

static void got_filter_listed(SieveSession *session, gboolean abort,
		SieveScript *script, SieveManagerPage *page)
{
	if (abort)
		return;
	if (!script) {
		got_session_error(session, "Unable to list scripts", page);
		return;
	}
	if (!script->name) {
		/* done receiving list */
		page->got_list = TRUE;
		gtk_widget_set_sensitive(GTK_WIDGET(page->vbox_buttons), TRUE);
		gtk_label_set_text(GTK_LABEL(page->status_text), "");
		return;
	}
	filters_list_insert_filter(page, script);
}

/*
 * An account was selected from the menu. Get its list of scripts.
 */
static void account_changed(GtkWidget *widget, SieveManagerPage *page)
{
	gint account_id;
	PrefsAccount *account;
	SieveSession *session;

	if (page->accounts_menu == NULL)
		return;

	account_id = combobox_get_active_data(GTK_COMBO_BOX(page->accounts_menu));
	account = account_find_from_id(account_id);
	if (!account)
		return;
	session = page->active_session = sieve_session_get_for_account(account);
	sieve_session_handle_status(session,
			sieve_manager_on_error,
			sieve_manager_on_connected,
			page);
	filters_list_clear(page);
	if (session_is_connected(SESSION(session))) {
		gtk_label_set_text(GTK_LABEL(page->status_text),
				_("Listing scripts..."));
	} else {
		gtk_label_set_text(GTK_LABEL(page->status_text),
				_("Connecting..."));
	}
	sieve_session_list_scripts(session,
			(sieve_session_data_cb_fn)got_filter_listed, (gpointer)page);
}

static SieveManagerPage *sieve_manager_page_new()
{
	SieveManagerPage *page;
	GtkWidget *window;
	GtkWidget *hbox, *vbox, *vbox_allbuttons, *vbox_buttons;
	GtkWidget *accounts_menu;
	GtkWidget *label;
	GtkWidget *scrolledwin;
	GtkWidget *list_view;
	GtkWidget *btn;
	GtkWidget *status_text;
	GtkTreeIter iter;
	GtkListStore *menu;
	GList *account_list, *cur;
	PrefsAccount *ap;
	SieveAccountConfig *config;
	PrefsAccount *default_account = NULL;

	static GdkGeometry geometry;

	page = g_new0(SieveManagerPage, 1);

	/* Manage Window */

	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "sievemanager");
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_window_set_title (GTK_WINDOW (window), _("Manage Sieve Filters"));
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
	MANAGE_WINDOW_SIGNALS_CONNECT (window);

	g_signal_connect (G_OBJECT (window), "key_press_event",
			G_CALLBACK (manager_key_pressed), page);
	g_signal_connect (G_OBJECT(window), "size_allocate",
			 G_CALLBACK (size_allocate_cb), NULL);
	g_signal_connect (G_OBJECT(window), "delete_event",
			 G_CALLBACK (sieve_manager_deleted), page);

	if (!geometry.min_height) {
		geometry.min_width = 350;
		geometry.min_height = 280;
	}

	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &geometry,
				      GDK_HINT_MIN_SIZE);
	gtk_window_set_default_size(GTK_WINDOW(window), sieve_config.manager_win_width,
			sieve_config.manager_win_height);
	gtk_window_set_type_hint(GTK_WINDOW(window),
			GDK_WINDOW_TYPE_HINT_DIALOG);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

	/* Accounts list */

	label = gtk_label_new (_("Account"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	accounts_menu = gtkut_sc_combobox_create(NULL, FALSE);
	menu = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(accounts_menu)));
	gtk_box_pack_start (GTK_BOX (hbox), accounts_menu, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT(accounts_menu), "changed",
			  G_CALLBACK (account_changed), page);

	account_list = account_get_list();
	for (cur = account_list; cur != NULL; cur = cur->next) {
		ap = (PrefsAccount *)cur->data;
		config = sieve_prefs_account_get_config(ap);
		if (config->enable) {
			COMBOBOX_ADD (menu, ap->account_name, ap->account_id);
			if (!default_account || ap->is_default)
				default_account = ap;
		}
		sieve_prefs_account_free_config(config);
	}

	if (!default_account) {
		gtk_widget_destroy(label);
		gtk_widget_destroy(accounts_menu);
		accounts_menu = NULL;
	}

	/* status */
	status_text = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (hbox), status_text, FALSE, FALSE, 0);
	gtk_label_set_justify (GTK_LABEL (status_text), GTK_JUSTIFY_LEFT);

	/* Filters list */

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 2);

	/* Table */

	scrolledwin = gtk_scrolled_window_new (NULL, NULL);
	gtk_box_pack_start (GTK_BOX (hbox), scrolledwin, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwin),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	list_view = filters_list_view_create(page);
	gtk_container_add(GTK_CONTAINER(scrolledwin), list_view);

	/* Buttons */

	vbox_allbuttons = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_box_pack_start (GTK_BOX (hbox), vbox_allbuttons, FALSE, FALSE, 0);

	/* buttons that depend on there being a connection */
	vbox_buttons = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_set_sensitive(vbox_buttons, FALSE);
	gtk_box_pack_start (GTK_BOX (vbox_allbuttons), vbox_buttons, FALSE, FALSE, 0);

	/* new */
	btn = gtk_button_new_with_mnemonic("_New");
	gtk_box_pack_start (GTK_BOX (vbox_buttons), btn, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT(btn), "clicked",
			  G_CALLBACK (filter_add), page);

	/* edit */
	btn = gtk_button_new_with_mnemonic("_Edit");
	gtk_box_pack_start (GTK_BOX (vbox_buttons), btn, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT(btn), "clicked",
			G_CALLBACK (filter_edit), page);

	/* delete */
	btn = gtkut_stock_button("edit-delete", _("D_elete"));
	gtk_box_pack_start (GTK_BOX (vbox_buttons), btn, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT(btn), "clicked",
			G_CALLBACK (filter_delete), page);

	/* rename */
	btn = gtk_button_new_with_mnemonic(_("_Rename"));
	gtk_box_pack_start (GTK_BOX (vbox_buttons), btn, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT(btn), "clicked",
			G_CALLBACK (filter_rename), page);

	/* refresh */
	btn = gtkut_stock_button("view-refresh", "_Refresh");
	gtk_box_pack_end (GTK_BOX (vbox_allbuttons), btn, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT(btn), "clicked",
			G_CALLBACK (account_changed), page);

	/* bottom area stuff */

	gtkut_stock_button_set_create(&hbox,
			&btn, "window-close", _("_Close"),
			NULL, NULL, NULL, NULL, NULL, NULL);

	/* close */
	gtk_box_pack_end (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_grab_default (btn);
	g_signal_connect (G_OBJECT (btn), "clicked",
			  G_CALLBACK (sieve_manager_close), page);

	page->window = window;
	page->accounts_menu = accounts_menu;
	page->filters_list = list_view;
	page->status_text = status_text;
	page->vbox_buttons = vbox_buttons;

	/* select default (first) account */
	if (default_account) {
		combobox_select_by_data(GTK_COMBO_BOX(accounts_menu),
				default_account->account_id);
	} else {
		gtk_label_set_text(GTK_LABEL(status_text),
				_("To use Sieve, enable it in an account's preferences."));
	}

	return page;
}

static gboolean sieve_manager_deleted(GtkWidget *widget, GdkEvent *event,
		SieveManagerPage *page)
{
	sieve_manager_done(page);
	return FALSE;
}

static void sieve_manager_close(GtkWidget *widget, SieveManagerPage *page)
{
	sieve_manager_done(page);
}

void sieve_manager_done(SieveManagerPage *page)
{
	manager_pages = g_slist_remove(manager_pages, page);
	sieve_sessions_discard_callbacks(page);
	gtk_widget_destroy(page->window);
	g_free(page);
}

void sieve_manager_show()
{
	SieveManagerPage *page = sieve_manager_page_new();
	manager_pages = g_slist_prepend(manager_pages, page);
	gtk_widget_show_all(page->window);
}

void sieve_manager_script_created(SieveSession *session, const gchar *name)
{
	SieveManagerPage *page;
	SieveScript script = {.name = (gchar *)name};
	GSList *cur;

	manager_sessions_foreach(cur, session, page) {
		filters_list_insert_filter(page, &script);
	}
}
