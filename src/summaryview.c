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

#include "defs.h"

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkpixmap.h>
#include <gtk/gtkctree.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktext.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkstyle.h>
#include <gtk/gtkarrow.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkstatusbar.h>
#include <gtk/gtkmenuitem.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/stat.h>

#include "intl.h"
#include "main.h"
#include "menu.h"
#include "mainwindow.h"
#include "folderview.h"
#include "summaryview.h"
#include "messageview.h"
#include "foldersel.h"
#include "procmsg.h"
#include "procheader.h"
#include "headerwindow.h"
#include "sourcewindow.h"
#include "prefs_common.h"
#include "account.h"
#include "compose.h"
#include "utils.h"
#include "gtkutils.h"
#include "filesel.h"
#include "manage_window.h"
#include "alertpanel.h"
#include "inputdialog.h"
#include "statusbar.h"
#include "filter.h"
#include "folder.h"
#include "addressbook.h"
#include "addr_compl.h"
#include "scoring.h"
#include "prefs_folder_item.h"
#include "filtering.h"
#include "labelcolors.h"

#include "pixmaps/dir-open.xpm"
#include "pixmaps/mark.xpm"
#include "pixmaps/deleted.xpm"
#include "pixmaps/new.xpm"
#include "pixmaps/unread.xpm"
#include "pixmaps/replied.xpm"
#include "pixmaps/forwarded.xpm"
#include "pixmaps/clip.xpm"
#include "pixmaps/ignorethread.xpm"

#define STATUSBAR_PUSH(mainwin, str) \
{ \
	gtk_statusbar_push(GTK_STATUSBAR(mainwin->statusbar), \
			   mainwin->summaryview_cid, str); \
	gtkut_widget_wait_for_draw(mainwin->hbox_stat); \
}

#define STATUSBAR_POP(mainwin) \
{ \
	gtk_statusbar_pop(GTK_STATUSBAR(mainwin->statusbar), \
			  mainwin->summaryview_cid); \
}

#define SUMMARY_COL_MARK_WIDTH		10
#define SUMMARY_COL_UNREAD_WIDTH	13
#define SUMMARY_COL_MIME_WIDTH		10

static GdkFont *boldfont;
static GdkFont *smallfont;

static GtkStyle *bold_style;
static GtkStyle *bold_marked_style;
static GtkStyle *bold_deleted_style;
static GtkStyle *small_style;
static GtkStyle *small_marked_style;
static GtkStyle *small_deleted_style;

static GdkPixmap *folderxpm;
static GdkBitmap *folderxpmmask;

static GdkPixmap *markxpm;
static GdkBitmap *markxpmmask;
static GdkPixmap *deletedxpm;
static GdkBitmap *deletedxpmmask;

static GdkPixmap *newxpm;
static GdkBitmap *newxpmmask;
static GdkPixmap *unreadxpm;
static GdkBitmap *unreadxpmmask;
static GdkPixmap *repliedxpm;
static GdkBitmap *repliedxpmmask;
static GdkPixmap *forwardedxpm;
static GdkBitmap *forwardedxpmmask;
static GdkPixmap *ignorethreadxpm;
static GdkBitmap *ignorethreadxpmmask;

static GdkPixmap *clipxpm;
static GdkBitmap *clipxpmmask;

static void summary_free_msginfo_func	(GtkCTree		*ctree,
					 GtkCTreeNode		*node,
					 gpointer		 data);
static void summary_set_marks_func	(GtkCTree		*ctree,
					 GtkCTreeNode		*node,
					 gpointer		 data);
static void summary_write_cache_func	(GtkCTree		*ctree,
					 GtkCTreeNode		*node,
					 gpointer		 data);

static void summary_set_menu_sensitive	(SummaryView		*summaryview);
static void summary_set_add_sender_menu	(SummaryView		*summaryview);

static void summary_select_node		(SummaryView		*summaryview,
					 GtkCTreeNode		*node,
					 gboolean		 display_msg);

static guint summary_get_msgnum		(SummaryView		*summaryview,
					 GtkCTreeNode		*node);

static GtkCTreeNode *summary_find_next_unread_msg
					(SummaryView		*summaryview,
					 GtkCTreeNode		*current_node);
static GtkCTreeNode *summary_find_next_marked_msg
					(SummaryView		*summaryview,
					 GtkCTreeNode		*current_node);
static GtkCTreeNode *summary_find_prev_marked_msg
					(SummaryView		*summaryview,
					 GtkCTreeNode		*current_node);
static GtkCTreeNode *summary_find_msg_by_msgnum
					(SummaryView		*summaryview,
					 guint			 msgnum);
#if 0
static GtkCTreeNode *summary_find_prev_unread_msg
					(SummaryView		*summaryview,
					 GtkCTreeNode		*current_node);
#endif

static void summary_update_status	(SummaryView		*summaryview);

/* display functions */
static void summary_status_show		(SummaryView		*summaryview);
static void summary_set_ctree_from_list	(SummaryView		*summaryview,
					 GSList			*mlist);
static void summary_set_header		(gchar			*text[],
					 MsgInfo		*msginfo);
static void summary_display_msg		(SummaryView		*summaryview,
					 GtkCTreeNode		*row,
					 gboolean		 new_window);
static void summary_toggle_view		(SummaryView		*summaryview);
static void summary_set_row_marks	(SummaryView		*summaryview,
					 GtkCTreeNode		*row);

/* message handling */
static void summary_mark_row		(SummaryView		*summaryview,
					 GtkCTreeNode		*row);
static void summary_mark_row_as_read	(SummaryView		*summaryview,
					 GtkCTreeNode		*row);
static void summary_mark_row_as_unread	(SummaryView		*summaryview,
					 GtkCTreeNode		*row);
static void summary_delete_row		(SummaryView		*summaryview,
					 GtkCTreeNode		*row);
static void summary_unmark_row		(SummaryView		*summaryview,
					 GtkCTreeNode		*row);
static void summary_move_row_to		(SummaryView		*summaryview,
					 GtkCTreeNode		*row,
					 FolderItem		*to_folder);
static void summary_copy_row_to		(SummaryView		*summaryview,
					 GtkCTreeNode		*row,
					 FolderItem		*to_folder);

static void summary_delete_duplicated_func
					(GtkCTree		*ctree,
					 GtkCTreeNode		*node,
					 SummaryView		*summaryview);

static void summary_execute_move	(SummaryView		*summaryview);
static void summary_execute_move_func	(GtkCTree		*ctree,
					 GtkCTreeNode		*node,
					 gpointer		 data);
static void summary_execute_copy	(SummaryView		*summaryview);
static void summary_execute_copy_func	(GtkCTree		*ctree,
					 GtkCTreeNode		*node,
					 gpointer		 data);
static void summary_execute_delete	(SummaryView		*summaryview);
static void summary_execute_delete_func	(GtkCTree		*ctree,
					 GtkCTreeNode		*node,
					 gpointer		 data);
static void summary_ignore_thread(SummaryView *summaryview);
static void summary_unignore_thread(SummaryView *summaryview);

static void summary_unthread_for_exec		(SummaryView	*summaryview);
static void summary_unthread_for_exec_func	(GtkCTree	*ctree,
						 GtkCTreeNode	*node,
						 gpointer	 data);

static void summary_filter_func		(GtkCTree		*ctree,
					 GtkCTreeNode		*node,
					 gpointer		 data);

/* callback functions */
static void summary_toggle_pressed	(GtkWidget		*eventbox,
					 GdkEventButton		*event,
					 SummaryView		*summaryview);
static void summary_button_pressed	(GtkWidget		*ctree,
					 GdkEventButton		*event,
					 SummaryView		*summaryview);
static void summary_button_released	(GtkWidget		*ctree,
					 GdkEventButton		*event,
					 SummaryView		*summaryview);
static void summary_key_pressed		(GtkWidget		*ctree,
					 GdkEventKey		*event,
					 SummaryView		*summaryview);
static void summary_open_row		(GtkSCTree		*sctree,
					 SummaryView		*summaryview);
static void summary_tree_expanded	(GtkCTree		*ctree,
					 GtkCTreeNode		*node,
					 SummaryView		*summaryview);
static void summary_tree_collapsed	(GtkCTree		*ctree,
					 GtkCTreeNode		*node,
					 SummaryView		*summaryview);
static void summary_selected		(GtkCTree		*ctree,
					 GtkCTreeNode		*row,
					 gint			 column,
					 SummaryView		*summaryview);
static void summary_col_resized		(GtkCList		*clist,
					 gint			 column,
					 gint			 width,
					 SummaryView		*summaryview);
static void summary_reply_cb		(SummaryView		*summaryview,
					 guint			 action,
					 GtkWidget		*widget);
static void summary_show_all_header_cb	(SummaryView		*summaryview,
					 guint			 action,
					 GtkWidget		*widget);

static void summary_num_clicked		(GtkWidget		*button,
					 SummaryView		*summaryview);
static void summary_score_clicked       (GtkWidget *button,
					 SummaryView *summaryview);
static void summary_size_clicked	(GtkWidget		*button,
					 SummaryView		*summaryview);
static void summary_date_clicked	(GtkWidget		*button,
					 SummaryView		*summaryview);
static void summary_from_clicked	(GtkWidget		*button,
					 SummaryView		*summaryview);
static void summary_subject_clicked	(GtkWidget		*button,
					 SummaryView		*summaryview);
static void summary_mark_clicked	(GtkWidget		*button,
					 SummaryView		*summaryview);

static void summary_start_drag		(GtkWidget        *widget, 
					 int button,
					 GdkEvent *event,
					 SummaryView      *summaryview);
static void summary_drag_data_get       (GtkWidget        *widget,
					 GdkDragContext   *drag_context,
					 GtkSelectionData *selection_data,
					 guint             info,
					 guint             time,
					 SummaryView      *summaryview);

/* custom compare functions for sorting */

static gint summary_cmp_by_num		(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);
static gint summary_cmp_by_size		(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);
static gint summary_cmp_by_date		(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);
static gint summary_cmp_by_from		(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);
static gint summary_cmp_by_subject	(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);
static gint summary_cmp_by_score	(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);
static gint summary_cmp_by_label	(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);

#if MARK_ALL_READ
static void summary_mark_all_read (SummaryView *summaryview);					 
#endif

GtkTargetEntry summary_drag_types[1] =
{
	{"text/plain", GTK_TARGET_SAME_APP, TARGET_DUMMY}
};

static GtkItemFactoryEntry summary_popup_entries[] =
{
	{N_("/M_ove..."),		NULL, summary_move_to,	0, NULL},
	{N_("/_Copy..."),		NULL, summary_copy_to,	0, NULL},
	{N_("/_Delete"),		NULL, summary_delete,	0, NULL},
	{N_("/E_xecute"),		NULL, summary_execute,	0, NULL},
	{N_("/_Mark"),			NULL, NULL,		0, "<Branch>"},
	{N_("/_Mark/_Mark"),		NULL, summary_mark,	0, NULL},
	{N_("/_Mark/_Unmark"),		NULL, summary_unmark,	0, NULL},
	{N_("/_Mark/---"),		NULL, NULL,		0, "<Separator>"},
	{N_("/_Mark/Mark as unr_ead"),	NULL, summary_mark_as_unread, 0, NULL},
	{N_("/_Mark/Mark as rea_d"),	NULL, summary_mark_as_read, 0, NULL},
#if MARK_ALL_READ	
	{N_("/_Mark/Mark all read"),    NULL, summary_mark_all_read, 0, NULL},
#endif	
	{N_("/_Mark/Ignore thread"),	NULL, summary_ignore_thread, 0, NULL},
	{N_("/_Mark/Unignore thread"),	NULL, summary_unignore_thread, 0, NULL},

	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/_Reply"),			NULL, summary_reply_cb,	COMPOSE_REPLY, NULL},
	{N_("/Repl_y to sender"),	NULL, summary_reply_cb,	COMPOSE_REPLY_TO_SENDER, NULL},
	{N_("/Follow-up and reply to"),	NULL, summary_reply_cb,	COMPOSE_FOLLOWUP_AND_REPLY_TO, NULL},
	{N_("/Reply to a_ll"),		NULL, summary_reply_cb,	COMPOSE_REPLY_TO_ALL, NULL},
	{N_("/_Forward"),		NULL, summary_reply_cb, COMPOSE_FORWARD, NULL},
	{N_("/Forward as a_ttachment"),
					NULL, summary_reply_cb, COMPOSE_FORWARD_AS_ATTACH, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/Add sender to address _book"),
					NULL, NULL,		0, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/Open in new _window"),	NULL, summary_open_msg,	0, NULL},
	{N_("/View so_urce"),		NULL, summary_view_source, 0, NULL},
	{N_("/Show all _header"),	NULL, summary_show_all_header_cb, 0, NULL},
	{N_("/Re-_edit"),		NULL, summary_reedit,   0, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/_Save as..."),		NULL, summary_save_as,	0, NULL},
	{N_("/_Print..."),		NULL, summary_print,	0, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/Select _all"),		NULL, summary_select_all, 0, NULL}
};

#define LABEL_COLORS_ELEMS labelcolors_get_color_count() 

static void label_menu_item_activate_cb(GtkWidget *widget, gpointer data)
{
	guint color = GPOINTER_TO_UINT(data);
	SummaryView *view = gtk_object_get_data(GTK_OBJECT(widget), "view");

	g_return_if_fail(view);

	/* "dont_toggle" state set? */
	if (gtk_object_get_data(GTK_OBJECT(view->label_menu), "dont_toggle"))
		return;

	summary_set_label(view, color, NULL);
}

/* summary_set_label_color() - labelcolor parameter is the color *flag*
 * for the messsage; not the color index */
void summary_set_label_color(GtkCTree *ctree, GtkCTreeNode *node,
			     guint labelcolor)
{
	GdkColor  color;
	GtkStyle *style, *prev_style, *ctree_style;
	MsgInfo  *msginfo;
	gint     color_index;

	color_index = labelcolor == 0 ? -1 :  (gint) labelcolor - 1;

	ctree_style = gtk_widget_get_style(GTK_WIDGET(ctree));

	prev_style = gtk_ctree_node_get_row_style(ctree, node);

	if (!prev_style)
		prev_style = ctree_style;

	style = gtk_style_copy(prev_style);

	if (color_index < 0 || color_index >= LABEL_COLORS_ELEMS) {
		color_index = 0;
		color.red = ctree_style->fg[GTK_STATE_NORMAL].red;
		color.green = ctree_style->fg[GTK_STATE_NORMAL].green;
		color.blue = ctree_style->fg[GTK_STATE_NORMAL].blue;
		style->fg[GTK_STATE_NORMAL] = color;

		color.red = ctree_style->fg[GTK_STATE_SELECTED].red;
		color.green = ctree_style->fg[GTK_STATE_SELECTED].green;
		color.blue = ctree_style->fg[GTK_STATE_SELECTED].blue;
		style->fg[GTK_STATE_SELECTED] = color;
		gtk_ctree_node_set_row_style(ctree, node, style);
	}
	else {
		color = labelcolors_get_color(color_index);
	}		

	msginfo = gtk_ctree_node_get_row_data(ctree, node);

	MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_LABEL);
	MSG_SET_LABEL_VALUE(msginfo->flags, labelcolor);

	if ( style ) {
		style->fg[GTK_STATE_NORMAL] = color;
		style->fg[GTK_STATE_SELECTED] = color;
		gtk_ctree_node_set_row_style(ctree, node, style);
	}
}

void summary_set_label(SummaryView *summaryview, guint labelcolor, GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCList *clist = GTK_CLIST(summaryview->ctree);
	GList *cur;
  
	for (cur = clist->selection; cur != NULL; cur = cur->next)
		summary_set_label_color(ctree, GTK_CTREE_NODE(cur->data), labelcolor);
}

static void label_menu_item_activate_item_cb(GtkMenuItem *label_menu_item, gpointer data)
{
	SummaryView  *summaryview;
	GtkMenuShell *label_menu;
	GtkCheckMenuItem **items;
	int  n;
	GList *cur, *sel;

	summaryview = (SummaryView *) data;
	g_return_if_fail(summaryview);
	if (NULL == (sel = GTK_CLIST(summaryview->ctree)->selection))
		return;
	
	label_menu = GTK_MENU_SHELL(summaryview->label_menu);
	g_return_if_fail(label_menu);

	items = alloca( (LABEL_COLORS_ELEMS + 1) * sizeof(GtkWidget *));
	g_return_if_fail(items);

	/* NOTE: don't return prematurely because we set the "dont_toggle" state
	 * for check menu items */
	gtk_object_set_data(GTK_OBJECT(label_menu), "dont_toggle", GINT_TO_POINTER(1));

	/* clear items. get item pointers. */
	for (n = 0, cur = label_menu->children; cur != NULL; cur = cur->next) {
		if (GTK_IS_CHECK_MENU_ITEM(cur->data)) {
			gtk_check_menu_item_set_state(GTK_CHECK_MENU_ITEM(cur->data), FALSE);
			items[n] = GTK_CHECK_MENU_ITEM(cur->data);
			n++;
		}
	}

	if (n == (LABEL_COLORS_ELEMS + 1)) {
		/* iterate all messages and set the state of the appropriate items */
		for (; sel != NULL; sel = sel->next) {
			MsgInfo *msginfo = gtk_ctree_node_get_row_data(GTK_CTREE(summaryview->ctree),
						GTK_CTREE_NODE(sel->data));
			gint menu_item;   			
			if (msginfo) {
				menu_item = MSG_GET_LABEL_VALUE(msginfo->flags);
				if (!items[menu_item]->active)
					gtk_check_menu_item_set_state(items[menu_item], TRUE);
			}
		}
	}
	else 
		g_warning("invalid number of color elements (%d)\n", n);
	
	/* reset "dont_toggle" state */
	gtk_object_set_data(GTK_OBJECT(label_menu), "dont_toggle", GINT_TO_POINTER(0));
}

static void summary_create_label_menu(SummaryView *summaryview)
{
	const gint LABEL_MENU_POS = 5;
	GtkWidget *label_menu_item;
	GtkWidget *label_menu;
	GtkWidget *item;
	gint       i;

	label_menu_item = gtk_menu_item_new_with_label(_("Label"));
	gtk_menu_insert(GTK_MENU(summaryview->popupmenu), label_menu_item, LABEL_MENU_POS);
	gtk_signal_connect(GTK_OBJECT(label_menu_item), "activate",
		GTK_SIGNAL_FUNC(label_menu_item_activate_item_cb), summaryview);
		
	gtk_widget_show(label_menu_item);
	summaryview->label_menu_item = label_menu_item;

	label_menu = gtk_menu_new();

	/* create sub items. for the menu item activation callback we pass the 
	 * index of label_colors[] as data parameter. for the None color we pass
	 * an invalid (high) value. also we attach a data pointer so we can
	 * always get back the SummaryView pointer. */
	 
	item = gtk_check_menu_item_new_with_label(_("None"));
	gtk_menu_append(GTK_MENU(label_menu), item);
	gtk_signal_connect(GTK_OBJECT(item), "activate",  
		GTK_SIGNAL_FUNC(label_menu_item_activate_cb),
		GUINT_TO_POINTER(0));
	gtk_object_set_data(GTK_OBJECT(item), "view", summaryview);	
	gtk_widget_show(item);
	
	item = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(label_menu), item);
	gtk_widget_show(item);

	/* create pixmap/label menu items */
	for (i = 0; i < LABEL_COLORS_ELEMS; i++) {
		item = labelcolors_create_check_color_menu_item(i);
		gtk_menu_append(GTK_MENU(label_menu), item);
		gtk_signal_connect(GTK_OBJECT(item), "activate", 
				   GTK_SIGNAL_FUNC(label_menu_item_activate_cb),
				   GUINT_TO_POINTER(i + 1));
		gtk_object_set_data(GTK_OBJECT(item), "view", summaryview);
		gtk_widget_show(item);
	}
	
	gtk_widget_show(label_menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(label_menu_item), label_menu);
	summaryview->label_menu = label_menu;	
}

SummaryView *summary_create(void)
{
	SummaryView *summaryview;
	gchar *titles[N_SUMMARY_COLS] = {_("M"), _("U")};
	GtkWidget *vbox;
	GtkWidget *scrolledwin;
	GtkWidget *ctree;
	GtkWidget *hbox;
	GtkWidget *statlabel_folder;
	GtkWidget *statlabel_select;
	GtkWidget *statlabel_msgs;
	GtkWidget *toggle_eventbox;
	GtkWidget *toggle_arrow;
	GtkWidget *popupmenu;
	GtkItemFactory *popupfactory;
	gint n_entries;
	gint i;

	debug_print(_("Creating summary view...\n"));
	summaryview = g_new0(SummaryView, 1);

	vbox = gtk_vbox_new(FALSE, 2);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(vbox), scrolledwin, TRUE, TRUE, 0);
	gtk_widget_set_usize(vbox,
			     prefs_common.summaryview_width,
			     prefs_common.summaryview_height);

	if (prefs_common.trans_hdr) {
		titles[S_COL_NUMBER]  = _("No.");
		titles[S_COL_DATE]    = _("Date");
		titles[S_COL_FROM]    = _("From");
		titles[S_COL_SUBJECT] = _("Subject");
	} else {
		titles[S_COL_NUMBER]  = "No.";
		titles[S_COL_DATE]    = "Date";
		titles[S_COL_FROM]    = "From";
		titles[S_COL_SUBJECT] = "Subject";
	}
	titles[S_COL_SIZE]  = _("Size");
	titles[S_COL_SCORE] = _("Score");

	ctree = gtk_sctree_new_with_titles(N_SUMMARY_COLS, S_COL_SUBJECT, titles);
	gtk_scrolled_window_set_hadjustment(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_CLIST(ctree)->hadjustment);
	gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_CLIST(ctree)->vadjustment);
	gtk_container_add(GTK_CONTAINER(scrolledwin), ctree);
	gtk_clist_set_selection_mode(GTK_CLIST(ctree), GTK_SELECTION_EXTENDED);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), S_COL_MARK,
					   GTK_JUSTIFY_CENTER);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), S_COL_UNREAD,
					   GTK_JUSTIFY_CENTER);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), S_COL_MIME,
					   GTK_JUSTIFY_CENTER);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), S_COL_NUMBER,
					   GTK_JUSTIFY_RIGHT);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), S_COL_SCORE,
					   GTK_JUSTIFY_RIGHT);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), S_COL_SIZE,
					   GTK_JUSTIFY_RIGHT);
	gtk_clist_set_column_width(GTK_CLIST(ctree), S_COL_MARK,
				   SUMMARY_COL_MARK_WIDTH);
	gtk_clist_set_column_width(GTK_CLIST(ctree), S_COL_UNREAD,
				   SUMMARY_COL_UNREAD_WIDTH);
	gtk_clist_set_column_width(GTK_CLIST(ctree), S_COL_MIME,
				   SUMMARY_COL_MIME_WIDTH);
	gtk_clist_set_column_width(GTK_CLIST(ctree), S_COL_NUMBER,
				   prefs_common.summary_col_number);
	gtk_clist_set_column_width(GTK_CLIST(ctree), S_COL_SCORE,
				   prefs_common.summary_col_score);
	gtk_clist_set_column_width(GTK_CLIST(ctree), S_COL_SIZE,
				   prefs_common.summary_col_size);
	gtk_clist_set_column_width(GTK_CLIST(ctree), S_COL_DATE,
				   prefs_common.summary_col_date);
	gtk_clist_set_column_width(GTK_CLIST(ctree), S_COL_FROM,
				   prefs_common.summary_col_from);
	gtk_clist_set_column_width(GTK_CLIST(ctree), S_COL_SUBJECT,
				   prefs_common.summary_col_subject);
	gtk_ctree_set_line_style(GTK_CTREE(ctree), GTK_CTREE_LINES_DOTTED);
	gtk_ctree_set_expander_style(GTK_CTREE(ctree),
				     GTK_CTREE_EXPANDER_SQUARE);
