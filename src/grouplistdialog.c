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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
// #include <gtk/gtkclist.h>
#include <gtk/gtkctree.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkhbbox.h>
#include <string.h>
#include <fnmatch.h>

#include "intl.h"
#include "grouplistdialog.h"
#include "manage_window.h"
#include "gtkutils.h"
#include "utils.h"
#include "news.h"
#include "folder.h"
#include "alertpanel.h"
#include "recv.h"
#include "socket.h"

#define GROUPLIST_DIALOG_WIDTH	500
#define GROUPLIST_NAMES	        250
#define GROUPLIST_DIALOG_HEIGHT	400

static GList * subscribed = NULL;
gboolean dont_unsubscribed = FALSE;

static gboolean ack;
static gboolean locked;

static GtkWidget *dialog;
static GtkWidget *entry;
// static GtkWidget *clist;
static GtkWidget *groups_tree;
static GtkWidget *status_label;
static GtkWidget *ok_button;
static Folder *news_folder;

static void grouplist_dialog_create	(void);
static void grouplist_dialog_set_list	(gchar * pattern);
static void grouplist_clear		(void);
static void grouplist_recv_func		(SockInfo	*sock,
					 gint		 count,
					 gint		 read_bytes,
					 gpointer	 data);

static void ok_clicked		(GtkWidget	*widget,
				 gpointer	 data);
static void cancel_clicked	(GtkWidget	*widget,
				 gpointer	 data);
static void refresh_clicked	(GtkWidget	*widget,
				 gpointer	 data);
static void key_pressed		(GtkWidget	*widget,
				 GdkEventKey	*event,
				 gpointer	 data);
static void groups_tree_selected	(GtkCTree	*groups_tree,
					 GtkCTreeNode   * node,
					 gint		 column,
					 GdkEventButton	*event,
					 gpointer	 user_data);
static void groups_tree_unselected	(GtkCTree	*groups_tree,
					 GtkCTreeNode   * node,
					 gint		 column,
					 GdkEventButton	*event,
					 gpointer	 user_data);
static void entry_activated	(GtkEditable	*editable);

GList *grouplist_dialog(Folder *folder, GList * cur_subscriptions)
{
	gchar *str;
	GList * l;

	if (dialog && GTK_WIDGET_VISIBLE(dialog)) return NULL;

	if (!dialog)
		grouplist_dialog_create();

	news_folder = folder;

	gtk_widget_show(dialog);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	manage_window_set_transient(GTK_WINDOW(dialog));
	GTK_EVENTS_FLUSH();

	subscribed = NULL;

	for(l = cur_subscriptions ; l != NULL ; l = l->next)
	  subscribed = g_list_append(subscribed, g_strdup((gchar *) l->data));

	grouplist_dialog_set_list(NULL);

	gtk_main();

	manage_window_focus_out(dialog, NULL, NULL);
	gtk_widget_hide(dialog);

	if (!ack) {
	  list_free_strings(subscribed);
	  g_list_free(subscribed);

	  subscribed = NULL;
	  
	  for(l = cur_subscriptions ; l != NULL ; l = l->next)
	    subscribed = g_list_append(subscribed, g_strdup((gchar *) l->data));
	}

	GTK_EVENTS_FLUSH();

	debug_print("return string = %s\n", str ? str : "(none)");
	return subscribed;
}

static void grouplist_clear(void)
{
	dont_unsubscribed = TRUE;
	gtk_clist_clear(GTK_CLIST(groups_tree));
	gtk_entry_set_text(GTK_ENTRY(entry), "");
	dont_unsubscribed = FALSE;
}

