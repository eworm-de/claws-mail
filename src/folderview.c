/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Hiroyuki Yamamoto and the Claws Mail team
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

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkctree.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkclist.h>
#include <gtk/gtkstyle.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkstatusbar.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkitemfactory.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "main.h"
#include "mainwindow.h"
#include "folderview.h"
#include "summaryview.h"
#include "summary_search.h"
#include "inputdialog.h"
#include "manage_window.h"
#include "alertpanel.h"
#include "menu.h"
#include "stock_pixmap.h"
#include "procmsg.h"
#include "utils.h"
#include "gtkutils.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "prefs_filtering.h"
#include "prefs_folder_item.h"
#include "account.h"
#include "folder.h"
#include "foldersel.h"
#include "inc.h"
#include "statusbar.h"
#include "hooks.h"
#include "folderutils.h"
#include "partial_download.h"
#include "prefs_folder_column.h"
#include "filtering.h"
#include "quicksearch.h"
#include "manual.h"
#include "timing.h"
#include "log.h"

#define COL_FOLDER_WIDTH	150
#define COL_NUM_WIDTH		32

static GList *folderview_list = NULL;

static GtkStyle *normal_style;
static GtkStyle *normal_color_style;
static GtkStyle *bold_style;
static GtkStyle *bold_color_style;
static GtkStyle *bold_tgtfold_style;

static GdkPixmap *inboxxpm;
static GdkBitmap *inboxxpmmask;
static GdkPixmap *inboxhrmxpm;
static GdkBitmap *inboxhrmxpmmask;
static GdkPixmap *inboxopenxpm;
static GdkBitmap *inboxopenxpmmask;
static GdkPixmap *inboxopenhrmxpm;
static GdkBitmap *inboxopenhrmxpmmask;
static GdkPixmap *outboxxpm;
static GdkBitmap *outboxxpmmask;
static GdkPixmap *outboxhrmxpm;
static GdkBitmap *outboxhrmxpmmask;
static GdkPixmap *outboxopenxpm;
static GdkBitmap *outboxopenxpmmask;
static GdkPixmap *outboxopenhrmxpm;
static GdkBitmap *outboxopenhrmxpmmask;
static GdkPixmap *folderxpm;
static GdkBitmap *folderxpmmask;
static GdkPixmap *folderhrmxpm;
static GdkBitmap *folderhrmxpmmask;
static GdkPixmap *folderopenxpm;
static GdkBitmap *folderopenxpmmask;
static GdkPixmap *folderopenhrmxpm;
static GdkBitmap *folderopenhrmxpmmask;
static GdkPixmap *trashopenxpm;
static GdkBitmap *trashopenxpmmask;
static GdkPixmap *trashopenhrmxpm;
static GdkBitmap *trashopenhrmxpmmask;
static GdkPixmap *trashxpm;
static GdkBitmap *trashxpmmask;
static GdkPixmap *trashhrmxpm;
static GdkBitmap *trashhrmxpmmask;
static GdkPixmap *queuexpm;
static GdkBitmap *queuexpmmask;
static GdkPixmap *queuehrmxpm;
static GdkBitmap *queuehrmxpmmask;
static GdkPixmap *queueopenxpm;
static GdkBitmap *queueopenxpmmask;
static GdkPixmap *queueopenhrmxpm;
static GdkBitmap *queueopenhrmxpmmask;
static GdkPixmap *draftsxpm;
static GdkBitmap *draftsxpmmask;
static GdkPixmap *draftsopenxpm;
static GdkBitmap *draftsopenxpmmask;
static GdkPixmap *noselectxpm;
static GdkBitmap *noselectxpmmask;

static GdkPixmap *m_inboxxpm;
static GdkBitmap *m_inboxxpmmask;
static GdkPixmap *m_inboxhrmxpm;
static GdkBitmap *m_inboxhrmxpmmask;
static GdkPixmap *m_inboxopenxpm;
static GdkBitmap *m_inboxopenxpmmask;
static GdkPixmap *m_inboxopenhrmxpm;
static GdkBitmap *m_inboxopenhrmxpmmask;
static GdkPixmap *m_outboxxpm;
static GdkBitmap *m_outboxxpmmask;
static GdkPixmap *m_outboxhrmxpm;
static GdkBitmap *m_outboxhrmxpmmask;
static GdkPixmap *m_outboxopenxpm;
static GdkBitmap *m_outboxopenxpmmask;
static GdkPixmap *m_outboxopenhrmxpm;
static GdkBitmap *m_outboxopenhrmxpmmask;
static GdkPixmap *m_folderxpm;
static GdkBitmap *m_folderxpmmask;
static GdkPixmap *m_folderhrmxpm;
static GdkBitmap *m_folderhrmxpmmask;
static GdkPixmap *m_folderopenxpm;
static GdkBitmap *m_folderopenxpmmask;
static GdkPixmap *m_folderopenhrmxpm;
static GdkBitmap *m_folderopenhrmxpmmask;
static GdkPixmap *m_trashopenxpm;
static GdkBitmap *m_trashopenxpmmask;
static GdkPixmap *m_trashopenhrmxpm;
static GdkBitmap *m_trashopenhrmxpmmask;
static GdkPixmap *m_trashxpm;
static GdkBitmap *m_trashxpmmask;
static GdkPixmap *m_trashhrmxpm;
static GdkBitmap *m_trashhrmxpmmask;
static GdkPixmap *m_queuexpm;
static GdkBitmap *m_queuexpmmask;
static GdkPixmap *m_queuehrmxpm;
static GdkBitmap *m_queuehrmxpmmask;
static GdkPixmap *m_queueopenxpm;
static GdkBitmap *m_queueopenxpmmask;
static GdkPixmap *m_queueopenhrmxpm;
static GdkBitmap *m_queueopenhrmxpmmask;
static GdkPixmap *m_draftsxpm;
static GdkBitmap *m_draftsxpmmask;
static GdkPixmap *m_draftsopenxpm;
static GdkBitmap *m_draftsopenxpmmask;

static GdkPixmap *newxpm;
static GdkBitmap *newxpmmask;
static GdkPixmap *unreadxpm;
static GdkBitmap *unreadxpmmask;
static GdkPixmap *readxpm;
static GdkBitmap *readxpmmask;

static void folderview_select_node	 (FolderView	*folderview,
					  GtkCTreeNode	*node);
static void folderview_set_folders	 (FolderView	*folderview);
static void folderview_sort_folders	 (FolderView	*folderview,
					  GtkCTreeNode	*root,
					  Folder	*folder);
static void folderview_append_folder	 (FolderView	*folderview,
					  Folder	*folder);
static void folderview_update_node	 (FolderView	*folderview,
					  GtkCTreeNode	*node);

static gint folderview_clist_compare	(GtkCList	*clist,
					 gconstpointer	 ptr1,
					 gconstpointer	 ptr2);

/* callback functions */
static gboolean folderview_button_pressed	(GtkWidget	*ctree,
						 GdkEventButton	*event,
						 FolderView	*folderview);
static gboolean folderview_button_released	(GtkWidget	*ctree,
						 GdkEventButton	*event,
						 FolderView	*folderview);
static gboolean folderview_key_pressed	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 FolderView	*folderview);
static void folderview_selected		(GtkCTree	*ctree,
					 GtkCTreeNode	*row,
					 gint		 column,
					 FolderView	*folderview);
static void folderview_tree_expanded	(GtkCTree	*ctree,
					 GtkCTreeNode	*node,
					 FolderView	*folderview);
static void folderview_tree_collapsed	(GtkCTree	*ctree,
					 GtkCTreeNode	*node,
					 FolderView	*folderview);
static void folderview_popup_close	(GtkMenuShell	*menu_shell,
					 FolderView	*folderview);
static void folderview_col_resized	(GtkCList	*clist,
					 gint		 column,
					 gint		 width,
					 FolderView	*folderview);

static void mark_all_read_cb            (FolderView    *folderview,
                                         guint           action,
                                         GtkWidget      *widget);

static void folderview_empty_trash_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static void folderview_send_queue_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static void folderview_search_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static void folderview_property_cb	(FolderView	*folderview,
					 guint		 action,
					 GtkWidget	*widget);

static gboolean folderview_drag_motion_cb(GtkWidget      *widget,
					  GdkDragContext *context,
					  gint            x,
					  gint            y,
					  guint           time,
					  FolderView     *folderview);
static void folderview_drag_leave_cb     (GtkWidget        *widget,
					  GdkDragContext   *context,
					  guint             time,
					  FolderView       *folderview);
static void folderview_drag_received_cb  (GtkWidget        *widget,
					  GdkDragContext   *drag_context,
					  gint              x,
					  gint              y,
					  GtkSelectionData *data,
					  guint             info,
					  guint             time,
					  FolderView       *folderview);
static void folderview_start_drag	 (GtkWidget *widget, gint button, GdkEvent *event,
			                  FolderView       *folderview);
static void folderview_drag_data_get     (GtkWidget        *widget,
					  GdkDragContext   *drag_context,
					  GtkSelectionData *selection_data,
					  guint             info,
					  guint             time,
					  FolderView       *folderview);
static void folderview_drag_end_cb	 (GtkWidget	   *widget,
					  GdkDragContext   *drag_context,
					  FolderView	   *folderview);

static void folderview_create_folder_node       (FolderView       *folderview, 
					  FolderItem       *item);
static gboolean folderview_update_folder	 (gpointer 	    source,
					  gpointer 	    userdata);
static gboolean folderview_update_item_claws	 (gpointer 	    source,
					  gpointer	    data);
static void folderview_processing_cb(FolderView *folderview, guint action,
				     GtkWidget *widget);
static void folderview_set_sens_and_popup_menu(FolderView *folderview, gint row, 
				GdkEventButton *event);

GHashTable *folderview_popups;

static GtkItemFactoryEntry folderview_common_popup_entries[] =
{
	{N_("/Mark all re_ad"),		NULL, mark_all_read_cb, 0, NULL},
	{"/---",                     	NULL, NULL, 0, "<Separator>"},
	{N_("/_Search folder..."),	NULL, folderview_search_cb, 0, NULL},
	{N_("/_Properties..."),		NULL, folderview_property_cb, 0, NULL},
	{N_("/Process_ing..."),		NULL, folderview_processing_cb, 0, NULL},
};

static GtkItemFactoryEntry folder_view_trash_popup_entries[] = {
	{"/------trashsep",		NULL, NULL, 0, "<Separator>"},
	{N_("/Empty _trash..."),	NULL, folderview_empty_trash_cb, 0, NULL},
};

static GtkItemFactoryEntry folder_view_queue_popup_entries[] = {
	{"/------queuesep",	NULL, NULL, 0, "<Separator>"},
	{N_("/Send _queue..."),	NULL, folderview_send_queue_cb, 0, NULL},
};


GtkTargetEntry folderview_drag_types[] =
{
	{"claws-mail/internal", GTK_TARGET_SAME_APP, TARGET_DUMMY},
	{"text/uri-list", 0, TARGET_MAIL_URI_LIST}
};

void folderview_initialize(void)
{
	FolderViewPopup *fpopup;
	guint i, n_entries;
	GSList *entries = NULL;

	fpopup = g_new0(FolderViewPopup, 1);

	n_entries = sizeof(folderview_common_popup_entries) /
		sizeof(folderview_common_popup_entries[0]);
	for (i = 0; i < n_entries; i++)
		entries = g_slist_append(entries, &folderview_common_popup_entries[i]);

	fpopup->klass = "common";
	fpopup->path = "<CommonFolder>";
	fpopup->entries = entries;
	fpopup->set_sensitivity = NULL;

	folderview_popups = g_hash_table_new(g_str_hash, g_str_equal);
	g_hash_table_insert(folderview_popups, "common", fpopup);
}

static GtkItemFactory *create_ifactory(FolderView *folderview, FolderViewPopup *fpopup)
{
	GSList *entries;
	GtkItemFactory *factory;
	FolderViewPopup *fpopup_common;
	GtkWidget *popup;

	factory = gtk_item_factory_new(GTK_TYPE_MENU, fpopup->path, NULL);
	gtk_item_factory_set_translate_func(factory, menu_translate,
    					    NULL, NULL);

	for (entries = fpopup->entries; entries != NULL; entries = g_slist_next(entries))
		gtk_item_factory_create_item(factory, entries->data, folderview, 1);

	fpopup_common = g_hash_table_lookup(folderview_popups, "common");
	if (fpopup_common != fpopup)
		for (entries = fpopup_common->entries; entries != NULL; entries = g_slist_next(entries))
			gtk_item_factory_create_item(factory, entries->data, folderview, 1);

	popup = gtk_item_factory_get_widget(factory, fpopup->path);
        g_signal_connect(G_OBJECT(popup), "selection_done",
                         G_CALLBACK(folderview_popup_close),
                         folderview);

	return factory;
}

static void create_ifactories(gpointer key, gpointer value, gpointer data)
{
	FolderView *folderview = data;
	FolderViewPopup *fpopup = value;
	GtkItemFactory *factory;

	factory = create_ifactory(folderview, fpopup);
	g_hash_table_insert(folderview->popups, fpopup->klass, factory);
}