#if 0
	gtk_ctree_set_line_style(GTK_CTREE(ctree), GTK_CTREE_LINES_NONE);
	gtk_ctree_set_expander_style(GTK_CTREE(ctree),
				     GTK_CTREE_EXPANDER_TRIANGLE);
#endif
	gtk_ctree_set_indent(GTK_CTREE(ctree), 18);
	gtk_object_set_user_data(GTK_OBJECT(ctree), summaryview);

	/* don't let title buttons take key focus */
	for (i = 0; i < N_SUMMARY_COLS; i++)
		GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(ctree)->column[i].button,
				       GTK_CAN_FOCUS);

	/* connect signal to the buttons for sorting */
	gtk_signal_connect
		(GTK_OBJECT(GTK_CLIST(ctree)->column[S_COL_NUMBER].button),
		 "clicked",
		 GTK_SIGNAL_FUNC(summary_num_clicked),
		 summaryview);
	gtk_signal_connect
		(GTK_OBJECT(GTK_CLIST(ctree)->column[S_COL_SCORE].button),
		 "clicked",
		 GTK_SIGNAL_FUNC(summary_score_clicked),
		 summaryview);
	gtk_signal_connect
		(GTK_OBJECT(GTK_CLIST(ctree)->column[S_COL_SIZE].button),
		 "clicked",
		 GTK_SIGNAL_FUNC(summary_size_clicked),
		 summaryview);
	gtk_signal_connect
		(GTK_OBJECT(GTK_CLIST(ctree)->column[S_COL_DATE].button),
		 "clicked",
		 GTK_SIGNAL_FUNC(summary_date_clicked),
		 summaryview);
	gtk_signal_connect
		(GTK_OBJECT(GTK_CLIST(ctree)->column[S_COL_FROM].button),
		 "clicked",
		 GTK_SIGNAL_FUNC(summary_from_clicked),
		 summaryview);
	gtk_signal_connect
		(GTK_OBJECT(GTK_CLIST(ctree)->column[S_COL_SUBJECT].button),
		 "clicked",
		 GTK_SIGNAL_FUNC(summary_subject_clicked),
		 summaryview);
	gtk_signal_connect
		(GTK_OBJECT(GTK_CLIST(ctree)->column[S_COL_MARK].button),
		 "clicked",
		 GTK_SIGNAL_FUNC(summary_mark_clicked),
		 summaryview);

	/* create status label */
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	statlabel_folder = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox), statlabel_folder, FALSE, FALSE, 2);
	statlabel_select = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox), statlabel_select, FALSE, FALSE, 16);

	/* toggle view button */
	toggle_eventbox = gtk_event_box_new();
	gtk_box_pack_end(GTK_BOX(hbox), toggle_eventbox, FALSE, FALSE, 4);
	toggle_arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_container_add(GTK_CONTAINER(toggle_eventbox), toggle_arrow);

	statlabel_msgs = gtk_label_new("");
	gtk_box_pack_end(GTK_BOX(hbox), statlabel_msgs, FALSE, FALSE, 4);

	/* create popup menu */
	n_entries = sizeof(summary_popup_entries) /
		sizeof(summary_popup_entries[0]);
	popupmenu = menu_create_items(summary_popup_entries, n_entries,
				      "<SummaryView>", &popupfactory,
				      summaryview);

	/* connect signals */
	gtk_signal_connect(GTK_OBJECT(ctree), "tree_select_row",
			   GTK_SIGNAL_FUNC(summary_selected), summaryview);
	gtk_signal_connect(GTK_OBJECT(ctree), "button_press_event",
			   GTK_SIGNAL_FUNC(summary_button_pressed),
			   summaryview);
	gtk_signal_connect(GTK_OBJECT(ctree), "button_release_event",
			   GTK_SIGNAL_FUNC(summary_button_released),
			   summaryview);
	gtk_signal_connect(GTK_OBJECT(ctree), "key_press_event",
			   GTK_SIGNAL_FUNC(summary_key_pressed), summaryview);
	gtk_signal_connect(GTK_OBJECT(ctree), "resize_column",
			   GTK_SIGNAL_FUNC(summary_col_resized), summaryview);
        gtk_signal_connect(GTK_OBJECT(ctree), "open_row",
			   GTK_SIGNAL_FUNC(summary_open_row), summaryview);

	gtk_signal_connect_after(GTK_OBJECT(ctree), "tree_expand",
				 GTK_SIGNAL_FUNC(summary_tree_expanded),
				 summaryview);
	gtk_signal_connect_after(GTK_OBJECT(ctree), "tree_collapse",
				 GTK_SIGNAL_FUNC(summary_tree_collapsed),
				 summaryview);

	gtk_signal_connect(GTK_OBJECT(ctree), "start_drag",
			   GTK_SIGNAL_FUNC(summary_start_drag),
			   summaryview);
	gtk_signal_connect(GTK_OBJECT(ctree), "drag_data_get",
			   GTK_SIGNAL_FUNC(summary_drag_data_get),
			   summaryview);

	gtk_signal_connect(GTK_OBJECT(toggle_eventbox), "button_press_event",
			   GTK_SIGNAL_FUNC(summary_toggle_pressed),
			   summaryview);

	summaryview->vbox = vbox;
	summaryview->scrolledwin = scrolledwin;
	summaryview->ctree = ctree;
	summaryview->hbox = hbox;
	summaryview->statlabel_folder = statlabel_folder;
	summaryview->statlabel_select = statlabel_select;
	summaryview->statlabel_msgs = statlabel_msgs;
	summaryview->toggle_eventbox = toggle_eventbox;
	summaryview->toggle_arrow = toggle_arrow;
	summaryview->popupmenu = popupmenu;
	summaryview->popupfactory = popupfactory;
	summaryview->msg_is_toggled_on = TRUE;
	summaryview->sort_mode = SORT_BY_NONE;
	summaryview->sort_type = GTK_SORT_ASCENDING;

	summary_change_display_item(summaryview);

	gtk_widget_show_all(vbox);

	return summaryview;
}

void summary_init(SummaryView *summaryview)
{
	GtkStyle *style;
	GtkWidget *pixmap;

	PIXMAP_CREATE(summaryview->ctree, markxpm, markxpmmask, mark_xpm);
	PIXMAP_CREATE(summaryview->ctree, deletedxpm, deletedxpmmask,
		      deleted_xpm);
	PIXMAP_CREATE(summaryview->ctree, newxpm, newxpmmask, new_xpm);
	PIXMAP_CREATE(summaryview->ctree, unreadxpm, unreadxpmmask, unread_xpm);
	PIXMAP_CREATE(summaryview->ctree, repliedxpm, repliedxpmmask,
		      replied_xpm);
	PIXMAP_CREATE(summaryview->ctree, forwardedxpm, forwardedxpmmask,
		      forwarded_xpm);
	PIXMAP_CREATE(summaryview->ctree, ignorethreadxpm, ignorethreadxpmmask,
		      ignorethread_xpm);
	PIXMAP_CREATE(summaryview->ctree, clipxpm, clipxpmmask, clip_xpm);
	PIXMAP_CREATE(summaryview->hbox, folderxpm, folderxpmmask,
		      DIRECTORY_OPEN_XPM);

	pixmap = gtk_pixmap_new(clipxpm, clipxpmmask);
	gtk_clist_set_column_widget(GTK_CLIST(summaryview->ctree),
				    S_COL_MIME, pixmap);
	gtk_widget_show(pixmap);

	if (!small_style) {
		small_style = gtk_style_copy
			(gtk_widget_get_style(summaryview->ctree));
		if (!smallfont)
			smallfont = gdk_fontset_load(SMALL_FONT);
		small_style->font = smallfont;
		small_marked_style = gtk_style_copy(small_style);
		small_marked_style->fg[GTK_STATE_NORMAL] =
			summaryview->color_marked;
		small_deleted_style = gtk_style_copy(small_style);
		small_deleted_style->fg[GTK_STATE_NORMAL] =
			summaryview->color_dim;
	}
	if (!bold_style) {
		bold_style = gtk_style_copy
			(gtk_widget_get_style(summaryview->ctree));
		if (!boldfont)
			boldfont = gdk_fontset_load(BOLD_FONT);
		bold_style->font = boldfont;
		bold_marked_style = gtk_style_copy(bold_style);
		bold_marked_style->fg[GTK_STATE_NORMAL] =
			summaryview->color_marked;
		bold_deleted_style = gtk_style_copy(bold_style);
		bold_deleted_style->fg[GTK_STATE_NORMAL] =
			summaryview->color_dim;
	}

	style = gtk_style_copy(gtk_widget_get_style
				(summaryview->statlabel_folder));

	gtk_widget_set_style(summaryview->statlabel_folder, style);
	gtk_widget_set_style(summaryview->statlabel_select, style);
	gtk_widget_set_style(summaryview->statlabel_msgs, style);

	pixmap = gtk_pixmap_new(folderxpm, folderxpmmask);
	gtk_box_pack_start(GTK_BOX(summaryview->hbox), pixmap, FALSE, FALSE, 4);
	gtk_box_reorder_child(GTK_BOX(summaryview->hbox), pixmap, 0);
	gtk_widget_show(pixmap);

	summary_clear_list(summaryview);
	summary_set_menu_sensitive(summaryview);
	summary_create_label_menu(summaryview);

}

GtkCTreeNode * summary_find_next_important_score(SummaryView *summaryview,
						 GtkCTreeNode *current_node)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	MsgInfo *msginfo;
	gint best_score = MIN_SCORE;
	GtkCTreeNode *best_node = NULL;

	if (current_node)
		/*node = current_node;*/
		node = GTK_CTREE_NODE_NEXT(current_node);
	else
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	for (; node != NULL; node = GTK_CTREE_NODE_NEXT(node)) {
		msginfo = gtk_ctree_node_get_row_data(ctree, node);
		if (msginfo->score >= summaryview->important_score)
			break;
		if (msginfo->score > best_score) {
			best_score = msginfo->score;
			best_node = node;
		}
	}

	if (node != NULL)
		return node;
	else
		return best_node;
}

GtkCTreeNode * summary_find_prev_important_score(SummaryView *summaryview,
						 GtkCTreeNode *current_node)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	MsgInfo *msginfo;
	gint best_score = MIN_SCORE;
	GtkCTreeNode *best_node = NULL;

	if (current_node)
		/*node = current_node;*/
		node = GTK_CTREE_NODE_PREV(current_node);
	else
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	for (; node != NULL; node = GTK_CTREE_NODE_PREV(node)) {
		msginfo = gtk_ctree_node_get_row_data(ctree, node);
		if (msginfo->score >= summaryview->important_score)
			break;
		if (msginfo->score > best_score) {
			best_score = msginfo->score;
			best_node = node;
		}
	}

	if (node != NULL)
		return node;
	else
		return best_node;
}