static void grouplist_dialog_create(void)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *msg_label;
	GtkWidget *confirm_area;
	GtkWidget *cancel_button;	
	GtkWidget *refresh_button;	
	GtkWidget *scrolledwin;
	gchar * col_names[3] = {
		_("name"), _("count of messages"), _("type")
	};

	dialog = gtk_dialog_new();
	gtk_window_set_policy(GTK_WINDOW(dialog), FALSE, TRUE, FALSE);
	gtk_widget_set_usize(dialog,
			     GROUPLIST_DIALOG_WIDTH, GROUPLIST_DIALOG_HEIGHT);
	gtk_container_set_border_width
		(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), 5);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Subscribe to newsgroup"));
	gtk_signal_connect(GTK_OBJECT(dialog), "delete_event",
			   GTK_SIGNAL_FUNC(cancel_clicked), NULL);
	gtk_signal_connect(GTK_OBJECT(dialog), "key_press_event",
			   GTK_SIGNAL_FUNC(key_pressed), NULL);
	gtk_signal_connect(GTK_OBJECT(dialog), "focus_in_event",
			   GTK_SIGNAL_FUNC(manage_window_focus_in), NULL);
	gtk_signal_connect(GTK_OBJECT(dialog), "focus_out_event",
			   GTK_SIGNAL_FUNC(manage_window_focus_out), NULL);

	gtk_widget_realize(dialog);

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), vbox);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	msg_label = gtk_label_new(_("Input subscribing newsgroup:"));
	gtk_box_pack_start(GTK_BOX(hbox), msg_label, FALSE, FALSE, 0);

	entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(entry), "activate",
			   GTK_SIGNAL_FUNC(entry_activated), NULL);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_box_pack_start(GTK_BOX (vbox), scrolledwin, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);

	groups_tree = gtk_ctree_new_with_titles(3, 0, col_names);
	gtk_container_add(GTK_CONTAINER(scrolledwin), groups_tree);
	gtk_clist_set_selection_mode(GTK_CLIST(groups_tree),
				     GTK_SELECTION_MULTIPLE);
	/*
	GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(clist)->column[0].button,
			       GTK_CAN_FOCUS);
	*/
	gtk_signal_connect(GTK_OBJECT(groups_tree), "tree_select_row",
			   GTK_SIGNAL_FUNC(groups_tree_selected), NULL);
	gtk_signal_connect(GTK_OBJECT(groups_tree), "tree_unselect_row",
			   GTK_SIGNAL_FUNC(groups_tree_unselected), NULL);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	status_label = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox), status_label, FALSE, FALSE, 0);

	gtkut_button_set_create(&confirm_area,
				&ok_button,      _("OK"),
				&cancel_button,  _("Cancel"),
				&refresh_button, _("Refresh"));
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area),
			  confirm_area);
	gtk_widget_grab_default(ok_button);

	gtk_signal_connect(GTK_OBJECT(ok_button), "clicked",
			   GTK_SIGNAL_FUNC(ok_clicked), NULL);
	gtk_signal_connect(GTK_OBJECT(cancel_button), "clicked",
			   GTK_SIGNAL_FUNC(cancel_clicked), NULL);
	gtk_signal_connect(GTK_OBJECT(refresh_button), "clicked",
			   GTK_SIGNAL_FUNC(refresh_clicked), NULL);

	gtk_widget_show_all(GTK_DIALOG(dialog)->vbox);
}

static GHashTable * hash_last_node;
static GHashTable * hash_branch_node;

static void hash_news_init()
{
	hash_last_node = g_hash_table_new(g_str_hash, g_str_equal);
	hash_branch_node = g_hash_table_new(g_str_hash, g_str_equal);
}

static void free_key(gchar * key, void * value, void * user_data)
{
	g_free(key);
}

static void hash_news_done()
{
	g_hash_table_foreach(hash_branch_node, free_key, NULL);
	g_hash_table_foreach(hash_last_node, free_key, NULL);

	g_hash_table_destroy(hash_branch_node);
	g_hash_table_destroy(hash_last_node);
}

static GtkCTreeNode * hash_news_get_branch_node(gchar * name)
{
	return g_hash_table_lookup(hash_branch_node, name);
}

static void hash_news_set_branch_node(gchar * name, GtkCTreeNode * node)
{
	g_hash_table_insert(hash_branch_node, g_strdup(name), node);
}

static GtkCTreeNode * hash_news_get_last_node(gchar * name)
{
	return g_hash_table_lookup(hash_last_node, name);
}

static void hash_news_set_last_node(gchar * name, GtkCTreeNode * node)
{
	gchar * key;
	void * value;

	if (g_hash_table_lookup_extended(hash_last_node, name,
					 (void *) &key, &value)) {
		g_hash_table_remove(hash_last_node, name);
		g_free(key);
	}

	g_hash_table_insert(hash_last_node, g_strdup(name), node);
}

