/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto & The Sylpheed Claws Team
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
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "prefs_gtk.h"
#include "inc.h"
#include "utils.h"
#include "gtkutils.h"
#include "manage_window.h"
#include "mainwindow.h"
#include "prefs_common.h"
#include "alertpanel.h"
#include "prefs_actions.h"
#include "action.h"
#include "description_window.h"
#include "gtkutils.h"

enum {
	PREFS_ACTIONS_STRING,	/*!< string pointer managed by list store, 
				 *   and never touched or retrieved by 
				 *   us */ 
	PREFS_ACTIONS_DATA,	/*!< pointer to string that is not managed by 
				 *   the list store, and which is retrieved
				 *   and touched by us */
	PREFS_ACTIONS_VALID,	/*!< contains a valid action, otherwise "(New)" */
	N_PREFS_ACTIONS_COLUMNS
};

static struct Actions
{
	GtkWidget *window;

	GtkWidget *ok_btn;

	GtkWidget *name_entry;
	GtkWidget *cmd_entry;

	GtkWidget *actions_list_view;
} actions;

static int modified = FALSE;

/* widget creating functions */
static void prefs_actions_create	(MainWindow *mainwin);
static void prefs_actions_set_dialog	(void);
static gint prefs_actions_clist_set_row	(GtkTreeIter *row);

/* callback functions */
static void prefs_actions_help_cb	(GtkWidget	*w,
					 gpointer	 data);
static void prefs_actions_register_cb	(GtkWidget	*w,
					 gpointer	 data);
static void prefs_actions_substitute_cb	(GtkWidget	*w,
					 gpointer	 data);
static void prefs_actions_delete_cb	(GtkWidget	*w,
					 gpointer	 data);
static void prefs_actions_up		(GtkWidget	*w,
					 gpointer	 data);
static void prefs_actions_down		(GtkWidget	*w,
					 gpointer	 data);
static gint prefs_actions_deleted	(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	*data);
static gboolean prefs_actions_key_pressed(GtkWidget	*widget,
					  GdkEventKey	*event,
					  gpointer	 data);
static void prefs_actions_cancel	(GtkWidget	*w,
					 gpointer	 data);
static void prefs_actions_ok		(GtkWidget	*w,
					 gpointer	 data);


static GtkListStore* prefs_actions_create_data_store	(void);

static void prefs_actions_list_view_insert_action	(GtkWidget *list_view,
							 GtkTreeIter *row_iter,
							 gchar *action,
							 gboolean is_valid);
static GtkWidget *prefs_actions_list_view_create	(void);
static void prefs_actions_create_list_view_columns	(GtkWidget *list_view);
static gboolean prefs_actions_selected			(GtkTreeSelection *selector,
							 GtkTreeModel *model, 
							 GtkTreePath *path,
							 gboolean currently_selected,
							 gpointer data);

void prefs_actions_open(MainWindow *mainwin)
{
	inc_lock();

	if (!actions.window)
		prefs_actions_create(mainwin);

	manage_window_set_transient(GTK_WINDOW(actions.window));
	gtk_widget_grab_focus(actions.ok_btn);

	prefs_actions_set_dialog();

	gtk_widget_show(actions.window);
}

