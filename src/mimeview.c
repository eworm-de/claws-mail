/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2024 the Claws Mail team and Hiroyuki Yamamoto
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include "defs.h"

#ifdef G_OS_WIN32
#define UNICODE
#define _UNICODE
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "main.h"
#include "mimeview.h"
#include "textview.h"
#include "procmime.h"
#include "summaryview.h"
#include "menu.h"
#include "filesel.h"
#include "alertpanel.h"
#include "inputdialog.h"
#include "utils.h"
#include "gtkutils.h"
#include "prefs_common.h"
#include "procheader.h"
#include "stock_pixmap.h"
#include "gtk/gtkvscrollbutton.h"
#include "gtk/logwindow.h"
#include "timing.h"
#include "manage_window.h"
#include "privacy.h"
#include "file-utils.h"

typedef enum
{
	COL_MIMETYPE = 0,
	COL_SIZE     = 1,
	COL_NAME     = 2,
	COL_DATA     = 3,
	N_MIMEVIEW_COLUMNS
} MimeViewColumnPos;

#define N_MIMEVIEW_COLS	(N_MIMEVIEW_COLUMNS)

static void mimeview_set_multipart_tree		(MimeView	*mimeview,
						 MimeInfo	*mimeinfo,
						 GtkTreeIter	*parent);
static void mimeview_show_message_part		(MimeView	*mimeview,
						 MimeInfo	*partinfo);
static void mimeview_change_view_type		(MimeView	*mimeview,
						 MimeViewType	 type);
static gchar *mimeview_get_filename_for_part	(MimeInfo	*partinfo,
						 const gchar	*basedir,
						 gint		 number);
static gint mimeview_write_part		(const gchar	*filename,
					 MimeInfo	*partinfo,
					 gboolean	 handle_error);

static void mimeview_selected		(GtkTreeSelection	*selection,
					 MimeView	*mimeview);
static gint mimeview_button_pressed	(GtkWidget	*widget,
					 GdkEventButton	*event,
					 MimeView	*mimeview);
static gint mimeview_key_pressed	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 MimeView	*mimeview);

static void mimeview_drag_data_get      (GtkWidget	  *widget,
					 GdkDragContext   *drag_context,
					 GtkSelectionData *selection_data,
					 guint		   info,
					 guint		   time,
					 MimeView	  *mimeview);

static gboolean mimeview_scrolled	(GtkWidget	*widget,
					 GdkEventScroll	*event,
					 MimeView	*mimeview);

static void mimeview_save_all		(MimeView	*mimeview,
					 gboolean	 attachments_only);
#ifndef G_OS_WIN32
static void mimeview_open_part_with	(MimeView	*mimeview,
					 MimeInfo	*partinfo,
					 gboolean	 automatic);
#endif
static void mimeview_send_to		(MimeView	*mimeview,
					 MimeInfo	*partinfo);
static void mimeview_view_file		(const gchar	*filename,
					 MimeInfo	*partinfo,
					 const gchar	*cmd,
					 MimeView  	*mimeview);
static gboolean icon_clicked_cb		(GtkWidget 	*button, 
					 GdkEventButton	*event, 
					 MimeView 	*mimeview);
static void icon_selected               (MimeView       *mimeview, 
					 gint            num, 
					 MimeInfo       *partinfo);
static gint icon_key_pressed            (GtkWidget      *button, 
					 GdkEventKey    *event,
					 MimeView       *mimeview);
static void icon_list_append_icon 	(MimeView 	*mimeview, 
					 MimeInfo 	*mimeinfo);
static void icon_list_create		(MimeView 	*mimeview, 
					 MimeInfo 	*mimeinfo);
static void icon_list_clear		(MimeView	*mimeview);
static void icon_list_toggle_by_mime_info (MimeView	*mimeview,
					   MimeInfo	*mimeinfo);
static void ctree_size_allocate_cb	(GtkWidget	*widget,
					 GtkAllocation	*allocation,
					 MimeView	*mimeview);
static gint mime_toggle_button_cb(GtkWidget *button, GdkEventButton *event,
				    MimeView *mimeview);
static gboolean part_button_pressed	(MimeView 	*mimeview, 
					 GdkEventButton *event, 
					 MimeInfo 	*partinfo);
static void icon_scroll_size_allocate_cb(GtkWidget 	*widget, 
					 GtkAllocation  *layout_size, 
					 MimeView 	*mimeview);
static MimeInfo *mimeview_get_part_to_use(MimeView *mimeview);
static const gchar *get_part_name(MimeInfo *partinfo);
static const gchar *get_part_description(MimeInfo *partinfo);

static void mimeview_launch_cb(GtkAction *action, gpointer data)
{
	MimeView *mimeview = (MimeView *)data;
	mimeview_launch(mimeview, mimeview_get_part_to_use(mimeview));
}

#ifndef G_OS_WIN32
static void mimeview_open_with_cb(GtkAction *action, gpointer data)
{
	mimeview_open_with((MimeView *)data);
}
#endif

static void mimeview_copy_cb(GtkAction *action, gpointer data)
{
	MimeView *mimeview = (MimeView *)data;
	MimeInfo *mimeinfo = mimeview_get_part_to_use(mimeview);

	if (mimeinfo == NULL)
		return;

	if (mimeinfo->type == MIMETYPE_IMAGE) {
		GError *error = NULL;
		GdkPixbuf *pixbuf = procmime_get_part_as_pixbuf(mimeinfo, &error);
		if (error != NULL) {
			g_warning("could not copy: %s", error->message);
			g_error_free(error);
		} else {
			gtk_clipboard_set_image(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),
						pixbuf);
		}
		g_object_unref(pixbuf);
	} else {
		void *data = procmime_get_part_as_string(mimeinfo, FALSE);
		gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),
				       data, (gint)mimeinfo->length);
		g_free(data);
	}
}

static void mimeview_send_to_cb(GtkAction *action, gpointer data)
{
	MimeView *mimeview = (MimeView *)data;	
	mimeview_send_to(mimeview, mimeview_get_part_to_use(mimeview));
}

static void mimeview_display_as_text_cb(GtkAction *action, gpointer data)
{
	mimeview_display_as_text((MimeView *)data);
}

static void mimeview_save_as_cb(GtkAction *action, gpointer data)
{
	mimeview_save_as((MimeView *)data);
}

static void mimeview_save_all_cb(GtkAction *action, gpointer data)
{
	mimeview_save_all((MimeView *)data, FALSE);
}

static void mimeview_save_all_attachments_cb(GtkAction *action, gpointer data)
{
	mimeview_save_all((MimeView *)data, TRUE);
}

static void mimeview_select_next_part_cb(GtkAction *action, gpointer data)
{
	mimeview_select_next_part((MimeView *)data);
}

static void mimeview_select_prev_part_cb(GtkAction *action, gpointer data)
{
	mimeview_select_prev_part((MimeView *)data);
}

static GtkActionEntry mimeview_menu_actions[] = {
	{ "MimeView", NULL, "MimeView", NULL, NULL, NULL },
	{ "MimeView/Open", NULL, N_("_Open"), NULL, "Open MIME part", G_CALLBACK(mimeview_launch_cb) },
#if (!defined G_OS_WIN32)
	{ "MimeView/OpenWith", NULL, N_("Open _with..."), NULL, "Open MIME part with...", G_CALLBACK(mimeview_open_with_cb) },
#endif
	{ "MimeView/Copy", NULL, N_("Copy"), NULL, "Copy", G_CALLBACK(mimeview_copy_cb) },
	{ "MimeView/SendTo", NULL, N_("Send to..."), NULL, "Send to", G_CALLBACK(mimeview_send_to_cb) },
	{ "MimeView/DisplayAsText", NULL, N_("_Display as text"), NULL, "Display as text", G_CALLBACK(mimeview_display_as_text_cb) },
	{ "MimeView/SaveAs", NULL, N_("_Save as..."), NULL, "Save as", G_CALLBACK(mimeview_save_as_cb) },
	{ "MimeView/SaveAll", NULL, N_("Save _all..."), NULL, "Save all parts", G_CALLBACK(mimeview_save_all_cb) },
	{ "MimeView/SaveAllAttachments", NULL, N_("Save all attachments..."), NULL, "Save all attachments", G_CALLBACK(mimeview_save_all_attachments_cb) },
	{ "MimeView/NextPart", NULL, N_("Next part"), NULL, "Next part", G_CALLBACK(mimeview_select_next_part_cb) },
	{ "MimeView/PrevPart", NULL, N_("Previous part"), NULL, "Previous part", G_CALLBACK(mimeview_select_prev_part_cb) }
};

static GtkTargetEntry mimeview_mime_types[] =
{
	{"text/uri-list", 0, 0}
};

GSList *mimeviewer_factories;
GSList *mimeviews;

static GdkCursor *hand_cursor = NULL;

static gboolean mimeview_visi_notify(GtkWidget *widget,
				       GdkEventVisibility *event,
				       MimeView *mimeview)
{
	gdk_window_set_cursor(gtk_widget_get_window(widget), hand_cursor);
	return FALSE;
}

static gboolean mimeview_leave_notify(GtkWidget *widget,
				      GdkEventCrossing *event,
				      MimeView *mimeview)
{
	gdk_window_set_cursor(gtk_widget_get_window(widget), NULL);
	return FALSE;
}

static gboolean mimeview_enter_notify(GtkWidget *widget,
				      GdkEventCrossing *event,
				      MimeView *mimeview)
{
	gdk_window_set_cursor(gtk_widget_get_window(widget), hand_cursor);
	return FALSE;
}