static gchar * get_parent_name(gchar * name)
{
	gchar * p;

	p = (gchar *) strrchr(name, '.');
	if (p == NULL)
		return g_strdup("");

	return g_strndup(name, p - name);
}

static gchar * get_node_name(gchar * name)
{
	gchar * p;

	p = (gchar *) strrchr(name, '.');
	if (p == NULL)
		return name;

	return p + 1;
}

static GtkCTreeNode * create_parent(GtkCTree * groups_tree, gchar * name)
{
	gchar * cols[3];
	GtkCTreeNode * parent;
	GtkCTreeNode * node;
	gchar * parent_name;

	if (* name == 0)
		return;

	if (hash_news_get_branch_node(name) != NULL)
		return;

	cols[0] = get_node_name(name);
	cols[1] = "";
	cols[2] = "";
	
	parent_name = get_parent_name(name);
	create_parent(groups_tree, parent_name);

	parent = hash_news_get_branch_node(parent_name);
	node = hash_news_get_last_node(parent_name);
	node = gtk_ctree_insert_node(groups_tree,
				     parent, node,
				     cols, 0, NULL, NULL,
				     NULL, NULL,
				     FALSE, FALSE);
	gtk_ctree_node_set_selectable(groups_tree, node, FALSE);
	hash_news_set_last_node(parent_name, node);
	hash_news_set_branch_node(name, node);

	g_free(parent_name);
}

static GtkCTreeNode * create_branch(GtkCTree * groups_tree, gchar * name,
				    struct NNTPGroupInfo * info)
{
	gchar * parent_name;

	GtkCTreeNode * node;
	GtkCTreeNode * parent;

	gchar count_str[10];
	gchar * cols[3];
	gint count;

	count = info->last - info->first;
	if (count < 0)
		count = 0;
	snprintf(count_str, 10, "%i", count);
	
	cols[0] = get_node_name(info->name);
	cols[1] = count_str;
	if (info->type == 'y')
		cols[2] = "";
	else if (info->type == 'm')
		cols[2] = "moderated";
	else if (info->type == 'n')
		cols[2] = "readonly";
	else
		cols[2] = "unkown";
	
	parent_name = get_parent_name(name);

	create_parent(groups_tree, parent_name);

	parent = hash_news_get_branch_node(parent_name);
	node = hash_news_get_last_node(parent_name);
	node = gtk_ctree_insert_node(groups_tree,
				     parent, node,
				     cols, 0, NULL, NULL,
				     NULL, NULL,
				     TRUE, FALSE);
	gtk_ctree_node_set_selectable(groups_tree, node, TRUE);
	hash_news_set_last_node(parent_name, node);

	g_free(parent_name);

	if (node == NULL)
		return NULL;

	gtk_ctree_node_set_row_data(GTK_CTREE(groups_tree), node, info);

	return node;
}

static void grouplist_dialog_set_list(gchar * pattern)
{
	GSList *cur;
	GtkCTreeNode * node;
	GSList * group_list = NULL;
	GSList * r_list;

	if (pattern == NULL)
		pattern = "*";

	if (locked) return;
	locked = TRUE;

	grouplist_clear();

	recv_set_ui_func(grouplist_recv_func, NULL);
	group_list = news_get_group_list(news_folder);
	recv_set_ui_func(NULL, NULL);
	if (group_list == NULL) {
		alertpanel_error(_("Can't retrieve newsgroup list."));
		locked = FALSE;
		return;
	}

	dont_unsubscribed = TRUE;

	hash_news_init();

	gtk_clist_freeze(GTK_CLIST(groups_tree));

	r_list = g_slist_copy(group_list);
	r_list = g_slist_reverse(r_list);

	for (cur = r_list; cur != NULL ; cur = cur->next) {
		struct NNTPGroupInfo * info;

		info = (struct NNTPGroupInfo *) cur->data;

		if (fnmatch(pattern, info->name, 0) == 0) {
			GList * l;

			node = create_branch(GTK_CTREE(groups_tree),
					     info->name, info);

			l = g_list_find_custom(subscribed, info->name,
					       (GCompareFunc) g_strcasecmp);
			
			if (l != NULL)
			  gtk_ctree_select(GTK_CTREE(groups_tree), node);
		}
	}

	g_slist_free(r_list);

	hash_news_done();

	node = gtk_ctree_node_nth(GTK_CTREE(groups_tree), 0);
	gtk_ctree_node_moveto(GTK_CTREE(groups_tree), node, 0, 0, 0);

	dont_unsubscribed = FALSE;

	gtk_clist_set_column_width(GTK_CLIST(groups_tree), 0, GROUPLIST_NAMES);
	gtk_clist_set_column_justification (GTK_CLIST(groups_tree), 1,
					    GTK_JUSTIFY_RIGHT);
	gtk_clist_set_column_auto_resize(GTK_CLIST(groups_tree), 1, TRUE);
	gtk_clist_set_column_auto_resize(GTK_CLIST(groups_tree), 2, TRUE);

	gtk_clist_thaw(GTK_CLIST(groups_tree));

	gtk_widget_grab_focus(ok_button);
	gtk_widget_grab_focus(groups_tree);

	gtk_label_set_text(GTK_LABEL(status_label), _("Done."));

	locked = FALSE;
}