gboolean summary_show(SummaryView *summaryview, FolderItem *item,
		      gboolean update_cache)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	GSList *mlist = NULL;
	gchar *buf;
	gboolean is_refresh;
	guint selected_msgnum = 0;
	guint displayed_msgnum = 0;
	GtkCTreeNode *selected_node = summaryview->folderview->selected;
	GSList *cur;
	gint sort_mode;
	gint sort_type;
        static gboolean locked = FALSE;

	if (locked)
		return FALSE;
	locked = TRUE;

	STATUSBAR_POP(summaryview->mainwin);

	is_refresh = (!prefs_common.open_inbox_on_inc &&
		      item == summaryview->folder_item) ? TRUE : FALSE;
	if (is_refresh) {
		selected_msgnum = summary_get_msgnum(summaryview,
						     summaryview->selected);
		displayed_msgnum = summary_get_msgnum(summaryview,
						      summaryview->displayed);
	}

	/* process the marks if any */
	if (summaryview->mainwin->lock_count == 0 &&
	    (summaryview->moved > 0 || summaryview->copied > 0)) {
		AlertValue val;

		val = alertpanel(_("Process mark"),
				 _("Some marks are left. Process it?"),
				 _("Yes"), _("No"), _("Cancel"));
		if (G_ALERTDEFAULT == val)
			summary_execute(summaryview);
		else if (G_ALERTALTERNATE == val)
			summary_write_cache(summaryview);
		else {
			locked = FALSE;
                        return FALSE;
		}
   		folder_update_op_count();
	}
	else if (!summaryview->filtering_happened) {
		summary_write_cache(summaryview);
	}

	summaryview->filtering_happened = FALSE;

	summaryview->folderview->opened = selected_node;

	gtk_clist_freeze(GTK_CLIST(ctree));

	summary_clear_list(summaryview);
	summary_set_menu_sensitive(summaryview);
	if (!is_refresh)
		messageview_clear(summaryview->messageview);

	buf = NULL;
	if (!item || !item->path || !item->parent || item->no_select ||
	    (item->folder->type == F_MH &&
	     ((buf = folder_item_get_path(item)) == NULL ||
	      change_dir(buf) < 0))) {
		g_free(buf);
		debug_print(_("empty folder\n\n"));
		if (is_refresh)
			messageview_clear(summaryview->messageview);
		summary_clear_all(summaryview);
		summaryview->folder_item = item;
		gtk_clist_thaw(GTK_CLIST(ctree));
		locked = FALSE;
		return TRUE;
	}
	g_free(buf);

	summaryview->folder_item = item;

	gtk_signal_handler_block_by_data(GTK_OBJECT(ctree), summaryview);

	buf = g_strdup_printf(_("Scanning folder (%s)..."), item->path);
	debug_print("%s\n", buf);
	STATUSBAR_PUSH(summaryview->mainwin, buf);
	g_free(buf);

	main_window_cursor_wait(summaryview->mainwin);

	mlist = item->folder->get_msg_list(item->folder, item, !update_cache);

	for(cur = mlist ; cur != NULL ; cur = g_slist_next(cur)) {
		MsgInfo * msginfo = (MsgInfo *) cur->data;

		msginfo->score = score_message(global_scoring, msginfo);
		if (msginfo->score != MAX_SCORE &&
		    msginfo->score != MIN_SCORE) {
			msginfo->score += score_message(item->prefs->scoring,
							msginfo);
		}
	}

	summaryview->killed_messages = NULL;
	if ((global_scoring || item->prefs->scoring) &&
	    (item->folder->type == F_NEWS)) {
		GSList *not_killed;
		gint kill_score;

		not_killed = NULL;
		kill_score = prefs_common.kill_score;
		if (item->prefs->kill_score > kill_score)
			kill_score = item->prefs->kill_score;
		for(cur = mlist ; cur != NULL ; cur = g_slist_next(cur)) {
			MsgInfo * msginfo = (MsgInfo *) cur->data;

			if (MSG_IS_NEWS(msginfo->flags) &&
			    (msginfo->score <= kill_score))
				summaryview->killed_messages = g_slist_append(summaryview->killed_messages, msginfo);
			else
				not_killed = g_slist_append(not_killed,
							    msginfo);
		}
		g_slist_free(mlist);
		mlist = not_killed;
	}

	STATUSBAR_POP(summaryview->mainwin);

	/* set ctree and hash table from the msginfo list
	   creating thread, and count the number of messages */
	summary_set_ctree_from_list(summaryview, mlist);

	g_slist_free(mlist);

	summary_write_cache(summaryview);

	gtk_signal_handler_unblock_by_data(GTK_OBJECT(ctree), summaryview);

	gtk_clist_thaw(GTK_CLIST(ctree));

	/* sort before */
	sort_mode = prefs_folder_item_get_sort_mode(item);
	sort_type = prefs_folder_item_get_sort_type(item);

	if (sort_mode != SORT_BY_NONE) {
		summaryview->sort_mode = sort_mode;
		if (sort_type == GTK_SORT_DESCENDING)
			summaryview->sort_type = GTK_SORT_ASCENDING;
		else
			summaryview->sort_type = GTK_SORT_DESCENDING;

		summary_sort(summaryview, sort_mode);
	}

	if (is_refresh) {
		summaryview->displayed =
			summary_find_msg_by_msgnum(summaryview,
						   displayed_msgnum);
		if (!summaryview->displayed)
			messageview_clear(summaryview->messageview);
		summary_select_by_msgnum(summaryview, selected_msgnum);
		if (!summaryview->selected) {
			/* no selected message - select first unread
			   message, but do not display it */
			node = summary_find_next_unread_msg(summaryview, NULL);
			if (node == NULL && GTK_CLIST(ctree)->row_list != NULL)
				node = GTK_CTREE_NODE
					(GTK_CLIST(ctree)->row_list_end);
			summary_select_node(summaryview, node, FALSE);
		}
	} else {
		/* select first unread message */
		if (sort_mode == SORT_BY_SCORE)
			node = summary_find_next_important_score(summaryview,
								 NULL);
		else
			node = summary_find_next_unread_msg(summaryview, NULL);

		if (node == NULL && GTK_CLIST(ctree)->row_list != NULL)
			node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list_end);
		summary_select_node(summaryview, node,
				    prefs_common.open_unread_on_enter);
	}

	summary_status_show(summaryview);

	summary_set_menu_sensitive(summaryview);

	main_window_set_toolbar_sensitive(summaryview->mainwin);

	debug_print("\n");
	STATUSBAR_PUSH(summaryview->mainwin, _("Done."));

	main_window_cursor_normal(summaryview->mainwin);
	locked = FALSE;

	return TRUE;
}

void summary_clear_list(SummaryView *summaryview)
{
	GtkCList *clist = GTK_CLIST(summaryview->ctree);
	gint optimal_width;
	GSList * cur;

	gtk_clist_freeze(clist);

	for(cur = summaryview->killed_messages ; cur != NULL ; 
	    cur = g_slist_next(cur)) {
		MsgInfo * msginfo = (MsgInfo *) cur->data;

		procmsg_msginfo_free(msginfo);
	}
	g_slist_free(summaryview->killed_messages);
	summaryview->killed_messages = NULL;

	gtk_ctree_pre_recursive(GTK_CTREE(summaryview->ctree),
				NULL, summary_free_msginfo_func, NULL);

	summaryview->folder_item = NULL;

	summaryview->display_msg = FALSE;

	summaryview->selected  = NULL;
	summaryview->displayed = NULL;
	summaryview->newmsgs   = summaryview->unread     = 0;
	summaryview->messages  = summaryview->total_size = 0;
	summaryview->deleted   = summaryview->moved      = 0;
	summaryview->copied    = 0;
	if (summaryview->msgid_table) {
		g_hash_table_destroy(summaryview->msgid_table);
		summaryview->msgid_table = NULL;
	}
	if (summaryview->subject_table) {
		g_hash_table_destroy(summaryview->subject_table);
		summaryview->subject_table = NULL;
	}
	summaryview->mlist = NULL;
	if (summaryview->folder_table) {
		g_hash_table_destroy(summaryview->folder_table);
		summaryview->folder_table = NULL;
	}
	summaryview->sort_mode = SORT_BY_NONE;
	summaryview->sort_type = GTK_SORT_ASCENDING;

	gtk_clist_clear(clist);
	optimal_width = gtk_clist_optimal_column_width(clist, S_COL_SUBJECT);
	gtk_clist_set_column_width(clist, S_COL_SUBJECT, optimal_width);

	gtk_clist_thaw(clist);
}

void summary_clear_all(SummaryView *summaryview)
{
	summary_clear_list(summaryview);
	summary_set_menu_sensitive(summaryview);
	main_window_set_toolbar_sensitive(summaryview->mainwin);
	summary_status_show(summaryview);
}

SummarySelection summary_get_selection_type(SummaryView *summaryview)
{
	GtkCList *clist = GTK_CLIST(summaryview->ctree);
	SummarySelection selection;

	if (!clist->row_list)
		selection = SUMMARY_NONE;
	else if (!clist->selection)
		selection = SUMMARY_SELECTED_NONE;
	else if (!clist->selection->next)
		selection = SUMMARY_SELECTED_SINGLE;
	else
		selection = SUMMARY_SELECTED_MULTIPLE;

	return selection;
}

static void summary_set_menu_sensitive(SummaryView *summaryview)
{
	GtkItemFactory *ifactory = summaryview->popupfactory;
	SummarySelection selection;
	gboolean sens;

	selection = summary_get_selection_type(summaryview);
	main_window_set_menu_sensitive(summaryview->mainwin);

	if (selection == SUMMARY_NONE) {
		GtkWidget *submenu;

		submenu = gtk_item_factory_get_widget
			(summaryview->popupfactory, "/Mark");
		menu_set_insensitive_all(GTK_MENU_SHELL(submenu));
		menu_set_insensitive_all
			(GTK_MENU_SHELL(summaryview->popupmenu));
		return;
	}

	if (summaryview->folder_item->folder->type != F_NEWS) {
		if (summaryview->folder_item->stype != F_TRASH)
			menu_set_sensitive(ifactory, "/Delete", TRUE);
		menu_set_sensitive(ifactory, "/Move...", TRUE);
		menu_set_sensitive(ifactory, "/Copy...", TRUE);
	}

	gtk_widget_set_sensitive(summaryview->label_menu_item, TRUE);
	menu_set_sensitive(ifactory, "/Execute", TRUE);

	sens = (selection == SUMMARY_SELECTED_MULTIPLE) ? FALSE : TRUE;
	menu_set_sensitive(ifactory, "/Reply",			  sens);
	menu_set_sensitive(ifactory, "/Reply to sender",	  sens);
	menu_set_sensitive(ifactory, "/Reply to all",		  sens);
	menu_set_sensitive(ifactory, "/Forward",		  TRUE);
	menu_set_sensitive(ifactory, "/Forward as attachment",    TRUE);
	
	menu_set_sensitive(ifactory, "/Open in new window", sens);
	menu_set_sensitive(ifactory, "/View source", sens);
	menu_set_sensitive(ifactory, "/Show all header", sens);
	if ((summaryview->folder_item->stype == F_DRAFT) ||
	    (summaryview->folder_item->stype == F_OUTBOX) ||
	    (summaryview->folder_item->stype == F_QUEUE))
		menu_set_sensitive(ifactory, "/Re-edit", sens);

	menu_set_sensitive(ifactory, "/Save as...", sens);
	menu_set_sensitive(ifactory, "/Print...",   TRUE);

	menu_set_sensitive(ifactory, "/Mark", TRUE);

	menu_set_sensitive(ifactory, "/Mark/Mark",   TRUE);
	menu_set_sensitive(ifactory, "/Mark/Unmark", TRUE);

	menu_set_sensitive(ifactory, "/Mark/Mark as unread", TRUE);
	menu_set_sensitive(ifactory, "/Mark/Mark as read",   TRUE);
#if MARK_ALL_READ	
	menu_set_sensitive(ifactory, "/Mark/Mark all read", TRUE);
#endif	
	menu_set_sensitive(ifactory, "/Mark/Ignore thread",   TRUE);
	menu_set_sensitive(ifactory, "/Mark/Unignore thread", TRUE);

	menu_set_sensitive(ifactory, "/Select all", TRUE);

	if (summaryview->folder_item->folder->account)
		sens = summaryview->folder_item->folder->account->protocol
			== A_NNTP;
	else
		sens = FALSE;
	menu_set_sensitive(ifactory, "/Follow-up and reply to",	sens);
}

static void summary_set_add_sender_menu(SummaryView *summaryview)
{
	GtkWidget *menu;
	GtkWidget *submenu;
	MsgInfo *msginfo;
	gchar *from;

	menu = gtk_item_factory_get_item(summaryview->popupfactory,
					 "/Add sender to address book");
	msginfo = gtk_ctree_node_get_row_data(GTK_CTREE(summaryview->ctree),
					      summaryview->selected);
	if (!msginfo || !msginfo->from) {
		gtk_widget_set_sensitive(menu, FALSE);
		submenu = gtk_menu_new();
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu), submenu);
		return;
	}

	gtk_widget_set_sensitive(menu, TRUE);
	Xstrdup_a(from, msginfo->from, return);
	eliminate_address_comment(from);
	extract_address(from);
	addressbook_add_submenu(menu, msginfo->fromname, from, NULL);

}

void summary_select_next_unread(SummaryView *summaryview)
{
	GtkCTreeNode *node;
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);

	node = summary_find_next_unread_msg(summaryview,
					    summaryview->selected);

	if (node) {
		gtkut_ctree_expand_parent_all(ctree, node);
		gtk_sctree_unselect_all(GTK_SCTREE(ctree));
		gtk_sctree_select(GTK_SCTREE(ctree), node);

		/* BUGFIX: select next unread message 
		 * REVERT: causes incorrect folder stats
		 * gtk_ctree_node_moveto(ctree, node, -1, 0.5, 0.0); 
		 */

		if (summaryview->displayed == node)
			summaryview->displayed = NULL;
		summary_display_msg(summaryview, node, FALSE);
	} else {
		AlertValue val;

		val = alertpanel(_("No unread message"),
				 _("No unread message found. Go to next folder?"),
				 _("Yes"), _("No"), NULL);
		if (val == G_ALERTDEFAULT) {
			if (gtk_signal_n_emissions_by_name
				(GTK_OBJECT(ctree), "key_press_event") > 0)
					gtk_signal_emit_stop_by_name
						(GTK_OBJECT(ctree),
						 "key_press_event");
			folderview_select_next_unread(summaryview->folderview);
		}
	}
}

void summary_select_next_marked(SummaryView *summaryview)
{
	GtkCTreeNode *node;
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);

	node = summary_find_next_marked_msg(summaryview,
					    summaryview->selected);

	if (!node) {
		AlertValue val;

		val = alertpanel(_("No more marked messages"),
				 _("No marked message found. "
				   "Search from the beginning?"),
				 _("Yes"), _("No"), NULL);
		if (val != G_ALERTDEFAULT) return;
		node = summary_find_next_marked_msg(summaryview,
						    NULL);
	}
	if (!node) {
		alertpanel_notice(_("No marked messages."));
	} else {
		gtk_sctree_unselect_all(GTK_SCTREE(ctree));
		gtk_sctree_select(GTK_SCTREE(ctree), node);
		if (summaryview->displayed == node)
			summaryview->displayed = NULL;
		summary_display_msg(summaryview, node, FALSE);
	}
}

void summary_select_prev_marked(SummaryView *summaryview)
{
	GtkCTreeNode *node;
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);

	node = summary_find_prev_marked_msg(summaryview,
					    summaryview->selected);

	if (!node) {
		AlertValue val;

		val = alertpanel(_("No more marked messages"),
				 _("No marked message found. "
				   "Search from the end?"),
				 _("Yes"), _("No"), NULL);
		if (val != G_ALERTDEFAULT) return;
		node = summary_find_prev_marked_msg(summaryview,
						    NULL);
	}
	if (!node) {
		alertpanel_notice(_("No marked messages."));
	} else {
		gtk_sctree_unselect_all(GTK_SCTREE(ctree));
		gtk_sctree_select(GTK_SCTREE(ctree), node);
		if (summaryview->displayed == node)
			summaryview->displayed = NULL;
		summary_display_msg(summaryview, node, FALSE);
	}
}

void summary_select_by_msgnum(SummaryView *summaryview, guint msgnum)
{
	GtkCTreeNode *node;

	node = summary_find_msg_by_msgnum(summaryview, msgnum);
	summary_select_node(summaryview, node, FALSE);
}

/**
 * summary_select_node:
 * @summaryview: Summary view.
 * @node: Summary tree node.
 * @display_msg: TRUE to display the selected message.
 *
 * Select @node (bringing it into view by scrolling and expanding its
 * thread, if necessary) and unselect all others.  If @display_msg is
 * TRUE, display the corresponding message in the message view.
 **/
static void summary_select_node(SummaryView *summaryview, GtkCTreeNode *node,
				gboolean display_msg)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);

	if (node) {
		GTK_EVENTS_FLUSH();
		gtkut_ctree_expand_parent_all(ctree, node);
		gtk_ctree_node_moveto(ctree, node, -1, 0.5, 0);
		gtk_widget_grab_focus(GTK_WIDGET(ctree));
		gtk_sctree_unselect_all(GTK_SCTREE(ctree));
		summaryview->display_msg = display_msg;
		gtk_sctree_select(GTK_SCTREE(ctree), node);
	}
}

static guint summary_get_msgnum(SummaryView *summaryview, GtkCTreeNode *node)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	if (!node)
		return 0;
	msginfo = gtk_ctree_node_get_row_data(ctree, node);
	return msginfo->msgnum;
}

static GtkCTreeNode *summary_find_next_unread_msg(SummaryView *summaryview,
						  GtkCTreeNode *current_node)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	MsgInfo *msginfo;

	if (current_node)
		node = current_node;
	else
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	for (; node != NULL; node = gtkut_ctree_node_next(ctree, node)) {
		msginfo = gtk_ctree_node_get_row_data(ctree, node);
		if (MSG_IS_UNREAD(msginfo->flags) && !MSG_IS_IGNORE_THREAD(msginfo->flags)) break;
	}

	return node;
}