MimeView *mimeview_create(MainWindow *mainwin)
{
	MimeView *mimeview;

	GtkWidget *paned;
	GtkWidget *scrolledwin;
	GtkWidget *ctree;
	GtkWidget *mime_notebook;
	GtkWidget *popupmenu;
	GtkWidget *ctree_mainbox;
	GtkWidget *vbox;
	GtkWidget *mime_toggle;
	GtkWidget *icon_mainbox;
	GtkWidget *icon_scroll;
	GtkWidget *icon_grid;
	GtkWidget *arrow;
	GtkWidget *scrollbutton;
	GtkWidget *hbox;
	NoticeView *siginfoview;
	GtkTreeStore *model;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;

	gchar *titles[N_MIMEVIEW_COLS];
	gint cols;

	if (!hand_cursor)
		hand_cursor = gdk_cursor_new_for_display(
				gtk_widget_get_display(mainwin->window), GDK_HAND2);

	debug_print("Creating MIME view...\n");
	mimeview = g_new0(MimeView, 1);

	titles[COL_MIMETYPE] = _("MIME Type");
	titles[COL_SIZE]     = _("Size");
	titles[COL_NAME]     = _("Name");

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwin);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
				       
	model = gtk_tree_store_new(N_MIMEVIEW_COLS,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_POINTER);

	ctree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(model));
	g_object_unref(model);
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(ctree), FALSE);
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(ctree),
					prefs_common.show_col_headers);

	renderer = gtk_cell_renderer_text_new();
	cols = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(ctree),
					-1, titles[COL_MIMETYPE],renderer,
					"text", COL_MIMETYPE, NULL);
	column = gtk_tree_view_get_column(GTK_TREE_VIEW(ctree), cols-1);
							   
	renderer = gtk_cell_renderer_text_new();
	gtk_cell_renderer_set_alignment(renderer, 1, 0.5);
	cols = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(ctree),
					-1, titles[COL_SIZE], renderer,
					"text", COL_SIZE, NULL);
	column = gtk_tree_view_get_column(GTK_TREE_VIEW(ctree), cols-1);
	gtk_tree_view_column_set_alignment(GTK_TREE_VIEW_COLUMN(column), 1);
	
	renderer = gtk_cell_renderer_text_new();
	cols = gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(ctree),
					-1, titles[COL_NAME], renderer,
					"text", COL_NAME, NULL);
	column = gtk_tree_view_get_column(GTK_TREE_VIEW(ctree), cols-1);
	gtk_tree_view_column_set_expand(GTK_TREE_VIEW_COLUMN(column), TRUE);

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ctree));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);

	gtk_widget_show(ctree);
	gtk_container_add(GTK_CONTAINER(scrolledwin), ctree);
	gtk_drag_source_set(ctree, GDK_BUTTON1_MASK|GDK_BUTTON3_MASK, 
			    mimeview_mime_types, 1, GDK_ACTION_COPY);

	g_signal_connect(G_OBJECT(selection), "changed",
			 G_CALLBACK(mimeview_selected), mimeview);
	g_signal_connect(G_OBJECT(ctree), "button_release_event",
			 G_CALLBACK(mimeview_button_pressed), mimeview);
	g_signal_connect(G_OBJECT(ctree), "key_press_event",
			 G_CALLBACK(mimeview_key_pressed), mimeview);
	g_signal_connect(G_OBJECT(ctree), "drag_data_get",
			 G_CALLBACK(mimeview_drag_data_get), mimeview);

	mime_notebook = gtk_notebook_new();
	gtk_widget_set_name(GTK_WIDGET(mime_notebook), "mime_notebook");
	gtk_widget_show(mime_notebook);
	gtk_widget_set_can_focus(mime_notebook, FALSE);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(mime_notebook), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(mime_notebook), FALSE);

	icon_grid = gtk_grid_new();
	gtk_orientable_set_orientation(GTK_ORIENTABLE(icon_grid),
			GTK_ORIENTATION_VERTICAL);
	gtk_grid_set_row_spacing(GTK_GRID(icon_grid), 0);
	gtk_widget_show(icon_grid);
	icon_scroll = gtk_layout_new(NULL, NULL);
	gtk_widget_show(icon_scroll);
	gtk_layout_put(GTK_LAYOUT(icon_scroll), icon_grid, 0, 0);
	scrollbutton = gtk_vscrollbutton_new(gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(icon_scroll)));
	gtk_widget_show(scrollbutton);

	g_signal_connect(G_OBJECT(icon_scroll), "scroll_event",
			 G_CALLBACK(mimeview_scrolled), mimeview);

    	mime_toggle = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(mime_toggle), FALSE);

	g_signal_connect(G_OBJECT(mime_toggle), "motion-notify-event",
			 G_CALLBACK(mimeview_visi_notify), mimeview);
	g_signal_connect(G_OBJECT(mime_toggle), "leave-notify-event",
			 G_CALLBACK(mimeview_leave_notify), mimeview);
	g_signal_connect(G_OBJECT(mime_toggle), "enter-notify-event",
			 G_CALLBACK(mimeview_enter_notify), mimeview);

	gtk_container_set_border_width(GTK_CONTAINER(mime_toggle), 2);
	gtk_widget_show(mime_toggle);
	mimeview->ctree_mode = FALSE;
	arrow = gtk_image_new_from_icon_name("pan-start-symbolic", GTK_ICON_SIZE_MENU);
	gtk_widget_show(arrow);
	gtk_container_add(GTK_CONTAINER(mime_toggle), arrow);
	g_signal_connect(G_OBJECT(mime_toggle), "button_release_event", 
			 G_CALLBACK(mime_toggle_button_cb), mimeview);

	icon_mainbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_name(GTK_WIDGET(icon_mainbox), "mimeview_icon_mainbox");
	gtk_widget_show(icon_mainbox);
	gtk_widget_set_size_request(icon_mainbox, 32, -1);
	gtk_box_pack_start(GTK_BOX(icon_mainbox), mime_toggle, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(icon_mainbox), icon_scroll, TRUE, TRUE, 3);
	gtk_box_pack_end(GTK_BOX(icon_mainbox), scrollbutton, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(icon_mainbox), "size_allocate", 
			 G_CALLBACK(icon_scroll_size_allocate_cb), mimeview);
	
	ctree_mainbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);	
	gtk_widget_set_name(GTK_WIDGET(ctree_mainbox), "mimeview_ctree_mainbox");
	gtk_box_pack_start(GTK_BOX(ctree_mainbox), scrolledwin, TRUE, TRUE, 0);
	g_signal_connect(G_OBJECT(ctree_mainbox), "size_allocate", 
			 G_CALLBACK(ctree_size_allocate_cb), mimeview);

	mimeview->ui_manager = gtk_ui_manager_new();
	mimeview->action_group = cm_menu_create_action_group_full(mimeview->ui_manager,
			"MimeView", mimeview_menu_actions,
			G_N_ELEMENTS(mimeview_menu_actions), (gpointer)mimeview);

	MENUITEM_ADDUI_MANAGER(mimeview->ui_manager, "/", "Menus", "Menus", GTK_UI_MANAGER_MENUBAR)
	MENUITEM_ADDUI_MANAGER(mimeview->ui_manager, 
			"/Menus/", "MimeView", "MimeView", GTK_UI_MANAGER_MENU);
	MENUITEM_ADDUI_MANAGER(mimeview->ui_manager, 
			"/Menus/MimeView/", "Open", "MimeView/Open",
			GTK_UI_MANAGER_MENUITEM);
#if (!defined G_OS_WIN32)
	MENUITEM_ADDUI_MANAGER(mimeview->ui_manager, 
			"/Menus/MimeView/", "OpenWith", "MimeView/OpenWith",
			GTK_UI_MANAGER_MENUITEM);
#endif
	MENUITEM_ADDUI_MANAGER(mimeview->ui_manager, 
			"/Menus/MimeView/", "Copy", "MimeView/Copy",
			GTK_UI_MANAGER_MENUITEM);
	MENUITEM_ADDUI_MANAGER(mimeview->ui_manager,
			"/Menus/MimeView/", "SendTo", "MimeView/SendTo",
			GTK_UI_MANAGER_MENUITEM);
	MENUITEM_ADDUI_MANAGER(mimeview->ui_manager, 
			"/Menus/MimeView/", "DisplayAsText", "MimeView/DisplayAsText",
			GTK_UI_MANAGER_MENUITEM);
	MENUITEM_ADDUI_MANAGER(mimeview->ui_manager, 
			"/Menus/MimeView/", "SaveAs", "MimeView/SaveAs",
			GTK_UI_MANAGER_MENUITEM);
	MENUITEM_ADDUI_MANAGER(mimeview->ui_manager, 
			"/Menus/MimeView/", "SaveAll", "MimeView/SaveAll",
			GTK_UI_MANAGER_MENUITEM);
	MENUITEM_ADDUI_MANAGER(mimeview->ui_manager, 
			"/Menus/MimeView/", "SaveAllAttachments", "MimeView/SaveAllAttachments",
			GTK_UI_MANAGER_MENUITEM);
	MENUITEM_ADDUI_MANAGER(mimeview->ui_manager, 
			"/Menus/MimeView/", "NextPart", "MimeView/NextPart",
			GTK_UI_MANAGER_MENUITEM);
	MENUITEM_ADDUI_MANAGER(mimeview->ui_manager, 
			"/Menus/MimeView/", "PrevPart", "MimeView/PrevPart",
			GTK_UI_MANAGER_MENUITEM);

	popupmenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(
				gtk_ui_manager_get_widget(mimeview->ui_manager, "/Menus/MimeView")) );


	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show(vbox);
	siginfoview = noticeview_create(mainwin);
	gtk_widget_set_name(GTK_WIDGET(siginfoview->vgrid), "siginfoview");
	noticeview_hide(siginfoview);
	noticeview_set_icon_clickable(siginfoview, TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), mime_notebook, TRUE, TRUE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), GTK_WIDGET_PTR(siginfoview), FALSE, FALSE, 0);

	paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
	gtk_widget_show(paned);
	gtk_paned_pack1(GTK_PANED(paned), ctree_mainbox, FALSE, TRUE);
	gtk_paned_pack2(GTK_PANED(paned), vbox, TRUE, TRUE);
	
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_name(GTK_WIDGET(hbox), "mimeview");
	gtk_box_pack_start(GTK_BOX(hbox), paned, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), icon_mainbox, FALSE, FALSE, 0);

	gtk_widget_show(hbox);
	gtk_widget_hide(ctree_mainbox);
#ifdef GENERIC_UMPC
	gtk_widget_set_size_request(mime_toggle, -1, r.height + 8);
#endif
	mimeview->hbox          = hbox;
	mimeview->paned         = paned;
	mimeview->scrolledwin   = scrolledwin;
	mimeview->ctree         = ctree;
	mimeview->mime_notebook = mime_notebook;
	mimeview->popupmenu     = popupmenu;
	mimeview->type          = -1;
	mimeview->ctree_mainbox = ctree_mainbox;
	mimeview->icon_scroll   = icon_scroll;
	mimeview->icon_grid     = icon_grid;
	mimeview->icon_mainbox  = icon_mainbox;
	mimeview->icon_count    = 0;
	mimeview->mainwin       = mainwin;
	mimeview->mime_toggle   = mime_toggle;
	mimeview->siginfoview	= siginfoview;
	mimeview->scrollbutton  = scrollbutton;
	mimeview->arrow		= arrow;
	mimeview->target_list	= gtk_target_list_new(mimeview_mime_types, 1); 
	
	mimeviews = g_slist_prepend(mimeviews, mimeview);

	return mimeview;
}

void mimeview_init(MimeView *mimeview)
{
	textview_init(mimeview->textview);

	gtk_container_add(GTK_CONTAINER(mimeview->mime_notebook),
		GTK_WIDGET_PTR(mimeview->textview));
}

static gboolean any_part_is_signed(MimeInfo *mimeinfo)
{
	while (mimeinfo) {
		if (privacy_mimeinfo_is_signed(mimeinfo))
			return TRUE;
		mimeinfo = procmime_mimeinfo_next(mimeinfo);
	}

	return FALSE;
}

void mimeview_show_message(MimeView *mimeview, MimeInfo *mimeinfo,
			   const gchar *file)
{
	GtkTreeView *ctree = GTK_TREE_VIEW(mimeview->ctree);

	mimeview_clear(mimeview);

	cm_return_if_fail(file != NULL);
	cm_return_if_fail(mimeinfo != NULL);

	mimeview->mimeinfo = mimeinfo;

	mimeview->file = g_strdup(file);

	g_signal_handlers_block_by_func(G_OBJECT(ctree), mimeview_selected,
					mimeview);

	/* check if the mail's signed - it can change the mail structure */
	
	if (any_part_is_signed(mimeinfo))
		debug_print("signed mail\n");

	mimeview_set_multipart_tree(mimeview, mimeinfo, NULL);
	gtk_tree_view_expand_all(ctree);
	icon_list_clear(mimeview);
	icon_list_create(mimeview, mimeinfo);
	
	g_signal_handlers_unblock_by_func(G_OBJECT(ctree),
					  mimeview_selected, mimeview);
}

static void mimeview_free_mimeinfo(MimeView *mimeview)
{
	if (mimeview->mimeinfo != NULL) {
		procmime_mimeinfo_free_all(&mimeview->mimeinfo);
		mimeview->mimeinfo = NULL;
	}
}

void mimeview_destroy(MimeView *mimeview)
{
	GSList *cur;
	
	for (cur = mimeview->viewers; cur != NULL; cur = g_slist_next(cur)) {
		MimeViewer *viewer = (MimeViewer *) cur->data;
		viewer->destroy_viewer(viewer);
	}
	g_slist_free(mimeview->viewers);
	gtk_target_list_unref(mimeview->target_list);

	if (mimeview->sig_check_timeout_tag != 0)
		g_source_remove(mimeview->sig_check_timeout_tag);
	if (mimeview->sig_check_cancellable != NULL) {
		/* Set last_sig_check_task to NULL to discard results in async_cb */
		mimeview->siginfo->last_sig_check_task = NULL;
		g_cancellable_cancel(mimeview->sig_check_cancellable);
		g_object_unref(mimeview->sig_check_cancellable);
	}
	mimeview_free_mimeinfo(mimeview);
	gtk_tree_path_free(mimeview->opened);
	g_free(mimeview->file);
	mimeviews = g_slist_remove(mimeviews, mimeview);
	g_free(mimeview);
}

MimeInfo *mimeview_get_selected_part(MimeView *mimeview)
{
	return gtkut_tree_view_get_selected_pointer(
			GTK_TREE_VIEW(mimeview->ctree), COL_DATA,
			NULL, NULL, NULL);
}

MimeInfo *mimeview_get_node_part(MimeView *mimeview, GtkTreePath *path)
{
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(mimeview->ctree));
	GtkTreeIter iter;
	MimeInfo *partinfo;
	
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_model_get(model, &iter, COL_DATA, &partinfo, -1);
	return partinfo;
}

gboolean mimeview_tree_is_empty(MimeView *mimeview)
{
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(mimeview->ctree));
	GtkTreeIter iter;
	return !gtk_tree_model_get_iter_first(model, &iter);
}

static gboolean mimeview_tree_next(GtkTreeModel *model, GtkTreePath *path)
{
	GtkTreeIter iter, parent;
	gboolean has_parent;
	
	gtk_tree_model_get_iter(model, &iter, path);
	
	if (gtk_tree_model_iter_has_child(model, &iter)) {
		gtk_tree_path_down(path);
		return TRUE;
	}

	has_parent = gtk_tree_model_iter_parent(model, &parent, &iter);
	
	if (!gtk_tree_model_iter_next(model, &iter)) {
		while (has_parent) {
			GtkTreeIter saved_parent = parent;
			gtk_tree_path_up(path);
			if (gtk_tree_model_iter_next(model, &parent)) {
				gtk_tree_path_next(path);
				return TRUE;
			} else {
				has_parent = gtk_tree_model_iter_parent(model, &parent, &saved_parent);
			}
		}
		
	} else {
		gtk_tree_path_next(path);
		return TRUE;
	}

	return FALSE;
}

static gboolean mimeview_tree_prev(MimeView *mimeview, GtkTreePath *path)
{
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(mimeview->ctree));
	GtkTreeIter iter, child, parent;
	gboolean has_parent;

	gtk_tree_model_get_iter(model, &iter, path);	
	has_parent = gtk_tree_model_iter_parent(model, &parent, &iter);

	if (gtk_tree_path_prev(path)) {
		gtk_tree_model_get_iter(model, &iter, path);
		
		if (gtk_tree_model_iter_nth_child(model, &child, &iter, 0)) {
			gtk_tree_path_down(path);
			
			while (gtk_tree_model_iter_next(model, &child))
				gtk_tree_path_next(path);
		}
		
		return TRUE;
	}

	if (has_parent) {
		gtk_tree_path_up(path);
		return TRUE;
	}

	return FALSE;
}