static void folderview_column_set_titles(FolderView *folderview)
{
	GtkWidget *ctree = folderview->ctree;
	GtkWidget *label_new;
	GtkWidget *label_unread;
	GtkWidget *label_total;
	GtkWidget *hbox_new;
	GtkWidget *hbox_unread;
	GtkWidget *hbox_total;
	gint *col_pos = folderview->col_pos;

	debug_print("setting titles...\n");
	gtk_widget_realize(folderview->ctree);
	gtk_widget_show_all(folderview->scrolledwin);
	
	/* CLAWS: titles for "New" and "Unread" show new & unread pixmaps
	 * instead text (text overflows making them unreadable and ugly) */
        stock_pixmap_gdk(ctree, STOCK_PIXMAP_NEW,
			 &newxpm, &newxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_UNREAD,
			 &unreadxpm, &unreadxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_READ,
			 &readxpm, &readxpmmask);
		
	label_new = gtk_image_new_from_pixmap(newxpm, newxpmmask);
	label_unread = gtk_image_new_from_pixmap(unreadxpm, unreadxpmmask);
	label_total = gtk_image_new_from_pixmap(readxpm, readxpmmask);
	
	gtk_clist_column_titles_active(GTK_CLIST(ctree));
	 
	hbox_new = gtk_hbox_new(FALSE, 4);
	hbox_unread = gtk_hbox_new(FALSE, 4);
	hbox_total = gtk_hbox_new(FALSE, 4);

	/* left justified */
	gtk_box_pack_start(GTK_BOX(hbox_new), label_new, TRUE, TRUE, 0);
	gtk_misc_set_alignment (GTK_MISC (label_new), 1, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox_unread), label_unread, TRUE, TRUE, 0);
	gtk_misc_set_alignment (GTK_MISC (label_unread), 1, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox_total), label_total, TRUE, TRUE, 0);
	gtk_misc_set_alignment (GTK_MISC (label_total), 1, 0.5);

	gtk_widget_show_all(hbox_new);
	gtk_widget_show_all(hbox_unread);
	gtk_widget_show_all(hbox_total);

	gtk_clist_set_column_widget(GTK_CLIST(ctree),col_pos[F_COL_NEW],hbox_new);
	gtk_clist_set_column_widget(GTK_CLIST(ctree),col_pos[F_COL_UNREAD],hbox_unread);
	gtk_clist_set_column_widget(GTK_CLIST(ctree),col_pos[F_COL_TOTAL],hbox_total);

	gtk_sctree_set_column_tooltip(GTK_SCTREE(ctree), col_pos[F_COL_NEW], _("New"));
	gtk_sctree_set_column_tooltip(GTK_SCTREE(ctree), col_pos[F_COL_UNREAD], _("Unread"));
	gtk_sctree_set_column_tooltip(GTK_SCTREE(ctree), col_pos[F_COL_TOTAL], _("Total"));
}

static gboolean folderview_popup_menu(GtkWidget *widget, gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	GdkEventButton event;
	if (folderview_get_selected_item(folderview) == NULL)
		return FALSE;
	
	event.button = 3;
	event.time = gtk_get_current_event_time();
	
	folderview_set_sens_and_popup_menu(folderview, -1, 
				&event);

	return TRUE;
}


static GtkWidget *folderview_ctree_create(FolderView *folderview)
{
	GtkWidget *ctree;
	gint *col_pos;
	FolderColumnState *col_state;
	FolderColumnType type;
	gchar *titles[N_FOLDER_COLS];
	gint i;
	GtkWidget *scrolledwin = folderview->scrolledwin;

	debug_print("creating tree...\n");
	memset(titles, 0, sizeof(titles));

	col_state = prefs_folder_column_get_config();
	memset(titles, 0, sizeof(titles));

	col_pos = folderview->col_pos;

	for (i = 0; i < N_FOLDER_COLS; i++) {
		folderview->col_state[i] = col_state[i];
		type = col_state[i].type;
		col_pos[type] = i;
	}

	titles[col_pos[F_COL_FOLDER]] = _("Folder");
	titles[col_pos[F_COL_NEW]]    = _("New");
	titles[col_pos[F_COL_UNREAD]] = _("Unread");
	/* TRANSLATORS: This in Number sign in American style */
	titles[col_pos[F_COL_TOTAL]]  = _("#");

	ctree = gtk_sctree_new_with_titles(N_FOLDER_COLS, col_pos[F_COL_FOLDER],
					   titles);

	gtk_clist_set_selection_mode(GTK_CLIST(ctree), GTK_SELECTION_BROWSE);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), col_pos[F_COL_NEW],
					   GTK_JUSTIFY_RIGHT);
	gtk_clist_set_column_justification(GTK_CLIST(ctree),
					   col_pos[F_COL_UNREAD],
					   GTK_JUSTIFY_RIGHT);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), 
					   col_pos[F_COL_TOTAL],
					   GTK_JUSTIFY_RIGHT);
	if (prefs_common.enable_dotted_lines) {
		gtk_ctree_set_line_style(GTK_CTREE(ctree), GTK_CTREE_LINES_DOTTED);
		gtk_ctree_set_expander_style(GTK_CTREE(ctree),
				     GTK_CTREE_EXPANDER_SQUARE);
	} else {
		gtk_ctree_set_line_style(GTK_CTREE(ctree), GTK_CTREE_LINES_NONE);
		gtk_ctree_set_expander_style(GTK_CTREE(ctree),
				     GTK_CTREE_EXPANDER_TRIANGLE);
	}

	gtk_sctree_set_stripes(GTK_SCTREE(ctree), prefs_common.use_stripes_in_summaries);
	gtk_sctree_set_recursive_expand(GTK_SCTREE(ctree), FALSE);

	gtk_ctree_set_indent(GTK_CTREE(ctree), CTREE_INDENT);
	gtk_clist_set_compare_func(GTK_CLIST(ctree), folderview_clist_compare);

	/* don't let title buttons take key focus */
	for (i = 0; i < N_FOLDER_COLS; i++) {
		GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(ctree)->column[i].button,
				       GTK_CAN_FOCUS);
		gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[i],
				   prefs_common.folder_col_size[i]);
		gtk_clist_set_column_visibility
			(GTK_CLIST(ctree), i, col_state[i].visible);
	}

	g_signal_connect(G_OBJECT(ctree), "key_press_event",
			 G_CALLBACK(folderview_key_pressed),
			 folderview);
	g_signal_connect(G_OBJECT(ctree), "button_press_event",
			 G_CALLBACK(folderview_button_pressed),
			 folderview);
	g_signal_connect(G_OBJECT(ctree), "popup-menu",
			 G_CALLBACK(folderview_popup_menu), folderview);
	g_signal_connect(G_OBJECT(ctree), "button_release_event",
			 G_CALLBACK(folderview_button_released),
			 folderview);
	g_signal_connect(G_OBJECT(ctree), "tree_select_row",
			 G_CALLBACK(folderview_selected), folderview);
	g_signal_connect(G_OBJECT(ctree), "start_drag",
			 G_CALLBACK(folderview_start_drag), folderview);
	g_signal_connect(G_OBJECT(ctree), "drag_data_get",
			 G_CALLBACK(folderview_drag_data_get),
			 folderview);

	g_signal_connect_after(G_OBJECT(ctree), "tree_expand",
			       G_CALLBACK(folderview_tree_expanded),
			       folderview);
	g_signal_connect_after(G_OBJECT(ctree), "tree_collapse",
			       G_CALLBACK(folderview_tree_collapsed),
			       folderview);

	g_signal_connect(G_OBJECT(ctree), "resize_column",
			 G_CALLBACK(folderview_col_resized),
			 folderview);

        /* drop callback */
	gtk_drag_dest_set(ctree, GTK_DEST_DEFAULT_ALL & ~GTK_DEST_DEFAULT_HIGHLIGHT,
			  folderview_drag_types, 2,
			  GDK_ACTION_MOVE | GDK_ACTION_COPY | GDK_ACTION_DEFAULT);
	g_signal_connect(G_OBJECT(ctree), "drag_motion",
			 G_CALLBACK(folderview_drag_motion_cb),
			 folderview);
	g_signal_connect(G_OBJECT(ctree), "drag_leave",
			 G_CALLBACK(folderview_drag_leave_cb),
			 folderview);
	g_signal_connect(G_OBJECT(ctree), "drag_data_received",
			 G_CALLBACK(folderview_drag_received_cb),
			 folderview);
	g_signal_connect(G_OBJECT(ctree), "drag_end",
			 G_CALLBACK(folderview_drag_end_cb),
			 folderview);

	gtk_container_add(GTK_CONTAINER(scrolledwin), ctree);

	return ctree;
}

void folderview_set_column_order(FolderView *folderview)
{
	GtkWidget *ctree;
	FolderItem *item = folderview_get_selected_item(folderview);
	GtkWidget *scrolledwin = folderview->scrolledwin;

	debug_print("recreating tree...\n");
	gtk_widget_destroy(folderview->ctree);

	folderview->ctree = ctree = folderview_ctree_create(folderview);
	gtk_scrolled_window_set_hadjustment(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_CLIST(ctree)->hadjustment);
	gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_CLIST(ctree)->vadjustment);
	gtk_widget_show(ctree);
	
	folderview_set(folderview);
	folderview_column_set_titles(folderview);

	folderview_select(folderview,item);
}

FolderView *folderview_create(void)
{
	FolderView *folderview;
	GtkWidget *scrolledwin;
	GtkWidget *ctree;

	debug_print("Creating folder view...\n");
	folderview = g_new0(FolderView, 1);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy
		(GTK_SCROLLED_WINDOW(scrolledwin),
		 GTK_POLICY_AUTOMATIC,
		 prefs_common.folderview_vscrollbar_policy);
	gtk_widget_set_size_request(scrolledwin,
			     prefs_common.folderview_width,
			     prefs_common.folderview_height);

	folderview->scrolledwin  = scrolledwin;
	ctree = folderview_ctree_create(folderview);
	
	/* create popup factories */
	folderview->popups = g_hash_table_new(g_str_hash, g_str_equal);
	g_hash_table_foreach(folderview_popups, create_ifactories, folderview);

	folderview->ctree        = ctree;

	folderview->folder_update_callback_id =
		hooks_register_hook(FOLDER_UPDATE_HOOKLIST, folderview_update_folder, (gpointer) folderview);
	folderview->folder_item_update_callback_id =
		hooks_register_hook(FOLDER_ITEM_UPDATE_HOOKLIST, folderview_update_item_claws, (gpointer) folderview);

	gtk_widget_show_all(scrolledwin);
	
	folderview->target_list = gtk_target_list_new(folderview_drag_types, 2);
	folderview_list = g_list_append(folderview_list, folderview);

	return folderview;
}

void folderview_init(FolderView *folderview)
{
	GtkWidget *ctree = folderview->ctree;
	GdkColor gdk_color;

	stock_pixmap_gdk(ctree, STOCK_PIXMAP_INBOX_CLOSE, &inboxxpm, &inboxxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_INBOX_CLOSE_HRM, &inboxhrmxpm, &inboxhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_INBOX_OPEN, &inboxopenxpm, &inboxopenxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_INBOX_OPEN_HRM, &inboxopenhrmxpm, &inboxopenhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_OUTBOX_CLOSE, &outboxxpm, &outboxxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_OUTBOX_CLOSE_HRM, &outboxhrmxpm, &outboxhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_OUTBOX_OPEN, &outboxopenxpm, &outboxopenxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_OUTBOX_OPEN_HRM, &outboxopenhrmxpm, &outboxopenhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_DIR_CLOSE, &folderxpm, &folderxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_DIR_CLOSE_HRM, &folderhrmxpm, &folderhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_DIR_OPEN, &folderopenxpm, &folderopenxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_DIR_OPEN_HRM, &folderopenhrmxpm, &folderopenhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_TRASH_OPEN, &trashopenxpm, &trashopenxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_TRASH_OPEN_HRM, &trashopenhrmxpm, &trashopenhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_TRASH_CLOSE, &trashxpm, &trashxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_TRASH_CLOSE_HRM, &trashhrmxpm, &trashhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_QUEUE_CLOSE, &queuexpm, &queuexpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_QUEUE_CLOSE_HRM, &queuehrmxpm, &queuehrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_QUEUE_OPEN, &queueopenxpm, &queueopenxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_QUEUE_OPEN_HRM, &queueopenhrmxpm, &queueopenhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_DRAFTS_CLOSE, &draftsxpm, &draftsxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_DRAFTS_OPEN, &draftsopenxpm, &draftsopenxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_DIR_NOSELECT, &noselectxpm, &noselectxpmmask);

	stock_pixmap_gdk(ctree, STOCK_PIXMAP_INBOX_CLOSE_MARK, &m_inboxxpm, &m_inboxxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_INBOX_CLOSE_HRM_MARK, &m_inboxhrmxpm, &m_inboxhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_INBOX_OPEN_MARK, &m_inboxopenxpm, &m_inboxopenxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_INBOX_OPEN_HRM_MARK, &m_inboxopenhrmxpm, &m_inboxopenhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_OUTBOX_CLOSE_MARK, &m_outboxxpm, &m_outboxxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_OUTBOX_CLOSE_HRM_MARK, &m_outboxhrmxpm, &m_outboxhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_OUTBOX_OPEN_MARK, &m_outboxopenxpm, &m_outboxopenxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_OUTBOX_OPEN_HRM_MARK, &m_outboxopenhrmxpm, &m_outboxopenhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_DIR_CLOSE_MARK, &m_folderxpm, &m_folderxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_DIR_CLOSE_HRM_MARK, &m_folderhrmxpm, &m_folderhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_DIR_OPEN_MARK, &m_folderopenxpm, &m_folderopenxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_DIR_OPEN_HRM_MARK, &m_folderopenhrmxpm, &m_folderopenhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_TRASH_OPEN_MARK, &m_trashopenxpm, &m_trashopenxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_TRASH_OPEN_HRM_MARK, &m_trashopenhrmxpm, &m_trashopenhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_TRASH_CLOSE_MARK, &m_trashxpm, &m_trashxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_TRASH_CLOSE_HRM_MARK, &m_trashhrmxpm, &m_trashhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_QUEUE_CLOSE_MARK, &m_queuexpm, &m_queuexpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_QUEUE_CLOSE_HRM_MARK, &m_queuehrmxpm, &m_queuehrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_QUEUE_OPEN_MARK, &m_queueopenxpm, &m_queueopenxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_QUEUE_OPEN_HRM_MARK, &m_queueopenhrmxpm, &m_queueopenhrmxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_DRAFTS_CLOSE_MARK, &m_draftsxpm, &m_draftsxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_DRAFTS_OPEN_MARK, &m_draftsopenxpm, &m_draftsopenxpmmask);
			
	if (!normal_style) {
		PangoFontDescription *font_desc;
		normal_style = gtk_style_copy(gtk_widget_get_style(ctree));
		font_desc = pango_font_description_from_string(NORMAL_FONT);
		if (font_desc) {
			if (normal_style->font_desc)
				pango_font_description_free
					(normal_style->font_desc);
			normal_style->font_desc = font_desc;
		}
		gtkut_convert_int_to_gdk_color(prefs_common.color_new, &gdk_color);
		normal_color_style = gtk_style_copy(normal_style);
		normal_color_style->fg[GTK_STATE_NORMAL] = gdk_color;

		gtk_widget_set_style(ctree, normal_style);
	}

	if (!bold_style) {
		gtkut_convert_int_to_gdk_color(prefs_common.color_new, &gdk_color);
		bold_style = gtk_style_copy(gtk_widget_get_style(ctree));
		pango_font_description_set_weight
			(bold_style->font_desc, PANGO_WEIGHT_BOLD);
		bold_color_style = gtk_style_copy(bold_style);
		bold_color_style->fg[GTK_STATE_NORMAL] = gdk_color;

		bold_tgtfold_style = gtk_style_copy(bold_style);
		bold_tgtfold_style->fg[GTK_STATE_NORMAL] = folderview->color_op;
	}
}