static GtkCTreeNode *summary_find_next_marked_msg(SummaryView *summaryview,
						  GtkCTreeNode *current_node)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	MsgInfo *msginfo;

	if (current_node)
		node = GTK_CTREE_NODE_NEXT(current_node);
	else
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	for (; node != NULL; node = GTK_CTREE_NODE_NEXT(node)) {
		msginfo = gtk_ctree_node_get_row_data(ctree, node);
		if (MSG_IS_MARKED(msginfo->flags)) break;
	}

	return node;
}

static GtkCTreeNode *summary_find_prev_marked_msg(SummaryView *summaryview,
						  GtkCTreeNode *current_node)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	MsgInfo *msginfo;

	if (current_node)
		node = GTK_CTREE_NODE_PREV(current_node);
	else
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list_end);

	for (; node != NULL; node = GTK_CTREE_NODE_PREV(node)) {
		msginfo = gtk_ctree_node_get_row_data(ctree, node);
		if (MSG_IS_MARKED(msginfo->flags)) break;
	}

	return node;
}

static GtkCTreeNode *summary_find_msg_by_msgnum(SummaryView *summaryview,
						guint msgnum)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	MsgInfo *msginfo;

	node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	for (; node != NULL; node = gtkut_ctree_node_next(ctree, node)) {
		msginfo = gtk_ctree_node_get_row_data(ctree, node);
		if (msginfo->msgnum == msgnum) break;
	}

	return node;
}

#if 0
static GtkCTreeNode *summary_find_prev_unread_msg(SummaryView *summaryview,
						  GtkCTreeNode *current_node)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	MsgInfo *msginfo;

	if (current_node)
		node = current_node;
		/*node = GTK_CTREE_NODE_PREV(current_node);*/
	else
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list_end);

	for (; node != NULL; node = GTK_CTREE_NODE_PREV(node)) {
		msginfo = gtk_ctree_node_get_row_data(ctree, node);
		if (MSG_IS_UNREAD(msginfo->flags)) break;
	}

	return node;
}
#endif

static guint attract_hash_func(gconstpointer key)
{
	gchar *str;
	gchar *p;
	guint h;

	Xstrdup_a(str, (const gchar *)key, return 0);
	trim_subject(str);

	p = str;
	h = *p;

	if (h) {
		for (p += 1; *p != '\0'; p++)
			h = (h << 5) - h + *p;
	}

	return h;
}

static gint attract_compare_func(gconstpointer a, gconstpointer b)
{
	return subject_compare((const gchar *)a, (const gchar *)b) == 0;
}

void summary_attract_by_subject(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCList *clist = GTK_CLIST(ctree);
	GtkCTreeNode *src_node;
	GtkCTreeNode *dst_node, *sibling;
	GtkCTreeNode *tmp;
	MsgInfo *src_msginfo, *dst_msginfo;
	GHashTable *subject_table;

	debug_print(_("Attracting messages by subject..."));
	STATUSBAR_PUSH(summaryview->mainwin,
		       _("Attracting messages by subject..."));

	main_window_cursor_wait(summaryview->mainwin);
	gtk_clist_freeze(clist);

	subject_table = g_hash_table_new(attract_hash_func,
					 attract_compare_func);

	for (src_node = GTK_CTREE_NODE(clist->row_list);
	     src_node != NULL;
	     src_node = tmp) {
		tmp = GTK_CTREE_ROW(src_node)->sibling;
		src_msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(src_node);
		if (!src_msginfo) continue;
		if (!src_msginfo->subject) continue;

		/* find attracting node */
		dst_node = g_hash_table_lookup(subject_table,
					       src_msginfo->subject);

		if (dst_node) {
			dst_msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(dst_node);

			/* if the time difference is more than 20 days,
			   don't attract */
			if (ABS(src_msginfo->date_t - dst_msginfo->date_t)
			    > 60 * 60 * 24 * 20)
				continue;

			sibling = GTK_CTREE_ROW(dst_node)->sibling;
			if (src_node != sibling)
				gtk_ctree_move(ctree, src_node, NULL, sibling);
		}

		g_hash_table_insert(subject_table,
				    src_msginfo->subject, src_node);
	}

	g_hash_table_destroy(subject_table);

	gtk_ctree_node_moveto(ctree, summaryview->selected, -1, 0.5, 0);

	gtk_clist_thaw(clist);

	debug_print(_("done.\n"));
	STATUSBAR_POP(summaryview->mainwin);

	main_window_cursor_normal(summaryview->mainwin);
}

static void summary_free_msginfo_func(GtkCTree *ctree, GtkCTreeNode *node,
				      gpointer data)
{
	MsgInfo *msginfo = gtk_ctree_node_get_row_data(ctree, node);

	if (msginfo)
		procmsg_msginfo_free(msginfo);
}

static void summary_set_marks_func(GtkCTree *ctree, GtkCTreeNode *node,
				   gpointer data)
{
	SummaryView *summaryview = data;
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, node);

	if (MSG_IS_NEW(msginfo->flags) && !MSG_IS_IGNORE_THREAD(msginfo->flags))
		summaryview->newmsgs++;
	if (MSG_IS_UNREAD(msginfo->flags) && !MSG_IS_IGNORE_THREAD(msginfo->flags))
		summaryview->unread++;
	if (MSG_IS_DELETED(msginfo->flags))
		summaryview->deleted++;

	summaryview->messages++;
	summaryview->total_size += msginfo->size;

	summary_set_row_marks(summaryview, node);
}

static void summary_update_status(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	MsgInfo *msginfo;

	summaryview->newmsgs = summaryview->unread =
	summaryview->messages = summaryview->total_size =
	summaryview->deleted = summaryview->moved = summaryview->copied = 0;

	for (node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	     node != NULL; node = gtkut_ctree_node_next(ctree, node)) {
		msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);

		if (MSG_IS_NEW(msginfo->flags) && !MSG_IS_IGNORE_THREAD(msginfo->flags))
			summaryview->newmsgs++;
		if (MSG_IS_UNREAD(msginfo->flags)&& !MSG_IS_IGNORE_THREAD(msginfo->flags))
			summaryview->unread++;
		if (MSG_IS_DELETED(msginfo->flags))
			summaryview->deleted++;
		if (MSG_IS_MOVE(msginfo->flags))
			summaryview->moved++;
		if (MSG_IS_COPY(msginfo->flags))
			summaryview->copied++;
		summaryview->messages++;
		summaryview->total_size += msginfo->size;
	}
}

static void summary_status_show(SummaryView *summaryview)
{
	gchar *str;
	gchar *del, *mv, *cp;
	gchar *sel;
	gchar *spc;
	GList *rowlist, *cur;
	guint n_selected = 0;
	off_t sel_size = 0;
	MsgInfo *msginfo;

	if (!summaryview->folder_item) {
		gtk_label_set(GTK_LABEL(summaryview->statlabel_folder), "");
		gtk_label_set(GTK_LABEL(summaryview->statlabel_select), "");
		gtk_label_set(GTK_LABEL(summaryview->statlabel_msgs),   "");
		return;
	}

	rowlist = GTK_CLIST(summaryview->ctree)->selection;
	for (cur = rowlist; cur != NULL; cur = cur->next) {
		msginfo = gtk_ctree_node_get_row_data
			(GTK_CTREE(summaryview->ctree),
			 GTK_CTREE_NODE(cur->data));
		sel_size += msginfo->size;
		n_selected++;
	}

	gtk_label_set(GTK_LABEL(summaryview->statlabel_folder),
		      summaryview->folder_item &&
		      summaryview->folder_item->folder->type == F_NEWS
		      ? g_basename(summaryview->folder_item->path)
		      : summaryview->folder_item->path);

	if (summaryview->deleted)
		del = g_strdup_printf(_("%d deleted"), summaryview->deleted);
	else
		del = g_strdup("");
	if (summaryview->moved)
		mv = g_strdup_printf(_("%s%d moved"),
				     summaryview->deleted ? _(", ") : "",
				     summaryview->moved);
	else
		mv = g_strdup("");
	if (summaryview->copied)
		cp = g_strdup_printf(_("%s%d copied"),
				     summaryview->deleted ||
				     summaryview->moved ? _(", ") : "",
				     summaryview->copied);
	else
		cp = g_strdup("");

	if (summaryview->deleted || summaryview->moved || summaryview->copied)
		spc = "    ";
	else
		spc = "";

	if (n_selected)
		sel = g_strdup_printf(" (%s)", to_human_readable(sel_size));
	else
		sel = g_strdup("");
	str = g_strconcat(n_selected ? itos(n_selected) : "",
			  n_selected ? _(" item(s) selected") : "",
			  sel, spc, del, mv, cp, NULL);
	gtk_label_set(GTK_LABEL(summaryview->statlabel_select), str);
	g_free(str);
	g_free(sel);
	g_free(del);
	g_free(mv);
	g_free(cp);

	if (summaryview->folder_item &&
	    FOLDER_IS_LOCAL(summaryview->folder_item->folder)) {
		str = g_strdup_printf(_("%d new, %d unread, %d total (%s)"),
				      summaryview->newmsgs,
				      summaryview->unread,
				      summaryview->messages,
				      to_human_readable(summaryview->total_size));
	} else {
		str = g_strdup_printf(_("%d new, %d unread, %d total"),
				      summaryview->newmsgs,
				      summaryview->unread,
				      summaryview->messages);
	}
	gtk_label_set(GTK_LABEL(summaryview->statlabel_msgs), str);
	g_free(str);

	folderview_update_msg_num(summaryview->folderview,
				  summaryview->folderview->opened,
				  summaryview->newmsgs,
				  summaryview->unread,
				  summaryview->messages);
}

void summary_sort(SummaryView *summaryview, SummarySortType type)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCList *clist = GTK_CLIST(summaryview->ctree);
	GtkCListCompareFunc cmp_func;

	if (!summaryview->folder_item)
		return;

	switch (type) {
	case SORT_BY_NUMBER:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_num;
		break;
	case SORT_BY_SIZE:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_size;
		break;
	case SORT_BY_DATE:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_date;
		break;
	case SORT_BY_FROM:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_from;
		break;
	case SORT_BY_SUBJECT:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_subject;
		break;
	case SORT_BY_SCORE:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_score;
		break;
	case SORT_BY_LABEL:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_label;
		break;
	default:
		return;
	}

	debug_print(_("Sorting summary..."));
	STATUSBAR_PUSH(summaryview->mainwin, _("Sorting summary..."));

	main_window_cursor_wait(summaryview->mainwin);

	gtk_clist_set_compare_func(clist, cmp_func);

	/* toggle sort type if the same column is selected */
	if (summaryview->sort_mode == type)
		summaryview->sort_type =
			summaryview->sort_type == GTK_SORT_ASCENDING
			? GTK_SORT_DESCENDING : GTK_SORT_ASCENDING;
	else
		summaryview->sort_type = GTK_SORT_ASCENDING;
	gtk_clist_set_sort_type(clist, summaryview->sort_type);
	summaryview->sort_mode = type;

	gtk_ctree_sort_node(ctree, NULL);

	gtk_ctree_node_moveto(ctree, summaryview->selected, -1, 0.5, 0);
	/*gtkut_ctree_set_focus_row(ctree, summaryview->selected);*/

	prefs_folder_item_set_config(summaryview->folder_item,
				     summaryview->sort_type,
				     summaryview->sort_mode);
	prefs_folder_item_save_config(summaryview->folder_item);

	debug_print(_("done.\n"));
	STATUSBAR_POP(summaryview->mainwin);

	main_window_cursor_normal(summaryview->mainwin);
}

static GtkCTreeNode * subject_table_lookup(GHashTable *subject_table,
					   gchar * subject)
{
	if (g_strncasecmp(subject, "Re: ", 4) == 0)
		return g_hash_table_lookup(subject_table, subject + 4);
	else
		return g_hash_table_lookup(subject_table, subject);
}

static void subject_table_insert(GHashTable *subject_table, gchar * subject,
				 GtkCTreeNode * node)
{
	if (g_strncasecmp(subject, "Re: ", 4) == 0)
		g_hash_table_insert(subject_table, subject + 4, node);
	else
		g_hash_table_insert(subject_table, subject, node);
}

static void summary_set_ctree_from_list(SummaryView *summaryview,
					GSList *mlist)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;
	MsgInfo *parentinfo;
	MsgInfo *cur_msginfo;
	GtkCTreeNode *node, *parent;
	gchar *text[N_SUMMARY_COLS];
	GHashTable *msgid_table;
	GHashTable *subject_table;
	GSList * cur;
	GtkCTreeNode *cur_parent;

	if (!mlist) return;

	debug_print(_("\tSetting summary from message data..."));
	STATUSBAR_PUSH(summaryview->mainwin,
		       _("Setting summary from message data..."));
	gdk_flush();

	msgid_table = g_hash_table_new(g_str_hash, g_str_equal);
	summaryview->msgid_table = msgid_table;
	subject_table = g_hash_table_new(g_str_hash, g_str_equal);
	summaryview->subject_table = subject_table;

	if (prefs_common.use_addr_book)
		start_address_completion();
	
	/*main_window_set_thread_option(summaryview->mainwin)*/;

	for (cur = mlist ; cur != NULL; cur = cur->next) {
		msginfo = (MsgInfo *)cur->data;
		msginfo->threadscore = msginfo->score;
	}

	if (global_scoring || summaryview->folder_item->prefs->scoring) {
		summaryview->important_score = prefs_common.important_score;
		if (summaryview->folder_item->prefs->important_score >
		    summaryview->important_score)
			summaryview->important_score =
				summaryview->folder_item->prefs->important_score;
	}
	
		if (prefs_common.enable_thread) {
	/*if (summaryview->folder_item->prefs->enable_thread) { */
		for (; mlist != NULL; mlist = mlist->next) {
			msginfo = (MsgInfo *)mlist->data;
			parent = NULL;

			summary_set_header(text, msginfo);

			/* search parent node for threading */
			if (msginfo->inreplyto && *msginfo->inreplyto) {
				parent = g_hash_table_lookup
					(msgid_table, msginfo->inreplyto);
			}
			if (parent == NULL && msginfo->subject &&
			    g_strncasecmp(msginfo->subject, "Re: ", 4) == 0) {
				parent = subject_table_lookup
					(subject_table, msginfo->subject);
			}
			if(parent) {
				parentinfo = gtk_ctree_node_get_row_data(ctree, parent);
				if(!MSG_IS_IGNORE_THREAD(msginfo->flags) && parentinfo && MSG_IS_IGNORE_THREAD(parentinfo->flags)) {
					MSG_SET_PERM_FLAGS(msginfo->flags, MSG_IGNORE_THREAD);
				}
			}

			node = gtk_ctree_insert_node
				(ctree, parent, NULL, text, 2,
				 NULL, NULL, NULL, NULL, FALSE,
				 parent ? TRUE : FALSE);
			GTKUT_CTREE_NODE_SET_ROW_DATA(node, msginfo);
			summary_set_marks_func(ctree, node, summaryview);
			
			if (MSG_GET_LABEL(msginfo->flags))
				summary_set_label_color(ctree, node, MSG_GET_LABEL_VALUE(msginfo->flags));

			/* preserve previous node if the message is
			   duplicated */
			if (msginfo->msgid && *msginfo->msgid &&
			    g_hash_table_lookup(msgid_table, msginfo->msgid)
			    == NULL)
				g_hash_table_insert(msgid_table,
						    msginfo->msgid, node);
			if (msginfo->subject &&
			    subject_table_lookup(subject_table,
						 msginfo->subject) == NULL) {
				subject_table_insert(subject_table,
						     msginfo->subject, node);
			}

			cur_parent = parent;
			cur_msginfo = msginfo;
			while (cur_parent != NULL) {
				parentinfo = gtk_ctree_node_get_row_data(ctree, cur_parent);

				if (!parentinfo)
					break;
				
				if (parentinfo->threadscore <
				    cur_msginfo->threadscore) {
					gchar * s;
					parentinfo->threadscore =
						cur_msginfo->threadscore;
#if 0
					s = itos(parentinfo->threadscore);
					gtk_ctree_node_set_text(ctree, cur_parent, S_COL_SCORE, s);
#endif
				}
				else break;
				
				cur_msginfo = parentinfo;
				if (cur_msginfo->inreplyto &&
				    *cur_msginfo->inreplyto) {
					cur_parent = g_hash_table_lookup(msgid_table, cur_msginfo->inreplyto);
				}
				if (cur_parent == NULL &&
				    cur_msginfo->subject &&
				    g_strncasecmp(cur_msginfo->subject,
						  "Re: ", 4) == 0) {
					cur_parent = subject_table_lookup(subject_table, cur_msginfo->subject);
				}
			}
		}

		/* complete the thread */
		summary_thread_build(summaryview, TRUE);
	} else {
		for (; mlist != NULL; mlist = mlist->next) {
			msginfo = (MsgInfo *)mlist->data;

			summary_set_header(text, msginfo);

			node = gtk_ctree_insert_node
				(ctree, NULL, NULL, text, 2,
				 NULL, NULL, NULL, NULL, FALSE, FALSE);
			GTKUT_CTREE_NODE_SET_ROW_DATA(node, msginfo);
			summary_set_marks_func(ctree, node, summaryview);

			if ( MSG_GET_LABEL(msginfo->flags) )
			  summary_set_label_color(ctree, node, MSG_GET_LABEL_VALUE(msginfo->flags));

			if (msginfo->msgid && *msginfo->msgid &&
			    g_hash_table_lookup(msgid_table, msginfo->msgid)
			    == NULL)
				g_hash_table_insert(msgid_table,
						    msginfo->msgid, node);

			if (msginfo->subject &&
			    subject_table_lookup(subject_table,
						 msginfo->subject) == NULL)
				subject_table_insert(subject_table,
						     msginfo->subject, node);
		}
	}

	if (prefs_common.enable_hscrollbar) {
		gint optimal_width;

		optimal_width = gtk_clist_optimal_column_width
			(GTK_CLIST(ctree), S_COL_SUBJECT);
		gtk_clist_set_column_width(GTK_CLIST(ctree), S_COL_SUBJECT,
					   optimal_width);
	}

	if (prefs_common.use_addr_book)
		end_address_completion();

	debug_print(_("done.\n"));
	STATUSBAR_POP(summaryview->mainwin);
	if (debug_mode) {
		debug_print("\tmsgid hash table size = %d\n",
			    g_hash_table_size(msgid_table));
		debug_print("\tsubject hash table size = %d\n",
			    g_hash_table_size(subject_table));
	}
}