gint mimeview_get_selected_part_num(MimeView *mimeview)
{
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(mimeview->ctree));
	GtkTreeIter iter;
	GtkTreePath *path;
	gint i = 0;

	if (!gtk_tree_model_get_iter_first(model, &iter))
		return -1;
	path = gtk_tree_model_get_path(model, &iter);
	
	do {
		if (!gtk_tree_path_compare(mimeview->opened, path)) {
			gtk_tree_path_free(path);
			return i;
		}

		i++;
	} while (mimeview_tree_next(model, path));

	gtk_tree_path_free(path);

	return -1;
}

void mimeview_select_part_num(MimeView *mimeview, gint i)
{
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(mimeview->ctree));
	GtkTreeIter iter;
	GtkTreePath *path;
	GtkTreeSelection *selection;
	gint x = 0;
	
	if (i < 0)
		return;
	
	if (!gtk_tree_model_get_iter_first(model, &iter))
		return;
	path = gtk_tree_model_get_path(model, &iter);
	
	while (x != i) {
		if (!mimeview_tree_next(model, path)) {
			gtk_tree_path_free(path);
			return;
		}
		x++;
	}
	
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(mimeview->ctree));
	gtk_tree_selection_select_path(selection, path);
	gtk_tree_path_free(path);
}

static void mimeview_set_multipart_tree(MimeView *mimeview,
					MimeInfo *mimeinfo,
					GtkTreeIter *parent)
{
	GtkTreeStore *model = GTK_TREE_STORE(gtk_tree_view_get_model(
					GTK_TREE_VIEW(mimeview->ctree)));
	GtkTreeIter iter;
	gchar *content_type, *length, *name;

	cm_return_if_fail(mimeinfo != NULL);

	while (mimeinfo != NULL) {
		if (mimeinfo->type != MIMETYPE_UNKNOWN && mimeinfo->subtype)
			content_type = g_strdup_printf("%s/%s",
					procmime_get_media_type_str(mimeinfo->type),
					mimeinfo->subtype);
		else
			content_type = g_strdup("UNKNOWN");

		length = g_strdup(to_human_readable((goffset) mimeinfo->length));

		if (prefs_common.attach_desc)
			name = g_strdup(get_part_description(mimeinfo));
		else
			name = g_strdup(get_part_name(mimeinfo));

		gtk_tree_store_append(model, &iter, parent);
		gtk_tree_store_set(model, &iter,
				COL_MIMETYPE, content_type,
				COL_SIZE, length,
				COL_NAME, name,
				COL_DATA, mimeinfo, -1);
		g_free(content_type);
		g_free(length);
		g_free(name);

		if (mimeinfo->node->children)
			mimeview_set_multipart_tree(mimeview, 
				(MimeInfo *) mimeinfo->node->children->data, &iter);
		mimeinfo = mimeinfo->node->next != NULL ? 
				(MimeInfo *) mimeinfo->node->next->data : NULL;
	}
}

static const gchar *get_real_part_name(MimeInfo *partinfo)
{
	const gchar *name = NULL;

	name = procmime_mimeinfo_get_parameter(partinfo, "filename");
	if (name == NULL)
		name = procmime_mimeinfo_get_parameter(partinfo, "name");

	return name;
}

static const gchar *get_part_name(MimeInfo *partinfo)
{
	const gchar *name;

	name = get_real_part_name(partinfo);
	if (name == NULL)
		name = "";

	return name;
}

static const gchar *get_part_description(MimeInfo *partinfo)
{
	if (partinfo->description)
		return partinfo->description;
	else
		return get_part_name(partinfo);
}

static void mimeview_show_message_part(MimeView *mimeview, MimeInfo *partinfo)
{
	FILE *fp;
	const gchar *fname;

	if (!partinfo) return;

	fname = mimeview->file;
	if (!fname) return;

	if ((fp = claws_fopen(fname, "rb")) == NULL) {
		FILE_OP_ERROR(fname, "claws_fopen");
		return;
	}

	if (fseek(fp, partinfo->offset, SEEK_SET) < 0) {
		FILE_OP_ERROR(mimeview->file, "fseek");
		claws_fclose(fp);
		return;
	}

	mimeview_change_view_type(mimeview, MIMEVIEW_TEXT);
	textview_show_part(mimeview->textview, partinfo, fp);

	claws_fclose(fp);
}

static MimeViewer *get_viewer_for_content_type(MimeView *mimeview, const gchar *content_type)
{
	GSList *cur;
	MimeViewerFactory *factory = NULL;
	MimeViewer *viewer = NULL;
	gchar *real_contenttype = NULL, *tmp;

	real_contenttype = g_utf8_strdown((gchar *)content_type, -1);
	
	for (cur = mimeviewer_factories; cur != NULL; cur = g_slist_next(cur)) {
		MimeViewerFactory *curfactory = cur->data;
		gint i = 0;

		while (curfactory->content_types[i] != NULL) {
			tmp = g_utf8_strdown(curfactory->content_types[i], -1);
			if (g_pattern_match_simple(tmp, real_contenttype)) {
				debug_print("%s\n", curfactory->content_types[i]);
				factory = curfactory;
				g_free(tmp);
				break;
			}
			g_free(tmp);
			i++;
		}
		if (factory != NULL)
			break;
	}
	g_free(real_contenttype);
	if (factory == NULL)
		return NULL;

	for (cur = mimeview->viewers; cur != NULL; cur = g_slist_next(cur)) {
		MimeViewer *curviewer = cur->data;
		
		if (curviewer->factory == factory)
			return curviewer;
	}
	viewer = factory->create_viewer();
	gtk_container_add(GTK_CONTAINER(mimeview->mime_notebook),
		GTK_WIDGET(viewer->get_widget(viewer)));
		
	mimeview->viewers = g_slist_append(mimeview->viewers, viewer);

	return viewer;
}

gboolean mimeview_has_viewer_for_content_type(MimeView *mimeview, const gchar *content_type)
{
	return (get_viewer_for_content_type(mimeview, content_type) != NULL);
}

static MimeViewer *get_viewer_for_mimeinfo(MimeView *mimeview, MimeInfo *partinfo)
{
	gchar *content_type = NULL;
	MimeViewer *viewer = NULL;

	if ((partinfo->type == MIMETYPE_APPLICATION) &&
            (!g_ascii_strcasecmp(partinfo->subtype, "octet-stream"))) {
		const gchar *filename;

		filename = procmime_mimeinfo_get_parameter(partinfo, "filename");
		if (filename == NULL)
			filename = procmime_mimeinfo_get_parameter(partinfo, "name");
		if (filename != NULL)
			content_type = procmime_get_mime_type(filename);
	} else {
		content_type = procmime_get_content_type_str(partinfo->type, partinfo->subtype);
	}

	if (content_type != NULL) {
		viewer = get_viewer_for_content_type(mimeview, content_type);
		g_free(content_type);
	}

	return viewer;
}

gboolean mimeview_show_part(MimeView *mimeview, MimeInfo *partinfo)
{
	MimeViewer *viewer;
	
	if (mimeview->messageview->partial_display_shown) {
		noticeview_hide(mimeview->messageview->noticeview);
		mimeview->messageview->partial_display_shown = FALSE;
	}

	viewer = get_viewer_for_mimeinfo(mimeview, partinfo);
	if (viewer == NULL) {
		if (mimeview->mimeviewer != NULL)
			mimeview->mimeviewer->clear_viewer(mimeview->mimeviewer);
		mimeview->mimeviewer = NULL;
		return FALSE;
	}

	if (mimeview->mimeviewer != NULL)
		mimeview->mimeviewer->clear_viewer(mimeview->mimeviewer);

	if (mimeview->mimeviewer != viewer)
		mimeview->mimeviewer = viewer;

	mimeview_change_view_type(mimeview, MIMEVIEW_VIEWER);
	viewer->mimeview = mimeview;
	viewer->show_mimepart(viewer, mimeview->file, partinfo);

	return TRUE;
}

static void mimeview_change_view_type(MimeView *mimeview, MimeViewType type)
{
	TextView  *textview  = mimeview->textview;
	GtkWidget *focused = NULL;
	
	if (mainwindow_get_mainwindow())
		focused = gtkut_get_focused_child(
				GTK_CONTAINER(mainwindow_get_mainwindow()->window));

	if ((mimeview->type != MIMEVIEW_VIEWER) && 
	    (mimeview->type == type)) return;

	switch (type) {
	case MIMEVIEW_TEXT:
		gtk_notebook_set_current_page(GTK_NOTEBOOK(mimeview->mime_notebook),
			gtk_notebook_page_num(GTK_NOTEBOOK(mimeview->mime_notebook), 
			GTK_WIDGET_PTR(textview)));
		break;
	case MIMEVIEW_VIEWER:
		gtk_notebook_set_current_page(GTK_NOTEBOOK(mimeview->mime_notebook),
			gtk_notebook_page_num(GTK_NOTEBOOK(mimeview->mime_notebook), 
			GTK_WIDGET(mimeview->mimeviewer->get_widget(mimeview->mimeviewer))));
		break;
	default:
		return;
	}
	if (focused)
		gtk_widget_grab_focus(focused);
	mimeview->type = type;
}

void mimeview_clear(MimeView *mimeview)
{
	GtkTreeModel *model;
	
	if (!mimeview)
		return;

	if (g_slist_find(mimeviews, mimeview) == NULL)
		return;
	
	if (mimeview->sig_check_timeout_tag != 0) {
		g_source_remove(mimeview->sig_check_timeout_tag);
		mimeview->sig_check_timeout_tag = 0;
	}

	if (mimeview->sig_check_cancellable != NULL) {
		/* Set last_sig_check_task to NULL to discard results in async_cb */
		mimeview->siginfo->last_sig_check_task = NULL;
		g_cancellable_cancel(mimeview->sig_check_cancellable);
		g_object_unref(mimeview->sig_check_cancellable);
		mimeview->sig_check_cancellable = NULL;
	}

	noticeview_hide(mimeview->siginfoview);

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(mimeview->ctree));
	gtk_tree_store_clear(GTK_TREE_STORE(model));

	textview_clear(mimeview->textview);
	if (mimeview->mimeviewer != NULL)
		mimeview->mimeviewer->clear_viewer(mimeview->mimeviewer);

	mimeview_free_mimeinfo(mimeview);

	mimeview->mimeinfo = NULL;

	gtk_tree_path_free(mimeview->opened);
	mimeview->opened = NULL;

	g_free(mimeview->file);
	mimeview->file = NULL;

	icon_list_clear(mimeview);
	mimeview_change_view_type(mimeview, MIMEVIEW_TEXT);
}

gchar * get_message_check_signature_shortcut(MessageView *messageview) {
	GtkUIManager *ui_manager;

	if (messageview->window != NULL)
			ui_manager = messageview->ui_manager;
		else
			ui_manager = messageview->mainwin->ui_manager;

	return cm_menu_item_get_shortcut(ui_manager, "Menu/Message/CheckSignature");
}

static void check_signature_cb(GtkWidget *widget, gpointer user_data);
static void display_full_info_cb(GtkWidget *widget, gpointer user_data);