void folderview_set(FolderView *folderview)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	MainWindow *mainwin = folderview->mainwin;
	FolderItem *sel_item = NULL, *op_item = NULL;

	if (!mainwin)
		return;

	debug_print("Setting folder info...\n");
	STATUSBAR_PUSH(mainwin, _("Setting folder info..."));

	main_window_cursor_wait(mainwin);

	if (folderview->selected)
		sel_item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	if (folderview->opened)
		op_item = gtk_ctree_node_get_row_data(ctree, folderview->opened);

	folderview->selected = NULL;
	folderview->opened = NULL;

	gtk_clist_freeze(GTK_CLIST(ctree));
	gtk_clist_clear(GTK_CLIST(ctree));

	folderview_set_folders(folderview);

	if (sel_item)
		folderview->selected = gtk_ctree_find_by_row_data(ctree, NULL, sel_item);
	if (op_item)
		folderview->opened = gtk_ctree_find_by_row_data(ctree, NULL, op_item);

	gtk_clist_thaw(GTK_CLIST(ctree));
	main_window_cursor_normal(mainwin);
	STATUSBAR_POP(mainwin);
}

void folderview_set_all(void)
{
	GList *list;

	for (list = folderview_list; list != NULL; list = list->next)
		folderview_set((FolderView *)list->data);
}

void folderview_select(FolderView *folderview, FolderItem *item)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	GtkCTreeNode *node;
	GtkCTreeNode *old_selected = folderview->selected;

	if (!item) return;

	node = gtk_ctree_find_by_row_data(ctree, NULL, item);
	if (node) folderview_select_node(folderview, node);

	if (old_selected != node)
		folder_update_op_count();
}

static void mark_all_read_cb(FolderView *folderview, guint action,
                             GtkWidget *widget)
{
	FolderItem *item;
	AlertValue val;
	
	item = folderview_get_selected_item(folderview);
	if (item == NULL)
		return;

	if (folderview->summaryview->folder_item != item
	&&  prefs_common.ask_mark_all_read) {
		val = alertpanel_full(_("Mark all as read"),
			_("Do you really want to mark all mails in this "
			  "folder as read ?"), GTK_STOCK_NO, GTK_STOCK_YES, NULL,
			  TRUE, NULL, ALERT_QUESTION, G_ALERTDEFAULT);

		if ((val & ~G_ALERTDISABLE) != G_ALERTALTERNATE)
			return;
		else if (val & G_ALERTDISABLE)
			prefs_common.ask_mark_all_read = FALSE;
	}

	summary_lock(folderview->summaryview);
	folder_item_update_freeze();
	if (folderview->summaryview->folder_item == item)
		summary_freeze(folderview->summaryview);
	folderutils_mark_all_read(item);
	if (folderview->summaryview->folder_item == item)
		summary_thaw(folderview->summaryview);
	folder_item_update_thaw();
	summary_unlock(folderview->summaryview);
}

static void folderview_select_node(FolderView *folderview, GtkCTreeNode *node)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);

	g_return_if_fail(node != NULL);

	if (folderview->open_folder) {
		return;
	}

	folderview->open_folder = TRUE;
	gtkut_ctree_set_focus_row(ctree, node);
	gtk_ctree_select(ctree, node);
	if (folderview->summaryview->folder_item &&
	    folderview->summaryview->folder_item->total_msgs > 0)
		summary_grab_focus(folderview->summaryview);
	else
		gtk_widget_grab_focus(folderview->ctree);

	gtkut_ctree_expand_parent_all(ctree, node);
}

void folderview_unselect(FolderView *folderview)
{
	if (folderview->opened && !GTK_CTREE_ROW(folderview->opened)->children)
		gtk_ctree_collapse
			(GTK_CTREE(folderview->ctree), folderview->opened);

	folderview->selected = folderview->opened = NULL;
}

static GtkCTreeNode *folderview_find_next_marked(GtkCTree *ctree,
						 GtkCTreeNode *node)
{
	FolderItem *item;

	if (node)
		node = gtkut_ctree_node_next(ctree, node);
	else
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	for (; node != NULL; node = gtkut_ctree_node_next(ctree, node)) {
		item = gtk_ctree_node_get_row_data(ctree, node);
		if (item && item->marked_msgs > 0 && item->stype != F_TRASH)
			return node;
	}

	return NULL;
}

void folderview_select_next_marked(FolderView *folderview)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	GtkCTreeNode *node = NULL;
	SelectOnEntry last_sel = prefs_common.select_on_entry;
	gboolean last_open = prefs_common.always_show_msg;
	
	prefs_common.select_on_entry = SELECTONENTRY_MNU;
	prefs_common.always_show_msg = TRUE;

	if ((node = folderview_find_next_marked(ctree, folderview->opened))
	    != NULL) {
		folderview_select_node(folderview, node);
		goto out;
	}

	if (!folderview->opened ||
	    folderview->opened == GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list)) {
		goto out;
	}
	/* search again from the first node */
	if ((node = folderview_find_next_marked(ctree, NULL)) != NULL)
		folderview_select_node(folderview, node);

out:
	prefs_common.select_on_entry = last_sel;
	prefs_common.always_show_msg = last_open;
}

static GtkCTreeNode *folderview_find_next_unread(GtkCTree *ctree,
						 GtkCTreeNode *node)
{
	FolderItem *item;

	if (node)
		node = gtkut_ctree_node_next(ctree, node);
	else
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	for (; node != NULL; node = gtkut_ctree_node_next(ctree, node)) {
		item = gtk_ctree_node_get_row_data(ctree, node);
		if (item && item->unread_msgs > 0 && item->stype != F_TRASH)
			return node;
	}

	return NULL;
}

void folderview_select_next_unread(FolderView *folderview, gboolean force_open)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	GtkCTreeNode *node = NULL;
	SelectOnEntry last_sel = prefs_common.select_on_entry;
	gboolean last_open = prefs_common.always_show_msg;
	
	prefs_common.select_on_entry = SELECTONENTRY_UNM;
	prefs_common.always_show_msg = force_open ? TRUE : last_open;

	if ((node = folderview_find_next_unread(ctree, folderview->opened))
	    != NULL) {
		folderview_select_node(folderview, node);
		goto out;
	}

	if (!folderview->opened ||
	    folderview->opened == GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list)) {
		goto out;
	}
	/* search again from the first node */
	if ((node = folderview_find_next_unread(ctree, NULL)) != NULL)
		folderview_select_node(folderview, node);

out:
	prefs_common.select_on_entry = last_sel;
	prefs_common.always_show_msg = last_open;
}

static GtkCTreeNode *folderview_find_next_new(GtkCTree *ctree,
						 GtkCTreeNode *node)
{
	FolderItem *item;

	if (node)
		node = gtkut_ctree_node_next(ctree, node);
	else
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	for (; node != NULL; node = gtkut_ctree_node_next(ctree, node)) {
		item = gtk_ctree_node_get_row_data(ctree, node);
		if (item && item->new_msgs > 0 && item->stype != F_TRASH)
			return node;
	}

	return NULL;
}

void folderview_select_next_new(FolderView *folderview)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	GtkCTreeNode *node = NULL;
	SelectOnEntry last_sel = prefs_common.select_on_entry;
	gboolean last_open = prefs_common.always_show_msg;
	
	prefs_common.select_on_entry = SELECTONENTRY_NUM;
	prefs_common.always_show_msg = TRUE;

	if ((node = folderview_find_next_new(ctree, folderview->opened))
	    != NULL) {
		folderview_select_node(folderview, node);
		goto out;
	}

	if (!folderview->opened ||
	    folderview->opened == GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list)) {
		goto out;
	}
	/* search again from the first node */
	if ((node = folderview_find_next_new(ctree, NULL)) != NULL)
		folderview_select_node(folderview, node);

out:
	prefs_common.select_on_entry = last_sel;
	prefs_common.always_show_msg = last_open;
}

FolderItem *folderview_get_selected_item(FolderView *folderview)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);

	if (!folderview->selected) return NULL;
	return gtk_ctree_node_get_row_data(ctree, folderview->selected);
}

static void folderview_set_folders(FolderView *folderview)
{
	GList *list;
	list = folder_get_list();

	for (; list != NULL; list = list->next) {
		folderview_append_folder(folderview, FOLDER(list->data));
	}
}

static gchar *get_scan_str(FolderItem *item)
{
	if (item->path)
		return g_strdup_printf(_("Scanning folder %s%c%s ..."),
				      item->folder->name, G_DIR_SEPARATOR,
				      item->path);
	else
		return g_strdup_printf(_("Scanning folder %s ..."),
				      item->folder->name);	
}
static void folderview_scan_tree_func(Folder *folder, FolderItem *item,
				      gpointer data)
{
	GList *list;
	for (list = folderview_list; list != NULL; list = list->next) {
		FolderView *folderview = (FolderView *)list->data;
		MainWindow *mainwin = folderview->mainwin;
		gchar *str = get_scan_str(item);

		STATUSBAR_PUSH(mainwin, str);
		STATUSBAR_POP(mainwin);
		g_free(str);
	}
}

void folderview_rescan_tree(Folder *folder, gboolean rebuild)
{
	GtkWidget *window;
	MainWindow *mainwin = mainwindow_get_mainwindow();
	FolderView *folderview = NULL;
	GtkAdjustment *pos = NULL;
	gint height = 0;

	g_return_if_fail(folder != NULL);

	if (!folder->klass->scan_tree) return;

	if (rebuild && 
	    alertpanel_full(_("Rebuild folder tree"), 
	    		 _("Rebuilding the folder tree will remove "
			   "local caches. Do you want to continue?"),
		       	 GTK_STOCK_NO, GTK_STOCK_YES, NULL, FALSE,
		       	 NULL, ALERT_WARNING, G_ALERTDEFAULT) 
		!= G_ALERTALTERNATE) {
		return;
	}

	inc_lock();
	if (rebuild)
		window = label_window_create(_("Rebuilding folder tree..."));
	else 
		window = label_window_create(_("Scanning folder tree..."));

	if (mainwin)
		folderview = mainwin->folderview;
	
	if (folderview) {
		pos = gtk_scrolled_window_get_vadjustment(
					GTK_SCROLLED_WINDOW(folderview->scrolledwin));
		height = pos->value;
	}

	folder_set_ui_func(folder, folderview_scan_tree_func, NULL);
	folder_scan_tree(folder, rebuild);
	folder_set_ui_func(folder, NULL, NULL);

	folderview_set_all();

	if (folderview) {
		pos = gtk_scrolled_window_get_vadjustment(
					GTK_SCROLLED_WINDOW(folderview->scrolledwin));
		gtk_adjustment_set_value(pos, height);
	}
	gtk_widget_destroy(window);
	inc_unlock();
}