struct wcachefp
{
	FILE *cache_fp;
	FILE *mark_fp;
};

gint summary_write_cache(SummaryView *summaryview)
{
	struct wcachefp fps;
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	gint ver = CACHE_VERSION;
	gchar *buf;
	gchar *cachefile, *markfile;
	GSList * cur;
	gint filemode = 0;
	PrefsFolderItem *prefs;

	if (!summaryview->folder_item || !summaryview->folder_item->path)
		return -1;

	if (summaryview->folder_item->folder->update_mark != NULL)
		summaryview->folder_item->folder->update_mark(summaryview->folder_item->folder, summaryview->folder_item);

	cachefile = folder_item_get_cache_file(summaryview->folder_item);
	g_return_val_if_fail(cachefile != NULL, -1);
	if ((fps.cache_fp = fopen(cachefile, "w")) == NULL) {
		FILE_OP_ERROR(cachefile, "fopen");
		g_free(cachefile);
		return -1;
	}
	if (change_file_mode_rw(fps.cache_fp, cachefile) < 0)
		FILE_OP_ERROR(cachefile, "chmod");

	prefs = summaryview->folder_item->prefs;
        if (prefs && prefs->enable_folder_chmod && prefs->folder_chmod) {
		/* for cache file */
		filemode = prefs->folder_chmod;
		if (filemode & S_IRGRP) filemode |= S_IWGRP;
		if (filemode & S_IROTH) filemode |= S_IWOTH;
#if HAVE_FCHMOD
		fchmod(fileno(fps.cache_fp), filemode);
#else
		chmod(cachefile, filemode);
#endif
        }

	g_free(cachefile);

	markfile = folder_item_get_mark_file(summaryview->folder_item);
	if ((fps.mark_fp = fopen(markfile, "w")) == NULL) {
		FILE_OP_ERROR(markfile, "fopen");
		fclose(fps.cache_fp);
		g_free(markfile);
		return -1;
	}
	if (change_file_mode_rw(fps.mark_fp, markfile) < 0)
		FILE_OP_ERROR(markfile, "chmod");
        if (prefs && prefs->enable_folder_chmod && prefs->folder_chmod) {
#if HAVE_FCHMOD
		fchmod(fileno(fps.mark_fp), filemode);
#else
		chmod(markfile, filemode);
#endif
        }

	g_free(markfile);

	buf = g_strdup_printf(_("Writing summary cache (%s)..."),
			      summaryview->folder_item->path);
	debug_print(buf);
	STATUSBAR_PUSH(summaryview->mainwin, buf);
	g_free(buf);

	WRITE_CACHE_DATA_INT(ver, fps.cache_fp);
	ver = MARK_VERSION;
	WRITE_CACHE_DATA_INT(ver, fps.mark_fp);

	gtk_ctree_pre_recursive(ctree, NULL, summary_write_cache_func, &fps);

	for(cur = summaryview->killed_messages ; cur != NULL ;
	    cur = g_slist_next(cur)) {
		MsgInfo *msginfo = (MsgInfo *) cur->data;

		procmsg_write_cache(msginfo, fps.cache_fp);
		procmsg_write_flags(msginfo, fps.mark_fp);
	}

	fclose(fps.cache_fp);
	fclose(fps.mark_fp);

	debug_print(_("done.\n"));
	STATUSBAR_POP(summaryview->mainwin);

	return 0;
}

static void summary_write_cache_func(GtkCTree *ctree, GtkCTreeNode *node,
				     gpointer data)
{
	struct wcachefp *fps = data;
	MsgInfo *msginfo = gtk_ctree_node_get_row_data(ctree, node);

	if (msginfo == NULL) return;

	procmsg_write_cache(msginfo, fps->cache_fp);
	procmsg_write_flags(msginfo, fps->mark_fp);
}

static void summary_set_header(gchar *text[], MsgInfo *msginfo)
{
	static gchar date_modified[80];
	static gchar *to = NULL;
	static gchar *from_name = NULL;
	static gchar col_number[11];
	static gchar col_score[11];

	text[S_COL_MARK]   = NULL;
	text[S_COL_UNREAD] = NULL;
	text[S_COL_MIME]   = NULL;
	text[S_COL_NUMBER] = itos_buf(col_number, msginfo->msgnum);
	text[S_COL_SIZE]   = to_human_readable(msginfo->size);
#if 0
	text[S_COL_SCORE]  = itos_buf(col_score, msginfo->threadscore);
#else
	text[S_COL_SCORE]  = itos_buf(col_score, msginfo->score);
#endif

	if (msginfo->date_t) {
		procheader_date_get_localtime(date_modified,
					      sizeof(date_modified),
					      msginfo->date_t);
		text[S_COL_DATE] = date_modified;
	} else if (msginfo->date)
		text[S_COL_DATE] = msginfo->date;
	else
		text[S_COL_DATE] = _("(No Date)");

	text[S_COL_FROM] = msginfo->fromname ? msginfo->fromname :
		_("(No From)");
	if (prefs_common.swap_from && msginfo->from && msginfo->to &&
	    !MSG_IS_NEWS(msginfo->flags)) {
		gchar *from;

		Xalloca(from, strlen(msginfo->from) + 1, return);
		strcpy(from, msginfo->from);
		extract_address(from);
		if (account_find_from_address(from)) {
			g_free(to);
			to = g_strconcat("-->", msginfo->to, NULL);
			text[S_COL_FROM] = to;
		}
	}

	if ((text[S_COL_FROM] != to) && prefs_common.use_addr_book &&
	    msginfo->from) {
		gint count;
		gchar *from;
  
		Xalloca(from, strlen(msginfo->from) + 1, return);
		strcpy(from, msginfo->from);
		extract_address(from);
		if (*from) {
			count = complete_address(from);
			if (count > 1) {
				g_free(from_name);
				from = get_complete_address(1);
				from_name = procheader_get_fromname(from);
				g_free(from);
				text[S_COL_FROM] = from_name;
			}
		}
	}

	text[S_COL_SUBJECT] = msginfo->subject ? msginfo->subject :
		_("(No Subject)");
}

#define CHANGE_FLAGS(msginfo) \
{ \
if (msginfo->folder->folder->change_flags != NULL) \
msginfo->folder->folder->change_flags(msginfo->folder->folder, \
				      msginfo->folder, \
				      msginfo); \
}

static void summary_display_msg(SummaryView *summaryview, GtkCTreeNode *row,
				gboolean new_window)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;
	gchar *filename;
	static gboolean lock = FALSE;

	if (!new_window && summaryview->displayed == row) return;
	g_return_if_fail(row != NULL);

	if (lock) return;
	lock = TRUE;

	STATUSBAR_POP(summaryview->mainwin);
	GTK_EVENTS_FLUSH();

	msginfo = gtk_ctree_node_get_row_data(ctree, row);

	filename = procmsg_get_message_file(msginfo);
	if (!filename) {
		lock = FALSE;
		return;
	}
	g_free(filename);

	if (MSG_IS_NEW(msginfo->flags) && !MSG_IS_IGNORE_THREAD(msginfo->flags))
		summaryview->newmsgs--;
	if (MSG_IS_UNREAD(msginfo->flags) && !MSG_IS_IGNORE_THREAD(msginfo->flags))
		summaryview->unread--;
	if (MSG_IS_NEW(msginfo->flags) || MSG_IS_UNREAD(msginfo->flags)) {
		MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_NEW | MSG_UNREAD);
		CHANGE_FLAGS(msginfo);
		summary_set_row_marks(summaryview, row);
		gtk_clist_thaw(GTK_CLIST(ctree));
		summary_status_show(summaryview);
	}

	if (new_window) {
		MessageView *msgview;

		msgview = messageview_create_with_new_window();
		messageview_show(msgview, msginfo);
	} else {
		MessageView *msgview;

		msgview = summaryview->messageview;

		summaryview->displayed = row;
		if (!summaryview->msg_is_toggled_on)
			summary_toggle_view(summaryview);
		messageview_show(msgview, msginfo);
		if (msgview->type == MVIEW_TEXT ||
		    (msgview->type == MVIEW_MIME &&
		     GTK_CLIST(msgview->mimeview->ctree)->row_list == NULL))
			gtk_widget_grab_focus(summaryview->ctree);
		GTK_EVENTS_FLUSH();
		gtkut_ctree_node_move_if_on_the_edge(ctree, row);
	}

	if (GTK_WIDGET_VISIBLE(summaryview->headerwin->window))
		header_window_show(summaryview->headerwin, msginfo);

	lock = FALSE;
}

void summary_redisplay_msg(SummaryView *summaryview)
{
	GtkCTreeNode *node;

	if (summaryview->displayed) {
		node = summaryview->displayed;
		summaryview->displayed = NULL;
		summary_display_msg(summaryview, node, FALSE);
	}
}

void summary_open_msg(SummaryView *summaryview)
{
	if (!summaryview->selected) return;

	summary_display_msg(summaryview, summaryview->selected, TRUE);
}

void summary_view_source(SummaryView * summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;
	SourceWindow *srcwin;

	if (!summaryview->selected) return;

	srcwin = source_window_create();
	msginfo = gtk_ctree_node_get_row_data(ctree, summaryview->selected);
	source_window_show_msg(srcwin, msginfo);
	source_window_show(srcwin);
}

void summary_reedit(SummaryView *summaryview)
{
	MsgInfo *msginfo;

	if (!summaryview->selected) return;
	if (!summaryview->folder_item ||
	    (summaryview->folder_item->stype != F_DRAFT &&
	     summaryview->folder_item->stype != F_OUTBOX &&
	     summaryview->folder_item->stype != F_QUEUE)) return;

	msginfo = gtk_ctree_node_get_row_data(GTK_CTREE(summaryview->ctree),
					      summaryview->selected);
	if (!msginfo) return;

	compose_reedit(msginfo);
}

void summary_step(SummaryView *summaryview, GtkScrollType type)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);

	if (type == GTK_SCROLL_STEP_FORWARD) {
		GtkCTreeNode *node;
		node = gtkut_ctree_node_next(ctree, summaryview->selected);
		if (node)
			gtkut_ctree_expand_parent_all(ctree, node);
	}

	gtk_signal_emit_by_name(GTK_OBJECT(ctree), "scroll_vertical",
				type, 0.0);

	if (summaryview->msg_is_toggled_on)
		summary_display_msg(summaryview, summaryview->selected, FALSE);
}

static void summary_toggle_view(SummaryView *summaryview)
{
	MainWindow *mainwin = summaryview->mainwin;
	union CompositeWin *cwin = &mainwin->win;
	GtkWidget *vpaned = NULL;
	GtkWidget *container = NULL;

	switch (mainwin->type) {
	case SEPARATE_NONE:
		vpaned = cwin->sep_none.vpaned;
		container = cwin->sep_none.hpaned;
		break;
	case SEPARATE_FOLDER:
		vpaned = cwin->sep_folder.vpaned;
		container = mainwin->vbox_body;
		break;
	case SEPARATE_MESSAGE:
	case SEPARATE_BOTH:
		return;
	}

	if (vpaned->parent != NULL) {
		summaryview->msg_is_toggled_on = FALSE;
		summaryview->displayed = NULL;
		gtk_widget_ref(vpaned);
		gtk_container_remove(GTK_CONTAINER(container), vpaned);
		gtk_widget_reparent(GTK_WIDGET_PTR(summaryview), container);
		gtk_arrow_set(GTK_ARROW(summaryview->toggle_arrow),
			      GTK_ARROW_UP, GTK_SHADOW_OUT);
	} else {
		summaryview->msg_is_toggled_on = TRUE;
		gtk_widget_reparent(GTK_WIDGET_PTR(summaryview), vpaned);
		gtk_container_add(GTK_CONTAINER(container), vpaned);
		gtk_widget_unref(vpaned);
		gtk_arrow_set(GTK_ARROW(summaryview->toggle_arrow),
			      GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	}

	gtk_widget_grab_focus(summaryview->ctree);
}

static gboolean summary_search_unread_recursive(GtkCTree *ctree,
						GtkCTreeNode *node)
{
	MsgInfo *msginfo;

	if (node) {
		msginfo = gtk_ctree_node_get_row_data(ctree, node);
		if (msginfo && MSG_IS_UNREAD(msginfo->flags))
			return TRUE;
		node = GTK_CTREE_ROW(node)->children;
	} else
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	while (node) {
		if (summary_search_unread_recursive(ctree, node) == TRUE)
			return TRUE;
		node = GTK_CTREE_ROW(node)->sibling;
	}

	return FALSE;
}

static gboolean summary_have_unread_children(SummaryView *summaryview,
					     GtkCTreeNode *node)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);

	if (!node) return FALSE;

	node = GTK_CTREE_ROW(node)->children;

	while (node) {
		if (summary_search_unread_recursive(ctree, node) == TRUE)
			return TRUE;
		node = GTK_CTREE_ROW(node)->sibling;
	}

	return FALSE;
}

static void summary_set_row_marks(SummaryView *summaryview, GtkCTreeNode *row)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkStyle *style = NULL;
	MsgInfo *msginfo;
	MsgFlags flags;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	if (!msginfo) return;

	flags = msginfo->flags;

	gtk_ctree_node_set_foreground(ctree, row, NULL);

	/* set new/unread column */
	if (MSG_IS_IGNORE_THREAD(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, S_COL_UNREAD,
					  ignorethreadxpm, ignorethreadxpmmask);
	} else if (MSG_IS_NEW(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, S_COL_UNREAD,
					  newxpm, newxpmmask);
	} else if (MSG_IS_UNREAD(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, S_COL_UNREAD,
					  unreadxpm, unreadxpmmask);
	} else if (MSG_IS_REPLIED(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, S_COL_UNREAD,
					  repliedxpm, repliedxpmmask);
	} else if (MSG_IS_FORWARDED(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, S_COL_UNREAD,
					  forwardedxpm, forwardedxpmmask);
	} else {
		gtk_ctree_node_set_text(ctree, row, S_COL_UNREAD, NULL);
	}

	if (prefs_common.bold_unread &&
	    (MSG_IS_UNREAD(flags) ||
	     (!GTK_CTREE_ROW(row)->expanded &&
	      GTK_CTREE_ROW(row)->children &&
	      summary_have_unread_children(summaryview, row))))
		style = bold_style;

	/* set mark column */
	if (MSG_IS_DELETED(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, S_COL_MARK,
					  deletedxpm, deletedxpmmask);
		if (style)
			style = bold_deleted_style;
		else {
			style = small_deleted_style;
		}
			gtk_ctree_node_set_foreground
				(ctree, row, &summaryview->color_dim);
	} else if (MSG_IS_MARKED(flags)) {
        	gtk_ctree_node_set_pixmap(ctree, row, S_COL_MARK,
					  markxpm, markxpmmask);
	} else if (MSG_IS_MOVE(flags)) {
		gtk_ctree_node_set_text(ctree, row, S_COL_MARK, "o");
		if (style)
			style = bold_marked_style;
		else {
			style = small_marked_style;
		}
			gtk_ctree_node_set_foreground
				(ctree, row, &summaryview->color_marked);
	} else if (MSG_IS_COPY(flags)) {
		gtk_ctree_node_set_text(ctree, row, S_COL_MARK, "O");
                if (style)
			style = bold_marked_style;
		else {
			style = small_marked_style;
		}
			gtk_ctree_node_set_foreground
                        	(ctree, row, &summaryview->color_marked);
	}
	else if ((global_scoring ||
		  summaryview->folder_item->prefs->scoring) &&
		 (msginfo->score >= summaryview->important_score) &&
		 (MSG_IS_MARKED(msginfo->flags) || MSG_IS_MOVE(msginfo->flags) || MSG_IS_COPY(msginfo->flags))) {
		gtk_ctree_node_set_text(ctree, row, S_COL_MARK, "!");
		gtk_ctree_node_set_foreground(ctree, row,
					      &summaryview->color_important);
	} else {
		gtk_ctree_node_set_text(ctree, row, S_COL_MARK, NULL);
	}

	if (MSG_IS_MIME(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, S_COL_MIME,
					  clipxpm, clipxpmmask);
	} else {
		gtk_ctree_node_set_text(ctree, row, S_COL_MIME, NULL);
	}
        if (!style)
		style = small_style;

	gtk_ctree_node_set_row_style(ctree, row, style);
}

void summary_set_marks_selected(SummaryView *summaryview)
{
	summary_set_row_marks(summaryview, summaryview->selected);
}