static void update_signature_noticeview(MimeView *mimeview, gboolean special, SignatureStatus code)
{
	gchar *text = NULL, *button_text = NULL;
	void  *func = NULL;
	StockPixmap icon = STOCK_PIXMAP_PRIVACY_SIGNED;
	SignatureStatus mycode = SIGNATURE_UNCHECKED;
	
	if (mimeview == NULL || mimeview->siginfo == NULL)
		g_error("bad call to update noticeview");

	if (special)
		mycode = code;
	else
		mycode = privacy_mimeinfo_get_sig_status(mimeview->siginfo);

	switch (mycode) {
	case SIGNATURE_UNCHECKED:
		button_text = _("Check signature");
		func = check_signature_cb;
		icon = STOCK_PIXMAP_PRIVACY_SIGNED;
		break;
	case SIGNATURE_OK:
		button_text = _("View full information");
		func = display_full_info_cb;
		icon = STOCK_PIXMAP_PRIVACY_PASSED;
		break;
	case SIGNATURE_WARN:
		button_text = _("View full information");
		func = display_full_info_cb;
		icon = STOCK_PIXMAP_PRIVACY_WARN;
		break;
	case SIGNATURE_KEY_EXPIRED:
		button_text = _("View full information");
		func = display_full_info_cb;
		icon = STOCK_PIXMAP_PRIVACY_EXPIRED;
		break;
	case SIGNATURE_INVALID:
		button_text = _("View full information");
		func = display_full_info_cb;
		icon = STOCK_PIXMAP_PRIVACY_FAILED;
		break;
	case SIGNATURE_CHECK_ERROR:
	case SIGNATURE_CHECK_FAILED:
	case SIGNATURE_CHECK_TIMEOUT:
		button_text = _("Check again");
		func = check_signature_cb;
		icon = STOCK_PIXMAP_PRIVACY_UNKNOWN;
		break;
	default:
		break;
	}

	if (mycode == SIGNATURE_UNCHECKED) {
		gchar *tmp = privacy_mimeinfo_get_sig_info(mimeview->siginfo, FALSE);
		gchar *shortcut = get_message_check_signature_shortcut(mimeview->messageview);

		if (*shortcut == '\0')
			text = g_strdup_printf(_("%s Click the icon to check it."), tmp);
		else
			text = g_strdup_printf(_("%s Click the icon or hit '%s' to check it."), tmp, shortcut);

		g_free(shortcut);
	} else if (mycode == SIGNATURE_CHECK_TIMEOUT) {
		gchar *shortcut = get_message_check_signature_shortcut(mimeview->messageview);

		if (*shortcut == '\0')
			text = g_strdup(_("Timeout checking the signature. Click the icon to try again."));
		else
			text = g_strdup_printf(_("Timeout checking the signature. Click the icon or hit '%s' to try again."), shortcut);

		g_free(shortcut);
	} else if (mycode == SIGNATURE_CHECK_ERROR) {
		gchar *shortcut = get_message_check_signature_shortcut(mimeview->messageview);

		if (*shortcut == '\0')
			text = g_strdup(_("Error checking the signature. Click the icon to try again."));
		else
			text = g_strdup_printf(_("Error checking the signature. Click the icon or hit '%s' to try again."), shortcut);

		g_free(shortcut);
	} else {
		text = g_strdup(privacy_mimeinfo_get_sig_info(mimeview->siginfo, FALSE));
	}

	noticeview_set_text(mimeview->siginfoview, text);
	gtk_label_set_selectable(GTK_LABEL(mimeview->siginfoview->text), TRUE);
	g_free(text);

	noticeview_set_button_text(mimeview->siginfoview, NULL);
	noticeview_set_button_press_callback(
		mimeview->siginfoview,
		G_CALLBACK(func),
		(gpointer) mimeview);
	noticeview_set_icon(mimeview->siginfoview, icon);
	noticeview_set_tooltip(mimeview->siginfoview, button_text);

	icon_list_clear(mimeview);
	icon_list_create(mimeview, mimeview->mimeinfo);
}

static void check_signature_async_cb(GObject *source_object,
	GAsyncResult *async_result,
	gpointer user_data)
{
	GTask *task = G_TASK(async_result);
	GCancellable *cancellable;
	MimeView *mimeview = (MimeView *)user_data;
	gboolean cancelled;
	SigCheckTaskResult *result;
	GError *error = NULL;

	if (mimeview->siginfo == NULL) {
		debug_print("discarding stale sig check task result task:%p\n", task);
		return;
	} else if (task != mimeview->siginfo->last_sig_check_task) {
		debug_print("discarding stale sig check task result last_task:%p task:%p\n",
			mimeview->siginfo->last_sig_check_task, task);
		return;
	} else {
		debug_print("using sig check task result task:%p\n", task);
		mimeview->siginfo->last_sig_check_task = NULL;
	}

	cancellable = g_task_get_cancellable(task);
	cancelled = g_cancellable_set_error_if_cancelled(cancellable, &error);
	if (cancelled) {
		debug_print("sig check task was cancelled: task:%p GError: domain:%s code:%d message:\"%s\"\n",
			task, g_quark_to_string(error->domain), error->code, error->message);
		g_error_free(error);
		update_signature_noticeview(mimeview, TRUE, SIGNATURE_CHECK_TIMEOUT);
		return;
	} else {
		if (mimeview->sig_check_cancellable == NULL)
			g_error("bad cancellable");
		if (mimeview->sig_check_timeout_tag == 0)
			g_error("bad cancel source tag");

		g_source_remove(mimeview->sig_check_timeout_tag);
		mimeview->sig_check_timeout_tag = 0;
		g_object_unref(mimeview->sig_check_cancellable);
		mimeview->sig_check_cancellable = NULL;
	}

	result = g_task_propagate_pointer(task, &error);

	if (mimeview->siginfo->sig_data) {
		privacy_free_signature_data(mimeview->siginfo->sig_data);
		mimeview->siginfo->sig_data = NULL;
	}

	if (result == NULL) {
		debug_print("sig check task propagated NULL task:%p GError: domain:%s code:%d message:\"%s\"\n",
			task, g_quark_to_string(error->domain), error->code, error->message);
		g_error_free(error);
		update_signature_noticeview(mimeview, TRUE, SIGNATURE_CHECK_ERROR);
		return;
	}

	mimeview->siginfo->sig_data = result->sig_data;
	update_signature_noticeview(mimeview, FALSE, 0);

	if (result->newinfo) {
		g_warning("Check sig task returned an unexpected new MimeInfo");
		procmime_mimeinfo_free_all(&result->newinfo);
	}

	g_free(result);
}

gboolean mimeview_check_sig_timeout(gpointer user_data)
{
	MimeView *mimeview = (MimeView *)user_data;
	GCancellable *cancellable = mimeview->sig_check_cancellable;

	mimeview->sig_check_timeout_tag = 0;

	if (cancellable == NULL) {
		return G_SOURCE_REMOVE;
	}

	mimeview->sig_check_cancellable = NULL;
	g_cancellable_cancel(cancellable);
	g_object_unref(cancellable);

	return G_SOURCE_REMOVE;
}

static void check_signature_cb(GtkWidget *widget, gpointer user_data)
{
	MimeView *mimeview = (MimeView *) user_data;
	MimeInfo *mimeinfo = mimeview->siginfo;
	gint ret;
	
	if (mimeinfo == NULL || !noticeview_is_visible(mimeview->siginfoview))
		return;

	noticeview_set_text(mimeview->siginfoview, _("Checking signature..."));
	GTK_EVENTS_FLUSH();

	if (mimeview->sig_check_cancellable != NULL) {
		if (mimeview->sig_check_timeout_tag == 0)
			g_error("bad cancel source tag");
		g_source_remove(mimeview->sig_check_timeout_tag);
		g_cancellable_cancel(mimeview->sig_check_cancellable);
		g_object_unref(mimeview->sig_check_cancellable);
	}

	mimeview->sig_check_cancellable = g_cancellable_new();

	ret = privacy_mimeinfo_check_signature(mimeview->siginfo,
		mimeview->sig_check_cancellable,
		check_signature_async_cb,
		mimeview);
	if (ret == 0) {
		mimeview->sig_check_timeout_tag = g_timeout_add_seconds(prefs_common.io_timeout_secs,
			mimeview_check_sig_timeout, mimeview);
	} else if (ret < 0) {
		g_object_unref(mimeview->sig_check_cancellable);
		mimeview->sig_check_cancellable = NULL;
		update_signature_noticeview(mimeview, TRUE, SIGNATURE_CHECK_ERROR);
	} else {
		g_object_unref(mimeview->sig_check_cancellable);
		mimeview->sig_check_cancellable = NULL;
		update_signature_noticeview(mimeview, FALSE, 0);
	}
}

void mimeview_check_signature(MimeView *mimeview)
{
	check_signature_cb(NULL, mimeview);	
}

static void redisplay_email(GtkWidget *widget, gpointer user_data)
{
	MimeView *mimeview = (MimeView *) user_data;
	gtk_tree_path_free(mimeview->opened);
	mimeview->opened = NULL;
	mimeview_selected(gtk_tree_view_get_selection(
			GTK_TREE_VIEW(mimeview->ctree)), mimeview);
}

static void display_full_info_cb(GtkWidget *widget, gpointer user_data)
{
	MimeView *mimeview = (MimeView *) user_data;

	textview_set_text(mimeview->textview, privacy_mimeinfo_get_sig_info(mimeview->siginfo, TRUE));
	noticeview_set_button_text(mimeview->siginfoview, NULL);
	noticeview_set_button_press_callback(
		mimeview->siginfoview,
		G_CALLBACK(redisplay_email),
		(gpointer) mimeview);
	noticeview_set_tooltip(mimeview->siginfoview, _("Go back to email"));
}

static void update_signature_info(MimeView *mimeview, MimeInfo *selected)
{
	MimeInfo *siginfo;
	MimeInfo *first_text;
	
	cm_return_if_fail(mimeview != NULL);
	cm_return_if_fail(selected != NULL);
	
	if (selected->type == MIMETYPE_MESSAGE 
	&&  !g_ascii_strcasecmp(selected->subtype, "rfc822")) {
		/* if the first text part is signed, check that */
		first_text = selected;
		while (first_text && first_text->type != MIMETYPE_TEXT) {
			first_text = procmime_mimeinfo_next(first_text);
		}
		if (first_text) {
			update_signature_info(mimeview, first_text);
			return;
		}	
	}

	siginfo = selected;
	while (siginfo != NULL) {
		if (privacy_mimeinfo_is_signed(siginfo))
			break;
		siginfo = procmime_mimeinfo_parent(siginfo);
	}
	mimeview->siginfo = siginfo;

	/* This shortcut boolean is there to correctly set the menu's
	 * CheckSignature item sensitivity without killing performance
	 * each time the menu sensitiveness is updated (a lot).
	 */
	mimeview->signed_part = (siginfo != NULL);

	if (siginfo == NULL) {
		noticeview_hide(mimeview->siginfoview);
		return;
	}
	
	update_signature_noticeview(mimeview, FALSE, 0);
	noticeview_show(mimeview->siginfoview);
}

void mimeview_show_part_as_text(MimeView *mimeview, MimeInfo *partinfo)
{
	cm_return_if_fail(mimeview != NULL);
	cm_return_if_fail(partinfo != NULL);

	mimeview_show_message_part(mimeview, partinfo);
}

static void mimeview_selected(GtkTreeSelection *selection, MimeView *mimeview)
{
	GtkTreeView *ctree = GTK_TREE_VIEW(mimeview->ctree);
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path;
	MimeInfo *partinfo;
	MainWindow *mainwin;
	GdkDisplay *display;
	GdkSeat *seat;
	GdkDevice *device;

	selection = gtk_tree_view_get_selection(ctree);
	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		return;

	path = gtk_tree_model_get_path(model, &iter);

	if (mimeview->opened && !gtk_tree_path_compare(mimeview->opened, path)) {
		gtk_tree_path_free(path);
		return;
	}
	
	gtk_tree_path_free(mimeview->opened);
	mimeview->opened = path;
	mimeview->spec_part = NULL;
	gtk_tree_view_scroll_to_cell(ctree, path, NULL, TRUE, 0.5, 0);

	partinfo = mimeview_get_node_part(mimeview, path);
	if (!partinfo) return;

	/* ungrab the mouse event */
	if (gtk_widget_has_grab(GTK_WIDGET(ctree))) {
		gtk_grab_remove(GTK_WIDGET(ctree));
		display = gdk_window_get_display(gtk_widget_get_window(GTK_WIDGET(ctree)));
		seat = gdk_display_get_default_seat(display);
		device = gdk_seat_get_pointer(seat);
		if (gdk_display_device_is_grabbed(display, device))
			gdk_seat_ungrab(seat);
	}
	
	mimeview->textview->default_text = FALSE;

	update_signature_info(mimeview, partinfo);

	if (!mimeview_show_part(mimeview, partinfo)) {
		switch (partinfo->type) {
		case MIMETYPE_TEXT:
		case MIMETYPE_MESSAGE:
		case MIMETYPE_MULTIPART:
			mimeview_show_message_part(mimeview, partinfo);
		
			break;
		default:
			mimeview->textview->default_text = TRUE;
			mimeview_change_view_type(mimeview, MIMEVIEW_TEXT);
			textview_clear(mimeview->textview);
			textview_show_mime_part(mimeview->textview, partinfo);
			break;
		}
	}
	mainwin = mainwindow_get_mainwindow();
	if (mainwin)
		main_window_set_menu_sensitive(mainwin);

	if (mimeview->siginfo && privacy_auto_check_signatures(mimeview->siginfo)
	&&  privacy_mimeinfo_get_sig_status(mimeview->siginfo) == SIGNATURE_UNCHECKED) {
		mimeview_check_signature(mimeview);
	}
}

static gint mimeview_button_pressed(GtkWidget *widget, GdkEventButton *event,
				    MimeView *mimeview)
{
	GtkTreeSelection *selection;
	GtkTreeIter iter;

	if (!event) return FALSE;

	if (event->button == 2 || event->button == 3) {
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
		if (!gtk_tree_selection_get_selected(selection, NULL, &iter))
			return FALSE;
		
		gtk_tree_selection_select_iter(selection, &iter);
	}
	part_button_pressed(mimeview, event, mimeview_get_selected_part(mimeview));

	return FALSE;
}

static gboolean mimeview_scrolled(GtkWidget *widget, GdkEventScroll *event,
				    MimeView *mimeview)
{
	GtkVScrollbutton *scrollbutton = (GtkVScrollbutton *)mimeview->scrollbutton;
	if (event->direction == GDK_SCROLL_UP) {
		scrollbutton->scroll_type = GTK_SCROLL_STEP_BACKWARD;
	} else if (event->direction == GDK_SCROLL_DOWN) {
		scrollbutton->scroll_type = GTK_SCROLL_STEP_FORWARD;
	} else {
		gdouble x, y;

		if ((event->direction == GDK_SCROLL_SMOOTH) &&
				gdk_event_get_scroll_deltas((GdkEvent*)event, &x, &y)) {
			if (y < 0)
				scrollbutton->scroll_type = GTK_SCROLL_STEP_BACKWARD;
			else
				if (y >0)
					scrollbutton->scroll_type = GTK_SCROLL_STEP_FORWARD;
		} else
			return FALSE; /* Scrolling left or right */
	}
	gtk_vscrollbutton_scroll(scrollbutton);
	return TRUE;
}