void folderview_fast_rescan_tree(Folder *folder)
{
	GtkWidget *window;
	MainWindow *mainwin = mainwindow_get_mainwindow();
	FolderView *folderview = NULL;
	GtkAdjustment *pos = NULL;
	gint height = 0;

	g_return_if_fail(folder != NULL);

	if (!folder->klass->scan_tree) return;

	inc_lock();

	window = label_window_create(_("Scanning folder tree..."));

	if (mainwin)
		folderview = mainwin->folderview;
	
	if (folderview) {
		pos = gtk_scrolled_window_get_vadjustment(
					GTK_SCROLLED_WINDOW(folderview->scrolledwin));
		height = pos->value;
	}

	folder_set_ui_func(folder, folderview_scan_tree_func, NULL);
	folder_fast_scan_tree(folder);
	folder_set_ui_func(folder, NULL, NULL);

	folderview_set_all();

	if (folderview) {
		pos = gtk_scrolled_window_get_vadjustment(
					GTK_SCROLLED_WINDOW(folderview->scrolledwin));
		gtk_adjustment_set_value(pos, height);
	}
	gtk_widget_destroy(window);
	inc_unlock();
}

/** folderview_check_new()
 *  Scan and update the folder and return the 
 *  count the number of new messages since last check. 
 *  \param folder the folder to check for new messages
 *  \return the number of new messages since last check
 */
gint folderview_check_new(Folder *folder)
{
	GList *list;
	FolderItem *item;
	FolderView *folderview;
	GtkCTree *ctree;
	GtkCTreeNode *node;
	gint new_msgs = 0;
	gint former_new_msgs = 0;
	gint former_new = 0, former_unread = 0, former_total;

	for (list = folderview_list; list != NULL; list = list->next) {
		folderview = (FolderView *)list->data;
		ctree = GTK_CTREE(folderview->ctree);

		inc_lock();
		main_window_lock(folderview->mainwin);

		for (node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
		     node != NULL; node = gtkut_ctree_node_next(ctree, node)) {
			gchar *str = NULL;
			item = gtk_ctree_node_get_row_data(ctree, node);
			if (!item || !item->path || !item->folder) continue;
			if (item->no_select) continue;
			if (folder && folder != item->folder) continue;
			if (!folder && !FOLDER_IS_LOCAL(item->folder)) continue;
			if (!item->prefs->newmailcheck) continue;
			if (item->processing_pending == TRUE) {
				debug_print("skipping %s, processing pending\n",
					item->path ? item->path : item->name);
				continue;
			}

			str = get_scan_str(item);

			STATUSBAR_PUSH(folderview->mainwin, str);
			GTK_EVENTS_FLUSH();
			g_free(str);

			folderview_scan_tree_func(item->folder, item, NULL);
			former_new    = item->new_msgs;
			former_unread = item->unread_msgs;
			former_total  = item->total_msgs;

			if (item->folder->klass->scan_required &&
			    (item->folder->klass->scan_required(item->folder, item) ||
			     item->folder->inbox == item ||
			     item->opened == TRUE ||
			     item->processing_pending == TRUE)) {
				if (folder_item_scan(item) < 0) {
					if (folder) {
						summaryview_unlock(folderview->summaryview, item);
						if (FOLDER_TYPE(item->folder) == F_NEWS || FOLDER_IS_LOCAL(folder)) {
							log_error(LOG_PROTOCOL, _("Couldn't scan folder %s\n"),
								item->path ? item->path:item->name);
							continue;
						} else if (!FOLDER_IS_LOCAL(folder)) {
							STATUSBAR_POP(folderview->mainwin);
							break;
						}
					}
				}
			} else if (!item->folder->klass->scan_required) {
				if (folder_item_scan(item) < 0) {
					summaryview_unlock(folderview->summaryview, item);
					if (folder && !FOLDER_IS_LOCAL(folder)) {
						STATUSBAR_POP(folderview->mainwin);
						break;
					}
				}
			}
			if (former_new    != item->new_msgs ||
			    former_unread != item->unread_msgs ||
			    former_total  != item->total_msgs)
				folderview_update_node(folderview, node);

			new_msgs += item->new_msgs;
			former_new_msgs += former_new;
			STATUSBAR_POP(folderview->mainwin);
		}

		main_window_unlock(folderview->mainwin);
		inc_unlock();
	}

	folder_write_list();
	/* Number of new messages since last check is the just the difference 
	 * between former_new_msgs and new_msgs. If new_msgs is less than
	 * former_new_msgs, that would mean another session accessed the folder
	 * and the result is not well defined.
	 */
	new_msgs = (former_new_msgs < new_msgs ? new_msgs - former_new_msgs : 0);
	return new_msgs;
}

void folderview_check_new_all(void)
{
	GList *list;
	GtkWidget *window;
	FolderView *folderview;

	folderview = (FolderView *)folderview_list->data;

	inc_lock();
	main_window_lock(folderview->mainwin);
	window = label_window_create
		(_("Checking for new messages in all folders..."));

	list = folder_get_list();
	for (; list != NULL; list = list->next) {
		Folder *folder = list->data;

		folderview_check_new(folder);
	}

	folder_write_list();
	folderview_set_all();

	gtk_widget_destroy(window);
	main_window_unlock(folderview->mainwin);
	inc_unlock();
}

static gboolean folderview_have_new_children_sub(FolderView *folderview,
						 FolderItem *item,
						 gboolean in_sub)
{
	GNode *node = NULL;
	
	if (!item || !item->folder || !item->folder->node)
		return FALSE;
		
	node = item->folder->node;
	
	node = g_node_find(node, G_PRE_ORDER, G_TRAVERSE_ALL, item);
	node = node->children;

	if (in_sub &&
	    (item->new_msgs > 0 ||
	    (folder_has_parent_of_type(item, F_QUEUE) && item->total_msgs > 0))) {
		return TRUE;
	}

	while (node != NULL) {
		if (node && node->data) {
			FolderItem *next_item = (FolderItem*) node->data;
			node = node->next;
			if (folderview_have_new_children_sub(folderview, 
							     next_item, TRUE))
				return TRUE;
		}
	}

	return FALSE;
}

static gboolean folderview_have_new_children(FolderView *folderview,
					     FolderItem *item)
{
	return folderview_have_new_children_sub(folderview, item, FALSE);
}

static gboolean folderview_have_unread_children_sub(FolderView *folderview,
						    FolderItem *item, 
						    gboolean in_sub)
{
	GNode *node = NULL;
	
	if (!item || !item->folder || !item->folder->node)
		return FALSE;
	
	node = item->folder->node;
	
	node = g_node_find(node, G_PRE_ORDER, G_TRAVERSE_ALL, item);
	node = node->children;

	if (in_sub &&
	    (item->unread_msgs > 0 ||
	    (folder_has_parent_of_type(item, F_QUEUE) && item->total_msgs > 0))) {
		return TRUE;
	}

	while (node != NULL) {
		if (node && node->data) {
			FolderItem *next_item = (FolderItem*) node->data;
			node = node->next;
			if (folderview_have_unread_children_sub(folderview, 
							        next_item, 
								TRUE))
				return TRUE;
		}
	}

	return FALSE;
}

static gboolean folderview_have_unread_children(FolderView *folderview,
						FolderItem *item)
{
	return folderview_have_unread_children_sub(folderview, item, FALSE);
}

static gboolean folderview_have_matching_children_sub(FolderView *folderview,
						      FolderItem *item,
						      gboolean in_sub)
{
	GNode *node = NULL;

	if (!item || !item->folder || !item->folder->node)
		return FALSE;

	node = item->folder->node;
	
	node = g_node_find(node, G_PRE_ORDER, G_TRAVERSE_ALL, item);
	node = node->children;

	if (in_sub && item->search_match){
		return TRUE;
	}

	while (node != NULL) {
		if (node && node->data) {
			FolderItem *next_item = (FolderItem*) node->data;
			node = node->next;
			if (folderview_have_matching_children_sub(folderview, 
							          next_item, 
								  TRUE))
				return TRUE;
		}
	}

	return FALSE;
}

static gboolean folderview_have_matching_children(FolderView *folderview,
						  FolderItem *item)
{
	return folderview_have_matching_children_sub(folderview, item, FALSE);
}

static gboolean folderview_have_marked_children_sub(FolderView *folderview,
						    FolderItem *item,
						    gboolean in_sub)
{
	GNode *node = NULL;
	
	if (!item || !item->folder || !item->folder->node)
		return FALSE;
		
	node = item->folder->node;
	
	node = g_node_find(node, G_PRE_ORDER, G_TRAVERSE_ALL, item);
	node = node->children;

	if (item->marked_msgs != 0) {
		return TRUE;
	}

	while (node != NULL) {
		if (node && node->data) {
			FolderItem *next_item = (FolderItem*) node->data;
			node = node->next;
			if (folderview_have_marked_children_sub(folderview,
							        next_item, TRUE))
				return TRUE;
		}
	}

	return FALSE;
}

static gboolean folderview_have_marked_children(FolderView *folderview,
					     FolderItem *item)
{
	return folderview_have_marked_children_sub(folderview, item, FALSE);
}