static void summary_mark_row(SummaryView *summaryview, GtkCTreeNode *row)
{
	gboolean changed = FALSE;
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	msginfo->to_folder = NULL;
	if (MSG_IS_DELETED(msginfo->flags))
		summaryview->deleted--;
	if (MSG_IS_MOVE(msginfo->flags)) {
		summaryview->moved--;
		changed = TRUE;
	}
	if (MSG_IS_COPY(msginfo->flags)) {
		summaryview->copied--;
		changed = TRUE;
	}
	if (changed && !prefs_common.immediate_exec) {
		msginfo->to_folder->op_count--;
		if (msginfo->to_folder->op_count == 0)
			folderview_update_item(msginfo->to_folder, 0);
	}
	msginfo->to_folder = NULL;
	MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_DELETED);
	MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_MOVE | MSG_COPY);
	MSG_SET_PERM_FLAGS(msginfo->flags, MSG_MARKED);
	CHANGE_FLAGS(msginfo);
	summary_set_row_marks(summaryview, row);
	debug_print(_("Message %d is marked\n"), msginfo->msgnum);
}

void summary_mark(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	for (cur = GTK_CLIST(ctree)->selection; cur != NULL; cur = cur->next)
		summary_mark_row(summaryview, GTK_CTREE_NODE(cur->data));

	/* summary_step(summaryview, GTK_SCROLL_STEP_FORWARD); */
	summary_status_show(summaryview);
}

static void summary_mark_row_as_read(SummaryView *summaryview,
				     GtkCTreeNode *row)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	if (MSG_IS_NEW(msginfo->flags) && !MSG_IS_IGNORE_THREAD(msginfo->flags))
		summaryview->newmsgs--;
	if (MSG_IS_UNREAD(msginfo->flags) && !MSG_IS_IGNORE_THREAD(msginfo->flags))
		summaryview->unread--;
	if (MSG_IS_NEW(msginfo->flags) ||
	    MSG_IS_UNREAD(msginfo->flags)) {
		MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_NEW | MSG_UNREAD);
		CHANGE_FLAGS(msginfo);
		summary_set_row_marks(summaryview, row);
		debug_print(_("Message %d is marked as read\n"),
			    msginfo->msgnum);
	}
}

void summary_mark_as_read(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	for (cur = GTK_CLIST(ctree)->selection; cur != NULL; cur = cur->next)
		summary_mark_row_as_read(summaryview,
					 GTK_CTREE_NODE(cur->data));

	summary_status_show(summaryview);
}

#if MARK_ALL_READ
static void summary_mark_all_read(SummaryView *summaryview)
{
	summary_select_all(summaryview);
	summary_mark_as_read(summaryview);
	summary_unselect_all(summaryview);
}
#endif

static void summary_mark_row_as_unread(SummaryView *summaryview,
				       GtkCTreeNode *row)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	if (MSG_IS_DELETED(msginfo->flags)) {
		msginfo->to_folder = NULL;
		MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_DELETED);
		summaryview->deleted--;
	}
	MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_REPLIED | MSG_FORWARDED);
	if (!MSG_IS_UNREAD(msginfo->flags)) {
		MSG_SET_PERM_FLAGS(msginfo->flags, MSG_UNREAD);
		gtk_ctree_node_set_pixmap(ctree, row, S_COL_UNREAD,
					  unreadxpm, unreadxpmmask);
		summaryview->unread++;
		debug_print(_("Message %d is marked as unread\n"),
			    msginfo->msgnum);
	}

	CHANGE_FLAGS(msginfo);

	summary_set_row_marks(summaryview, row);
}

void summary_mark_as_unread(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	for (cur = GTK_CLIST(ctree)->selection; cur != NULL; cur = cur->next)
		summary_mark_row_as_unread(summaryview,
					   GTK_CTREE_NODE(cur->data));

	summary_status_show(summaryview);
}

static void summary_delete_row(SummaryView *summaryview, GtkCTreeNode *row)
{
	gboolean changed = FALSE;
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);

	if (MSG_IS_DELETED(msginfo->flags)) return;

	if (MSG_IS_MOVE(msginfo->flags)) {
		summaryview->moved--;
		changed = TRUE;
	}
	if (MSG_IS_COPY(msginfo->flags)) {
		summaryview->copied--;
		changed = TRUE;
	}
	if (changed && !prefs_common.immediate_exec) {
		msginfo->to_folder->op_count--;
		if (msginfo->to_folder->op_count == 0)
			folderview_update_item(msginfo->to_folder, 0);
	}
	msginfo->to_folder = NULL;
	MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_MARKED);
	MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_MOVE | MSG_COPY);
	MSG_SET_PERM_FLAGS(msginfo->flags, MSG_DELETED);
	CHANGE_FLAGS(msginfo);
	summaryview->deleted++;

	if (!prefs_common.immediate_exec)
		summary_set_row_marks(summaryview, row);

	debug_print(_("Message %s/%d is set to delete\n"),
		    msginfo->folder->path, msginfo->msgnum);
}

void summary_delete(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	if (!summaryview->folder_item ||
	    summaryview->folder_item->folder->type == F_NEWS) return;

	/* if current folder is trash, don't delete */
	if (summaryview->folder_item->stype == F_TRASH) {
		alertpanel_notice(_("Current folder is Trash."));
		return;
	}

	for (cur = GTK_CLIST(ctree)->selection; cur != NULL; cur = cur->next)
		summary_delete_row(summaryview, GTK_CTREE_NODE(cur->data));

	summary_step(summaryview, GTK_SCROLL_STEP_FORWARD);

	if (prefs_common.immediate_exec)
		summary_execute(summaryview);
	else
		summary_status_show(summaryview);
}

void summary_delete_duplicated(SummaryView *summaryview)
{
	if (!summaryview->folder_item ||
	    summaryview->folder_item->folder->type == F_NEWS) return;
	if (summaryview->folder_item->stype == F_TRASH) return;

	main_window_cursor_wait(summaryview->mainwin);
	debug_print(_("Deleting duplicated messages..."));
	STATUSBAR_PUSH(summaryview->mainwin,
		       _("Deleting duplicated messages..."));

	gtk_ctree_pre_recursive(GTK_CTREE(summaryview->ctree), NULL,
				GTK_CTREE_FUNC(summary_delete_duplicated_func),
				summaryview);

	if (prefs_common.immediate_exec)
		summary_execute(summaryview);
	else
		summary_status_show(summaryview);

	debug_print(_("done.\n"));
	STATUSBAR_POP(summaryview->mainwin);
	main_window_cursor_normal(summaryview->mainwin);
}

static void summary_delete_duplicated_func(GtkCTree *ctree, GtkCTreeNode *node,
					   SummaryView *summaryview)
{
	GtkCTreeNode *found;
	MsgInfo *msginfo = GTK_CTREE_ROW(node)->row.data;

	if (!msginfo->msgid || !*msginfo->msgid) return;

	found = g_hash_table_lookup(summaryview->msgid_table, msginfo->msgid);

	if (found && found != node)
		summary_delete_row(summaryview, node);
}

static void summary_unmark_row(SummaryView *summaryview, GtkCTreeNode *row)
{
	gboolean changed = FALSE;
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	msginfo->to_folder = NULL;
	if (MSG_IS_DELETED(msginfo->flags))
		summaryview->deleted--;
	if (MSG_IS_MOVE(msginfo->flags)) {
		summaryview->moved--;
		changed = TRUE;
	}
	if (MSG_IS_COPY(msginfo->flags)) {
		summaryview->copied--;
		changed = TRUE;
	}
	if (changed && !prefs_common.immediate_exec) {
		msginfo->to_folder->op_count--;
		if (msginfo->to_folder->op_count == 0)
			folderview_update_item(msginfo->to_folder, 0);
	}
	msginfo->to_folder = NULL;
	MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_MARKED | MSG_DELETED);
	MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_MOVE | MSG_COPY);
	CHANGE_FLAGS(msginfo);
	summary_set_row_marks(summaryview, row);

	debug_print(_("Message %s/%d is unmarked\n"),
		    msginfo->folder->path, msginfo->msgnum);
}

void summary_unmark(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	for (cur = GTK_CLIST(ctree)->selection; cur != NULL;
	     cur = cur->next)
		summary_unmark_row(summaryview, GTK_CTREE_NODE(cur->data));

	summary_status_show(summaryview);
}

static void summary_move_row_to(SummaryView *summaryview, GtkCTreeNode *row,
				FolderItem *to_folder)
{
	gboolean changed = FALSE;
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	g_return_if_fail(to_folder != NULL);

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	if (MSG_IS_MOVE(msginfo->flags)) {
		if (!prefs_common.immediate_exec) {
			msginfo->to_folder->op_count--;
			if (msginfo->to_folder->op_count == 0) {
				folderview_update_item(msginfo->to_folder, 0);
				changed = TRUE;
			}
		}
	}
	msginfo->to_folder = to_folder;
	if (MSG_IS_DELETED(msginfo->flags))
		summaryview->deleted--;
	if (MSG_IS_COPY(msginfo->flags)) {
		summaryview->copied--;
		if (!prefs_common.immediate_exec)
			msginfo->to_folder->op_count--;
	}
	MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_MARKED | MSG_DELETED);
	MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_COPY);
	if (!MSG_IS_MOVE(msginfo->flags)) {
		MSG_SET_TMP_FLAGS(msginfo->flags, MSG_MOVE);
		summaryview->moved++;
		changed = TRUE;
	}
	if (!prefs_common.immediate_exec) {
		summary_set_row_marks(summaryview, row);
		if (changed) {
			msginfo->to_folder->op_count++;
			if (msginfo->to_folder->op_count == 1)
				folderview_update_item(msginfo->to_folder, 0);
		}
	}

	debug_print(_("Message %d is set to move to %s\n"),
		    msginfo->msgnum, to_folder->path);
}

void summary_move_selected_to(SummaryView *summaryview, FolderItem *to_folder)
{
	GList *cur;

	if (!to_folder) return;
	if (!summaryview->folder_item ||
	    summaryview->folder_item->folder->type == F_NEWS) return;
	if (summaryview->folder_item == to_folder) {
		alertpanel_notice(_("Destination is same as current folder."));
		return;
	}

	for (cur = GTK_CLIST(summaryview->ctree)->selection;
	     cur != NULL; cur = cur->next)
		summary_move_row_to
			(summaryview, GTK_CTREE_NODE(cur->data), to_folder);

	summary_step(summaryview, GTK_SCROLL_STEP_FORWARD);

	if (prefs_common.immediate_exec)
		summary_execute(summaryview);
	else {
		summary_status_show(summaryview);

		folderview_update_item(to_folder, 0);
	}
}

void summary_move_to(SummaryView *summaryview)
{
	FolderItem *to_folder;

	if (!summaryview->folder_item ||
	    summaryview->folder_item->folder->type == F_NEWS) return;

	to_folder = foldersel_folder_sel(NULL, NULL);
	summary_move_selected_to(summaryview, to_folder);
}

static void summary_copy_row_to(SummaryView *summaryview, GtkCTreeNode *row,
				FolderItem *to_folder)
{
	gboolean changed = FALSE;
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	g_return_if_fail(to_folder != NULL);

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	if (MSG_IS_COPY(msginfo->flags)) {
		if (!prefs_common.immediate_exec) {
			msginfo->to_folder->op_count--;
			if (msginfo->to_folder->op_count == 0) {
				folderview_update_item(msginfo->to_folder, 0);
				changed = TRUE;
			}
		}
	}
	msginfo->to_folder = to_folder;
	if (MSG_IS_DELETED(msginfo->flags))
		summaryview->deleted--;
	if (MSG_IS_MOVE(msginfo->flags)) {
		summaryview->moved--;
		if (!prefs_common.immediate_exec)
			msginfo->to_folder->op_count--;
	}
	MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_MARKED | MSG_DELETED);
	MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_MOVE);
	if (!MSG_IS_COPY(msginfo->flags)) {
		MSG_SET_TMP_FLAGS(msginfo->flags, MSG_COPY);
		summaryview->copied++;
		changed = TRUE;
	}
	if (!prefs_common.immediate_exec) {
		summary_set_row_marks(summaryview, row);
		if (changed) {
			msginfo->to_folder->op_count++;
			if (msginfo->to_folder->op_count == 1)
				folderview_update_item(msginfo->to_folder, 0);
		}
	}

	debug_print(_("Message %d is set to copy to %s\n"),
		    msginfo->msgnum, to_folder->path);
}

void summary_copy_selected_to(SummaryView *summaryview, FolderItem *to_folder)
{
	GList *cur;

	if (!to_folder) return;
	if (!summaryview->folder_item ||
	    summaryview->folder_item->folder->type == F_NEWS) return;
	if (summaryview->folder_item == to_folder) {
		alertpanel_notice
			(_("Destination to copy is same as current folder."));
		return;
	}

	for (cur = GTK_CLIST(summaryview->ctree)->selection;
	     cur != NULL; cur = cur->next)
		summary_copy_row_to
			(summaryview, GTK_CTREE_NODE(cur->data), to_folder);

	summary_step(summaryview, GTK_SCROLL_STEP_FORWARD);

	if (prefs_common.immediate_exec)
		summary_execute(summaryview);
	else {
		summary_status_show(summaryview);

		folderview_update_item(to_folder, 0);
	}
}

void summary_copy_to(SummaryView *summaryview)
{
	FolderItem *to_folder;

	if (!summaryview->folder_item ||
	    summaryview->folder_item->folder->type == F_NEWS) return;

	to_folder = foldersel_folder_sel(NULL, NULL);
	summary_copy_selected_to(summaryview, to_folder);
}

void summary_select_all(SummaryView *summaryview)
{
	if (summaryview->messages >= 500) {
		STATUSBAR_PUSH(summaryview->mainwin,
			       _("Selecting all messages..."));
		main_window_cursor_wait(summaryview->mainwin);
	}

	gtk_clist_select_all(GTK_CLIST(summaryview->ctree));

	if (summaryview->messages >= 500) {
		STATUSBAR_POP(summaryview->mainwin);
		main_window_cursor_normal(summaryview->mainwin);
	}
}

void summary_unselect_all(SummaryView *summaryview)
{
	gtk_sctree_unselect_all(GTK_SCTREE(summaryview->ctree));
}

void summary_save_as(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;
	gchar *src, *dest;

	if (!summaryview->selected) return;
	msginfo = gtk_ctree_node_get_row_data(ctree, summaryview->selected);
	if (!msginfo) return;

	dest = filesel_select_file(_("Save as"), NULL);
	if (!dest) return;
	if (is_file_exist(dest)) {
		AlertValue aval;

		aval = alertpanel(_("Overwrite"),
				  _("Overwrite existing file?"),
				  _("OK"), _("Cancel"), NULL);
		if (G_ALERTDEFAULT != aval) return;
	}

	src = procmsg_get_message_file(msginfo);
	copy_file(src, dest);
	g_free(src);
}

void summary_print(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCList *clist = GTK_CLIST(summaryview->ctree);
	MsgInfo *msginfo;
	GList *cur;
	gchar *cmdline;
	gchar *p;

	if (clist->selection == NULL) return;

	cmdline = input_dialog(_("Print"),
			       _("Enter the print command line:\n"
				 "(`%s' will be replaced with file name)"),
			       prefs_common.print_cmd);
	if (!cmdline) return;
	if (!(p = strchr(cmdline, '%')) || *(p + 1) != 's' ||
	    strchr(p + 2, '%')) {
		alertpanel_error(_("Print command line is invalid:\n`%s'"),
				 cmdline);
		g_free(cmdline);
		return;
	}

	for (cur = clist->selection; cur != NULL; cur = cur->next) {
		msginfo = gtk_ctree_node_get_row_data
			(ctree, GTK_CTREE_NODE(cur->data));
		if (msginfo) procmsg_print_message(msginfo, cmdline);
	}

	g_free(cmdline);
}

void summary_execute(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCList *clist = GTK_CLIST(summaryview->ctree);
	GtkCTreeNode *node, *next;

	if (!summaryview->folder_item ||
	    summaryview->folder_item->folder->type == F_NEWS) return;

	gtk_clist_freeze(clist);

	/*if (summaryview->folder_item->prefs->enable_thread) */
			if (prefs_common.enable_thread)
		summary_unthread_for_exec(summaryview);

	summary_execute_move(summaryview);
	summary_execute_copy(summaryview);
	summary_execute_delete(summaryview);

	node = GTK_CTREE_NODE(clist->row_list);
	while (node != NULL) {
		next = gtkut_ctree_node_next(ctree, node);
		if (gtk_ctree_node_get_row_data(ctree, node) == NULL) {
			if (node == summaryview->displayed) {
				messageview_clear(summaryview->messageview);
				summaryview->displayed = NULL;
			}
			if (GTK_CTREE_ROW(node)->children != NULL)
				g_warning("summary_execute(): children != NULL\n");
			else
				gtk_ctree_remove_node(ctree, node);
		}
		node = next;
	}

	/*if (summaryview->folder_item->prefs->enable_thread) */
	if (prefs_common.enable_thread) 
		summary_thread_build(summaryview, FALSE);

	summaryview->selected = clist->selection ?
		GTK_CTREE_NODE(clist->selection->data) : NULL;

	if (!GTK_CLIST(summaryview->ctree)->row_list) {
		menu_set_insensitive_all
			(GTK_MENU_SHELL(summaryview->popupmenu));
		gtk_widget_grab_focus(summaryview->folderview->ctree);
	} else
		gtk_widget_grab_focus(summaryview->ctree);

	summary_update_status(summaryview);
	summary_status_show(summaryview);

	summary_write_cache(summaryview);

	gtk_ctree_node_moveto(ctree, summaryview->selected, -1, 0.5, 0);

	gtk_clist_thaw(clist);
}