static gboolean part_button_pressed(MimeView *mimeview, GdkEventButton *event, 
				    MimeInfo *partinfo)
{
	static MimeInfo *lastinfo;
	static guint32 lasttime;

	gint double_click_time;
	g_object_get(gtk_settings_get_default(), "gtk-double-click-time", &double_click_time, NULL);

	if (event->button == 2 ||
	    (event->button == 1 && (event->time - lasttime) < double_click_time && lastinfo == partinfo)) {
		/* call external program for image, audio or html */
		mimeview_launch(mimeview, partinfo);
		return TRUE;
	} else if (event->button == 3) {
		MainWindow *mainwin = mainwindow_get_mainwindow();

		if (partinfo && (partinfo->type == MIMETYPE_MESSAGE ||
				 partinfo->type == MIMETYPE_IMAGE ||
				 partinfo->type == MIMETYPE_MULTIPART))
			cm_menu_set_sensitive_full(mimeview->ui_manager, "Menus/MimeView/DisplayAsText", FALSE);
		else
			cm_menu_set_sensitive_full(mimeview->ui_manager, "Menus/MimeView/DisplayAsText", TRUE);

		if (partinfo && (partinfo->type == MIMETYPE_MESSAGE ||
				 partinfo->type == MIMETYPE_IMAGE ||
				 partinfo->type == MIMETYPE_TEXT))
			cm_menu_set_sensitive_full(mimeview->ui_manager, "Menus/MimeView/Copy", TRUE);
		else
			cm_menu_set_sensitive_full(mimeview->ui_manager, "Menus/MimeView/Copy", FALSE);
#ifndef G_OS_WIN32
		if (partinfo &&
		    partinfo->type == MIMETYPE_APPLICATION &&
		    !g_ascii_strcasecmp(partinfo->subtype, "octet-stream"))
			cm_menu_set_sensitive_full(mimeview->ui_manager, "Menus/MimeView/Open", FALSE);
		else
#endif
			cm_menu_set_sensitive_full(mimeview->ui_manager, "Menus/MimeView/Open", TRUE);
		if (mainwin)
			main_window_set_menu_sensitive(mainwin);
		g_object_set_data(G_OBJECT(mimeview->popupmenu),
				  "pop_partinfo", partinfo);

		gtk_menu_popup_at_pointer(GTK_MENU(mimeview->popupmenu),
				(GdkEvent *)event);
		return TRUE;
	}

	lastinfo = partinfo;
	lasttime = event->time;
	return FALSE;
}


gboolean mimeview_pass_key_press_event(MimeView *mimeview, GdkEventKey *event)
{
	return mimeview_key_pressed(mimeview->ctree, event, mimeview);
}

void mimeview_select_next_part(MimeView *mimeview)
{
	GtkTreeView *ctree = GTK_TREE_VIEW(mimeview->ctree);
	GtkTreeModel *model = gtk_tree_view_get_model(ctree);
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreePath *path;
	MimeInfo *partinfo = NULL;
	gboolean has_next;
	
	if (!mimeview->opened) return;

	gtk_tree_model_get_iter(model, &iter, mimeview->opened);
	path = gtk_tree_model_get_path(model, &iter);
skip:
	has_next = mimeview_tree_next(model, path);
	if (!has_next) {
		has_next = gtk_tree_model_get_iter_first(model, &iter);
		gtk_tree_path_free(path);
		path = gtk_tree_model_get_path(model, &iter);
	}

	if (has_next) {
		partinfo = mimeview_get_node_part(mimeview, path);
		if (partinfo->type == MIMETYPE_MULTIPART ||
		    (!prefs_common.show_inline_attachments && partinfo->id))
			goto skip;
		selection = gtk_tree_view_get_selection(ctree);
		gtk_tree_selection_select_path(selection, path);
		icon_list_toggle_by_mime_info(mimeview, partinfo);
	}
	
	gtk_tree_path_free(path);
}

void mimeview_select_prev_part(MimeView *mimeview)
{
	GtkTreeView *ctree = GTK_TREE_VIEW(mimeview->ctree);
	GtkTreeModel *model = gtk_tree_view_get_model(ctree);
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	GtkTreePath *path;
	MimeInfo *partinfo = NULL;
	gboolean has_prev;
	
	if (!mimeview->opened) return;

	gtk_tree_model_get_iter(model, &iter, mimeview->opened);
	path = gtk_tree_model_get_path(model, &iter);
skip:
	has_prev = mimeview_tree_prev(mimeview, path);
	if (!has_prev) {
		while (mimeview_tree_next(model, path)) {}
		has_prev = TRUE;
	}

	if (has_prev) {
		partinfo = mimeview_get_node_part(mimeview, path);
		if (partinfo->type == MIMETYPE_MULTIPART ||
		    (!prefs_common.show_inline_attachments && partinfo->id))
			goto skip;
		selection = gtk_tree_view_get_selection(ctree);
		gtk_tree_selection_select_path(selection, path);
		icon_list_toggle_by_mime_info(mimeview, partinfo);
	}
	
	gtk_tree_path_free(path);
}

#define BREAK_ON_MODIFIER_KEY() \
	if ((event->state & (GDK_MOD1_MASK|GDK_CONTROL_MASK)) != 0) break

static gint mimeview_key_pressed(GtkWidget *widget, GdkEventKey *event,
				 MimeView *mimeview)
{
	SummaryView *summaryview;

	if (!event) return FALSE;
	if (!mimeview->opened) return FALSE;

	summaryview = mimeview->messageview->mainwin->summaryview;
	
	if (summaryview && quicksearch_has_focus(summaryview->quicksearch))
		return FALSE;
		
	switch (event->keyval) {
	case GDK_KEY_Home:
	case GDK_KEY_End:
		textview_scroll_max(mimeview->textview,
				    (event->keyval == GDK_KEY_Home));
		return TRUE;
	case GDK_KEY_Page_Down:
	case GDK_KEY_space:
		if (mimeview_scroll_page(mimeview,
					 (event->state & GDK_SHIFT_MASK) != 0))
			return TRUE;
			
		if (!(event->state & GDK_SHIFT_MASK))
			mimeview_select_next_part(mimeview);
		return TRUE;
	case GDK_KEY_Page_Up:
	case GDK_KEY_BackSpace:
		mimeview_scroll_page(mimeview, TRUE);
		return TRUE;
	case GDK_KEY_Return:
	case GDK_KEY_KP_Enter:
		mimeview_scroll_one_line(mimeview,
					 (event->state & GDK_MOD1_MASK) != 0);
		return TRUE;
	case GDK_KEY_Up:
	case GDK_KEY_Down:
		mimeview_scroll_one_line(mimeview, (event->keyval == GDK_KEY_Up));
		return TRUE;
	default:
		break;
	}

	if (mimeview->messageview->new_window) return FALSE;

	return summary_pass_key_press_event(summaryview, event);
}

static void mimeview_drag_data_get(GtkWidget	    *widget,
				   GdkDragContext   *drag_context,
				   GtkSelectionData *selection_data,
				   guint	     info,
				   guint	     time,
				   MimeView	    *mimeview)
{
	gchar *filename = NULL, *uriname, *tmp = NULL;
	MimeInfo *partinfo;
	gint err;
	gint count = 0;

	if (!mimeview->opened) return;
	if (!mimeview->file) return;

	partinfo = mimeview_get_selected_part(mimeview);
	if (!partinfo) return;

	if (strlen(get_part_name(partinfo)) > 0) {
		filename = g_path_get_basename(get_part_name(partinfo));
		if (filename) {
			if (*filename == '\0') {
				g_free(filename);
				return;
			}
		}
	} else if (partinfo->type == MIMETYPE_MESSAGE 
		   && !g_ascii_strcasecmp(partinfo->subtype, "rfc822")) {
		gchar *name = NULL;
		GPtrArray *headers = NULL;
		FILE *fp;

		fp = claws_fopen(partinfo->data.filename, "rb");
		if (fp != NULL && fseek(fp, partinfo->offset, SEEK_SET) == 0) {
			headers = procheader_get_header_array(fp);
			if (headers) {
				gint i;
				for (i = 0; i < headers->len; i++) {
					Header *header = g_ptr_array_index(headers, i);
					if (procheader_headername_equal(header->name, "Subject")) {
						unfold_line(header->body);
						name = g_strconcat(header->body, ".txt", NULL);
						subst_for_filename(name);
					}
				}
				procheader_header_array_destroy(headers);
			}
		}
		if (fp != NULL)
			claws_fclose(fp);
		if (name)
			filename = g_path_get_basename(name);
		g_free(name);
	}
	if (filename == NULL)
		filename = g_path_get_basename("Unnamed part");

	if (!g_utf8_validate(filename, -1, NULL))
		tmp = conv_codeset_strdup(filename,
				conv_get_locale_charset_str(),
				CS_UTF_8);

	if (tmp == NULL) {
		g_warning("filename not in UTF-8");
		tmp = g_strdup(filename);
	}
	g_free(filename);
	filename = g_strconcat(get_mime_tmp_dir(), G_DIR_SEPARATOR_S,
			       tmp, NULL);

check_new_file:
	if (is_file_exist(filename)) {
		gchar *ext = NULL;
		gchar *prefix = NULL;
		gchar *new_name = NULL;
		if (strrchr(tmp, '.')) {
			prefix = g_strdup(tmp);
			ext = g_strdup(strrchr(tmp, '.'));
			*(strrchr(prefix, '.')) = '\0';
		} else {
			prefix = g_strdup(tmp);
			ext = g_strdup("");
		}
		count++;
		new_name = g_strdup_printf("%s.%d%s", prefix, count, ext);
		g_free(prefix);
		g_free(ext);
		g_free(filename);
		filename = g_strconcat(get_mime_tmp_dir(), G_DIR_SEPARATOR_S,
			       new_name, NULL);
		g_free(new_name);
		goto check_new_file;
	}

	g_free(tmp);

	if ((err = procmime_get_part(filename, partinfo)) < 0)
		alertpanel_error
			(_("Couldn't save the part of multipart message: %s"), 
				g_strerror(-err));

	tmp = g_filename_to_uri(filename, NULL, NULL);
	uriname = g_strconcat(tmp, "\r\n", NULL);
	g_free(tmp);

	gtk_selection_data_set(selection_data, 
                   gtk_selection_data_get_target(selection_data), 8,
			       (guchar *)uriname, strlen(uriname));

	g_free(uriname);
	g_free(filename);
}

/**
 * Returns a filename (with path) for an attachment
 * \param partinfo The attachment to save
 * \param basedir The target directory
 * \param number Used for dummy filename if attachment is unnamed
 */
static gchar *mimeview_get_filename_for_part(MimeInfo *partinfo,
				      const gchar *basedir,
				      gint number)
{
	gchar *fullname;
	gchar *filename;

	filename = g_strdup(get_part_name(partinfo));
	if (!filename || !*filename) {
		g_free(filename);
		filename = g_strdup_printf("noname.%d", number);
	}

	if (!g_utf8_validate(filename, -1, NULL)) {
		gchar *tmp = conv_filename_to_utf8(filename);
		g_free(filename);
		filename = tmp;
	}
	
	subst_for_filename(filename);

	fullname = g_strconcat
		(basedir, G_DIR_SEPARATOR_S, (g_path_is_absolute(filename))
		 ? &filename[1] : filename, NULL);

	g_free(filename);
	filename = conv_filename_from_utf8(fullname);
	g_free(fullname);
	return filename;
}

/**
 * Write a single attachment to file
 * \param filename Filename with path
 * \param partinfo Attachment to save
 */
static gint mimeview_write_part(const gchar *filename,
				MimeInfo *partinfo,
				gboolean handle_error)
{
	gchar *dir;
	gint err;

	dir= g_path_get_dirname(filename);
	if (!is_dir_exist(dir))
		make_dir_hier(dir);
	g_free(dir);

	if (is_file_exist(filename)) {
		AlertValue aval;
		gchar *res;
		gchar *tmp;
		
		if (!g_utf8_validate(filename, -1, NULL))
			tmp = conv_filename_to_utf8(filename);
		else 
			tmp = g_strdup(filename);
		
		res = g_strdup_printf(_("Overwrite existing file '%s'?"),
				      tmp);
		g_free(tmp);
		aval = alertpanel(_("Overwrite"), res, NULL, _("_No"),
				  NULL, _("_Yes"), NULL, NULL, ALERTFOCUS_FIRST);
		g_free(res);
		if (G_ALERTALTERNATE != aval)
			return 2;
	}

	if ((err = procmime_get_part(filename, partinfo)) < 0) {
		debug_print("error saving MIME part: %d\n", err);
		if (handle_error)
			alertpanel_error
				(_("Couldn't save the part of multipart message: %s"),
				 g_strerror(-err));
		return 0;
	}

	if (prefs_common.attach_save_chmod) {
		if (chmod(filename, prefs_common.attach_save_chmod) < 0)
			FILE_OP_ERROR(filename, "chmod");
	}

	return 1;
}