static void prefs_actions_create(MainWindow *mainwin)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *confirm_area;

	GtkWidget *vbox1;

	GtkWidget *entry_vbox;
	GtkWidget *hbox;
	GtkWidget *name_label;
	GtkWidget *name_entry;
	GtkWidget *cmd_label;
	GtkWidget *cmd_entry;

	GtkWidget *reg_hbox;
	GtkWidget *btn_hbox;
	GtkWidget *arrow;
	GtkWidget *reg_btn;
	GtkWidget *subst_btn;
	GtkWidget *del_btn;

	GtkWidget *cond_hbox;
	GtkWidget *cond_scrolledwin;
	GtkWidget *cond_list_view;

	GtkWidget *help_button;

	GtkWidget *btn_vbox;
	GtkWidget *up_btn;
	GtkWidget *down_btn;

	debug_print("Creating actions configuration window...\n");

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

	gtk_container_set_border_width(GTK_CONTAINER (window), 8);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
	gtk_window_set_default_size(GTK_WINDOW(window), 400, -1);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	gtkut_stock_button_set_create(&confirm_area, &ok_btn, GTK_STOCK_OK,
				      &cancel_btn, GTK_STOCK_CANCEL,
				      NULL, NULL);
	gtk_widget_show(confirm_area);
	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_btn);

	gtk_window_set_title(GTK_WINDOW(window), _("Actions configuration"));
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(prefs_actions_deleted), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(prefs_actions_key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);
	g_signal_connect(G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(prefs_actions_ok), mainwin);
	g_signal_connect(G_OBJECT(cancel_btn), "clicked",
			 G_CALLBACK(prefs_actions_cancel), NULL);

	vbox1 = gtk_vbox_new(FALSE, 8);
	gtk_widget_show(vbox1);
	gtk_box_pack_start(GTK_BOX(vbox), vbox1, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox1), 2);

	entry_vbox = gtk_vbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(vbox1), entry_vbox, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 8);
	gtk_box_pack_start(GTK_BOX(entry_vbox), hbox, FALSE, FALSE, 0);

	name_label = gtk_label_new(_("Menu name:"));
	gtk_box_pack_start(GTK_BOX(hbox), name_label, FALSE, FALSE, 0);

	name_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), name_entry, TRUE, TRUE, 0);

	hbox = gtk_hbox_new(FALSE, 8);
	gtk_box_pack_start(GTK_BOX(entry_vbox), hbox, TRUE, TRUE, 0);

	cmd_label = gtk_label_new(_("Command line:"));
	gtk_box_pack_start(GTK_BOX(hbox), cmd_label, FALSE, FALSE, 0);

	cmd_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), cmd_entry, TRUE, TRUE, 0);

	gtk_widget_show_all(entry_vbox);

	/* register / substitute / delete */

	reg_hbox = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(reg_hbox);
	gtk_box_pack_start(GTK_BOX(vbox1), reg_hbox, FALSE, FALSE, 0);

	arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_widget_show(arrow);
	gtk_box_pack_start(GTK_BOX(reg_hbox), arrow, FALSE, FALSE, 0);
	gtk_widget_set_size_request(arrow, -1, 16);

	btn_hbox = gtk_hbox_new(TRUE, 4);
	gtk_widget_show(btn_hbox);
	gtk_box_pack_start(GTK_BOX(reg_hbox), btn_hbox, FALSE, FALSE, 0);

	reg_btn = gtk_button_new_from_stock(GTK_STOCK_ADD);
	gtk_widget_show(reg_btn);
	gtk_box_pack_start(GTK_BOX(btn_hbox), reg_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(reg_btn), "clicked",
			 G_CALLBACK(prefs_actions_register_cb), NULL);

	subst_btn = gtk_button_new_with_label(_(" Replace "));
	gtk_widget_show(subst_btn);
	gtk_box_pack_start(GTK_BOX(btn_hbox), subst_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(subst_btn), "clicked",
			 G_CALLBACK(prefs_actions_substitute_cb),
			 NULL);

	del_btn = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	gtk_widget_show(del_btn);
	gtk_box_pack_start(GTK_BOX(btn_hbox), del_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(del_btn), "clicked",
			 G_CALLBACK(prefs_actions_delete_cb), NULL);

	help_button = gtk_button_new_with_label(_(" Syntax help "));
	gtk_widget_show(help_button);
	gtk_box_pack_end(GTK_BOX(reg_hbox), help_button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(help_button), "clicked",
			 G_CALLBACK(prefs_actions_help_cb), NULL);

	cond_hbox = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(cond_hbox);
	gtk_box_pack_start(GTK_BOX(vbox1), cond_hbox, TRUE, TRUE, 0);

	cond_scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(cond_scrolledwin);
	gtk_widget_set_size_request(cond_scrolledwin, -1, 150);
	gtk_box_pack_start(GTK_BOX(cond_hbox), cond_scrolledwin,
			   TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (cond_scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);

	cond_list_view = prefs_actions_list_view_create();				       
	gtk_widget_show(cond_list_view);
	gtk_container_add(GTK_CONTAINER (cond_scrolledwin), cond_list_view);

	btn_vbox = gtk_vbox_new(FALSE, 8);
	gtk_widget_show(btn_vbox);
	gtk_box_pack_start(GTK_BOX(cond_hbox), btn_vbox, FALSE, FALSE, 0);

	up_btn = gtk_button_new_from_stock(GTK_STOCK_GO_UP);
	gtk_widget_show(up_btn);
	gtk_box_pack_start(GTK_BOX(btn_vbox), up_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(up_btn), "clicked",
			 G_CALLBACK(prefs_actions_up), NULL);

	down_btn = gtk_button_new_from_stock(GTK_STOCK_GO_DOWN);
	gtk_widget_show(down_btn);
	gtk_box_pack_start(GTK_BOX(btn_vbox), down_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(down_btn), "clicked",
			 G_CALLBACK(prefs_actions_down), NULL);

	gtk_widget_show(window);

	actions.window = window;
	actions.ok_btn = ok_btn;

	actions.name_entry = name_entry;
	actions.cmd_entry  = cmd_entry;

	actions.actions_list_view = cond_list_view;
}