static void folderview_update_node(FolderView *folderview, GtkCTreeNode *node)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	GtkStyle *style = NULL;
	GtkStyle *color_style = NULL;
	FolderItem *item;
	GdkPixmap *xpm, *openxpm;
	GdkBitmap *mask, *openmask;
	static GdkPixmap *searchicon;
	static GdkBitmap *searchmask;
	gboolean mark = FALSE;
	gchar *name;
	gchar *str;
	gboolean add_unread_mark;
	gboolean add_sub_match_mark;
	gboolean use_bold, use_color;
	gint *col_pos = folderview->col_pos;
	SpecialFolderItemType stype;
	
	item = gtk_ctree_node_get_row_data(ctree, node);
	g_return_if_fail(item != NULL);

	if (!GTK_CTREE_ROW(node)->expanded)
		mark = folderview_have_marked_children(folderview, item);
	else
		mark = (item->marked_msgs != 0);

	stype = item->stype;
	if (stype == F_NORMAL) {
		if (folder_has_parent_of_type(item, F_TRASH))
			stype = F_TRASH;
		else if (folder_has_parent_of_type(item, F_DRAFT))
			stype = F_DRAFT;
		else if (folder_has_parent_of_type(item, F_OUTBOX))
			stype = F_OUTBOX;
		else if (folder_has_parent_of_type(item, F_QUEUE))
			stype = F_QUEUE;
	}
	switch (stype) {
	case F_INBOX:
		if (item->hide_read_msgs) {
			xpm = mark?m_inboxhrmxpm:inboxhrmxpm;
			mask = mark?m_inboxhrmxpmmask:inboxhrmxpmmask;
			openxpm = mark?m_inboxopenhrmxpm:inboxopenhrmxpm;
			openmask = mark?m_inboxopenhrmxpmmask:inboxopenhrmxpmmask;
		} else {
			xpm = mark?m_inboxxpm:inboxxpm;
			mask = mark?m_inboxxpmmask:inboxxpmmask;
			openxpm = mark?m_inboxopenxpm:inboxopenxpm;
			openmask = mark?m_inboxopenxpmmask:inboxopenxpmmask;
		}
		break;
	case F_OUTBOX:
		if (item->hide_read_msgs) {
			xpm = mark?m_outboxhrmxpm:outboxhrmxpm;
			mask = mark?m_outboxhrmxpmmask:outboxhrmxpmmask;
			openxpm = mark?m_outboxopenhrmxpm:outboxopenhrmxpm;
			openmask = mark?m_outboxopenhrmxpmmask:outboxopenhrmxpmmask;
		} else {
			xpm = mark?m_outboxxpm:outboxxpm;
			mask = mark?m_outboxxpmmask:outboxxpmmask;
			openxpm = mark?m_outboxopenxpm:outboxopenxpm;
			openmask = mark?m_outboxopenxpmmask:outboxopenxpmmask;
		}
		break;
	case F_QUEUE:
		if (item->hide_read_msgs) {
			xpm = mark?m_queuehrmxpm:queuehrmxpm;
			mask = mark?m_queuehrmxpmmask:queuehrmxpmmask;
			openxpm = mark?m_queueopenhrmxpm:queueopenhrmxpm;
			openmask = mark?m_queueopenhrmxpmmask:queueopenhrmxpmmask;
		} else {
			xpm = mark?m_queuexpm:queuexpm;
			mask = mark?m_queuexpmmask:queuexpmmask;
			openxpm = mark?m_queueopenxpm:queueopenxpm;
			openmask = mark?m_queueopenxpmmask:queueopenxpmmask;
		}
		break;
	case F_TRASH:
		if (item->hide_read_msgs) {
			xpm = mark?m_trashhrmxpm:trashhrmxpm;
			mask = mark?m_trashhrmxpmmask:trashhrmxpmmask;
			openxpm = mark?m_trashopenhrmxpm:trashopenhrmxpm;
			openmask = mark?m_trashopenhrmxpmmask:trashopenhrmxpmmask;
		} else {
			xpm = mark?m_trashxpm:trashxpm;
			mask = mark?m_trashxpmmask:trashxpmmask;
			openxpm = mark?m_trashopenxpm:trashopenxpm;
			openmask = mark?m_trashopenxpmmask:trashopenxpmmask;
		}
		break;
	case F_DRAFT:
		xpm = mark?m_draftsxpm:draftsxpm;
		mask = mark?m_draftsxpmmask:draftsxpmmask;
		openxpm = mark?m_draftsopenxpm:draftsopenxpm;
		openmask = mark?m_draftsopenxpmmask:draftsopenxpmmask;
		break;
	default:
		if (item->hide_read_msgs) {
			xpm = mark?m_folderhrmxpm:folderhrmxpm;
			mask = mark?m_folderhrmxpmmask:folderhrmxpmmask;
			openxpm = mark?m_folderopenhrmxpm:folderopenhrmxpm;
			openmask = mark?m_folderopenhrmxpmmask:folderopenhrmxpmmask;
		} else {
			xpm = mark?m_folderxpm:folderxpm;
			mask = mark?m_folderxpmmask:folderxpmmask;
			openxpm = mark?m_folderopenxpm:folderopenxpm;
			openmask = mark?m_folderopenxpmmask:folderopenxpmmask;
		}
	}
	
	if (item->no_select) {
		xpm = openxpm = noselectxpm;
		mask = openmask = noselectxpmmask;
	}

	name = folder_item_get_name(item);

	if (!GTK_CTREE_ROW(node)->expanded) {
		add_unread_mark = folderview_have_unread_children(
					folderview, item);
		add_sub_match_mark = folderview_have_matching_children(
					folderview, item);
	} else {
		add_unread_mark = FALSE;
		add_sub_match_mark = FALSE;
	}

	if (item->search_match) {
		if (!searchicon) {
			stock_pixmap_gdk(folderview->ctree, STOCK_PIXMAP_QUICKSEARCH,
			 &searchicon, &searchmask);
		}
		xpm = openxpm = searchicon;
		mask = openmask = searchmask;
	}

	str = NULL;
	if (prefs_common.display_folder_unread) {
		if (folder_has_parent_of_type(item, F_QUEUE)) {
			/* only total_msgs matters here */
			if (item->total_msgs > 0) {
				/* show total number (should be equal to the unread number)
				   and signs if any */
				str = g_strdup_printf("%s (%d%s%s)",
							name, item->total_msgs,
							(add_unread_mark || add_sub_match_mark) ? "+" : "", 
							(item->unreadmarked_msgs > 0) ? "!" : "");
			}
	} else {
			if (prefs_common.display_folder_unread == 1) {
				if (item->unread_msgs > 0) {
					/* show unread number and signs */
					str = g_strdup_printf("%s (%d%s%s)",
								name, item->unread_msgs,
								(add_unread_mark || add_sub_match_mark) ? "+" : "", 
								(item->unreadmarked_msgs > 0) ? "!" : "");
				}
			} else {
				if (item->total_msgs > 0) {
					/* show unread number, total number and signs if any */
					str = g_strdup_printf("%s (%d/%d%s%s)",
								name, item->unread_msgs, item->total_msgs,
								(add_unread_mark || add_sub_match_mark) ? "+" : "", 
								(item->unreadmarked_msgs > 0) ? "!" : "");
				}
			}
		}
		if ((str == NULL) &&
			(add_unread_mark || add_sub_match_mark || (item->unreadmarked_msgs > 0))) {
			/* no unread/total numbers, but at least one sign */
			str = g_strdup_printf("%s (%s%s)",
						name,
						(add_unread_mark || add_sub_match_mark) ? "+" : "", 
						(item->unreadmarked_msgs > 0) ? "!" : "");
		}
	}
	if (str == NULL) {
		/* last fallback, folder name only or with ! sign */
		str = g_strdup_printf("%s%s",
					name, (item->unreadmarked_msgs > 0) ? " (!)" : "");
	}
		gtk_sctree_set_node_info(ctree, node, str, FOLDER_SPACING,
					xpm, mask, openxpm, openmask,
					FALSE, GTK_CTREE_ROW(node)->expanded);
		g_free(str);
	g_free(name);

	if (!folder_item_parent(item)) {
		gtk_ctree_node_set_text(ctree, node, col_pos[F_COL_NEW],    "-");
		gtk_ctree_node_set_text(ctree, node, col_pos[F_COL_UNREAD], "-");
		gtk_ctree_node_set_text(ctree, node, col_pos[F_COL_TOTAL],  "-");
	} else {
		gtk_ctree_node_set_text(ctree, node, col_pos[F_COL_NEW],    itos(item->new_msgs));
		gtk_ctree_node_set_text(ctree, node, col_pos[F_COL_UNREAD], itos(item->unread_msgs));
		gtk_ctree_node_set_text(ctree, node, col_pos[F_COL_TOTAL],  itos(item->total_msgs));
	}

	if (folder_has_parent_of_type(item, F_OUTBOX) ||
	    folder_has_parent_of_type(item, F_DRAFT) ||
	    folder_has_parent_of_type(item, F_TRASH)) {
		use_bold = use_color = FALSE;
	} else if (folder_has_parent_of_type(item, F_QUEUE)) {
		/* highlight queue folder if there are any messages */
		use_bold = use_color = (item->total_msgs > 0);
	} else {
		/* if unread messages exist, print with bold font */
		use_bold = (item->unread_msgs > 0|| item->new_msgs > 0) 
				|| add_unread_mark;
		/* if new messages exist, print with colored letter */
		use_color =
			(item->new_msgs > 0) ||
			(add_unread_mark &&
			 folderview_have_new_children(folderview, item));	
	}

	gtk_ctree_node_set_foreground(ctree, node, NULL);

	if (use_bold) {
		GdkColor gdk_color;

		if (item->prefs->color > 0 && !use_color) {
			gtkut_convert_int_to_gdk_color(item->prefs->color, &gdk_color);
			color_style = gtk_style_copy(bold_style);
			color_style->fg[GTK_STATE_NORMAL] = gdk_color;
			style = color_style;
		} else if (use_color) {
			style = bold_color_style;
		} else
			style = bold_style;
		if (item->op_count > 0) {
			style = bold_tgtfold_style;
		}
	} else if (use_color) {
		style = normal_color_style;
		gtk_ctree_node_set_foreground(ctree, node,
					      &folderview->color_new);
	} else if (item->op_count > 0) {
		style = bold_tgtfold_style;
	} else if (item->prefs->color > 0) {
		GdkColor gdk_color;
		gtkut_convert_int_to_gdk_color(item->prefs->color, &gdk_color);
		color_style = gtk_style_copy(normal_style);
		color_style->fg[GTK_STATE_NORMAL] = gdk_color;
		style = color_style;
	} else {
		style = normal_style;
	}

	gtk_ctree_node_set_row_style(ctree, node, style);

	if ((node = gtkut_ctree_find_collapsed_parent(ctree, node)) != NULL)
		folderview_update_node(folderview, node);
}

void folderview_update_search_icon(FolderItem *item, gboolean matches)
{
	GList *list;
	FolderView *folderview;
	GtkCTree *ctree;
	GtkCTreeNode *node;

	g_return_if_fail(item != NULL);

	for (list = folderview_list; list != NULL; list = list->next) {
		folderview = (FolderView *)list->data;
		ctree = GTK_CTREE(folderview->ctree);

		node = gtk_ctree_find_by_row_data(ctree, NULL, item);
		if (node) {
			item->search_match = matches;
			folderview_update_node(folderview, node);
		}
	}
}

static gboolean folderview_update_item_claws(gpointer source, gpointer data)
{
	FolderItemUpdateData *update_info = (FolderItemUpdateData *)source;
	FolderView *folderview = (FolderView *)data;
	GtkCTree *ctree;
	GtkCTreeNode *node;
	g_return_val_if_fail(update_info != NULL, TRUE);
	g_return_val_if_fail(update_info->item != NULL, TRUE);
	g_return_val_if_fail(folderview != NULL, FALSE);

    	ctree = GTK_CTREE(folderview->ctree);

	node = gtk_ctree_find_by_row_data(ctree, NULL, update_info->item);

	if (node) {
		if (update_info->update_flags & (F_ITEM_UPDATE_MSGCNT | F_ITEM_UPDATE_NAME))
			folderview_update_node(folderview, node);

		if ((update_info->update_flags & F_ITEM_UPDATE_CONTENT) && 
		     update_info->item == folderview->summaryview->folder_item &&
		     update_info->item != NULL)
			if (!quicksearch_is_active(folderview->summaryview->quicksearch))
				summary_show(folderview->summaryview, update_info->item);
	}
	
	return FALSE;
}

static gboolean folderview_gnode_func(GtkCTree *ctree, guint depth,
				      GNode *gnode, GtkCTreeNode *cnode,
				      gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	FolderItem *item = FOLDER_ITEM(gnode->data);

	g_return_val_if_fail(item != NULL, FALSE);

	gtk_ctree_node_set_row_data(ctree, cnode, item);
	folderview_update_node(folderview, cnode);

	return TRUE;
}

static void folderview_expand_func(GtkCTree *ctree, GtkCTreeNode *node,
				   gpointer data)
{
	FolderView *folderview = (FolderView *)data;
	FolderItem *item;

	if (GTK_CTREE_ROW(node)->children) {
		item = gtk_ctree_node_get_row_data(ctree, node);
		g_return_if_fail(item != NULL);

		if (!item->collapsed)
			gtk_ctree_expand(ctree, node);
		else
			folderview_update_node(folderview, node);
	}
}

static void set_special_folder(GtkCTree *ctree, FolderItem *item,
			       GtkCTreeNode *root, GtkCTreeNode **prev)
{
	if (item) {
		GtkCTreeNode *node, *parent, *sibling;

		node = gtk_ctree_find_by_row_data(ctree, root, item);
		if (!node)
			g_warning("%s not found.\n", item->path);
		else {
			parent = GTK_CTREE_ROW(node)->parent;
			if (*prev && parent == GTK_CTREE_ROW(*prev)->parent)
				sibling = GTK_CTREE_ROW(*prev)->sibling;
			else
				sibling = GTK_CTREE_ROW(parent)->children;
			while (sibling) {
				FolderItem *tmp;

				tmp = gtk_ctree_node_get_row_data
					(ctree, sibling);
				if (tmp->stype != F_NORMAL)
					sibling = GTK_CTREE_ROW(sibling)->sibling;
				else
					break;
			}
			if (node != sibling)
				gtk_ctree_move(ctree, node, parent, sibling);
		}

		*prev = node;
	}
}

static void folderview_sort_folders(FolderView *folderview, GtkCTreeNode *root,
				    Folder *folder)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	GtkCTreeNode *prev = NULL;

	gtk_clist_freeze(GTK_CLIST(ctree));
	gtk_sctree_sort_recursive(ctree, root);
	if (root && GTK_CTREE_ROW(root)->parent) {
		gtk_clist_thaw(GTK_CLIST(ctree));
		return;
	}
	set_special_folder(ctree, folder->inbox, root, &prev);
	set_special_folder(ctree, folder->outbox, root, &prev);
	set_special_folder(ctree, folder->draft, root, &prev);
	set_special_folder(ctree, folder->queue, root, &prev);
	set_special_folder(ctree, folder->trash, root, &prev);
	gtk_clist_thaw(GTK_CLIST(ctree));
}

static void folderview_append_folder(FolderView *folderview, Folder *folder)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	GtkCTreeNode *root;

	g_return_if_fail(folder != NULL);

	root = gtk_sctree_insert_gnode(ctree, NULL, NULL, folder->node,
				      folderview_gnode_func, folderview);
	gtk_ctree_pre_recursive(ctree, root, folderview_expand_func,
				folderview);
	folderview_sort_folders(folderview, root, folder);
}

/* callback functions */
static void folderview_set_sens_and_popup_menu(FolderView *folderview, gint row, 
				GdkEventButton *event)
{
	GtkCList *clist = GTK_CLIST(folderview->ctree);
	FolderItem *item;
	Folder *folder;
	FolderViewPopup *fpopup;
	GtkItemFactory *fpopup_factory;
	GtkWidget *popup;
	FolderItem *special_trash = NULL, *special_queue = NULL;
	PrefsAccount *ac;

	if (row > 0)
		item = gtk_clist_get_row_data(clist, row);
	else
		item = folderview_get_selected_item(folderview);

	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);
	folder = item->folder;

	fpopup = g_hash_table_lookup(folderview_popups, folder->klass->idstr);
	if (fpopup != NULL)
		fpopup_factory = g_hash_table_lookup(folderview->popups, folder->klass->idstr);
	else {
		fpopup = g_hash_table_lookup(folderview_popups, "common");
		fpopup_factory = g_hash_table_lookup(folderview->popups, "common");
	}

	if (fpopup->set_sensitivity != NULL)
		fpopup->set_sensitivity(fpopup_factory, item);

	if (NULL != (ac = account_find_from_item(item))) {
		special_trash = account_get_special_folder(ac, F_TRASH);
		special_queue = account_get_special_folder(ac, F_QUEUE);
	}
	
	if ((item == folder->trash || item == special_trash
	     || folder_has_parent_of_type(item, F_TRASH)) &&
	    gtk_item_factory_get_item(fpopup_factory, "/Empty trash...") == NULL) {
		gtk_item_factory_create_item(fpopup_factory, &folder_view_trash_popup_entries[0], folderview, 1);
		gtk_item_factory_create_item(fpopup_factory, &folder_view_trash_popup_entries[1], folderview, 1);
	} else if (item != folder->trash && (special_trash == NULL || item != special_trash)
	        && !folder_has_parent_of_type(item, F_TRASH)) {
		gtk_item_factory_delete_entry(fpopup_factory, &folder_view_trash_popup_entries[0]);
		gtk_item_factory_delete_entry(fpopup_factory, &folder_view_trash_popup_entries[1]);
	}
	
	if ((item == folder->queue || item == special_queue
	     || folder_has_parent_of_type(item, F_QUEUE)) &&
	    gtk_item_factory_get_item(fpopup_factory, "/Send queue...") == NULL) {
		gtk_item_factory_create_item(fpopup_factory, &folder_view_queue_popup_entries[0], folderview, 1);
		gtk_item_factory_create_item(fpopup_factory, &folder_view_queue_popup_entries[1], folderview, 1);
	} else if (item != folder->queue && (special_queue == NULL || item != special_queue)
	        && !folder_has_parent_of_type(item, F_QUEUE)) {
		gtk_item_factory_delete_entry(fpopup_factory, &folder_view_queue_popup_entries[0]);
		gtk_item_factory_delete_entry(fpopup_factory, &folder_view_queue_popup_entries[1]);
	}
	