static AlertValue mimeview_save_all_error_ask(gint n)
{
	gchar *message = g_strdup_printf(
		_("An error has occurred while saving message part %d. "
		"Do you want to cancel saving or ignore error and "
		"continue?"), n);
	AlertValue av = alertpanel_full(_("Error saving message part"),
		message, NULL, _("_Cancel"), NULL, _("Ignore"), NULL, _("Ignore all"),
		ALERTFOCUS_FIRST, FALSE, NULL, ALERT_WARNING);
	g_free(message);
	return av;
}

static void mimeview_save_all_info(gint errors, gint total)
{
	AlertValue aval;

	if (!errors && prefs_common.show_save_all_success) {
		gchar *msg = g_strdup_printf(
				ngettext("%d file saved successfully.",
					"%d files saved successfully.",
					total),
				total);
		aval = alertpanel_full(_("Notice"), msg, NULL, _("_Close"),
				       NULL, NULL, NULL, NULL, ALERTFOCUS_FIRST,
				       TRUE, NULL, ALERT_NOTICE);
		g_free(msg);
		if (aval & G_ALERTDISABLE)
			prefs_common.show_save_all_success = FALSE;
	} else if (prefs_common.show_save_all_failure) {
		gchar *msg1 = g_strdup_printf(
				ngettext("%d file saved successfully",
					"%d files saved successfully",
					total - errors),
				total - errors);
		gchar *msg2 = g_strdup_printf(
				ngettext("%s, %d file failed.",
					"%s, %d files failed.",
					errors),
				msg1, errors);
		aval = alertpanel_full(_("Warning"), msg2, NULL, _("_Close"),
				       NULL, NULL, NULL, NULL, ALERTFOCUS_FIRST,
				       TRUE, NULL, ALERT_WARNING);
		g_free(msg2);
		g_free(msg1);
		if (aval & G_ALERTDISABLE)
			prefs_common.show_save_all_failure = FALSE;
	}
}

/**
 * Menu callback: Save all attached files
 * \param mimeview Current display
 */
static void mimeview_save_all(MimeView *mimeview, gboolean attachments_only)
{
	MimeInfo *partinfo;
	gchar *dirname;
	gchar *startdir = NULL;
	gint number = 0, errors = 0;
	gboolean skip_errors = FALSE;

	if (!mimeview->opened) return;
	if (!mimeview->file) return;
	if (!mimeview->mimeinfo) return;

	partinfo = mimeview->mimeinfo;
	if (prefs_common.attach_save_dir && *prefs_common.attach_save_dir)
		startdir = g_strconcat(prefs_common.attach_save_dir, G_DIR_SEPARATOR_S, NULL);
	else
		startdir = g_strdup(get_home_dir());

	manage_window_focus_in(gtk_widget_get_ancestor(mimeview->hbox, GTK_TYPE_WINDOW), NULL, NULL);
	dirname = filesel_select_file_save_folder(_("Select destination folder"), startdir);
	if (!dirname) {
		g_free(startdir);
		return;
	}

	if (!is_dir_exist (dirname)) {
		alertpanel_error(_("'%s' is not a directory."), dirname);
		g_free(startdir);
		g_free(dirname);
		return;
	}

	if (dirname[strlen(dirname)-1] == G_DIR_SEPARATOR)
		dirname[strlen(dirname)-1] = '\0';

	/* Skip the first part, that is sometimes DISPOSITIONTYPE_UNKNOWN */
	if (partinfo && partinfo->type == MIMETYPE_MESSAGE)
		partinfo = procmime_mimeinfo_next(partinfo);
	if (partinfo && partinfo->type == MIMETYPE_MULTIPART) {
		partinfo = procmime_mimeinfo_next(partinfo);
		if (partinfo && partinfo->type == MIMETYPE_TEXT)
			partinfo = procmime_mimeinfo_next(partinfo);
	}

	while (partinfo != NULL) {
		if ((!attachments_only &&
		    (partinfo->type != MIMETYPE_MESSAGE &&
		     partinfo->type != MIMETYPE_MULTIPART &&
		     (partinfo->disposition != DISPOSITIONTYPE_INLINE ||
		      get_real_part_name(partinfo) != NULL))) ||
		    (attachments_only &&
		     (partinfo->disposition == DISPOSITIONTYPE_ATTACHMENT &&
		      get_real_part_name(partinfo) != NULL))) {
			gchar *filename = mimeview_get_filename_for_part(
				partinfo, dirname, number++);

			gint ok = mimeview_write_part(filename, partinfo, FALSE);
			g_free(filename);
			if (ok < 1) {
				++errors;
				if (!skip_errors) {
					AlertValue av = mimeview_save_all_error_ask(number);
					skip_errors = (av == G_ALERTOTHER);
					if (av == G_ALERTDEFAULT) /* cancel */
						break;
				}
			} else if (ok == 2)
				number--;
		}
		partinfo = procmime_mimeinfo_next(partinfo);
	}

	g_free(prefs_common.attach_save_dir);
	g_free(startdir);
	prefs_common.attach_save_dir = g_filename_to_utf8(dirname,
					-1, NULL, NULL, NULL);
	g_free(dirname);

	mimeview_save_all_info(errors, number);
}

static MimeInfo *mimeview_get_part_to_use(MimeView *mimeview)
{
	MimeInfo *partinfo = NULL;
	if (mimeview->spec_part) {
		partinfo = mimeview->spec_part;
		mimeview->spec_part = NULL;
	} else {
		partinfo = (MimeInfo *) g_object_get_data
			 (G_OBJECT(mimeview->popupmenu),
			 "pop_partinfo");
		g_object_set_data(G_OBJECT(mimeview->popupmenu),
				  "pop_partinfo", NULL);
		if (!partinfo) { 
			partinfo = mimeview_get_selected_part(mimeview);
		}			 
	}

	return partinfo;
}
/**
 * Menu callback: Save the selected attachment
 * \param mimeview Current display
 */
void mimeview_save_as(MimeView *mimeview)
{
	gchar *filename;
	gchar *filepath = NULL;
	gchar *filedir = NULL;
	MimeInfo *partinfo;
	gchar *partname = NULL;

	if (!mimeview->opened) return;
	if (!mimeview->file) return;

	partinfo = mimeview_get_part_to_use(mimeview);

	cm_return_if_fail(partinfo != NULL);
	
	if (get_part_name(partinfo) == NULL) {
		return;
	}
	partname = g_strdup(get_part_name(partinfo));
	
	if (!g_utf8_validate(partname, -1, NULL)) {
		gchar *tmp = conv_filename_to_utf8(partname);
		if (!tmp) {
			tmp = conv_codeset_strdup(partname,
				conv_get_locale_charset_str(),
				CS_UTF_8);
		}
		if (tmp) {
			g_free(partname);
			partname = tmp;
		}
	}

	subst_for_filename(partname);
	
	if (prefs_common.attach_save_dir && *prefs_common.attach_save_dir)
		filepath = g_strconcat(prefs_common.attach_save_dir,
				       G_DIR_SEPARATOR_S, partname, NULL);
	else
		filepath = g_strdup(partname);

	g_free(partname);

	manage_window_focus_in(gtk_widget_get_ancestor(mimeview->hbox, GTK_TYPE_WINDOW), NULL, NULL);
	filename = filesel_select_file_save(_("Save as"), filepath);
	if (!filename) {
		g_free(filepath);
		return;
	}

	mimeview_write_part(filename, partinfo, TRUE);

	filedir = g_path_get_dirname(filename);
	if (filedir && strcmp(filedir, ".")) {
		g_free(prefs_common.attach_save_dir);
		prefs_common.attach_save_dir = g_filename_to_utf8(filedir, -1, NULL, NULL, NULL);
	}

	g_free(filedir);
	g_free(filepath);
}

void mimeview_display_as_text(MimeView *mimeview)
{
	MimeInfo *partinfo;

	if (!mimeview->opened) return;

	partinfo = mimeview_get_part_to_use(mimeview);
	mimeview_select_mimepart_icon(mimeview, partinfo);
	cm_return_if_fail(partinfo != NULL);
	mimeview_show_message_part(mimeview, partinfo);
}

void mimeview_launch(MimeView *mimeview, MimeInfo *partinfo)
{
	gchar *filename;
	gint err;

	if (!mimeview->opened) return;
	if (!mimeview->file) return;

	if (!partinfo)
		partinfo = mimeview_get_part_to_use(mimeview);

	cm_return_if_fail(partinfo != NULL);

	filename = procmime_get_tmp_file_name(partinfo);

	if ((err = procmime_get_part(filename, partinfo)) < 0)
		alertpanel_error
			(_("Couldn't save the part of multipart message: %s"), 
				g_strerror(-err));
	else
		mimeview_view_file(filename, partinfo, NULL, mimeview);

	g_free(filename);
}

#ifndef G_OS_WIN32
void mimeview_open_with(MimeView *mimeview)
{
	MimeInfo *partinfo;

	if (!mimeview) return;
	if (!mimeview->opened) return;
	if (!mimeview->file) return;

	partinfo = mimeview_get_part_to_use(mimeview);

	mimeview_open_part_with(mimeview, partinfo, FALSE);
}

static void mimeview_open_part_with(MimeView *mimeview, MimeInfo *partinfo, gboolean automatic)
{
	gchar *filename;
	gchar *cmd;
	gchar *mime_command = NULL;
	gchar *content_type = NULL;
	gint err;

	cm_return_if_fail(partinfo != NULL);

	filename = procmime_get_tmp_file_name(partinfo);

	if ((err = procmime_get_part(filename, partinfo)) < 0) {
		alertpanel_error
			(_("Couldn't save the part of multipart message: %s"), 
				g_strerror(-err));
		g_free(filename);
		return;
	}
	
	if (!prefs_common.mime_open_cmd_history)
		prefs_common.mime_open_cmd_history =
			add_history(NULL, prefs_common.mime_open_cmd);

	if ((partinfo->type == MIMETYPE_APPLICATION) &&
            (!g_ascii_strcasecmp(partinfo->subtype, "octet-stream"))) {
	    	/* guess content-type from filename */
	    	content_type = procmime_get_mime_type(filename);
	} 
	if (content_type == NULL) {
		content_type = procmime_get_content_type_str(partinfo->type,
			partinfo->subtype);
	}
	
	if ((partinfo->type == MIMETYPE_TEXT && !strcmp(partinfo->subtype, "html"))
	&& prefs_common_get_uri_cmd() && prefs_common.uri_cmd[0]) {
		mime_command = g_strdup(prefs_common_get_uri_cmd());
		g_free(content_type);
		content_type = NULL;
	} else if (partinfo->type != MIMETYPE_TEXT || !prefs_common_get_ext_editor_cmd()
	||  !prefs_common_get_ext_editor_cmd()[0]) {
		mime_command = mailcap_get_command_for_type(content_type, filename);
	} else {
		mime_command = g_strdup(prefs_common_get_ext_editor_cmd());
		g_free(content_type);
		content_type = NULL;
	}
	if (mime_command == NULL) {
		/* try with extension this time */
		g_free(content_type);
		content_type = procmime_get_mime_type(filename);
		mime_command = mailcap_get_command_for_type(content_type, filename);
	}

	if (mime_command == NULL)
		automatic = FALSE;
	
	if (!automatic) {
		gboolean remember = FALSE;
		if (content_type != NULL)
			cmd = input_dialog_combo_remember
				(_("Open with"),
				 _("Enter the command-line to open file:\n"
				   "('%s' will be replaced with file name)"),
				 mime_command ? mime_command : prefs_common.mime_open_cmd,
				 prefs_common.mime_open_cmd_history, &remember);
		else
			cmd = input_dialog_combo
				(_("Open with"),
				 _("Enter the command-line to open file:\n"
				   "('%s' will be replaced with file name)"),
				 mime_command ? mime_command : prefs_common.mime_open_cmd,
				 prefs_common.mime_open_cmd_history);
		if (cmd && remember) {
			mailcap_update_default(content_type, cmd);
		}
		g_free(mime_command);
	} else {
		cmd = mime_command;
	}
	if (cmd) {
		mimeview_view_file(filename, partinfo, cmd, mimeview);
		g_free(prefs_common.mime_open_cmd);
		prefs_common.mime_open_cmd = cmd;
		prefs_common.mime_open_cmd_history =
			add_history(prefs_common.mime_open_cmd_history, cmd);
	}

	g_free(content_type);
	g_free(filename);
}
#endif