void prefs_actions_read_config(void)
{
	gchar *rcpath;
	FILE *fp;
	gchar buf[PREFSBUFSIZE];
	gchar *act;

	debug_print("Reading actions configurations...\n");

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, ACTIONS_RC, NULL);
	if ((fp = fopen(rcpath, "rb")) == NULL) {
		if (ENOENT != errno) FILE_OP_ERROR(rcpath, "fopen");
		g_free(rcpath);
		return;
	}
	g_free(rcpath);

	while (prefs_common.actions_list != NULL) {
		act = (gchar *)prefs_common.actions_list->data;
		prefs_common.actions_list =
			g_slist_remove(prefs_common.actions_list, act);
		g_free(act);
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		const gchar *src_codeset = conv_get_locale_charset_str();
		const gchar *dest_codeset = CS_UTF_8;
		gchar *tmp;

		tmp = conv_codeset_strdup(buf, src_codeset, dest_codeset);
		if (!tmp) {
			g_warning("Faild to convert character set of action configuration\n");
			tmp = g_strdup(buf);
		}

		g_strchomp(tmp);
		act = strstr(tmp, ": ");
		if (act && act[2] && 
		    action_get_type(&act[2]) != ACTION_ERROR)
			prefs_common.actions_list =
				g_slist_append(prefs_common.actions_list,
					       tmp);
		else
			g_free(tmp);
	}
	fclose(fp);
}

void prefs_actions_write_config(void)
{
	gchar *rcpath;
	PrefFile *pfile;
	GSList *cur;

	debug_print("Writing actions configuration...\n");

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, ACTIONS_RC, NULL);
	if ((pfile= prefs_write_open(rcpath)) == NULL) {
		g_warning("failed to write configuration to file\n");
		g_free(rcpath);
		return;
	}

	for (cur = prefs_common.actions_list; cur != NULL; cur = cur->next) {
		gchar *tmp = (gchar *)cur->data;
		const gchar *src_codeset = CS_UTF_8;
		const gchar *dest_codeset = conv_get_locale_charset_str();
		gchar *act;

		act = conv_codeset_strdup(tmp, src_codeset, dest_codeset);
		if (!act) {
			g_warning("Faild to convert character set of action configuration\n");
			act = g_strdup(act);
		}

		if (fputs(act, pfile->fp) == EOF ||
		    fputc('\n', pfile->fp) == EOF) {
			FILE_OP_ERROR(rcpath, "fputs || fputc");
			prefs_file_close_revert(pfile);
			g_free(rcpath);
			return;
		}
		g_free(act);
	}
	
	g_free(rcpath);

	if (prefs_file_close(pfile) < 0) {
		g_warning("failed to write configuration to file\n");
		return;
	}
}