static void summary_execute_move(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GSList *cur;

	summaryview->folder_table = g_hash_table_new(NULL, NULL);

	/* search moving messages and execute */
	gtk_ctree_pre_recursive(ctree, NULL, summary_execute_move_func,
				summaryview);

	if (summaryview->mlist) {
		procmsg_move_messages(summaryview->mlist);

		folder_item_scan_foreach(summaryview->folder_table);
		folderview_update_item_foreach(summaryview->folder_table);

		for (cur = summaryview->mlist; cur != NULL; cur = cur->next)
			procmsg_msginfo_free((MsgInfo *)cur->data);
		g_slist_free(summaryview->mlist);
		summaryview->mlist = NULL;
	}

	g_hash_table_destroy(summaryview->folder_table);
	summaryview->folder_table = NULL;
}

static void summary_execute_move_func(GtkCTree *ctree, GtkCTreeNode *node,
				      gpointer data)
{
	SummaryView *summaryview = data;
	MsgInfo *msginfo;

	msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);

	if (msginfo && MSG_IS_MOVE(msginfo->flags) && msginfo->to_folder) {
		g_hash_table_insert(summaryview->folder_table,
				    msginfo->to_folder, GINT_TO_POINTER(1));

		summaryview->mlist =
			g_slist_append(summaryview->mlist, msginfo);
		gtk_ctree_node_set_row_data(ctree, node, NULL);

		if (msginfo->msgid && *msginfo->msgid &&
		    node == g_hash_table_lookup(summaryview->msgid_table,
						msginfo->msgid))
			g_hash_table_remove(summaryview->msgid_table,
					    msginfo->msgid);
	}
}

static void summary_execute_copy(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);

	summaryview->folder_table = g_hash_table_new(NULL, NULL);

	/* search copying messages and execute */
	gtk_ctree_pre_recursive(ctree, NULL, summary_execute_copy_func,
				summaryview);

	if (summaryview->mlist) {
		procmsg_copy_messages(summaryview->mlist);

		folder_item_scan_foreach(summaryview->folder_table);
		folderview_update_item_foreach(summaryview->folder_table);

		g_slist_free(summaryview->mlist);
		summaryview->mlist = NULL;
	}

	g_hash_table_destroy(summaryview->folder_table);
	summaryview->folder_table = NULL;
}

static void summary_execute_copy_func(GtkCTree *ctree, GtkCTreeNode *node,
				      gpointer data)
{
	SummaryView *summaryview = data;
	MsgInfo *msginfo;

	msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);

	if (msginfo && MSG_IS_COPY(msginfo->flags) && msginfo->to_folder) {
		g_hash_table_insert(summaryview->folder_table,
				    msginfo->to_folder, GINT_TO_POINTER(1));

		summaryview->mlist =
			g_slist_append(summaryview->mlist, msginfo);

		MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_COPY);
		summary_set_row_marks(summaryview, node);
	}
}

static void summary_execute_delete(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	FolderItem *trash;
	GSList *cur;

	trash = summaryview->folder_item->folder->trash;
	if (summaryview->folder_item->folder->type == F_MH) {
		g_return_if_fail(trash != NULL);
	}
	if (summaryview->folder_item == trash) return;

	/* search deleting messages and execute */
	gtk_ctree_pre_recursive
		(ctree, NULL, summary_execute_delete_func, summaryview);

	if (!summaryview->mlist) return;

	for(cur = summaryview->mlist ; cur != NULL ; cur = cur->next) {
		MsgInfo * msginfo = cur->data;
		MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_DELETED);
	}

	folder_item_move_msgs_with_dest(trash, summaryview->mlist);

	for (cur = summaryview->mlist; cur != NULL; cur = cur->next)
		procmsg_msginfo_free((MsgInfo *)cur->data);

	g_slist_free(summaryview->mlist);
	summaryview->mlist = NULL;

	folder_item_scan(trash);
	folderview_update_item(trash, FALSE);
}

static void summary_execute_delete_func(GtkCTree *ctree, GtkCTreeNode *node,
					gpointer data)
{
	SummaryView *summaryview = data;
	MsgInfo *msginfo;

	msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);

	if (msginfo && MSG_IS_DELETED(msginfo->flags)) {
		summaryview->mlist =
			g_slist_append(summaryview->mlist, msginfo);
		gtk_ctree_node_set_row_data(ctree, node, NULL);

		if (msginfo->msgid && *msginfo->msgid &&
		    node == g_hash_table_lookup(summaryview->msgid_table,
						msginfo->msgid))
			g_hash_table_remove(summaryview->msgid_table,
					    msginfo->msgid);

		if (msginfo->subject && 
		    node == subject_table_lookup(summaryview->subject_table, 
						msginfo->subject)) {
			gchar *s = msginfo->subject + (g_strncasecmp(msginfo->subject, "Re: ", 4) == 0 ? 4 : 0);
			g_hash_table_remove(summaryview->subject_table, s);
		}			
	}
}

/* thread functions */

void summary_thread_build(SummaryView *summaryview, gboolean init)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	GtkCTreeNode *next;
	GtkCTreeNode *parent;
	MsgInfo *msginfo;

	debug_print(_("Building threads..."));
	STATUSBAR_PUSH(summaryview->mainwin, _("Building threads..."));
	main_window_cursor_wait(summaryview->mainwin);

	gtk_signal_handler_block_by_func(GTK_OBJECT(ctree),
					 summary_tree_expanded, summaryview);
	gtk_clist_freeze(GTK_CLIST(ctree));

	node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	while (node) {
		next = GTK_CTREE_ROW(node)->sibling;

		msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);
		if (msginfo && msginfo->inreplyto) {
			parent = g_hash_table_lookup(summaryview->msgid_table,
						     msginfo->inreplyto);
			if (parent && parent != node) {
				gtk_ctree_move(ctree, node, parent, NULL);
				gtk_ctree_expand(ctree, node);
			}
		}

		node = next;
	}

	node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	/* for optimization */
	if (init) {
		if (prefs_common.expand_thread) {
			while (node) {
				next = GTK_CTREE_ROW(node)->sibling;
				if (GTK_CTREE_ROW(node)->children)
					gtk_ctree_expand(ctree, node);
				node = next;
			}
		} else if (prefs_common.bold_unread) {
			while (node) {
				next = GTK_CTREE_ROW(node)->sibling;
				if (GTK_CTREE_ROW(node)->children)
					summary_set_row_marks
						(summaryview, node);
				node = next;
			}
		}
	} else {
		while (node) {
			next = GTK_CTREE_NODE_NEXT(node);
			if (prefs_common.expand_thread)
				gtk_ctree_expand(ctree, node);
			if (prefs_common.bold_unread &&
			    GTK_CTREE_ROW(node)->children)
				summary_set_row_marks(summaryview, node);
			node = next;
		}
	}

	gtk_clist_thaw(GTK_CLIST(ctree));
	gtk_signal_handler_unblock_by_func(GTK_OBJECT(ctree),
					   summary_tree_expanded, summaryview);

	debug_print(_("done.\n"));
	STATUSBAR_POP(summaryview->mainwin);
	main_window_cursor_normal(summaryview->mainwin);
}

void summary_unthread(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	GtkCTreeNode *child;
	GtkCTreeNode *sibling;
	GtkCTreeNode *next_child;

	debug_print(_("Unthreading..."));
	STATUSBAR_PUSH(summaryview->mainwin, _("Unthreading..."));
	main_window_cursor_wait(summaryview->mainwin);

	gtk_signal_handler_block_by_func(GTK_OBJECT(ctree),
					 summary_tree_collapsed, summaryview);
	gtk_clist_freeze(GTK_CLIST(ctree));

	for (node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	     node != NULL; node = GTK_CTREE_NODE_NEXT(node)) {
		child = GTK_CTREE_ROW(node)->children;
		sibling = GTK_CTREE_ROW(node)->sibling;

		while (child != NULL) {
			next_child = GTK_CTREE_ROW(child)->sibling;
			gtk_ctree_move(ctree, child, NULL, sibling);
			child = next_child;
		}
	}

	gtk_clist_thaw(GTK_CLIST(ctree));
	gtk_signal_handler_unblock_by_func(GTK_OBJECT(ctree),
					   summary_tree_collapsed, summaryview);

	debug_print(_("done.\n"));
	STATUSBAR_POP(summaryview->mainwin);
	main_window_cursor_normal(summaryview->mainwin);
}

static void summary_unthread_for_exec(SummaryView *summaryview)
{
	GtkCTreeNode *node;
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);

	debug_print(_("Unthreading for execution..."));

	gtk_clist_freeze(GTK_CLIST(ctree));

	for (node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	     node != NULL; node = GTK_CTREE_NODE_NEXT(node)) {
		summary_unthread_for_exec_func(ctree, node, NULL);
	}

	gtk_clist_thaw(GTK_CLIST(ctree));

	debug_print(_("done.\n"));
}

static void summary_unthread_for_exec_func(GtkCTree *ctree, GtkCTreeNode *node,
					   gpointer data)
{
	MsgInfo *msginfo;
	GtkCTreeNode *top_parent;
	GtkCTreeNode *child;
	GtkCTreeNode *sibling;

	msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);

	if (!msginfo ||
	    (!MSG_IS_MOVE(msginfo->flags) &&
	     !MSG_IS_DELETED(msginfo->flags)))
		return;
	child = GTK_CTREE_ROW(node)->children;
	if (!child) return;

	for (top_parent = node;
	     GTK_CTREE_ROW(top_parent)->parent != NULL;
	     top_parent = GTK_CTREE_ROW(top_parent)->parent)
		;
	sibling = GTK_CTREE_ROW(top_parent)->sibling;

	while (child != NULL) {
		GtkCTreeNode *next_child;

		next_child = GTK_CTREE_ROW(child)->sibling;
		gtk_ctree_move(ctree, child, NULL, sibling);
		child = next_child;
	}
}

void summary_filter(SummaryView *summaryview)
{
	if (!prefs_common.fltlist) return;

	debug_print(_("filtering..."));
	STATUSBAR_PUSH(summaryview->mainwin, _("Filtering..."));
	main_window_cursor_wait(summaryview->mainwin);

	gtk_clist_freeze(GTK_CLIST(summaryview->ctree));

	if (prefs_filtering == NULL) {
		gtk_ctree_pre_recursive(GTK_CTREE(summaryview->ctree), NULL,
					GTK_CTREE_FUNC(summary_filter_func),
					summaryview);
		
		gtk_clist_thaw(GTK_CLIST(summaryview->ctree));

		if (prefs_common.immediate_exec)
			summary_execute(summaryview);
		else
			summary_status_show(summaryview);
	}
	else {
		summaryview->folder_table = g_hash_table_new(NULL, NULL);

		gtk_ctree_pre_recursive(GTK_CTREE(summaryview->ctree), NULL,
					GTK_CTREE_FUNC(summary_filter_func),
					summaryview);

		gtk_clist_thaw(GTK_CLIST(summaryview->ctree));

		folder_item_scan_foreach(summaryview->folder_table);
		folderview_update_item_foreach(summaryview->folder_table);

		g_hash_table_destroy(summaryview->folder_table);
		summaryview->folder_table = NULL;

		summary_show(summaryview, summaryview->folder_item, FALSE);
	}

	debug_print(_("done.\n"));
	STATUSBAR_POP(summaryview->mainwin);
	main_window_cursor_normal(summaryview->mainwin);
}

static void summary_filter_func(GtkCTree *ctree, GtkCTreeNode *node,
				gpointer data)
{
	MsgInfo *msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);
	SummaryView *summaryview = data;
	gchar *file;
	FolderItem *dest;

	if (prefs_filtering == NULL) {
		/* old filtering */
		file = procmsg_get_message_file_path(msginfo);
		dest = filter_get_dest_folder(prefs_common.fltlist, file);
		g_free(file);

		if (dest && strcmp2(dest->path, FILTER_NOT_RECEIVE) != 0 &&
		    summaryview->folder_item != dest)
			summary_move_row_to(summaryview, node, dest);
	}
	else
		filter_msginfo_move_or_delete(prefs_filtering, msginfo,
					      summaryview->folder_table);
}

/* callback functions */

static void summary_toggle_pressed(GtkWidget *eventbox, GdkEventButton *event,
				   SummaryView *summaryview)
{
	if (!event)
		return;

	if (!summaryview->msg_is_toggled_on && summaryview->selected)
		summary_display_msg(summaryview, summaryview->selected, FALSE);
	else
		summary_toggle_view(summaryview);
}

static void summary_button_pressed(GtkWidget *ctree, GdkEventButton *event,
				   SummaryView *summaryview)
{
	if (!event) return;

	if (event->button == 3) {
		/* right clicked */
		summary_set_add_sender_menu(summaryview);
		gtk_menu_popup(GTK_MENU(summaryview->popupmenu), NULL, NULL,
			       NULL, NULL, event->button, event->time);
	} else if (event->button == 2) {
		summaryview->display_msg = TRUE;
	} else if (event->button == 1) {
		if (!prefs_common.emulate_emacs &&
		    summaryview->msg_is_toggled_on)
			summaryview->display_msg = TRUE;
	}
}

static void summary_button_released(GtkWidget *ctree, GdkEventButton *event,
				    SummaryView *summaryview)
{
}

void summary_pass_key_press_event(SummaryView *summaryview, GdkEventKey *event)
{
	summary_key_pressed(summaryview->ctree, event, summaryview);
}

#define BREAK_ON_MODIFIER_KEY() \
	if ((event->state & (GDK_MOD1_MASK|GDK_CONTROL_MASK)) != 0) break

#define RETURN_IF_LOCKED() \
	if (summaryview->mainwin->lock_count) return

#define KEY_PRESS_EVENT_STOP() \
	if (gtk_signal_n_emissions_by_name \
		(GTK_OBJECT(ctree), "key_press_event") > 0) { \
		gtk_signal_emit_stop_by_name(GTK_OBJECT(ctree), \
					     "key_press_event"); \
	}

static void summary_key_pressed(GtkWidget *widget, GdkEventKey *event,
				SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(widget);
	GtkCTreeNode *node;
	FolderItem *to_folder;

	if (!event) return;

	switch (event->keyval) {
	case GDK_g:		/* Go */
	case GDK_G:
		RETURN_IF_LOCKED();
		BREAK_ON_MODIFIER_KEY();
		KEY_PRESS_EVENT_STOP();
		to_folder = foldersel_folder_sel(NULL, NULL);
		if (to_folder) {
			debug_print(_("Go to %s\n"), to_folder->path);
			folderview_select(summaryview->folderview, to_folder);
		}
		return;
	case GDK_w:		/* Write new message */
		BREAK_ON_MODIFIER_KEY();
		if (summaryview->folder_item) {
			PrefsAccount *ac;
			ac = summaryview->folder_item->folder->account;
			if (ac && ac->protocol == A_NNTP)
				compose_new_with_recipient
					(ac, summaryview->folder_item->path);
			else
				compose_new_with_folderitem(ac, summaryview->folder_item);
		} else
			compose_new(NULL);
		return;
	case GDK_D:		/* Empty trash */
		RETURN_IF_LOCKED();
		BREAK_ON_MODIFIER_KEY();
		KEY_PRESS_EVENT_STOP();
		main_window_empty_trash(summaryview->mainwin, TRUE);
		return;
	case GDK_Q:		/* Quit */
		RETURN_IF_LOCKED();
		BREAK_ON_MODIFIER_KEY();

		if (prefs_common.confirm_on_exit) {
			if (alertpanel(_("Exit"), _("Exit this program?"),
				       _("OK"), _("Cancel"), NULL)
				       == G_ALERTDEFAULT) {
				manage_window_focus_in
					(summaryview->mainwin->window,
					 NULL, NULL);
				app_will_exit(NULL, summaryview->mainwin);
			}
		}
		return;
	case GDK_Left:		/* Move focus */
	case GDK_Escape:
		gtk_widget_grab_focus(summaryview->folderview->ctree);
		return;
	default:
		break;
	}

	if (!summaryview->selected) {
		node = gtk_ctree_node_nth(ctree, 0);
		if (node)
			gtk_ctree_select(ctree, node);
		else
			return;
	}

	switch (event->keyval) {
	case GDK_space:		/* Page down or go to the next */
		if (summaryview->displayed != summaryview->selected) {
			summary_display_msg(summaryview,
					    summaryview->selected, FALSE);
			break;
		}
		if (!textview_scroll_page(summaryview->messageview->textview,
					  FALSE))
			summary_select_next_unread(summaryview);
		break;
	case GDK_n:		/* Next */
	case GDK_N:
		BREAK_ON_MODIFIER_KEY();
		summary_step(summaryview, GTK_SCROLL_STEP_FORWARD);
		break;
	case GDK_BackSpace:	/* Page up */
	case GDK_Delete:
		textview_scroll_page(summaryview->messageview->textview, TRUE);
		break;
	case GDK_p:		/* Prev */
	case GDK_P:
		BREAK_ON_MODIFIER_KEY();
		summary_step(summaryview, GTK_SCROLL_STEP_BACKWARD);
		break;
	case GDK_v:		/* Toggle summary mode / message mode */
	case GDK_V:
		BREAK_ON_MODIFIER_KEY();

		if (!summaryview->msg_is_toggled_on && summaryview->selected)
			summary_display_msg(summaryview,
					    summaryview->selected, FALSE);
		else
			summary_toggle_view(summaryview);
		break;
	case GDK_Return:	/* Scroll up/down one line */
		if (summaryview->displayed != summaryview->selected) {
			summary_display_msg(summaryview,
					    summaryview->selected, FALSE);
			break;
		}
		textview_scroll_one_line(summaryview->messageview->textview,
					 (event->state & GDK_MOD1_MASK) != 0);
		break;
	case GDK_asterisk:	/* Mark */
		summary_mark(summaryview);
		break;
	case GDK_exclam:	/* Mark as unread */
		summary_mark_as_unread(summaryview);
		break;
	case GDK_d:		/* Delete */
		RETURN_IF_LOCKED();
		BREAK_ON_MODIFIER_KEY();
		summary_delete(summaryview);
		break;
	case GDK_u:		/* Unmark */
	case GDK_U:
		BREAK_ON_MODIFIER_KEY();
		summary_unmark(summaryview);
		break;
	case GDK_o:		/* Move */
		RETURN_IF_LOCKED();
		BREAK_ON_MODIFIER_KEY();
		summary_move_to(summaryview);
		break;
	case GDK_O:		/* Copy */
		RETURN_IF_LOCKED();
		BREAK_ON_MODIFIER_KEY();
		summary_copy_to(summaryview);
		break;
	case GDK_x:		/* Execute */
	case GDK_X:
		RETURN_IF_LOCKED();
		BREAK_ON_MODIFIER_KEY();
		KEY_PRESS_EVENT_STOP();
		summary_execute(summaryview);
		break;
	case GDK_a:		/* Reply to the message */
		BREAK_ON_MODIFIER_KEY();
		summary_reply_cb(summaryview,
				 COMPOSE_REPLY_TO_ALL_WITHOUT_QUOTE, NULL);
		break;
	case GDK_A:		/* Reply to the message with quotation */
		BREAK_ON_MODIFIER_KEY();
		summary_reply_cb(summaryview,
				 COMPOSE_REPLY_TO_ALL_WITH_QUOTE, NULL);
		break;
	case GDK_f:		/* Forward the message */
		BREAK_ON_MODIFIER_KEY();
		summary_reply_cb(summaryview, COMPOSE_FORWARD, NULL);
		break;
	case GDK_F:
		BREAK_ON_MODIFIER_KEY();
		summary_reply_cb(summaryview, COMPOSE_FORWARD_AS_ATTACH, NULL);
		break;
	case GDK_y:		/* Save the message */
		BREAK_ON_MODIFIER_KEY();
		summary_save_as(summaryview);
		break;
	default:
		break;
	}
}