#define SET_SENS(name, sens) \
	menu_set_sensitive(fpopup_factory, name, sens)

	SET_SENS("/Mark all read", item->unread_msgs >= 1);
	SET_SENS("/Search folder...", item->total_msgs >= 1 && 
		 folderview->selected == folderview->opened);
	SET_SENS("/Properties...", TRUE);
	SET_SENS("/Processing...", item->node->parent != NULL);
	if (item == folder->trash || item == special_trash
	    || folder_has_parent_of_type(item, F_TRASH)) {
		GSList *msglist = folder_item_get_msg_list(item);
		SET_SENS("/Empty trash...", msglist != NULL);
		procmsg_msg_list_free(msglist);
	}
	if (item == folder->queue || item == special_queue
	    || folder_has_parent_of_type(item, F_QUEUE)) {
		GSList *msglist = folder_item_get_msg_list(item);
		SET_SENS("/Send queue...", msglist != NULL);
		procmsg_msg_list_free(msglist);
	}
#undef SET_SENS

	popup = gtk_item_factory_get_widget(fpopup_factory, fpopup->path);
	gtk_menu_popup(GTK_MENU(popup), NULL, NULL, NULL, NULL,
		       event->button, event->time);


}

static gboolean folderview_button_pressed(GtkWidget *ctree, GdkEventButton *event,
					  FolderView *folderview)
{
	GtkCList *clist = GTK_CLIST(ctree);
	gint prev_row = -1, row = -1, column = -1;

	if (!event) return FALSE;

	if (event->button == 1 || event->button == 2) {
		if (!gtk_ctree_is_hot_spot (GTK_CTREE(clist), event->x, event->y))
			folderview->open_folder = TRUE;

	        if (event->type == GDK_2BUTTON_PRESS) {
			if (clist->selection) {
				GtkCTreeNode *node;

				node = GTK_CTREE_NODE(clist->selection->data);
				if (node) {
					gtk_ctree_toggle_expansion(
						GTK_CTREE(ctree),
						node);
					folderview->open_folder = FALSE;
				}
			}
		}
		return FALSE;
	}

	if (event->button == 2 || event->button == 3) {
		/* right clicked */
		if (clist->selection) {
			GtkCTreeNode *node;

			node = GTK_CTREE_NODE(clist->selection->data);
			if (node)
				prev_row = gtkut_ctree_get_nth_from_node
					(GTK_CTREE(ctree), node);
		}

		if (!gtk_clist_get_selection_info(clist, event->x, event->y,
						  &row, &column))
			return FALSE;
		if (prev_row != row) {
			gtk_clist_unselect_all(clist);
			if (event->button == 2)
				folderview_select_node
					(folderview,
					 gtk_ctree_node_nth(GTK_CTREE(ctree),
					 		    row));
			else
				gtk_clist_select_row(clist, row, column);
		}
	}

	if (event->button != 3) return FALSE;

	folderview_set_sens_and_popup_menu(folderview, row, event);
	return FALSE;
}

static gboolean folderview_button_released(GtkWidget *ctree, GdkEventButton *event,
					   FolderView *folderview)
{
	if (!event) return FALSE;

	if (event->button == 1 && folderview->open_folder == FALSE &&
	    folderview->opened != NULL) {
		gtkut_ctree_set_focus_row(GTK_CTREE(ctree),
					  folderview->opened);
		gtk_ctree_select(GTK_CTREE(ctree), folderview->opened);
	}

	return FALSE;
}

static gboolean folderview_key_pressed(GtkWidget *widget, GdkEventKey *event,
				       FolderView *folderview)
{
	if (!event) return FALSE;

	if (quicksearch_has_focus(folderview->summaryview->quicksearch))
		return FALSE;

	switch (event->keyval) {
	case GDK_Return:
		if (folderview->selected) {
			folderview_select_node(folderview,
					       folderview->selected);
		}
		break;
	case GDK_space:
		if (folderview->selected) {
			if (folderview->opened == folderview->selected &&
			    (!folderview->summaryview->folder_item ||
			     folderview->summaryview->folder_item->total_msgs == 0))
				folderview_select_next_unread(folderview, TRUE);
			else
				folderview_select_node(folderview,
						       folderview->selected);
		}
		break;
	default:
		break;
	}

	return FALSE;
}

typedef struct _PostponedSelectData
{
	GtkCTree *ctree;
	GtkCTreeNode *row;
	gint column;
	FolderView *folderview;
} PostponedSelectData;

static gboolean postpone_select(void *data)
{
	PostponedSelectData *psdata = (PostponedSelectData *)data;
	debug_print("trying again\n");
	psdata->folderview->open_folder = TRUE;
	main_window_cursor_normal(psdata->folderview->mainwin);
	STATUSBAR_POP(psdata->folderview->mainwin);
	folderview_selected(psdata->ctree, psdata->row,
			    psdata->column, psdata->folderview);
	g_free(psdata);
	return FALSE;
}

static void folderview_selected(GtkCTree *ctree, GtkCTreeNode *row,
				gint column, FolderView *folderview)
{
	static gboolean can_select = TRUE;	/* exclusive lock */
	gboolean opened;
	FolderItem *item;
	gchar *buf;
	int res = 0;
	START_TIMING("");
	folderview->selected = row;

	debug_print("newly selected %p, opened %p\n", folderview->selected, 
			folderview->opened);
	if (folderview->opened == row) {
		folderview->open_folder = FALSE;
		END_TIMING();
		return;
	}
	
	if (!can_select || summary_is_locked(folderview->summaryview)) {
		if (folderview->opened) {
			gtkut_ctree_set_focus_row(ctree, folderview->opened);
			gtk_ctree_select(ctree, folderview->opened);
		}
		folderview->open_folder = FALSE;
		END_TIMING();
		return;
	}

	if (!folderview->open_folder) {
		END_TIMING();
		return;
	}
	item = gtk_ctree_node_get_row_data(ctree, row);
	if (!item || item->no_select) {
		END_TIMING();
		folderview->open_folder = FALSE;
		return;
	}

	can_select = FALSE;

	/* Save cache for old folder */
	/* We don't want to lose all caches if sylpheed crashed */
	if (folderview->opened) {
		FolderItem *olditem;
		
		olditem = gtk_ctree_node_get_row_data(ctree, folderview->opened);
		if (olditem) {
			buf = g_strdup_printf(_("Closing Folder %s..."), 
				olditem->path ? olditem->path:olditem->name);
			/* will be null if we just moved the previously opened folder */
			STATUSBAR_PUSH(folderview->mainwin, buf);
			main_window_cursor_wait(folderview->mainwin);
			g_free(buf);
			summary_save_prefs_to_folderitem(folderview->summaryview, olditem);
			summary_show(folderview->summaryview, NULL);
			folder_item_close(olditem);
			main_window_cursor_normal(folderview->mainwin);
			STATUSBAR_POP(folderview->mainwin);
		}
	}

	/* CLAWS: set compose button type: news folder items 
	 * always have a news folder as parent */
	if (item->folder) 
		toolbar_set_compose_button
			(folderview->mainwin->toolbar,
			 FOLDER_TYPE(item->folder) == F_NEWS ? 
			 COMPOSEBUTTON_NEWS : COMPOSEBUTTON_MAIL);

	if (item->path)
		debug_print("Folder %s is selected\n", item->path);

	if (!GTK_CTREE_ROW(row)->children)
		gtk_ctree_expand(ctree, row);
	if (folderview->opened &&
	    !GTK_CTREE_ROW(folderview->opened)->children)
		gtk_ctree_collapse(ctree, folderview->opened);

	/* ungrab the mouse event */
	if (GTK_WIDGET_HAS_GRAB(ctree)) {
		gtk_grab_remove(GTK_WIDGET(ctree));
		if (gdk_pointer_is_grabbed())
			gdk_pointer_ungrab(GDK_CURRENT_TIME);
	}

	/* Open Folder */
    	buf = g_strdup_printf(_("Opening Folder %s..."), item->path ? 
					item->path : "(null)");
	debug_print("%s\n", buf);
	STATUSBAR_PUSH(folderview->mainwin, buf);
	g_free(buf);

	main_window_cursor_wait(folderview->mainwin);

	res = folder_item_open(item);
	if (res == -1) {
		main_window_cursor_normal(folderview->mainwin);
		STATUSBAR_POP(folderview->mainwin);

		alertpanel_error(_("Folder could not be opened."));

		folderview->open_folder = FALSE;
		can_select = TRUE;
		END_TIMING();
		return;
        } else if (res == -2) {
		PostponedSelectData *data = g_new0(PostponedSelectData, 1);
		data->ctree = ctree;
		data->row = row;
		data->column = column;
		data->folderview = folderview;
		debug_print("postponing open of %s till end of scan\n",
			item->path ? item->path:item->name);
		folderview->open_folder = FALSE;
		can_select = TRUE;
		g_timeout_add(500, postpone_select, data);
		END_TIMING();
		return;
	}
	
	main_window_cursor_normal(folderview->mainwin);

	/* Show messages */
	summary_set_prefs_from_folderitem(folderview->summaryview, item);
	opened = summary_show(folderview->summaryview, item);
	
	folder_clean_cache_memory(item);

	if (!opened) {
		gtkut_ctree_set_focus_row(ctree, folderview->opened);
		gtk_ctree_select(ctree, folderview->opened);
	} else {
		folderview->opened = row;
		if (gtk_ctree_node_is_visible(ctree, row)
		    != GTK_VISIBILITY_FULL)
			gtk_ctree_node_moveto(ctree, row, -1, 0.5, 0);
	}

	STATUSBAR_POP(folderview->mainwin);

	folderview->open_folder = FALSE;
	can_select = TRUE;
	END_TIMING();
}

static void folderview_tree_expanded(GtkCTree *ctree, GtkCTreeNode *node,
				     FolderView *folderview)
{
	FolderItem *item;

	item = gtk_ctree_node_get_row_data(ctree, node);
	g_return_if_fail(item != NULL);
	item->collapsed = FALSE;
	folderview_update_node(folderview, node);
}

static void folderview_tree_collapsed(GtkCTree *ctree, GtkCTreeNode *node,
				      FolderView *folderview)
{
	FolderItem *item;

	item = gtk_ctree_node_get_row_data(ctree, node);
	g_return_if_fail(item != NULL);
	item->collapsed = TRUE;
	folderview_update_node(folderview, node);
}

static void folderview_popup_close(GtkMenuShell *menu_shell,
				   FolderView *folderview)
{
	if (!folderview->opened) return;

	gtk_ctree_select(GTK_CTREE(folderview->ctree), folderview->opened);
}

static void folderview_col_resized(GtkCList *clist, gint column, gint width,
				   FolderView *folderview)
{
	FolderColumnType type = folderview->col_state[column].type;

	prefs_common.folder_col_size[type] = width;
}

static void folderview_create_folder_node_recursive(FolderView *folderview, FolderItem *item)
{
	GNode *srcnode;

	folderview_create_folder_node(folderview, item);

	if (!item || !item->folder || !item->folder->node)
		return;

	srcnode = item->folder->node;	
	srcnode = g_node_find(srcnode, G_PRE_ORDER, G_TRAVERSE_ALL, item);
	srcnode = srcnode->children;
	while (srcnode != NULL) {
		if (srcnode && srcnode->data) {
			FolderItem *next_item = (FolderItem*) srcnode->data;
			folderview_create_folder_node_recursive(folderview, next_item);
		}
		srcnode = srcnode->next;
	}
}

static void folderview_create_folder_node(FolderView *folderview, FolderItem *item)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	gchar *text[N_FOLDER_COLS] = {NULL, "0", "0", "0"};
	GtkCTreeNode *node, *parent_node;
	gint *col_pos = folderview->col_pos;
	FolderItemUpdateData hookdata;

	parent_node = gtk_ctree_find_by_row_data(ctree, NULL, folder_item_parent(item));
	if (parent_node == NULL)
		return;

	gtk_clist_freeze(GTK_CLIST(ctree));

	text[col_pos[F_COL_FOLDER]] = item->name;
	node = gtk_sctree_insert_node(ctree, parent_node, NULL, text,
				     FOLDER_SPACING,
				     folderxpm, folderxpmmask,
				     folderopenxpm, folderopenxpmmask,
				     FALSE, FALSE);
	gtk_ctree_expand(ctree, parent_node);
	gtk_ctree_node_set_row_data(ctree, node, item);
	if (normal_style)
		gtk_ctree_node_set_row_style(ctree, node, normal_style);
	folderview_sort_folders(folderview, parent_node, item->folder);

	hookdata.item = item;
	hookdata.update_flags = F_ITEM_UPDATE_NAME;
	hookdata.msg = NULL;
	hooks_invoke(FOLDER_ITEM_UPDATE_HOOKLIST, &hookdata);

	gtk_clist_thaw(GTK_CLIST(ctree));
}