static void prefs_actions_set_dialog(void)
{
	GtkListStore *store;
	GSList *cur;
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	store = GTK_LIST_STORE(gtk_tree_view_get_model
				(GTK_TREE_VIEW(actions.actions_list_view)));
	gtk_list_store_clear(store);

	prefs_actions_list_view_insert_action(actions.actions_list_view,
					      NULL, _("New"), FALSE);

	for (cur = prefs_common.actions_list; cur != NULL; cur = cur->next) {
		gchar *action = (gchar *) cur->data;
		
		prefs_actions_list_view_insert_action(actions.actions_list_view,
						      NULL, action, TRUE);
	}

	/* select first entry */
	selection = gtk_tree_view_get_selection
		(GTK_TREE_VIEW(actions.actions_list_view));
	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store),
					  &iter))
		gtk_tree_selection_select_iter(selection, &iter);
}

static void prefs_actions_set_list(void)
{
	GtkTreeIter iter;
	GtkListStore *store;
	
	g_slist_free(prefs_common.actions_list);
	prefs_common.actions_list = NULL;

	store = GTK_LIST_STORE(gtk_tree_view_get_model
				(GTK_TREE_VIEW(actions.actions_list_view)));

	if (gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter)) {
		do {
			gchar *action;
			gboolean is_valid;

			gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
					   PREFS_ACTIONS_DATA, &action,
					   PREFS_ACTIONS_VALID, &is_valid,
					   -1);
			
			if (is_valid) 
				prefs_common.actions_list = 
					g_slist_append(prefs_common.actions_list,
						       action);

		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(store),
						  &iter));
	}
}

#define GET_ENTRY(entry) \
	entry_text = gtk_entry_get_text(GTK_ENTRY(entry))

static gint prefs_actions_clist_set_row(GtkTreeIter *row)
{
	const gchar *entry_text;
	gint len;
	gchar action[PREFSBUFSIZE];
	gchar *new_action;
	GtkListStore *store;

	store = GTK_LIST_STORE(gtk_tree_view_get_model
				(GTK_TREE_VIEW(actions.actions_list_view)));
	

	GET_ENTRY(actions.name_entry);
	if (entry_text[0] == '\0') {
		alertpanel_error(_("Menu name is not set."));
		return -1;
	}

	if (strchr(entry_text, ':')) {
		alertpanel_error(_("Colon ':' is not allowed in the menu name."));
		return -1;
	}

	strncpy(action, entry_text, PREFSBUFSIZE - 1);
	g_strstrip(action);

	/* Keep space for the ': ' delimiter */
	len = strlen(action) + 2;
	if (len >= PREFSBUFSIZE - 1) {
		alertpanel_error(_("Menu name is too long."));
		return -1;
	}

	strcat(action, ": ");

	GET_ENTRY(actions.cmd_entry);

	if (entry_text[0] == '\0') {
		alertpanel_error(_("Command line not set."));
		return -1;
	}

	if (len + strlen(entry_text) >= PREFSBUFSIZE - 1) {
		alertpanel_error(_("Menu name and command are too long."));
		return -1;
	}

	if (action_get_type(entry_text) == ACTION_ERROR) {
		alertpanel_error(_("The command\n%s\nhas a syntax error."), 
				 entry_text);
		return -1;
	}

	strcat(action, entry_text);

	new_action = g_strdup(action);	
	prefs_actions_list_view_insert_action(actions.actions_list_view,
	                                      row, new_action, TRUE);
						
	prefs_actions_set_list();

	return 0;
}

/* callback functions */