#undef BREAK_ON_MODIFIER_KEY
#undef RETURN_IF_LOCKED
#undef KEY_PRESS_EVENT_STOP

static void summary_open_row(GtkSCTree *sctree, SummaryView *summaryview)
{
	if (summaryview->folder_item->stype == F_DRAFT)
		summary_reedit(summaryview);
	else
		summary_open_msg(summaryview);

	summaryview->display_msg = FALSE;
}

static void summary_tree_expanded(GtkCTree *ctree, GtkCTreeNode *node,
				  SummaryView *summaryview)
{
	summary_set_row_marks(summaryview, node);
}

static void summary_tree_collapsed(GtkCTree *ctree, GtkCTreeNode *node,
				   SummaryView *summaryview)
{
	summary_set_row_marks(summaryview, node);
}

static void summary_selected(GtkCTree *ctree, GtkCTreeNode *row,
			     gint column, SummaryView *summaryview)
{
	MsgInfo *msginfo;

	summary_status_show(summaryview);
	summary_set_menu_sensitive(summaryview);

	if (GTK_CLIST(ctree)->selection &&
	     GTK_CLIST(ctree)->selection->next) {
		summaryview->display_msg = FALSE;
		return;
	}

	summaryview->selected = row;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);

	switch (column) {
	case S_COL_MARK:
		if (MSG_IS_MARKED(msginfo->flags)) {
			MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_MARKED);
			CHANGE_FLAGS(msginfo);
			summary_set_row_marks(summaryview, row);
		} else
			summary_mark_row(summaryview, row);
		break;
	case S_COL_UNREAD:
		if (MSG_IS_UNREAD(msginfo->flags)) {
			summary_mark_row_as_read(summaryview, row);
			summary_status_show(summaryview);
		} else if (!MSG_IS_REPLIED(msginfo->flags) &&
			 !MSG_IS_FORWARDED(msginfo->flags)) {
			summary_mark_row_as_unread(summaryview, row);
			summary_status_show(summaryview);
		}
		break;
	default:
		break;
	}

	if (summaryview->display_msg)
		summary_display_msg(summaryview, row, FALSE);

	summaryview->display_msg = FALSE;
}

static void summary_col_resized(GtkCList *clist, gint column, gint width,
				SummaryView *summaryview)
{
	switch (column) {
	case S_COL_MARK:
		prefs_common.summary_col_mark = width;
		break;
	case S_COL_UNREAD:
		prefs_common.summary_col_unread = width;
		break;
	case S_COL_MIME:
		prefs_common.summary_col_mime = width;
		break;
	case S_COL_NUMBER:
		prefs_common.summary_col_number = width;
		break;
	case S_COL_SCORE:
		prefs_common.summary_col_score = width;
		break;
	case S_COL_SIZE:
		prefs_common.summary_col_size = width;
		break;
	case S_COL_DATE:
		prefs_common.summary_col_date = width;
		break;
	case S_COL_FROM:
		prefs_common.summary_col_from = width;
		break;
	case S_COL_SUBJECT:
		prefs_common.summary_col_subject = width;
		break;
	default:
		break;
	}
}

static void summary_reply_cb(SummaryView *summaryview, guint action,
			     GtkWidget *widget)
{
	MsgInfo *msginfo;
	GList  *sel = GTK_CLIST(summaryview->ctree)->selection;

	msginfo = gtk_ctree_node_get_row_data(GTK_CTREE(summaryview->ctree),
					      summaryview->selected);
	if (!msginfo) return;

	switch ((ComposeReplyMode)action) {
	case COMPOSE_REPLY:
		compose_reply(msginfo, prefs_common.reply_with_quote,
			      FALSE, FALSE);
		break;
	case COMPOSE_REPLY_WITH_QUOTE:
		compose_reply(msginfo, TRUE, FALSE, FALSE);
		break;
	case COMPOSE_REPLY_WITHOUT_QUOTE:
		compose_reply(msginfo, FALSE, FALSE, FALSE);
		break;
	case COMPOSE_REPLY_TO_SENDER:
		compose_reply(msginfo, prefs_common.reply_with_quote,
			      FALSE, TRUE);
		break;
	case COMPOSE_FOLLOWUP_AND_REPLY_TO:
		compose_followup_and_reply_to(msginfo,
					      prefs_common.reply_with_quote,
					      FALSE, TRUE);
		break;
	case COMPOSE_REPLY_TO_SENDER_WITH_QUOTE:
		compose_reply(msginfo, TRUE, FALSE, TRUE);
		break;
	case COMPOSE_REPLY_TO_SENDER_WITHOUT_QUOTE:
		compose_reply(msginfo, FALSE, FALSE, TRUE);
		break;
	case COMPOSE_REPLY_TO_ALL:
		compose_reply(msginfo, prefs_common.reply_with_quote,
			      TRUE, FALSE);
		break;
	case COMPOSE_REPLY_TO_ALL_WITH_QUOTE:
		compose_reply(msginfo, TRUE, TRUE, FALSE);
		break;
	case COMPOSE_REPLY_TO_ALL_WITHOUT_QUOTE:
		compose_reply(msginfo, FALSE, TRUE, FALSE);
		break;
	case COMPOSE_FORWARD:
		if (!sel->next)	{
			compose_forward(NULL, msginfo, FALSE);
			break;
		}
		/* if (sel->next) FALL THROUGH */
	case COMPOSE_FORWARD_AS_ATTACH:
		{
			GSList *msginfo_list = NULL;
			for ( ; sel != NULL; sel = sel->next)
				msginfo_list = g_slist_append(msginfo_list, 
					gtk_ctree_node_get_row_data(GTK_CTREE(summaryview->ctree),
						GTK_CTREE_NODE(sel->data)));
			compose_forward_multiple(NULL, msginfo_list);
			g_slist_free(msginfo_list);
		}			
		break;
	default:
		g_warning("summary_reply_cb(): invalid action: %d\n", action);
	}

	summary_set_marks_selected(summaryview);
}

static void summary_show_all_header_cb(SummaryView *summaryview,
				       guint action, GtkWidget *widget)
{
	header_window_show_cb(summaryview->mainwin, action, widget);
}

static void summary_num_clicked(GtkWidget *button, SummaryView *summaryview)
{
	summary_sort(summaryview, SORT_BY_NUMBER);
}

static void summary_score_clicked(GtkWidget *button,
				  SummaryView *summaryview)
{
	summary_sort(summaryview, SORT_BY_SCORE);
}

static void summary_size_clicked(GtkWidget *button, SummaryView *summaryview)
{
	summary_sort(summaryview, SORT_BY_SIZE);
}

static void summary_date_clicked(GtkWidget *button, SummaryView *summaryview)
{
	summary_sort(summaryview, SORT_BY_DATE);
}

static void summary_from_clicked(GtkWidget *button, SummaryView *summaryview)
{
	summary_sort(summaryview, SORT_BY_FROM);
}

static void summary_subject_clicked(GtkWidget *button,
				    SummaryView *summaryview)
{
	summary_sort(summaryview, SORT_BY_SUBJECT);
}

static void summary_mark_clicked(GtkWidget *button,
				 SummaryView *summaryview)
{
	summary_sort(summaryview, SORT_BY_LABEL);
}

void summary_change_display_item(SummaryView *summaryview)
{
	GtkCList *clist = GTK_CLIST(summaryview->ctree);

	gtk_clist_set_column_visibility(clist, S_COL_MARK, prefs_common.show_mark);
	gtk_clist_set_column_visibility(clist, S_COL_UNREAD, prefs_common.show_unread);
	gtk_clist_set_column_visibility(clist, S_COL_MIME, prefs_common.show_mime);
	gtk_clist_set_column_visibility(clist, S_COL_NUMBER, prefs_common.show_number);
	gtk_clist_set_column_visibility(clist, S_COL_SCORE, prefs_common.show_score);
	gtk_clist_set_column_visibility(clist, S_COL_SIZE, prefs_common.show_size);
	gtk_clist_set_column_visibility(clist, S_COL_DATE, prefs_common.show_date);
	gtk_clist_set_column_visibility(clist, S_COL_FROM, prefs_common.show_from);
	gtk_clist_set_column_visibility(clist, S_COL_SUBJECT, prefs_common.show_subject);
}

static void summary_start_drag(GtkWidget *widget, gint button, GdkEvent *event,
			       SummaryView *summaryview)
{
	GtkTargetList *list;
	GdkDragContext *context;

	g_return_if_fail(summaryview != NULL);
	g_return_if_fail(summaryview->folder_item != NULL);
	g_return_if_fail(summaryview->folder_item->folder != NULL);
	if (summaryview->folder_item->folder->type == F_NEWS ||
	    summaryview->selected == NULL)
		return;

	list = gtk_target_list_new(summary_drag_types, 1);

	context = gtk_drag_begin(widget, list,
				 GDK_ACTION_MOVE, button, event);
	gtk_drag_set_icon_default(context);
}

static void summary_drag_data_get(GtkWidget        *widget,
				  GdkDragContext   *drag_context,
				  GtkSelectionData *selection_data,
				  guint             info,
				  guint             time,
				  SummaryView      *summaryview)
{
	if (info == TARGET_MAIL_URI_LIST) {
		GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
		GList *cur;
		MsgInfo *msginfo;
		gchar *mail_list = NULL, *tmp1, *tmp2;

		for (cur = GTK_CLIST(ctree)->selection;
		     cur != NULL; cur = cur->next) {
			msginfo = gtk_ctree_node_get_row_data
				(ctree, GTK_CTREE_NODE(cur->data));
			tmp2 = procmsg_get_message_file_path(msginfo);
			if (!tmp2) continue;
			tmp1 = g_strconcat("file:/", tmp2, NULL);
			g_free(tmp2);

			if (!mail_list) {
				mail_list = tmp1;
			} else {
				tmp2 = g_strconcat(mail_list, tmp1, NULL);
				g_free(mail_list);
				g_free(tmp1);
				mail_list = tmp2;
			}
		}

		if (mail_list != NULL) {
			gtk_selection_data_set(selection_data,
					       selection_data->target, 8,
					       mail_list, strlen(mail_list));
			g_free(mail_list);
		} 
	} else if (info == TARGET_DUMMY) {
		if (GTK_CLIST(summaryview->ctree)->selection)
			gtk_selection_data_set(selection_data,
					       selection_data->target, 8,
					       "Dummy", 6);
	}
}


/* custom compare functions for sorting */

static gint summary_cmp_by_num(GtkCList *clist,
			       gconstpointer ptr1, gconstpointer ptr2)
{
	MsgInfo *msginfo1 = ((GtkCListRow *)ptr1)->data;
	MsgInfo *msginfo2 = ((GtkCListRow *)ptr2)->data;

	return msginfo1->msgnum - msginfo2->msgnum;
}

static gint summary_cmp_by_size(GtkCList *clist,
				gconstpointer ptr1, gconstpointer ptr2)
{
	MsgInfo *msginfo1 = ((GtkCListRow *)ptr1)->data;
	MsgInfo *msginfo2 = ((GtkCListRow *)ptr2)->data;

	return msginfo1->size - msginfo2->size;
}

static gint summary_cmp_by_date(GtkCList *clist,
			       gconstpointer ptr1, gconstpointer ptr2)
{
	MsgInfo *msginfo1 = ((GtkCListRow *)ptr1)->data;
	MsgInfo *msginfo2 = ((GtkCListRow *)ptr2)->data;

	return msginfo1->date_t - msginfo2->date_t;
}

static gint summary_cmp_by_from(GtkCList *clist,
			       gconstpointer ptr1, gconstpointer ptr2)
{
	MsgInfo *msginfo1 = ((GtkCListRow *)ptr1)->data;
	MsgInfo *msginfo2 = ((GtkCListRow *)ptr2)->data;

	if (!msginfo1->fromname)
		return (msginfo2->fromname != NULL);
	if (!msginfo2->fromname)
		return -1;

	return strcasecmp(msginfo1->fromname, msginfo2->fromname);
}

static gint summary_cmp_by_subject(GtkCList *clist,
			       gconstpointer ptr1, gconstpointer ptr2)
{
	MsgInfo *msginfo1 = ((GtkCListRow *)ptr1)->data;
	MsgInfo *msginfo2 = ((GtkCListRow *)ptr2)->data;

	if (!msginfo1->subject)
		return (msginfo2->subject != NULL);
	if (!msginfo2->subject)
		return -1;

	return strcasecmp(msginfo1->subject, msginfo2->subject);
}

static gint summary_cmp_by_label(GtkCList *clist,
				 gconstpointer ptr1, gconstpointer ptr2)
{
	MsgInfo *msginfo1 = ((GtkCListRow *)ptr1)->data;
	MsgInfo *msginfo2 = ((GtkCListRow *)ptr2)->data;

	return MSG_GET_LABEL(msginfo1->flags) - MSG_GET_LABEL(msginfo2->flags);
}

static gint summary_cmp_by_score(GtkCList *clist,
				 gconstpointer ptr1, gconstpointer ptr2)
{
	MsgInfo *msginfo1 = ((GtkCListRow *)ptr1)->data;
	MsgInfo *msginfo2 = ((GtkCListRow *)ptr2)->data;
	int diff;

	/* if score are equal, sort by date */

	diff = msginfo1->threadscore - msginfo2->threadscore;
	if (diff != 0)
		return diff;
	else
		return summary_cmp_by_date(clist, ptr1, ptr2);
}

static void summary_ignore_thread_func(GtkCTree *ctree, GtkCTreeNode *row, gpointer data)
{
	SummaryView *summaryview = (SummaryView *) data;
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	if (MSG_IS_NEW(msginfo->flags))
		summaryview->newmsgs--;
	if (MSG_IS_UNREAD(msginfo->flags))
		summaryview->unread--;
	MSG_SET_PERM_FLAGS(msginfo->flags, MSG_IGNORE_THREAD);

	CHANGE_FLAGS(msginfo);

	summary_set_row_marks(summaryview, row);
	debug_print(_("Message %d is marked as ignore thread\n"),
	    msginfo->msgnum);
}

static void summary_ignore_thread(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	for (cur = GTK_CLIST(ctree)->selection; cur != NULL; cur = cur->next) {
		gtk_ctree_pre_recursive(ctree, GTK_CTREE_NODE(cur->data), GTK_CTREE_FUNC(summary_ignore_thread_func), summaryview);
	}

	summary_status_show(summaryview);
}

static void summary_unignore_thread_func(GtkCTree *ctree, GtkCTreeNode *row, gpointer data)
{
	SummaryView *summaryview = (SummaryView *) data;
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	if (MSG_IS_NEW(msginfo->flags))
		summaryview->newmsgs++;
	if (MSG_IS_UNREAD(msginfo->flags))
		summaryview->unread++;
	MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_IGNORE_THREAD);

	CHANGE_FLAGS(msginfo);
		
	summary_set_row_marks(summaryview, row);
	debug_print(_("Message %d is marked as unignore thread\n"),
	    msginfo->msgnum);
}

static void summary_unignore_thread(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	for (cur = GTK_CLIST(ctree)->selection; cur != NULL; cur = cur->next) {
		gtk_ctree_pre_recursive(ctree, GTK_CTREE_NODE(cur->data), GTK_CTREE_FUNC(summary_unignore_thread_func), summaryview);
	}

	summary_status_show(summaryview);
}