static void mimeview_send_to(MimeView *mimeview, MimeInfo *partinfo)
{
	GList *attach_file = NULL;
	AttachInfo *ainfo = NULL;
	gchar *filename;
	gint err;

	if (!mimeview->opened) return;
	if (!mimeview->file) return;

	cm_return_if_fail(partinfo != NULL);

	filename = procmime_get_tmp_file_name(partinfo);

	if (!(err = procmime_get_part(filename, partinfo))) {
		ainfo = g_new0(AttachInfo, 1);
		ainfo->file = filename;
		ainfo->name = g_strdup(get_part_name(partinfo));
		ainfo->content_type = procmime_get_content_type_str(
					partinfo->type, partinfo->subtype);
		ainfo->charset = g_strdup(procmime_mimeinfo_get_parameter(
					partinfo, "charset"));
		attach_file = g_list_append(attach_file, ainfo);
		
		compose_new(NULL, NULL, attach_file);
		
		g_free(ainfo->name);
		g_free(ainfo->content_type);
		g_free(ainfo->charset);
		g_free(ainfo);
		g_list_free(attach_file);
	} else
		alertpanel_error
			(_("Couldn't save the part of multipart message: %s"), 
				g_strerror(-err));
	g_free(filename);
}

static void mimeview_view_file(const gchar *filename, MimeInfo *partinfo,
			       const gchar *cmd, MimeView *mimeview)
{
#ifndef G_OS_WIN32
	gchar *p;
	gchar buf[BUFFSIZE];

	if (cmd == NULL)
		mimeview_open_part_with(mimeview, partinfo, TRUE);
	else {
		if ((p = strchr(cmd, '%')) && *(p + 1) == 's' &&
		    !strchr(p + 2, '%')) {
			g_snprintf(buf, sizeof(buf), cmd, filename);
			if (!prefs_common.save_parts_readwrite)
				g_chmod(filename, S_IRUSR);
			else
				g_chmod(filename, S_IRUSR|S_IWUSR);
 		} else {
			g_warning("MIME viewer command-line is invalid: '%s'", cmd);
			cmd = NULL;
			mimeview_open_part_with(mimeview, partinfo, FALSE);
 		}
		if (cmd != NULL && execute_command_line(buf, TRUE, NULL) != 0) {
			if (!prefs_common.save_parts_readwrite)
				g_chmod(filename, S_IRUSR|S_IWUSR);
			mimeview_open_part_with(mimeview, partinfo, FALSE);
		}
	}
#else
	SHFILEINFO file_info;
	GError *error = NULL;
	gunichar2 *fn16 = g_utf8_to_utf16(filename, -1, NULL, NULL, &error);

	if (error != NULL) {
		alertpanel_error(_("Could not convert attachment name to UTF-16:\n\n%s"),
					error->message);
		debug_print("filename '%s' conversion to UTF-16 failed\n", filename);
		g_error_free(error);
		return;
	}

	if ((SHGetFileInfo((LPCWSTR)fn16, 0, &file_info, sizeof(SHFILEINFO), SHGFI_EXETYPE)) != 0) {
		AlertValue val = alertpanel_full(_("Execute untrusted binary?"), 
				      _("This attachment is an executable file. Executing "
				        "untrusted binaries is dangerous and could compromise "
					"your computer.\n\n"
					"Do you want to run this file?"), NULL, _("_Cancel"),
					NULL, _("Run binary"), NULL, NULL, ALERTFOCUS_FIRST,
					FALSE, NULL, ALERT_WARNING);
		if (val == G_ALERTALTERNATE) {
			debug_print("executing binary\n");
			ShellExecute(NULL, L"open", (LPCWSTR)fn16, NULL, NULL, SW_SHOW);
		}
	} else {
		ShellExecute(NULL, L"open", (LPCWSTR)fn16, NULL, NULL, SW_SHOW);
	}

	g_free(fn16);
	
#endif
}

void mimeview_register_viewer_factory(MimeViewerFactory *factory)
{
	mimeviewer_factories = g_slist_append(mimeviewer_factories, factory);
}

static gint cmp_viewer_by_factroy(gconstpointer a, gconstpointer b)
{
	return ((MimeViewer *) a)->factory == (MimeViewerFactory *) b ? 0 : -1;
}

void mimeview_unregister_viewer_factory(MimeViewerFactory *factory)
{
	GSList *mimeview_list, *viewer_list;

	for (mimeview_list = mimeviews; mimeview_list != NULL; mimeview_list = g_slist_next(mimeview_list)) {
		MimeView *mimeview = (MimeView *) mimeview_list->data;
		
		if (mimeview->mimeviewer && mimeview->mimeviewer->factory == factory) {
			mimeview_change_view_type(mimeview, MIMEVIEW_TEXT);
			mimeview->mimeviewer = NULL;
		}

		while ((viewer_list = g_slist_find_custom(mimeview->viewers, factory, cmp_viewer_by_factroy)) != NULL) {
			MimeViewer *mimeviewer = (MimeViewer *) viewer_list->data;

			mimeviewer->destroy_viewer(mimeviewer);
			mimeview->viewers = g_slist_remove(mimeview->viewers, mimeviewer);
		}
	}

	mimeviewer_factories = g_slist_remove(mimeviewer_factories, factory);
}

static gboolean icon_clicked_cb (GtkWidget *button, GdkEventButton *event, MimeView *mimeview)
{
	gint      num;
	MimeInfo *partinfo;

	num      = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "icon_number"));
	partinfo = g_object_get_data(G_OBJECT(button), "partinfo");

	if (event->button == 1) {
		icon_selected(mimeview, num, partinfo);
		gtk_widget_grab_focus(button);
		icon_list_toggle_by_mime_info(mimeview, partinfo);
	}
	part_button_pressed(mimeview, event, partinfo);

	return FALSE;
}

static void icon_selected (MimeView *mimeview, gint num, MimeInfo *partinfo)
{
	GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(mimeview->ctree));
	GtkTreeIter iter;
	GtkTreePath *path;
	MimeInfo *curr = NULL;
	
	if (!gtk_tree_model_get_iter_first(model, &iter))
		return;
	path = gtk_tree_model_get_path(model, &iter);
	
	do {
		gtk_tree_model_get_iter(model, &iter, path);
		gtk_tree_model_get(model, &iter, COL_DATA, &curr, -1);
		if (curr == partinfo) {
			GtkTreeSelection *sel = gtk_tree_view_get_selection(
						GTK_TREE_VIEW(mimeview->ctree));
			gtk_tree_selection_select_iter(sel, &iter);
			gtk_tree_path_free(path);
			return;
		}
	} while (mimeview_tree_next(model, path));
	
	gtk_tree_path_free(path);
}		

void mimeview_select_mimepart_icon(MimeView *mimeview, MimeInfo *partinfo)
{
	icon_list_toggle_by_mime_info(mimeview, partinfo);
	icon_selected(mimeview, -1, partinfo);
}

static gint icon_key_pressed(GtkWidget *button, GdkEventKey *event,
			     MimeView *mimeview)
{
	SummaryView  *summaryview;
	
	if (!event) return FALSE;

	switch (event->keyval) {
	case GDK_KEY_space:
		if (mimeview_scroll_page(mimeview, FALSE))
			return TRUE;

		mimeview_select_next_part(mimeview);
		return TRUE;

		break;
	case GDK_KEY_BackSpace:
		mimeview_scroll_page(mimeview, TRUE);
		return TRUE;
	case GDK_KEY_Return:
	case GDK_KEY_KP_Enter:
		mimeview_scroll_one_line(mimeview,
					 (event->state & GDK_MOD1_MASK) != 0);
		return TRUE;
	case GDK_KEY_y:
		BREAK_ON_MODIFIER_KEY();
		mimeview_save_as(mimeview);
		return TRUE;
	case GDK_KEY_t:
		BREAK_ON_MODIFIER_KEY();
		mimeview_display_as_text(mimeview);
		return TRUE;	
	case GDK_KEY_l:
		BREAK_ON_MODIFIER_KEY();
		mimeview_launch(mimeview, NULL);
		return TRUE;
#ifndef G_OS_WIN32
	case GDK_KEY_o:
		BREAK_ON_MODIFIER_KEY();
		mimeview_open_with(mimeview);
		return TRUE;
#endif
	case GDK_KEY_c:
		BREAK_ON_MODIFIER_KEY();
		mimeview_check_signature(mimeview);
		return TRUE;
	case GDK_KEY_a:
		BREAK_ON_MODIFIER_KEY();
		mimeview_select_next_part(mimeview);
		return TRUE;
	default:
		break;
	}

	if (!mimeview->messageview->mainwin) return FALSE;
	summaryview = mimeview->messageview->mainwin->summaryview;
	return summary_pass_key_press_event(summaryview, event);
}

static gboolean icon_popup_menu(GtkWidget *widget, gpointer data)
{
	MimeView *mimeview = (MimeView *)data;
	MimeInfo *partinfo = g_object_get_data(G_OBJECT(widget), "partinfo");

	g_object_set_data(G_OBJECT(mimeview->popupmenu),
			  "pop_partinfo", partinfo);
	gtk_menu_popup_at_pointer(GTK_MENU(mimeview->popupmenu), NULL);
	return TRUE;
}

static void icon_list_append_icon (MimeView *mimeview, MimeInfo *mimeinfo) 
{
	GtkWidget *pixmap = NULL;
	GtkWidget *grid;
	GtkWidget *button;
	gchar *tip;
	gchar *tiptmp;
	const gchar *desc = NULL; 
	gchar *sigshort = NULL;
	gchar *content_type;
	StockPixmap stockp;
	MimeInfo *partinfo;
	MimeInfo *siginfo = NULL;
	MimeInfo *encrypted = NULL;
#ifdef GENERIC_UMPC
	GtkRequisition r;
#endif
	
	if (!prefs_common.show_inline_attachments && mimeinfo->id)
		return;

	grid = mimeview->icon_grid;
	mimeview->icon_count++;
	button = gtk_event_box_new();

	g_signal_connect(G_OBJECT(button), "motion-notify-event",
			 G_CALLBACK(mimeview_visi_notify), mimeview);
	g_signal_connect(G_OBJECT(button), "leave-notify-event",
			 G_CALLBACK(mimeview_leave_notify), mimeview);
	g_signal_connect(G_OBJECT(button), "enter-notify-event",
			 G_CALLBACK(mimeview_enter_notify), mimeview);

	gtk_container_set_border_width(GTK_CONTAINER(button), 2);
	g_object_set_data(G_OBJECT(button), "icon_number", 
			  GINT_TO_POINTER(mimeview->icon_count));
	g_object_set_data(G_OBJECT(button), "partinfo", 
		          mimeinfo);

	switch (mimeinfo->type) {
		
	case MIMETYPE_TEXT:
		if (mimeinfo->subtype && !g_ascii_strcasecmp(mimeinfo->subtype, "html"))
			stockp = STOCK_PIXMAP_MIME_TEXT_HTML;
		else if  (mimeinfo->subtype && !g_ascii_strcasecmp(mimeinfo->subtype, "enriched"))
			stockp = STOCK_PIXMAP_MIME_TEXT_ENRICHED;
		else if  (mimeinfo->subtype && !g_ascii_strcasecmp(mimeinfo->subtype, "calendar"))
			stockp = STOCK_PIXMAP_MIME_TEXT_CALENDAR;
		else if  (mimeinfo->subtype && (!g_ascii_strcasecmp(mimeinfo->subtype, "x-patch")
				|| !g_ascii_strcasecmp(mimeinfo->subtype, "x-diff")))
			stockp = STOCK_PIXMAP_MIME_TEXT_PATCH;
		else
			stockp = STOCK_PIXMAP_MIME_TEXT_PLAIN;
		break;
	case MIMETYPE_MESSAGE:
		stockp = STOCK_PIXMAP_MIME_MESSAGE;
		break;
	case MIMETYPE_APPLICATION:
		if (mimeinfo->subtype && (!g_ascii_strcasecmp(mimeinfo->subtype, "pgp-signature")
		    || !g_ascii_strcasecmp(mimeinfo->subtype, "x-pkcs7-signature")
		    || !g_ascii_strcasecmp(mimeinfo->subtype, "pkcs7-signature")))
			stockp = STOCK_PIXMAP_MIME_PGP_SIG;
		else if (mimeinfo->subtype && !g_ascii_strcasecmp(mimeinfo->subtype, "pdf"))
			stockp = STOCK_PIXMAP_MIME_PDF;
		else if  (mimeinfo->subtype && !g_ascii_strcasecmp(mimeinfo->subtype, "postscript"))
			stockp = STOCK_PIXMAP_MIME_PS;
		else
			stockp = STOCK_PIXMAP_MIME_APPLICATION;
		break;
	case MIMETYPE_IMAGE:
		stockp = STOCK_PIXMAP_MIME_IMAGE;
		break;
	case MIMETYPE_AUDIO:
		stockp = STOCK_PIXMAP_MIME_AUDIO;
		break;
	default:
		stockp = STOCK_PIXMAP_MIME_UNKNOWN;
		break;
	}
	
	partinfo = mimeinfo;
	while (partinfo != NULL) {
		if (privacy_mimeinfo_is_signed(partinfo)) {
			siginfo = partinfo;
			break;
		}
		if (privacy_mimeinfo_is_encrypted(partinfo)) {
			encrypted = partinfo;
			break;
		}
		partinfo = procmime_mimeinfo_parent(partinfo);
	}	

	if (siginfo != NULL) {
		switch (privacy_mimeinfo_get_sig_status(siginfo)) {
		case SIGNATURE_UNCHECKED:
		case SIGNATURE_CHECK_ERROR:
		case SIGNATURE_CHECK_FAILED:
		case SIGNATURE_CHECK_TIMEOUT:
			pixmap = stock_pixmap_widget_with_overlay(stockp,
			    STOCK_PIXMAP_PRIVACY_EMBLEM_SIGNED, OVERLAY_BOTTOM_RIGHT, 6, 3);
			break;
		case SIGNATURE_OK:
			pixmap = stock_pixmap_widget_with_overlay(stockp,
			    STOCK_PIXMAP_PRIVACY_EMBLEM_PASSED, OVERLAY_BOTTOM_RIGHT, 6, 3);
			break;
		case SIGNATURE_WARN:
		case SIGNATURE_KEY_EXPIRED:
			pixmap = stock_pixmap_widget_with_overlay(stockp,
			    STOCK_PIXMAP_PRIVACY_EMBLEM_WARN, OVERLAY_BOTTOM_RIGHT, 6, 3);
			break;
		case SIGNATURE_INVALID:
			pixmap = stock_pixmap_widget_with_overlay(stockp,
			    STOCK_PIXMAP_PRIVACY_EMBLEM_FAILED, OVERLAY_BOTTOM_RIGHT, 6, 3);
			break;
		}
		sigshort = privacy_mimeinfo_get_sig_info(siginfo, FALSE);
	} else if (encrypted != NULL) {
			pixmap = stock_pixmap_widget_with_overlay(stockp,
			    STOCK_PIXMAP_PRIVACY_EMBLEM_ENCRYPTED, OVERLAY_BOTTOM_RIGHT, 6, 3);		
	} else {
		pixmap = stock_pixmap_widget_with_overlay(stockp, 0,
							  OVERLAY_NONE, 6, 3);
	}
	gtk_container_add(GTK_CONTAINER(button), pixmap);
	if (!desc) {
		if (prefs_common.attach_desc)
			desc = get_part_description(mimeinfo);
		else
			desc = get_part_name(mimeinfo);
	}

	content_type = procmime_get_content_type_str(mimeinfo->type,
						     mimeinfo->subtype);

	tip = g_strconcat("<b>", _("Type:"), "  </b>", content_type,
			  "\n<b>", _("Size:"), " </b>",
			  to_human_readable((goffset)mimeinfo->length), NULL);
	g_free(content_type);
	if (desc && *desc) {
		gchar *tmp = NULL, *escaped = NULL;
		if (!g_utf8_validate(desc, -1, NULL)) {
			tmp = conv_filename_to_utf8(desc);
		} else {
			tmp = g_strdup(desc);
		}
		escaped = g_markup_escape_text(tmp,-1);
		
		tiptmp = g_strconcat(tip, "\n<b>",
				prefs_common.attach_desc && mimeinfo->description ?
				_("Description:") : _("Filename:"),
				" </b>", escaped, NULL);
		g_free(tip);
		tip = tiptmp;
		g_free(tmp);
		g_free(escaped);
	}
	if (sigshort && *sigshort) {
		gchar *sigshort_escaped =
			g_markup_escape_text(sigshort, -1);

		tiptmp = g_strjoin("\n", tip, sigshort_escaped, NULL);
		g_free(tip);
		g_free(sigshort_escaped);
		tip = tiptmp;
	}

	gtk_widget_set_tooltip_markup(button, tip);
	g_free(tip);
	gtk_widget_show_all(button);
	gtk_drag_source_set(button, GDK_BUTTON1_MASK|GDK_BUTTON3_MASK, 
			    mimeview_mime_types, 1, GDK_ACTION_COPY);

	g_signal_connect(G_OBJECT(button), "popup-menu",
			 G_CALLBACK(icon_popup_menu), mimeview);
	g_signal_connect(G_OBJECT(button), "button_release_event", 
			 G_CALLBACK(icon_clicked_cb), mimeview);
	g_signal_connect(G_OBJECT(button), "key_press_event", 
			 G_CALLBACK(icon_key_pressed), mimeview);
	g_signal_connect(G_OBJECT(button), "drag_data_get",
			 G_CALLBACK(mimeview_drag_data_get), mimeview);
	gtk_container_add(GTK_CONTAINER(grid), button);
#ifdef GENERIC_UMPC
	gtk_widget_get_preferred_size(pixmap, &r, NULL);
	gtk_widget_set_size_request(button, -1, r.height + 4);
#endif

}