static void prefs_actions_register_cb(GtkWidget *w, gpointer data)
{
	prefs_actions_clist_set_row(NULL);
	modified = FALSE;
}

static void prefs_actions_substitute_cb(GtkWidget *w, gpointer data)
{
	GtkTreeIter isel, inew;
	GtkTreePath *path_sel, *path_new;
	GtkTreeSelection *selection = gtk_tree_view_get_selection
					(GTK_TREE_VIEW(actions.actions_list_view));
	GtkTreeModel *model;					

	if (!gtk_tree_selection_get_selected(selection, &model, &isel))
		return;
	if (!gtk_tree_model_get_iter_first(model, &inew))
		return;

	path_sel = gtk_tree_model_get_path(model, &isel);
	path_new = gtk_tree_model_get_path(model, &inew);

	if (path_sel && path_new 
	&&  gtk_tree_path_compare(path_sel, path_new) != 0)
		prefs_actions_clist_set_row(&isel);

	gtk_tree_path_free(path_sel);
	gtk_tree_path_free(path_new);
	modified = FALSE;
}

static void prefs_actions_delete_cb(GtkWidget *w, gpointer data)
{
	GtkTreeIter sel;
	GtkTreeModel *model;
	gchar *action;

	if (!gtk_tree_selection_get_selected(gtk_tree_view_get_selection
				(GTK_TREE_VIEW(actions.actions_list_view)),
				&model, &sel))
		return;				

	if (alertpanel(_("Delete action"),
		       _("Do you really want to delete this action?"),
		       GTK_STOCK_YES, GTK_STOCK_NO, NULL) != G_ALERTDEFAULT)
		return;

	/* XXX: Here's the reason why we need to store the original 
	 * pointer: we search the slist for it. */
	gtk_tree_model_get(model, &sel,
			   PREFS_ACTIONS_DATA, &action,
			   -1);
	gtk_list_store_remove(GTK_LIST_STORE(model), &sel);

	prefs_common.actions_list = g_slist_remove(prefs_common.actions_list,
						   action);
}

static void prefs_actions_up(GtkWidget *w, gpointer data)
{
	GtkTreePath *prev, *sel, *try;
	GtkTreeIter isel;
	GtkListStore *store;
	GtkTreeIter iprev;
	
	if (!gtk_tree_selection_get_selected
		(gtk_tree_view_get_selection
			(GTK_TREE_VIEW(actions.actions_list_view)),
		 (GtkTreeModel **) &store,	
		 &isel))
		return;

	sel = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &isel);
	if (!sel)
		return;
	
	/* no move if we're at row 0 or 1, looks phony, but other
	 * solutions are more convoluted... */
	try = gtk_tree_path_copy(sel);
	if (!gtk_tree_path_prev(try) || !gtk_tree_path_prev(try)) {
		gtk_tree_path_free(try);
		gtk_tree_path_free(sel);
		return;
	}
	gtk_tree_path_free(try);

	prev = gtk_tree_path_copy(sel);		
	if (!gtk_tree_path_prev(prev)) {
		gtk_tree_path_free(prev);
		gtk_tree_path_free(sel);
		return;
	}

	gtk_tree_model_get_iter(GTK_TREE_MODEL(store),
				&iprev, prev);
	gtk_tree_path_free(sel);
	gtk_tree_path_free(prev);

	gtk_list_store_swap(store, &iprev, &isel);
	prefs_actions_set_list();
}

static void prefs_actions_down(GtkWidget *w, gpointer data)
{
	GtkListStore *store;
	GtkTreeIter next, sel;
	GtkTreePath *try;
	
	if (!gtk_tree_selection_get_selected
		(gtk_tree_view_get_selection
			(GTK_TREE_VIEW(actions.actions_list_view)),
		 (GtkTreeModel **) &store,
		 &sel))
		return;

	try = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &sel);
	if (!try) 
		return;

	/* no move when we're at row 0 */
	if (!gtk_tree_path_prev(try)) {
		gtk_tree_path_free(try);
		return;
	}
	gtk_tree_path_free(try);

	next = sel;
	if (!gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &next)) 
		return;

	gtk_list_store_swap(store, &next, &sel);
	prefs_actions_set_list();
}