static void folderview_empty_trash_cb(FolderView *folderview, guint action,
				      GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	GSList *mlist = NULL;
	GSList *cur = NULL;
	FolderItem *special_trash = NULL;
	PrefsAccount *ac;

	if (!folderview->selected) return;
	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	if (NULL != (ac = account_find_from_item(item)))
		special_trash = account_get_special_folder(ac, F_TRASH);

	if (item != item->folder->trash && item != special_trash
	&&  !folder_has_parent_of_type(item, F_TRASH)) return;
	
	if (prefs_common.ask_on_clean) {
		if (alertpanel(_("Empty trash"),
			       _("Delete all messages in trash?"),
			       GTK_STOCK_CANCEL, _("+_Empty trash"), NULL) != G_ALERTALTERNATE)
			return;
	}
	
	mlist = folder_item_get_msg_list(item);
	
	for (cur = mlist ; cur != NULL ; cur = cur->next) {
		MsgInfo * msginfo = (MsgInfo *) cur->data;
		if (MSG_IS_LOCKED(msginfo->flags))
			continue;
		/* is it partially received? (partial_recv isn't cached) */
		if (msginfo->total_size != 0 && 
		    msginfo->size != (off_t)msginfo->total_size)
			partial_mark_for_delete(msginfo);
	}
	procmsg_msg_list_free(mlist);

	folder_item_remove_all_msg(item);
}

static void folderview_send_queue_cb(FolderView *folderview, guint action,
				      GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	FolderItem *special_queue = NULL;
	PrefsAccount *ac;
	gchar *errstr = NULL;

	if (!folderview->selected) return;
	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	if (NULL != (ac = account_find_from_item(item)))
		special_queue = account_get_special_folder(ac, F_QUEUE);

	if (item != item->folder->queue && item != special_queue
	&&  !folder_has_parent_of_type(item, F_QUEUE)) return;
	
	if (procmsg_queue_is_empty(item))
		return;

	if (prefs_common.work_offline)
		if (alertpanel(_("Offline warning"), 
			       _("You're working offline. Override?"),
			       GTK_STOCK_NO, GTK_STOCK_YES,
			       NULL) != G_ALERTALTERNATE)
		return;

	/* ask for confirmation before sending queued messages only
	   in online mode and if there is at least one message queued
	   in any of the folder queue
	*/
	if (prefs_common.confirm_send_queued_messages) {
		if (!prefs_common.work_offline) {
			if (alertpanel(_("Send queued messages"), 
			    	   _("Send all queued messages?"),
			    	   GTK_STOCK_CANCEL, _("_Send"),
				   NULL) != G_ALERTALTERNATE)
				return;
		}
	}

	if (procmsg_send_queue(item, prefs_common.savemsg, &errstr) < 0) {
		if (!errstr)
			alertpanel_error_log(_("Some errors occurred while "
					   "sending queued messages."));
		else {
			gchar *tmp = g_strdup_printf(_("Some errors occurred "
					"while sending queued messages:\n%s"), errstr);
			g_free(errstr);
			alertpanel_error_log(tmp);
			g_free(tmp);
		}
	}
}

static void folderview_search_cb(FolderView *folderview, guint action,
				 GtkWidget *widget)
{
	summary_search(folderview->summaryview);
}

static void folderview_property_cb(FolderView *folderview, guint action,
				   GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	prefs_folder_item_open(item);
}

static void folderview_recollapse_nodes(FolderView *folderview, GtkCTreeNode *node)
{
	GSList *list = NULL;
	GSList *done = NULL;
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	
	for (list = folderview->nodes_to_recollapse; list != NULL; list = g_slist_next(list)) {
		if (!gtkut_ctree_node_is_parent(GTK_CTREE_NODE(list->data), node)
		&&  list->data != node) {
			gtk_ctree_collapse(ctree, GTK_CTREE_NODE(list->data));
			done = g_slist_append(done, GTK_CTREE_NODE(list->data));
		}
	}
	for (list = done; list != NULL; list = g_slist_next(list)) {
		folderview->nodes_to_recollapse = g_slist_remove(folderview->nodes_to_recollapse, 
								 list->data);
	}
	g_slist_free(done);
}

void folderview_move_folder(FolderView *folderview, FolderItem *from_folder,
		            FolderItem *to_folder, gboolean copy)
{
	FolderItem *from_parent = NULL;
	FolderItem *new_folder = NULL;
	GtkCTreeNode *src_node = NULL;
	gchar *buf;
	gint status;

	g_return_if_fail(folderview != NULL);
	g_return_if_fail(from_folder != NULL);
	g_return_if_fail(to_folder != NULL);

	src_node = gtk_ctree_find_by_row_data(GTK_CTREE(folderview->ctree), NULL, from_folder);
	from_parent = folder_item_parent(from_folder);
	
	if (prefs_common.warn_dnd) {
		buf = g_strdup_printf(copy ? _("Do you really want to copy folder '%s' in '%s' ?"):
					     _("Do you really want to make folder '%s' a sub-folder of '%s' ?"), 
					from_folder->name, to_folder->name);
		status = alertpanel_full(copy ? _("Copy folder"):_("Move folder"), buf,
				       	 GTK_STOCK_NO, GTK_STOCK_YES, NULL, TRUE,
				       	 NULL, ALERT_QUESTION, G_ALERTDEFAULT);
		g_free(buf);

		if ((status & ~G_ALERTDISABLE) != G_ALERTALTERNATE)
			return;
		else if (status & G_ALERTDISABLE)
			prefs_common.warn_dnd = FALSE;
	}

	buf = g_strdup_printf(copy ? _("Copying %s to %s..."):_("Moving %s to %s..."), 
				from_folder->name, to_folder->name);
	STATUSBAR_PUSH(folderview->mainwin, buf);
	g_free(buf);
	summary_clear_all(folderview->summaryview);
	folderview->opened = NULL;
	folderview->selected = NULL;
	gtk_widget_set_sensitive(GTK_WIDGET(folderview->ctree), FALSE);
	inc_lock();
	main_window_cursor_wait(folderview->mainwin);

	statusbar_verbosity_set(FALSE);
	folder_item_update_freeze();
	if ((status = folder_item_move_to(from_folder, to_folder, &new_folder, copy)) == F_MOVE_OK) {
		statusbar_verbosity_set(FALSE);
		main_window_cursor_normal(folderview->mainwin);
		STATUSBAR_POP(folderview->mainwin);
		folder_item_update_thaw();
		folder_item_update_recursive(new_folder, F_ITEM_UPDATE_MSGCNT);

		folderview_sort_folders(folderview, 
			gtk_ctree_find_by_row_data(GTK_CTREE(folderview->ctree), 
				NULL, to_folder), new_folder->folder);
		folderview_select(folderview, new_folder);
	} else {
		statusbar_verbosity_set(FALSE);		
		main_window_cursor_normal(folderview->mainwin);
		STATUSBAR_POP(folderview->mainwin);
		folder_item_update_thaw();
		switch (status) {
		case F_MOVE_FAILED_DEST_IS_PARENT:
			alertpanel_error(_("Source and destination are the same."));
			break;
		case F_MOVE_FAILED_DEST_IS_CHILD:
			alertpanel_error(copy ? _("Can't copy a folder to one of its children."):
						_("Can't move a folder to one of its children."));
			break;
		case F_MOVE_FAILED_DEST_OUTSIDE_MAILBOX:
			alertpanel_error(_("Folder moving cannot be done between different mailboxes."));
			break;
		default:
			alertpanel_error(copy ? _("Copy failed!"):_("Move failed!"));
			break;
		}
	}	
	inc_unlock();		
	gtk_widget_set_sensitive(GTK_WIDGET(folderview->ctree), TRUE);
}

static gint folderview_clist_compare(GtkCList *clist,
				     gconstpointer ptr1, gconstpointer ptr2)
{
	FolderItem *item1 = ((GtkCListRow *)ptr1)->data;
	FolderItem *item2 = ((GtkCListRow *)ptr2)->data;

	if (!item1->name)
		return (item2->name != NULL);
	if (!item2->name)
		return -1;

	return g_utf8_collate(item1->name, item2->name);
}

static void folderview_processing_cb(FolderView *folderview, guint action,
				     GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(folderview->ctree);
	FolderItem *item;
	gchar *id, *title;

	if (!folderview->selected) return;

	item = gtk_ctree_node_get_row_data(ctree, folderview->selected);
	g_return_if_fail(item != NULL);
	g_return_if_fail(item->folder != NULL);

	id = folder_item_get_identifier(item);
	title = g_strdup_printf (_("Processing configuration for folder %s"), id);
	g_free (id);

	prefs_filtering_open(&item->prefs->processing, title,
			MANUAL_ANCHOR_PROCESSING, NULL, NULL, FALSE);
	g_free (title);
}

void folderview_set_target_folder_color(gint color_op) 
{
	gint firstone = 1;
	GList *list;
	FolderView *folderview;

	for (list = folderview_list; list != NULL; list = list->next) {
		folderview = (FolderView *)list->data;
		gtkut_convert_int_to_gdk_color(color_op, &folderview->color_op);
		if (firstone) {
			bold_tgtfold_style->fg[GTK_STATE_NORMAL] =
				folderview->color_op;
			firstone = 0;
		}
	}
}

static gchar *last_font = NULL;
void folderview_reflect_prefs_pixmap_theme(FolderView *folderview)
{
	/* force reinit */
	g_free(last_font);
	last_font = NULL;
	
}

void folderview_reflect_prefs(void)
{
	gboolean update_font = TRUE;
	FolderView *folderview = mainwindow_get_mainwindow()->folderview;
	FolderItem *item = folderview_get_selected_item(folderview);
	GtkAdjustment *pos = gtk_scrolled_window_get_vadjustment(
				GTK_SCROLLED_WINDOW(folderview->scrolledwin));
	gint height = pos->value;

	if (last_font && !strcmp(last_font, NORMAL_FONT))
		update_font = FALSE;

	g_free(last_font);
	last_font = g_strdup(NORMAL_FONT);

	if (update_font) {		
		normal_style = normal_color_style = bold_style = 
			bold_color_style = bold_tgtfold_style = NULL;

		folderview_init(folderview);
	}
	gtk_clist_freeze(GTK_CLIST(folderview->ctree));
	folderview_column_set_titles(folderview);
	folderview_set_all();

	g_signal_handlers_block_by_func
		(G_OBJECT(folderview->ctree),
		 G_CALLBACK(folderview_selected), folderview);

	if (item) {
		GtkCTreeNode *node = gtk_ctree_find_by_row_data(
			GTK_CTREE(folderview->ctree), NULL, item);

		folderview_select(folderview, item);
		folderview->open_folder = FALSE;
		folderview->selected = node;
	}

	g_signal_handlers_unblock_by_func
		(G_OBJECT(folderview->ctree),
		 G_CALLBACK(folderview_selected), folderview);

	pos = gtk_scrolled_window_get_vadjustment(
				GTK_SCROLLED_WINDOW(folderview->scrolledwin));
	gtk_adjustment_set_value(pos, height);
	gtk_clist_thaw(GTK_CLIST(folderview->ctree));
}

static void drag_state_stop(FolderView *folderview)
{
	if (folderview->drag_timer)
		g_source_remove(folderview->drag_timer);
	folderview->drag_timer = 0;
	folderview->drag_node = NULL;
}

static gint folderview_defer_expand(FolderView *folderview)
{
	if (folderview->drag_node) {
		folderview_recollapse_nodes(folderview, folderview->drag_node);
		if (folderview->drag_item->collapsed) {
			gtk_ctree_expand(GTK_CTREE(folderview->ctree), folderview->drag_node);
			folderview->nodes_to_recollapse = g_slist_append
				(folderview->nodes_to_recollapse, folderview->drag_node);
		}
	}
	folderview->drag_item  = NULL;
	folderview->drag_timer = 0;
	return FALSE;
}

static void drag_state_start(FolderView *folderview, GtkCTreeNode *node, FolderItem *item)
{
	/* the idea is that we call drag_state_start() whenever we want expansion to
	 * start after 'prefs_common.hover_time' msecs. if we want to cancel expansion,
	 * we need to call drag_state_stop() */
	drag_state_stop(folderview);
	/* request expansion */
	if (0 != (folderview->drag_timer = g_timeout_add
			(prefs_common.hover_timeout, 
			 (GtkFunction)folderview_defer_expand,
			 folderview))) {
		folderview->drag_node = node;
		folderview->drag_item = item;
	}			 
}

static void folderview_start_drag(GtkWidget *widget, gint button, GdkEvent *event,
			          FolderView       *folderview)
{
	GdkDragContext *context;

	g_return_if_fail(folderview != NULL);
	if (folderview->selected == NULL) return;
	if (folderview->nodes_to_recollapse) 
		g_slist_free(folderview->nodes_to_recollapse);
	folderview->nodes_to_recollapse = NULL;
	context = gtk_drag_begin(widget, folderview->target_list,
				 GDK_ACTION_MOVE|GDK_ACTION_COPY|GDK_ACTION_DEFAULT, button, event);
	gtk_drag_set_icon_default(context);
}