static void icon_list_clear (MimeView *mimeview)
{
	GList     *child, *orig;
	GtkAdjustment *adj;
		
	orig = gtk_container_get_children(GTK_CONTAINER(mimeview->icon_grid));
	for (child = orig; child != NULL; child = g_list_next(child)) {
		gtk_container_remove(GTK_CONTAINER(mimeview->icon_grid), 
				     GTK_WIDGET(child->data));
	}
	g_list_free(orig);
	mimeview->icon_count = 0;
	adj  = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(mimeview->icon_scroll));
	gtk_adjustment_set_value(adj, gtk_adjustment_get_lower(adj));
}

/*!
 *\brief        Used to 'click' the next or previous icon.
 *
 *\return       true if the icon 'number' exists and was selected.
 */
static void icon_scroll_size_allocate_cb(GtkWidget *widget, 
					 GtkAllocation *size, MimeView *mimeview)
{
	GtkAllocation grid_size;
	GtkAllocation layout_size;
	GtkAdjustment *adj;
	guint width;
	guint height;

	adj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(mimeview->icon_scroll));

	gtk_widget_get_allocation(mimeview->icon_grid, &grid_size);
	gtk_widget_get_allocation(mimeview->icon_scroll, &layout_size);
		
	gtk_layout_get_size(GTK_LAYOUT(mimeview->icon_scroll), &width, &height);
	gtk_layout_set_size(GTK_LAYOUT(mimeview->icon_scroll), 
			    MIN(grid_size.width, layout_size.width), 
			    MAX(grid_size.height, layout_size.height));
	gtk_adjustment_set_step_increment(adj, 10);
}

static void icon_list_create(MimeView *mimeview, MimeInfo *mimeinfo)
{
	gint min_width, width;

	cm_return_if_fail(mimeinfo != NULL);

	while (mimeinfo != NULL) {
		if (mimeinfo->type != MIMETYPE_MULTIPART)
			icon_list_append_icon(mimeview, mimeinfo);
		if (mimeinfo->node->children != NULL)
			icon_list_create(mimeview, 
				(MimeInfo *) mimeinfo->node->children->data);
		mimeinfo = mimeinfo->node->next != NULL 
			 ? (MimeInfo *) mimeinfo->node->next->data 
			 : NULL;
	}
	gtk_widget_get_preferred_width(mimeview->icon_mainbox, &min_width, &width);
	if (min_width < width) {
		gtk_widget_set_size_request(mimeview->icon_mainbox, 
					    min_width, -1);
	}
	if (mimeview->opened)
		icon_list_toggle_by_mime_info(mimeview,
			mimeview_get_node_part(mimeview, mimeview->opened));
}

static void icon_list_toggle_by_mime_info (MimeView	*mimeview,
					   MimeInfo	*mimeinfo)
{
	GList *children, *child;
	
	children = gtk_container_get_children(GTK_CONTAINER(mimeview->icon_grid));
	for (child = children; child != NULL; child = g_list_next(child)) {
		gboolean *highlight = NULL;
		GtkWidget *icon = gtk_bin_get_child(GTK_BIN(child->data));

		if (!GTK_IS_EVENT_BOX(child->data))
			continue;

		highlight = g_object_get_data(G_OBJECT(icon), "highlight");
		*highlight = (g_object_get_data(G_OBJECT(child->data),
			 	      "partinfo") == (gpointer)mimeinfo);

		gtk_widget_queue_draw(icon);
	}
	g_list_free(children);
}

static void ctree_size_allocate_cb(GtkWidget *widget, GtkAllocation *allocation,
				    MimeView *mimeview)
{
	prefs_common.mimeview_tree_height = allocation->height;
}

static gint mime_toggle_button_cb(GtkWidget *button, GdkEventButton *event,
				    MimeView *mimeview)
{
	g_object_ref(button); 

	mimeview_leave_notify(button, NULL, NULL);

	mimeview->ctree_mode = !mimeview->ctree_mode;
	if (mimeview->ctree_mode) {
		gtk_image_set_from_icon_name(GTK_IMAGE(mimeview->arrow),
					      "pan-end-symbolic", GTK_ICON_SIZE_MENU);
		gtk_widget_hide(mimeview->icon_mainbox);
		gtk_widget_show(mimeview->ctree_mainbox);
		gtk_paned_set_position(GTK_PANED(mimeview->paned),
					prefs_common.mimeview_tree_height);

		gtk_container_remove(GTK_CONTAINER(mimeview->icon_mainbox), 
				     button);
		gtk_box_pack_end(GTK_BOX(mimeview->ctree_mainbox), 
				   button, FALSE, FALSE, 0);
	} else {
		gtk_image_set_from_icon_name(GTK_IMAGE(mimeview->arrow),
					      "pan-start-symbolic", GTK_ICON_SIZE_MENU);
		gtk_widget_hide(mimeview->ctree_mainbox);
		gtk_widget_show(mimeview->icon_mainbox);
		gtk_paned_set_position(GTK_PANED(mimeview->paned), 0);

		gtk_container_remove(GTK_CONTAINER(mimeview->ctree_mainbox), 
				     button);
		gtk_box_pack_start(GTK_BOX(mimeview->icon_mainbox), 
				   button, FALSE, FALSE, 0);
		gtk_box_reorder_child(GTK_BOX(gtk_widget_get_parent(button)), button, 0);
		if (mimeview->opened)
			icon_list_toggle_by_mime_info(mimeview,
					mimeview_get_node_part(mimeview, mimeview->opened));
		summary_grab_focus(mimeview->mainwin->summaryview);
	}
	g_object_unref(button);
	return TRUE;
}

void mimeview_update (MimeView *mimeview)
{
	if (mimeview && mimeview->mimeinfo) {
		icon_list_clear(mimeview);
		icon_list_create(mimeview, mimeview->mimeinfo);
	}
}

void mimeview_handle_cmd(MimeView *mimeview, const gchar *cmd, GdkEventButton *event, gpointer data)
{
	MessageView *msgview = NULL;
	MainWindow *mainwin = NULL;
	
	if (!cmd)
		return;
	
	msgview = mimeview->messageview;
	if (!msgview)
		return;
		
	mainwin = msgview->mainwin;
	if (!mainwin)
		return;
		
	g_object_set_data(G_OBJECT(mimeview->popupmenu),
			  "pop_partinfo", NULL);

	if (!strcmp(cmd, "cm://view_log"))
		log_window_show(mainwin->logwin);
	else if (!strcmp(cmd, "cm://save_as"))
		mimeview_save_as(mimeview);
	else if (!strcmp(cmd, "cm://display_as_text"))
		mimeview_display_as_text(mimeview);
#ifndef G_OS_WIN32
	else if (!strcmp(cmd, "cm://open_with"))
		mimeview_open_with(mimeview);
#endif
	else if (!strcmp(cmd, "cm://open"))
		mimeview_launch(mimeview, NULL);
	else if (!strcmp(cmd, "cm://select_attachment") && data != NULL) {
		icon_list_toggle_by_mime_info(mimeview, (MimeInfo *)data);
		icon_selected(mimeview, -1, (MimeInfo *)data);
	} else if (!strcmp(cmd, "cm://open_attachment") && data != NULL) {
		mimeview_launch(mimeview, (MimeInfo *)data);
	} else if (!strcmp(cmd, "cm://menu_attachment") && data != NULL) {
		mimeview->spec_part = (MimeInfo *)data;
		part_button_pressed(mimeview, event, (MimeInfo *)data);
	} else if (!strncmp(cmd, "cm://search_tags:", strlen("cm://search_tags:"))) {
		const gchar *tagname = cmd + strlen("cm://search_tags:");
		gchar *buf = g_strdup_printf("tag matchcase \"%s\"", tagname);
		gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON(mimeview->messageview->mainwin->summaryview->toggle_search), 
				TRUE);
		quicksearch_set(mimeview->messageview->mainwin->summaryview->quicksearch, 
				ADVANCED_SEARCH_EXTENDED, buf);
		g_free(buf);
	}
}

gboolean mimeview_scroll_page(MimeView *mimeview, gboolean up)
{
	if (mimeview->type == MIMEVIEW_TEXT)
		return textview_scroll_page(mimeview->textview, up);
	else if (mimeview->mimeviewer) {
		MimeViewer *mimeviewer = mimeview->mimeviewer;
		if (mimeviewer->scroll_page)
			return mimeviewer->scroll_page(mimeviewer, up);
	}
	return TRUE;
}

void mimeview_scroll_one_line(MimeView *mimeview, gboolean up)
{
	if (mimeview->type == MIMEVIEW_TEXT)
		textview_scroll_one_line(mimeview->textview, up);
	else if (mimeview->mimeviewer) {
		MimeViewer *mimeviewer = mimeview->mimeviewer;
		if (mimeviewer->scroll_one_line)
			mimeviewer->scroll_one_line(mimeviewer, up);
	}
}