static gint prefs_actions_deleted(GtkWidget *widget, GdkEventAny *event,
				  gpointer *data)
{
	prefs_actions_cancel(widget, data);
	return TRUE;
}

static gboolean prefs_actions_key_pressed(GtkWidget *widget, GdkEventKey *event,
					  gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		prefs_actions_cancel(widget, data);
	else {
		GtkWidget *focused = gtkut_get_focused_child(
					GTK_CONTAINER(widget));
		if (focused && GTK_IS_EDITABLE(focused)) {
			modified = TRUE;
		}
	}
	return FALSE;
}

static void prefs_actions_cancel(GtkWidget *w, gpointer data)
{
	if (modified && alertpanel(_("Entry not saved"),
				 _("The entry was not saved. Close anyway?"),
				 GTK_STOCK_YES, GTK_STOCK_NO, NULL) != G_ALERTDEFAULT) {
		return;
	}
	modified = FALSE;
	prefs_actions_read_config();
	gtk_widget_hide(actions.window);
	inc_unlock();
}

static void prefs_actions_ok(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = (MainWindow *) data;
	GList *list;
	GList *iter;
	MessageView *msgview;
	Compose *compose;

	if (modified && alertpanel(_("Entry not saved"),
				 _("The entry was not saved. Close anyway?"),
				 GTK_STOCK_YES, GTK_STOCK_NO, NULL) != G_ALERTDEFAULT) {
		return;
	}
	modified = FALSE;
	prefs_actions_write_config();

	/* Update mainwindow actions menu */
	main_window_update_actions_menu(mainwin);

	/* Update separated message view actions menu */
	list = messageview_get_msgview_list();
	for (iter = list; iter; iter = iter->next) {
		msgview = (MessageView *) iter->data;
		messageview_update_actions_menu(msgview);
	}

	/* Update compose windows actions menu */
	list = compose_get_compose_list();
	for (iter = list; iter; iter = iter->next) {
		compose = (Compose *) iter->data;
		compose_update_actions_menu(compose);
	}

	gtk_widget_hide(actions.window);
	inc_unlock();
}

/*
 * Strings describing action format strings
 * 
 * When adding new lines, remember to put one string for each line
 */
static gchar *actions_desc_strings[] = {
	N_("MENU NAME:"), NULL,
	"      ",   N_("Use / in menu name to make submenus."),
	"", NULL,
	N_("COMMAND LINE:"), NULL,
	N_("Begin with:"), NULL,
	"     |",   N_("to send message body or selection to command's standard input"),
	"     >",   N_("to send user provided text to command's standard input"),
	"     *",   N_("to send user provided hidden text to command's standard input"),
	N_("End with:"), NULL, 
	"     |",   N_("to replace message body or selection with command's standard output"),
	"     >",   N_("to insert command's standard output without replacing old text"),
	"     &",   N_("to run command asynchronously"),
	N_("Use:"), NULL, 
	"     %f",  N_("for the file of the selected message in RFC822/2822 format "),
	"     %F",  N_("for the list of the files of the selected messages in RFC822/2822 format"),
	"     %p",  N_("for the file of the selected decoded message MIME part"),
	"     %u",  N_("for a user provided argument"),
	"     %h",  N_("for a user provided hidden argument (e.g. password)"),
	"     %s",  N_("for the text selection"),
	"  %as{}",  N_("apply filtering actions between {} to selected messages"),
	NULL
};


static DescriptionWindow actions_desc_win = { 
        NULL, 
        2,
        N_("Description of symbols"),
        actions_desc_strings
};


static void prefs_actions_help_cb(GtkWidget *w, gpointer data)
{
	description_window_create(&actions_desc_win);
}