static void folderview_drag_data_get(GtkWidget        *widget,
				     GdkDragContext   *drag_context,
				     GtkSelectionData *selection_data,
				     guint             info,
				     guint             time,
				     FolderView       *folderview)
{
	FolderItem *item;
	GList *cur;
	gchar *source = NULL;
	if (info == TARGET_DUMMY) {
		for (cur = GTK_CLIST(folderview->ctree)->selection;
		     cur != NULL; cur = cur->next) {
			item = gtk_ctree_node_get_row_data
				(GTK_CTREE(folderview->ctree), 
				 GTK_CTREE_NODE(cur->data));
			if (item) {
				source = g_strdup_printf ("FROM_OTHER_FOLDER%s", folder_item_get_identifier(item));
				gtk_selection_data_set(selection_data,
						       selection_data->target, 8,
						       source, strlen(source));
				break;
			} else
				return;
		}
	} else {
		g_warning("unknown info %d\n", info);
	}
}

static gboolean folderview_update_folder(gpointer source, gpointer userdata)
{
	FolderUpdateData *hookdata;
	FolderView *folderview;
	GtkWidget *ctree;

	hookdata = source;
	folderview = (FolderView *) userdata;	
	g_return_val_if_fail(hookdata != NULL, FALSE);
	g_return_val_if_fail(folderview != NULL, FALSE);

	ctree = folderview->ctree;
	g_return_val_if_fail(ctree != NULL, FALSE);

	if (hookdata->update_flags & FOLDER_ADD_FOLDERITEM)
		folderview_create_folder_node(folderview, hookdata->item);
	else if (hookdata->update_flags & FOLDER_RENAME_FOLDERITEM) {
		GtkCTreeNode *node = gtk_ctree_find_by_row_data(GTK_CTREE(ctree),
				NULL, folder_item_parent(hookdata->item));
		folderview_sort_folders(folderview, node, hookdata->folder);
	} else if (hookdata->update_flags & FOLDER_REMOVE_FOLDERITEM) {
		GtkCTreeNode *node;

		node = gtk_ctree_find_by_row_data(GTK_CTREE(ctree), NULL, hookdata->item);
		if (node != NULL) {
			gtk_ctree_remove_node(GTK_CTREE(ctree), node);
			if (folderview->selected == node)
				folderview->selected = NULL;
			if (folderview->opened == node)
				folderview->opened = NULL;
		}
	} else if (hookdata->update_flags & (FOLDER_TREE_CHANGED | FOLDER_ADD_FOLDER | FOLDER_REMOVE_FOLDER))
		folderview_set(folderview);

	return FALSE;
}

static gboolean folderview_drag_motion_cb(GtkWidget      *widget,
					  GdkDragContext *context,
					  gint            x,
					  gint            y,
					  guint           time,
					  FolderView     *folderview)
{
	gint row, column;
	FolderItem *item = NULL, *src_item = NULL;
	GtkCTreeNode *node = NULL;
	gboolean acceptable = FALSE;
	GtkAdjustment *pos = gtk_scrolled_window_get_vadjustment(
				GTK_SCROLLED_WINDOW(folderview->scrolledwin));
	int height = (int)pos->page_size;
	int total_height = (int)pos->upper;
	int vpos = (int) pos->value;

	if (gtk_clist_get_selection_info
		(GTK_CLIST(widget), x - 24, y - 24, &row, &column)) {
		GtkWidget *srcwidget;

		if (y > height - 24 && height + vpos < total_height)
			gtk_adjustment_set_value(pos, (vpos+5 > height ? height : vpos+5));

		if (y < 48 && y > 0)
			gtk_adjustment_set_value(pos, (vpos-5 < 0 ? 0 : vpos-5));

		node = gtk_ctree_node_nth(GTK_CTREE(widget), row);
		item = gtk_ctree_node_get_row_data(GTK_CTREE(widget), node);
		src_item = folderview->summaryview->folder_item;

		srcwidget = gtk_drag_get_source_widget(context);
		if (srcwidget == summary_get_main_widget(folderview->summaryview)) {
			/* comes from summaryview */
			/* we are copying messages, so only accept folder items that are not
			   the source item, are no root items and can copy messages */
			if (item && item->folder && folder_item_parent(item) != NULL && src_item &&
			    src_item != item && FOLDER_CLASS(item->folder)->copy_msg != NULL &&
			    FOLDER_TYPE(item->folder) != F_UNKNOWN)
				acceptable = TRUE;
		} else if (srcwidget == folderview->ctree) {
			/* comes from folderview */
			/* we are moving folder items, only accept folders that are not
                           the source items and can copy messages and create folder items */
			if (item && item->folder && src_item && src_item != item &&
			    FOLDER_CLASS(item->folder)->copy_msg != NULL &&
			    FOLDER_CLASS(item->folder)->create_folder != NULL &&
			    ((FOLDER_TYPE(item->folder) != F_UNKNOWN &&  FOLDER_TYPE(src_item->folder) != F_UNKNOWN)
			     || item->folder == src_item->folder))
				acceptable = TRUE;
		} else {
			/* comes from another app */
			/* we are adding messages, so only accept folder items that are 
			   no root items and can copy messages */
			if (item && item->folder && folder_item_parent(item) != NULL
			    && FOLDER_CLASS(item->folder)->add_msg != NULL &&
			    FOLDER_TYPE(item->folder) != F_UNKNOWN)
				acceptable = TRUE;
		}
	}

	if (acceptable || (src_item && src_item == item))
		drag_state_start(folderview, node, item);
	
	if (acceptable) {
		g_signal_handlers_block_by_func
			(G_OBJECT(widget),
			 G_CALLBACK(folderview_selected), folderview);
		gtk_ctree_select(GTK_CTREE(widget), node);
		g_signal_handlers_unblock_by_func
			(G_OBJECT(widget),
			 G_CALLBACK(folderview_selected), folderview);
		gdk_drag_status(context, 
					(context->actions == GDK_ACTION_COPY ?
					GDK_ACTION_COPY : GDK_ACTION_MOVE) , time);
	} else {
		if (folderview->opened)
			gtk_ctree_select(GTK_CTREE(widget), folderview->opened);
		gdk_drag_status(context, 0, time);
	}

	return acceptable;
}

static void folderview_drag_leave_cb(GtkWidget      *widget,
				     GdkDragContext *context,
				     guint           time,
				     FolderView     *folderview)
{
	drag_state_stop(folderview);
	gtk_ctree_select(GTK_CTREE(widget), folderview->opened);
}

static void free_info (gpointer stuff, gpointer data)
{
	g_free(stuff);
}

void folderview_finish_dnd(const gchar *data, GdkDragContext *drag_context,
			   guint time, FolderItem *item)
{
	GList *list, *tmp;
	GSList *msglist = NULL;
	list = uri_list_extract_filenames(data);
	if (!(item && item->folder && folder_item_parent(item) != NULL
		    && FOLDER_CLASS(item->folder)->add_msg != NULL))
	{
		gtk_drag_finish(drag_context, FALSE, FALSE, time);
		debug_print("item doesn't fit\n");			
		return;
	}	
	if (!list) {
		gtk_drag_finish(drag_context, FALSE, FALSE, time);
		debug_print("list is empty\n");			
		return;
	}
	for (tmp = list; tmp != NULL; tmp = tmp->next) {
		MsgFileInfo *info = NULL;

		if (file_is_email((gchar *)tmp->data)) {
			info = g_new0(MsgFileInfo, 1);
			info->msginfo = NULL;
			info->file = (gchar *)tmp->data;
			msglist = g_slist_prepend(msglist, info);
			debug_print("file is a mail\n");
		} else {
			debug_print("file isn't a mail\n");
		}
	}
	if (msglist) {
		msglist = g_slist_reverse(msglist);
		folder_item_add_msgs(item, msglist, FALSE);
		g_slist_foreach(msglist, free_info, NULL);
		g_slist_free(msglist);
		gtk_drag_finish(drag_context, TRUE, FALSE, time);
	} else {
		gtk_drag_finish(drag_context, FALSE, FALSE, time);			
	}
	list_free_strings(list);
	g_list_free(list);
}

static void folderview_drag_received_cb(GtkWidget        *widget,
					GdkDragContext   *drag_context,
					gint              x,
					gint              y,
					GtkSelectionData *data,
					guint             info,
					guint             time,
					FolderView       *folderview)
{
	gint row, column;
	FolderItem *item = NULL, *src_item;
	GtkCTreeNode *node;

	if (info == TARGET_DUMMY) {
		drag_state_stop(folderview);
		if ((void *)strstr(data->data, "FROM_OTHER_FOLDER") != (void *)data->data) {
			/* comes from summaryview */
			if (gtk_clist_get_selection_info
				(GTK_CLIST(widget), x - 24, y - 24, &row, &column) == 0)
				return;

			node = gtk_ctree_node_nth(GTK_CTREE(widget), row);
			item = gtk_ctree_node_get_row_data(GTK_CTREE(widget), node);
			src_item = folderview->summaryview->folder_item;

			/* re-check (due to acceptable possibly set for folder moves */
			if (!(item && item->folder && item->path && !item->no_select && 
			      src_item && src_item != item && FOLDER_CLASS(item->folder)->copy_msg != NULL)) {
				return;
			}
			if (item && src_item) {
				switch (drag_context->action) {
				case GDK_ACTION_COPY:
					summary_copy_selected_to(folderview->summaryview, item);
					gtk_drag_finish(drag_context, TRUE, FALSE, time);
					break;
				case GDK_ACTION_MOVE:
				case GDK_ACTION_DEFAULT:
				default:
					if (FOLDER_CLASS(src_item->folder)->remove_msg == NULL)
				        	summary_copy_selected_to(folderview->summaryview, item);
					else
						summary_move_selected_to(folderview->summaryview, item);
					gtk_drag_finish(drag_context, TRUE, TRUE, time);
				}
			} else
				gtk_drag_finish(drag_context, FALSE, FALSE, time);
		} else {
			/* comes from folderview */
			char *source;
			gboolean folder_is_normal = TRUE;
			gboolean copy = (drag_context->action == GDK_ACTION_COPY);

			source = data->data + 17;
			if (gtk_clist_get_selection_info
			    (GTK_CLIST(widget), x - 24, y - 24, &row, &column) == 0
			    || *source == 0) {
				gtk_drag_finish(drag_context, FALSE, FALSE, time);			
				return;
			}
			node = gtk_ctree_node_nth(GTK_CTREE(widget), row);
			item = gtk_ctree_node_get_row_data(GTK_CTREE(widget), node);
			src_item = folder_find_item_from_identifier(source);

			folder_is_normal = 
				src_item != NULL &&
				src_item->stype == F_NORMAL &&
				!folder_has_parent_of_type(src_item, F_OUTBOX) &&
				!folder_has_parent_of_type(src_item, F_DRAFT) &&
				!folder_has_parent_of_type(src_item, F_QUEUE) &&
				!folder_has_parent_of_type(src_item, F_TRASH);
			if (!item || item->no_select || !src_item 
			||  !folder_is_normal) {
				gtk_drag_finish(drag_context, FALSE, FALSE, time);			
				return;
			}

			folderview_move_folder(folderview, src_item, item, copy);
			gtk_drag_finish(drag_context, TRUE, TRUE, time);
		}
		folderview->nodes_to_recollapse = NULL;
	} else if (info == TARGET_MAIL_URI_LIST) {
		if (gtk_clist_get_selection_info
			(GTK_CLIST(widget), x - 24, y - 24, &row, &column) == 0)
			return;

		node = gtk_ctree_node_nth(GTK_CTREE(widget), row);
		if (!node) {
			gtk_drag_finish(drag_context, FALSE, FALSE, time);
			debug_print("no node\n");		
			return;
		}
		item = gtk_ctree_node_get_row_data(GTK_CTREE(widget), node);
		if (!item) {
			gtk_drag_finish(drag_context, FALSE, FALSE, time);			
			debug_print("no item\n");
			return;
		}
		folderview_finish_dnd(data->data, drag_context, time, item);
	}
}

static void folderview_drag_end_cb(GtkWidget	    *widget, 
				   GdkDragContext   *drag_context,
                                   FolderView	    *folderview)
{
	drag_state_stop(folderview);
	g_slist_free(folderview->nodes_to_recollapse);
	folderview->nodes_to_recollapse = NULL;
}

void folderview_register_popup(FolderViewPopup *fpopup)
{
	GList *folderviews;

	for (folderviews = folderview_list; folderviews != NULL; folderviews = g_list_next(folderviews)) {
		FolderView *folderview = folderviews->data;
		GtkItemFactory *factory;

		factory = create_ifactory(folderview, fpopup);
		g_hash_table_insert(folderview->popups, fpopup->klass, factory);
	}	
	g_hash_table_insert(folderview_popups, fpopup->klass, fpopup);
}

void folderview_unregister_popup(FolderViewPopup *fpopup)
{
	GList *folderviews;

	for (folderviews = folderview_list; folderviews != NULL; folderviews = g_list_next(folderviews)) {
		FolderView *folderview = folderviews->data;

		g_hash_table_remove(folderview->popups, fpopup->klass);
	}	
	g_hash_table_remove(folderview_popups, fpopup->klass);
}