static void grouplist_recv_func(SockInfo *sock, gint count, gint read_bytes,
				gpointer data)
{
	gchar buf[BUFFSIZE];

	g_snprintf(buf, sizeof(buf),
		   _("%d newsgroups received (%s read)"),
		   count, to_human_readable(read_bytes));
	gtk_label_set_text(GTK_LABEL(status_label), buf);
	GTK_EVENTS_FLUSH();
}

static void ok_clicked(GtkWidget *widget, gpointer data)
{
	gchar * str;
	gboolean update_list;

	str = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);

	update_list = FALSE;

	if (strchr(str, '*') != NULL)
	  update_list = TRUE;

	if (update_list) {
		grouplist_dialog_set_list(str);
		g_free(str);
	}
	else {
		g_free(str);
		ack = TRUE;
		if (gtk_main_level() > 1)
			gtk_main_quit();
	}
}

static void cancel_clicked(GtkWidget *widget, gpointer data)
{
	ack = FALSE;
	if (gtk_main_level() > 1)
		gtk_main_quit();
}

static void refresh_clicked(GtkWidget *widget, gpointer data)
{
	gchar * str;
 
	if (locked) return;

	news_cancel_group_list_cache(news_folder);

	str = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
	grouplist_dialog_set_list(str);
	g_free(str);
}

static void key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		cancel_clicked(NULL, NULL);
}

static void groups_tree_selected(GtkCTree *groups_tree, GtkCTreeNode * node,
				 gint column,
				 GdkEventButton *event, gpointer user_data)
{
	struct NNTPGroupInfo * group;
	GList * l;

	group = (struct NNTPGroupInfo *)
	  gtk_ctree_node_get_row_data(GTK_CTREE(groups_tree), node);

	if (group == NULL)
		return;

	if (!dont_unsubscribed) {
		subscribed = g_list_append(subscribed, g_strdup(group->name));
	}
}

static void groups_tree_unselected(GtkCTree *groups_tree, GtkCTreeNode * node,
				   gint column,
				   GdkEventButton *event, gpointer user_data)
{
	struct NNTPGroupInfo * group;
	GList * l;

	group = (struct NNTPGroupInfo *)
	  gtk_ctree_node_get_row_data(GTK_CTREE(groups_tree), node);

	if (group == NULL)
		return;

	if (!dont_unsubscribed) {
		l = g_list_find_custom(subscribed, group->name,
				       (GCompareFunc) g_strcasecmp);
		if (l != NULL) {
		  g_free(l->data);
			subscribed = g_list_remove(subscribed, l->data);
		}
	}
}

static void entry_activated(GtkEditable *editable)
{
	gchar * str;
	gboolean update_list;

	str = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);

	update_list = FALSE;

	if (strchr(str, '*') != NULL)
	  update_list = TRUE;

	if (update_list) {
		grouplist_dialog_set_list(str);
		g_free(str);
	}
	else {
		g_free(str);
		ok_clicked(NULL, NULL);
	}
}