static GtkListStore* prefs_actions_create_data_store(void)
{
	return gtk_list_store_new(N_PREFS_ACTIONS_COLUMNS,
				  G_TYPE_STRING,	
				  G_TYPE_POINTER,
				  G_TYPE_BOOLEAN,
				  -1);
}

static void prefs_actions_list_view_insert_action(GtkWidget *list_view,
						  GtkTreeIter *row_iter,
						  gchar *action,
						  gboolean is_valid) 
{
	GtkTreeIter iter;
	GtkListStore *list_store = GTK_LIST_STORE(gtk_tree_view_get_model
					(GTK_TREE_VIEW(list_view)));

	if (row_iter == NULL) {
		/* append new */
		gtk_list_store_append(list_store, &iter);
		gtk_list_store_set(list_store, &iter,
				   PREFS_ACTIONS_STRING, action,
				   PREFS_ACTIONS_DATA, action,
				   PREFS_ACTIONS_VALID,  is_valid,
				   -1);
	} else {
		/* change existing */
		gchar *old_action;

		gtk_tree_model_get(GTK_TREE_MODEL(list_store), row_iter,
				   PREFS_ACTIONS_DATA, &old_action,
				   -1);
		
		g_free(old_action);				
		gtk_list_store_set(list_store, row_iter,
				   PREFS_ACTIONS_STRING, action,
				   PREFS_ACTIONS_DATA, action,
				   -1);
	}
}

static GtkWidget *prefs_actions_list_view_create(void)
{
	GtkTreeView *list_view;
	GtkTreeSelection *selector;
	GtkTreeModel *model;

	model = GTK_TREE_MODEL(prefs_actions_create_data_store());
	list_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
	g_object_unref(model);	
	
	gtk_tree_view_set_rules_hint(list_view, prefs_common.enable_rules_hint);
	
	selector = gtk_tree_view_get_selection(list_view);
	gtk_tree_selection_set_mode(selector, GTK_SELECTION_BROWSE);
	gtk_tree_selection_set_select_function(selector, prefs_actions_selected,
					       NULL, NULL);

	/* create the columns */
	prefs_actions_create_list_view_columns(GTK_WIDGET(list_view));

	return GTK_WIDGET(list_view);
}

static void prefs_actions_create_list_view_columns(GtkWidget *list_view)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
		(_("Current actions"),
		 renderer,
		 "text", PREFS_ACTIONS_STRING,
		 NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);		
}

#define ENTRY_SET_TEXT(entry, str) \
	gtk_entry_set_text(GTK_ENTRY(entry), str ? str : "")

static gboolean prefs_actions_selected(GtkTreeSelection *selector,
				       GtkTreeModel *model, 
				       GtkTreePath *path,
				       gboolean currently_selected,
				       gpointer data)
{
	gchar *action;
	gchar *cmd;
	gchar buf[PREFSBUFSIZE];
	GtkTreeIter iter;
	gboolean is_valid;

	if (currently_selected)
		return TRUE;

	if (!gtk_tree_model_get_iter(model, &iter, path))
		return TRUE;

	gtk_tree_model_get(model, &iter, 
			   PREFS_ACTIONS_VALID,  &is_valid,
			   PREFS_ACTIONS_DATA, &action,
			   -1);
	if (!is_valid) {
		ENTRY_SET_TEXT(actions.name_entry, "");
		ENTRY_SET_TEXT(actions.cmd_entry, "");
		return TRUE;
	}
	
	strncpy(buf, action, PREFSBUFSIZE - 1);
	buf[PREFSBUFSIZE - 1] = 0x00;
	cmd = strstr(buf, ": ");

	if (cmd && cmd[2])
		ENTRY_SET_TEXT(actions.cmd_entry, &cmd[2]);
	else
		return TRUE;

	*cmd = 0x00;
	ENTRY_SET_TEXT(actions.name_entry, buf);

	return TRUE;
}

