/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
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

#include "main.h"
#include "menu.h"
#include "mbox.h"
#include "mainwindow.h"
#include "folderview.h"
#include "summaryview.h"
#include "messageview.h"
#include "foldersel.h"
#include "procmsg.h"
#include "procheader.h"
#include "sourcewindow.h"
#include "prefs_common.h"
#include "prefs_summary_column.h"
#include "prefs_filtering.h"
#include "account.h"
#include "compose.h"
#include "utils.h"
#include "gtkutils.h"
#include "stock_pixmap.h"
#include "filesel.h"
#include "alertpanel.h"
#include "inputdialog.h"
#include "statusbar.h"
#include "folder.h"
#include "colorlabel.h"
#include "inc.h"
#include "imap.h"
#include "addressbook.h"
#include "addr_compl.h"
#include "folder_item_prefs.h"
#include "filtering.h"
#include "string_match.h"
#include "toolbar.h"
#include "news.h"
#include "hooks.h"
#include "description_window.h"
#include "folderutils.h"
#include "quicksearch.h"
#include "partial_download.h"
#include "timing.h"
#include "gedit-print.h"

#define SUMMARY_COL_MARK_WIDTH		10
#define SUMMARY_COL_STATUS_WIDTH	13
#define SUMMARY_COL_LOCKED_WIDTH	13
#define SUMMARY_COL_MIME_WIDTH		11


static GtkStyle *bold_style;
static GtkStyle *bold_marked_style;
static GtkStyle *bold_deleted_style;
static GtkStyle *small_style;
static GtkStyle *small_marked_style;
static GtkStyle *small_deleted_style;

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
static GdkPixmap *lockedxpm;
static GdkBitmap *lockedxpmmask;

static GdkPixmap *clipxpm;
static GdkBitmap *clipxpmmask;
static GdkPixmap *keyxpm;
static GdkBitmap *keyxpmmask;
static GdkPixmap *clipkeyxpm;
static GdkBitmap *clipkeyxpmmask;
static GdkPixmap *gpgsignedxpm;
static GdkBitmap *gpgsignedxpmmask;
static GdkPixmap *clipgpgsignedxpm;
static GdkBitmap *clipgpgsignedxpmmask;

static void summary_free_msginfo_func	(GtkCTree		*ctree,
					 GtkCTreeNode		*node,
					 gpointer		 data);
static void summary_set_marks_func	(GtkCTree		*ctree,
					 GtkCTreeNode		*node,
					 gpointer		 data);

static void summary_set_menu_sensitive	(SummaryView		*summaryview);

static void summary_set_hide_read_msgs_menu (SummaryView *summaryview,
					     guint action);

static guint summary_get_msgnum		(SummaryView		*summaryview,
					 GtkCTreeNode		*node);

static GtkCTreeNode *summary_find_prev_msg
					(SummaryView		*summaryview,
					 GtkCTreeNode		*current_node);
static GtkCTreeNode *summary_find_next_msg
					(SummaryView		*summaryview,
					 GtkCTreeNode		*current_node);

static GtkCTreeNode *summary_find_prev_flagged_msg
					(SummaryView	*summaryview,
					 GtkCTreeNode	*current_node,
					 MsgPermFlags	 flags,
					 gboolean	 start_from_prev);
static GtkCTreeNode *summary_find_next_flagged_msg
					(SummaryView	*summaryview,
					 GtkCTreeNode	*current_node,
					 MsgPermFlags	 flags,
					 gboolean	 start_from_next);

static GtkCTreeNode *summary_find_msg_by_msgnum
					(SummaryView		*summaryview,
					 guint			 msgnum);

static void summary_update_status	(SummaryView		*summaryview);

/* display functions */
static void summary_status_show		(SummaryView		*summaryview);
static void summary_set_column_titles	(SummaryView		*summaryview);
static void summary_set_ctree_from_list	(SummaryView		*summaryview,
					 GSList			*mlist);
static void summary_set_header		(SummaryView		*summaryview,
					 gchar			*text[],
					 MsgInfo		*msginfo);
static void summary_display_msg		(SummaryView		*summaryview,
					 GtkCTreeNode		*row);
static void summary_display_msg_full	(SummaryView		*summaryview,
					 GtkCTreeNode		*row,
					 gboolean		 new_window,
					 gboolean		 all_headers);
static void summary_set_row_marks	(SummaryView		*summaryview,
					 GtkCTreeNode		*row);

/* message handling */
static void summary_mark_row		(SummaryView		*summaryview,
					 GtkCTreeNode		*row);
static void summary_lock_row		(SummaryView		*summaryview,
					 GtkCTreeNode		*row);
static void summary_unlock_row		(SummaryView		*summaryview,
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

static gint summary_execute_move	(SummaryView		*summaryview);
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

static void summary_thread_init		(SummaryView		*summaryview);
static void summary_ignore_thread	(SummaryView		*summaryview);
static void summary_unignore_thread     (SummaryView            *summaryview);

static void summary_unthread_for_exec		(SummaryView	*summaryview);
static void summary_unthread_for_exec_func	(GtkCTree	*ctree,
						 GtkCTreeNode	*node,
						 gpointer	 data);

void summary_simplify_subject(SummaryView *summaryview, gchar * rexp,
			      GSList * mlist);

#if 0
void summary_processing(SummaryView *summaryview, GSList * mlist);
#endif
static void summary_filter_func		(GtkCTree		*ctree,
					 GtkCTreeNode		*node,
					 gpointer		 data);

static void summary_colorlabel_menu_item_activate_cb
					  (GtkWidget	*widget,
					   gpointer	 data);
static void summary_colorlabel_menu_item_activate_item_cb
					  (GtkMenuItem	*label_menu_item,
					   gpointer	 data);
static void summary_colorlabel_menu_create(SummaryView	*summaryview);

static GtkWidget *summary_ctree_create	(SummaryView	*summaryview);

/* callback functions */
static gint summary_toggle_pressed	(GtkWidget		*eventbox,
					 GdkEventButton		*event,
					 SummaryView		*summaryview);
static gboolean summary_button_pressed	(GtkWidget		*ctree,
					 GdkEventButton		*event,
					 SummaryView		*summaryview);
static gboolean summary_button_released	(GtkWidget		*ctree,
					 GdkEventButton		*event,
					 SummaryView		*summaryview);
static gboolean summary_key_pressed	(GtkWidget		*ctree,
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

static void summary_add_address_cb	(SummaryView		*summaryview,
					 guint			 action,
					 GtkWidget		*widget);
static void summary_create_filter_cb	(SummaryView		*summaryview,
					 guint			 action,
					 GtkWidget		*widget);
static void summary_create_processing_cb(SummaryView		*summaryview,
					 guint			 action,
					 GtkWidget		*widget);

static void summary_mark_clicked	(GtkWidget		*button,
					 SummaryView		*summaryview);
static void summary_status_clicked	(GtkWidget		*button,
					 SummaryView		*summaryview);
static void summary_mime_clicked	(GtkWidget		*button,
					 SummaryView		*summaryview);
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
static void summary_to_clicked		(GtkWidget		*button,
					 SummaryView		*summaryview);
static void summary_subject_clicked	(GtkWidget		*button,
					 SummaryView		*summaryview);
static void summary_score_clicked	(GtkWidget		*button,
					 SummaryView		*summaryview);
static void summary_locked_clicked	(GtkWidget		*button,
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

static gint summary_cmp_by_mark		(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);
static gint summary_cmp_by_status	(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);
static gint summary_cmp_by_mime		(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);
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
static gint summary_cmp_by_simplified_subject
					(GtkCList 		*clist, 
					 gconstpointer 		 ptr1, 
					 gconstpointer 		 ptr2);
static gint summary_cmp_by_score	(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);
static gint summary_cmp_by_label	(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);
static gint summary_cmp_by_to		(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);
static gint summary_cmp_by_subject	(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);
static gint summary_cmp_by_locked	(GtkCList 		*clist,
				         gconstpointer 		 ptr1, 
					 gconstpointer 		 ptr2);

static void news_flag_crosspost		(MsgInfo *msginfo);

static void quicksearch_execute_cb	(QuickSearch    *quicksearch,
					 gpointer	 data);
static void tog_searchbar_cb		(GtkWidget	*w,
					 gpointer	 data);

static void summary_find_answers	(SummaryView 	*summaryview, 
					 MsgInfo	*msg);

static gboolean summary_update_msg	(gpointer source, gpointer data);

GtkTargetEntry summary_drag_types[1] =
{
	{"text/plain", GTK_TARGET_SAME_APP, TARGET_DUMMY}
};

static GtkItemFactoryEntry summary_popup_entries[] =
{
	{N_("/_Reply"),			"<control>R", summary_reply_cb,	COMPOSE_REPLY, NULL},
	{N_("/Repl_y to"),		NULL, NULL,		0, "<Branch>"},
	{N_("/Repl_y to/_all"),		"<shift><control>R", summary_reply_cb,	COMPOSE_REPLY_TO_ALL, NULL},
	{N_("/Repl_y to/_sender"),	NULL, summary_reply_cb,	COMPOSE_REPLY_TO_SENDER, NULL},
	{N_("/Repl_y to/mailing _list"),
					"<control>L", summary_reply_cb,	COMPOSE_REPLY_TO_LIST, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/_Forward"),		"<control><alt>F", summary_reply_cb, COMPOSE_FORWARD_INLINE, NULL},
	{N_("/For_ward as attachment"),	NULL, summary_reply_cb, COMPOSE_FORWARD_AS_ATTACH, NULL},
	{N_("/Redirect"),	        NULL, summary_reply_cb, COMPOSE_REDIRECT, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/M_ove..."),		"<control>O", summary_move_to,	0, NULL},
	{N_("/_Copy..."),		"<shift><control>O", summary_copy_to,	0, NULL},
	{N_("/Move to _trash"),		"<control>D", summary_delete_trash,	0, NULL},
	{N_("/_Delete..."),		NULL, summary_delete, 0, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/_Mark"),			NULL, NULL,		0, "<Branch>"},
	{N_("/_Mark/_Mark"),		NULL, summary_mark,	0, NULL},
	{N_("/_Mark/_Unmark"),		NULL, summary_unmark,	0, NULL},
	{N_("/_Mark/---"),		NULL, NULL,		0, "<Separator>"},
	{N_("/_Mark/Mark as unr_ead"),	NULL, summary_mark_as_unread, 0, NULL},
	{N_("/_Mark/Mark as rea_d"),	NULL, summary_mark_as_read, 0, NULL},
	{N_("/_Mark/Mark all read"),    NULL, summary_mark_all_read, 0, NULL},
	{N_("/_Mark/Ignore thread"),	NULL, summary_ignore_thread, 0, NULL},
	{N_("/_Mark/Unignore thread"),	NULL, summary_unignore_thread, 0, NULL},
	{N_("/_Mark/Lock"),		NULL, summary_msgs_lock, 0, NULL},
	{N_("/_Mark/Unlock"),		NULL, summary_msgs_unlock, 0, NULL},
	{N_("/Color la_bel"),		NULL, NULL, 		0, NULL},

	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/Add sender to address boo_k"),
					NULL, summary_add_address_cb, 0, NULL},
	{N_("/Create f_ilter rule"),	NULL, NULL,		0, "<Branch>"},
	{N_("/Create f_ilter rule/_Automatically"),
					NULL, summary_create_filter_cb, FILTER_BY_AUTO, NULL},
	{N_("/Create f_ilter rule/by _From"),
					NULL, summary_create_filter_cb, FILTER_BY_FROM, NULL},
	{N_("/Create f_ilter rule/by _To"),
					NULL, summary_create_filter_cb, FILTER_BY_TO, NULL},
	{N_("/Create f_ilter rule/by _Subject"),
					NULL, summary_create_filter_cb, FILTER_BY_SUBJECT, NULL},
	{N_("/Create processing rule"),	NULL, NULL,		0, "<Branch>"},
	{N_("/Create processing rule/_Automatically"),
					NULL, summary_create_processing_cb, FILTER_BY_AUTO, NULL},
	{N_("/Create processing rule/by _From"),
					NULL, summary_create_processing_cb, FILTER_BY_FROM, NULL},
	{N_("/Create processing rule/by _To"),
					NULL, summary_create_processing_cb, FILTER_BY_TO, NULL},
	{N_("/Create processing rule/by _Subject"),
					NULL, summary_create_processing_cb, FILTER_BY_SUBJECT, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/_View"),			NULL, NULL,		0, "<Branch>"},
	{N_("/_View/Open in new _window"),
					"<control><alt>N", summary_open_msg,	0, NULL},
	{N_("/_View/_Source"),		"<control>U", summary_view_source, 0, NULL},
	{N_("/_View/All _header"),	"<control>H", summary_show_all_header_cb, 0, "<ToggleItem>"},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/_Save as..."),		"<control>S", summary_save_as,   0, NULL},
	{N_("/_Print..."),		"<control>P", summary_print,   0, NULL},
};  /* see also list in menu_connect_identical_items() in menu.c if this changes */

static const gchar *const col_label[N_SUMMARY_COLS] = {
	N_("M"),	/* S_COL_MARK    */
	N_("S"),	/* S_COL_STATUS  */
	"",		/* S_COL_MIME    */
	N_("Subject"),	/* S_COL_SUBJECT */
	N_("From"),	/* S_COL_FROM    */
	N_("To"),	/* S_COL_TO      */
	N_("Date"),	/* S_COL_DATE    */
	N_("Size"),	/* S_COL_SIZE    */
	N_("No."),	/* S_COL_NUMBER  */
	N_("Score"),	/* S_COL_SCORE   */
	N_("L")		/* S_COL_LOCKED	 */
};

#define START_LONG_OPERATION(summaryview) {			\
	summary_lock(summaryview);				\
	main_window_cursor_wait(summaryview->mainwin);		\
	gtk_clist_freeze(GTK_CLIST(summaryview->ctree));	\
	folder_item_update_freeze();				\
	inc_lock();						\
}
#define END_LONG_OPERATION(summaryview) {			\
	inc_unlock();						\
	folder_item_update_thaw();				\
	gtk_clist_thaw(GTK_CLIST(summaryview->ctree));		\
	main_window_cursor_normal(summaryview->mainwin);	\
	summary_unlock(summaryview);				\
}

SummaryView *summary_create(void)
{
	SummaryView *summaryview;
	GtkWidget *vbox;
	GtkWidget *scrolledwin;
	GtkWidget *ctree;
	GtkWidget *hbox;
	GtkWidget *hbox_l;
	GtkWidget *statlabel_folder;
	GtkWidget *statlabel_select;
	GtkWidget *statlabel_msgs;
	GtkWidget *hbox_spc;
	GtkWidget *toggle_eventbox;
	GtkWidget *toggle_arrow;
	GtkWidget *popupmenu;
	GtkWidget *toggle_search;
	GtkTooltips *search_tip;
	GtkItemFactory *popupfactory;
	gint n_entries;
	QuickSearch *quicksearch;

	debug_print("Creating summary view...\n");
	summaryview = g_new0(SummaryView, 1);

#define SUMMARY_VBOX_SPACING 3
	vbox = gtk_vbox_new(FALSE, SUMMARY_VBOX_SPACING);
	
	/* create status label */
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox);

	search_tip = gtk_tooltips_new();
	toggle_search = gtk_toggle_button_new();
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(toggle_search),
				     prefs_common.show_searchbar);
	gtk_widget_show(toggle_search);

	gtk_tooltips_set_tip(GTK_TOOLTIPS(search_tip),
			     toggle_search,
			     _("Toggle quick-search bar"), NULL);
	
	gtk_box_pack_start(GTK_BOX(hbox), toggle_search, FALSE, FALSE, 2);	

	hbox_l = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox_l);
	gtk_box_pack_start(GTK_BOX(hbox), hbox_l, TRUE, TRUE, 0);
 
	statlabel_folder = gtk_label_new("");
	gtk_widget_show(statlabel_folder);
	gtk_box_pack_start(GTK_BOX(hbox_l), statlabel_folder, FALSE, FALSE, 2);
	statlabel_select = gtk_label_new("");
	gtk_widget_show(statlabel_select);
	gtk_box_pack_start(GTK_BOX(hbox_l), statlabel_select, FALSE, FALSE, 12);
 
	/* toggle view button */
	toggle_eventbox = gtk_event_box_new();
	gtk_widget_show(toggle_eventbox);
	gtk_box_pack_end(GTK_BOX(hbox), toggle_eventbox, FALSE, FALSE, 4);
	toggle_arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_widget_show(toggle_arrow);
	gtk_container_add(GTK_CONTAINER(toggle_eventbox), toggle_arrow);
	g_signal_connect(G_OBJECT(toggle_eventbox), "button_press_event",
			 G_CALLBACK(summary_toggle_pressed),
			 summaryview);
	
	
	statlabel_msgs = gtk_label_new("");
	gtk_widget_show(statlabel_msgs);
	gtk_box_pack_end(GTK_BOX(hbox), statlabel_msgs, FALSE, FALSE, 4);

	hbox_spc = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox_spc);
	gtk_box_pack_end(GTK_BOX(hbox), hbox_spc, FALSE, FALSE, 6);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwin);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_box_pack_start(GTK_BOX(vbox), scrolledwin, TRUE, TRUE, 0);
	gtk_widget_set_size_request(vbox,
			     prefs_common.summaryview_width,
			     prefs_common.summaryview_height);

	ctree = summary_ctree_create(summaryview);
	gtk_widget_show(ctree);

	gtk_scrolled_window_set_hadjustment(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_CLIST(ctree)->hadjustment);
	gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_CLIST(ctree)->vadjustment);
	gtk_container_add(GTK_CONTAINER(scrolledwin), ctree);

	/* status label */
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	/* quick search */
	quicksearch = quicksearch_new();
	gtk_box_pack_start(GTK_BOX(vbox), quicksearch_get_widget(quicksearch), FALSE, FALSE, 0);

	quicksearch_set_execute_callback(quicksearch, quicksearch_execute_cb, summaryview);

  	g_signal_connect (G_OBJECT(toggle_search), "toggled",
			  G_CALLBACK(tog_searchbar_cb), summaryview);

	/* create popup menu */
	n_entries = sizeof(summary_popup_entries) /
		sizeof(summary_popup_entries[0]);
	popupmenu = menu_create_items(summary_popup_entries, n_entries,
				      "<SummaryView>", &popupfactory,
				      summaryview);

	summaryview->vbox = vbox;
	summaryview->scrolledwin = scrolledwin;
	summaryview->ctree = ctree;
	summaryview->hbox = hbox;
	summaryview->hbox_l = hbox_l;
	summaryview->statlabel_folder = statlabel_folder;
	summaryview->statlabel_select = statlabel_select;
	summaryview->statlabel_msgs = statlabel_msgs;
	summaryview->toggle_eventbox = toggle_eventbox;
	summaryview->toggle_arrow = toggle_arrow;
	summaryview->toggle_search = toggle_search;
	summaryview->popupmenu = popupmenu;
	summaryview->popupfactory = popupfactory;
	summaryview->lock_count = 0;
	summaryview->msginfo_update_callback_id =
		hooks_register_hook(MSGINFO_UPDATE_HOOKLIST, summary_update_msg, (gpointer) summaryview);

	summaryview->target_list = gtk_target_list_new(summary_drag_types, 1);

	summaryview->quicksearch = quicksearch;

	/* CLAWS: need this to get the SummaryView * from
	 * the CList */
	g_object_set_data(G_OBJECT(ctree), "summaryview", (gpointer)summaryview); 

	gtk_widget_show_all(vbox);

	gtk_widget_show(vbox);

	if (prefs_common.show_searchbar)
		quicksearch_show(quicksearch);
	else
		quicksearch_hide(quicksearch);
	
	return summaryview;
}

static void summary_set_fonts(SummaryView *summaryview)
{
	PangoFontDescription *font_desc;
	gint size;

	font_desc = pango_font_description_from_string(NORMAL_FONT);
	gtk_widget_modify_font(summaryview->ctree, font_desc);
	pango_font_description_free(font_desc);

	if (!bold_style) {
		bold_style = gtk_style_copy
			(gtk_widget_get_style(summaryview->ctree));
		font_desc = pango_font_description_from_string(NORMAL_FONT);
		if (font_desc) {
			pango_font_description_free(bold_style->font_desc);
			bold_style->font_desc = font_desc;
		}
		
		pango_font_description_set_weight
				(bold_style->font_desc, PANGO_WEIGHT_BOLD);
		bold_marked_style = gtk_style_copy(bold_style);
		bold_marked_style->fg[GTK_STATE_NORMAL] =
			summaryview->color_marked;
		bold_deleted_style = gtk_style_copy(bold_style);
		bold_deleted_style->fg[GTK_STATE_NORMAL] =
			summaryview->color_dim;
	}

	font_desc = pango_font_description_new();
	size = pango_font_description_get_size
		(summaryview->ctree->style->font_desc);
	pango_font_description_set_size(font_desc, size * PANGO_SCALE_SMALL);
	gtk_widget_modify_font(summaryview->statlabel_folder, font_desc);
	gtk_widget_modify_font(summaryview->statlabel_select, font_desc);
	gtk_widget_modify_font(summaryview->statlabel_msgs, font_desc);
	pango_font_description_free(font_desc);
}

void summary_init(SummaryView *summaryview)
{
	GtkWidget *pixmap;

	gtk_widget_realize(summaryview->ctree);
	stock_pixmap_gdk(summaryview->ctree, STOCK_PIXMAP_MARK,
			 &markxpm, &markxpmmask);
	stock_pixmap_gdk(summaryview->ctree, STOCK_PIXMAP_DELETED,
			 &deletedxpm, &deletedxpmmask);
	stock_pixmap_gdk(summaryview->ctree, STOCK_PIXMAP_NEW,
			 &newxpm, &newxpmmask);
	stock_pixmap_gdk(summaryview->ctree, STOCK_PIXMAP_UNREAD,
			 &unreadxpm, &unreadxpmmask);
	stock_pixmap_gdk(summaryview->ctree, STOCK_PIXMAP_REPLIED,
			 &repliedxpm, &repliedxpmmask);
	stock_pixmap_gdk(summaryview->ctree, STOCK_PIXMAP_FORWARDED,
			 &forwardedxpm, &forwardedxpmmask);
	stock_pixmap_gdk(summaryview->ctree, STOCK_PIXMAP_CLIP,
			 &clipxpm, &clipxpmmask);
	stock_pixmap_gdk(summaryview->ctree, STOCK_PIXMAP_LOCKED,
			 &lockedxpm, &lockedxpmmask);
	stock_pixmap_gdk(summaryview->ctree, STOCK_PIXMAP_IGNORETHREAD,
			 &ignorethreadxpm, &ignorethreadxpmmask);
	stock_pixmap_gdk(summaryview->ctree, STOCK_PIXMAP_CLIP_KEY,
			 &clipkeyxpm, &clipkeyxpmmask);
	stock_pixmap_gdk(summaryview->ctree, STOCK_PIXMAP_KEY,
			 &keyxpm, &keyxpmmask);
	stock_pixmap_gdk(summaryview->ctree, STOCK_PIXMAP_GPG_SIGNED,
			 &gpgsignedxpm, &gpgsignedxpmmask);
	stock_pixmap_gdk(summaryview->ctree, STOCK_PIXMAP_CLIP_GPG_SIGNED,
			 &clipgpgsignedxpm, &clipgpgsignedxpmmask);

	summary_set_fonts(summaryview);

	pixmap = stock_pixmap_widget(summaryview->hbox_l, STOCK_PIXMAP_DIR_OPEN);
	gtk_box_pack_start(GTK_BOX(summaryview->hbox_l), pixmap, FALSE, FALSE, 4);
	gtk_box_reorder_child(GTK_BOX(summaryview->hbox_l), pixmap, 0);
	gtk_widget_show(pixmap);
	summaryview->folder_pixmap = pixmap;

	pixmap = stock_pixmap_widget(summaryview->hbox, STOCK_PIXMAP_QUICKSEARCH);
	gtk_container_add (GTK_CONTAINER(summaryview->toggle_search), pixmap);
	gtk_widget_show(pixmap);
	summaryview->quick_search_pixmap = pixmap;

	/* Init summaryview prefs */
	summaryview->sort_key = SORT_BY_NONE;
	summaryview->sort_type = SORT_ASCENDING;

	/* Init summaryview extra data */
	summaryview->simplify_subject_preg = NULL;

	summary_clear_list(summaryview);
	summary_set_column_titles(summaryview);
	summary_colorlabel_menu_create(summaryview);
	summary_set_menu_sensitive(summaryview);

}

#if 0
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
#endif

#define CURRENTLY_DISPLAYED(m) \
( (m->msgnum == displayed_msgnum) \
  && (!g_ascii_strcasecmp(m->folder->name,item->name)) )

gboolean summary_show(SummaryView *summaryview, FolderItem *item)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	GSList *mlist = NULL;
	gchar *buf;
	gboolean is_refresh;
	guint selected_msgnum = 0;
	guint displayed_msgnum = 0;
	GSList *cur;
        GSList *not_killed;
	gboolean hidden_removed = FALSE;

	if (summary_is_locked(summaryview)) return FALSE;

	if (!summaryview->mainwin)
		return FALSE;

	inc_lock();
	summary_lock(summaryview);

	if (!prefs_common.summary_quicksearch_sticky
	 && !prefs_common.summary_quicksearch_recurse
	 && !quicksearch_is_running(summaryview->quicksearch)) {
		quicksearch_set(summaryview->quicksearch, prefs_common.summary_quicksearch_type, "");
	}

	/* STATUSBAR_POP(summaryview->mainwin); */

	is_refresh = (item == summaryview->folder_item) ? TRUE : FALSE;

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
		gboolean changed = FALSE;

		val = alertpanel(_("Process mark"),
				 _("Some marks are left. Process it?"),
				 GTK_STOCK_YES, GTK_STOCK_NO, GTK_STOCK_CANCEL);
		if (G_ALERTDEFAULT == val) {
			summary_unlock(summaryview);
			summary_execute(summaryview);
			summary_lock(summaryview);
			changed = TRUE;
		} else if (G_ALERTALTERNATE == val) {
			/* DO NOTHING */
		} else {
			summary_unlock(summaryview);
			inc_unlock();
			return FALSE;
		}
		if (changed || !quicksearch_is_active(summaryview->quicksearch))
			folder_update_op_count();
	}
	
	gtk_clist_freeze(GTK_CLIST(ctree));

	summary_clear_list(summaryview);

	buf = NULL;
	if (!item || !item->path || !folder_item_parent(item) || item->no_select) {
		g_free(buf);
		debug_print("empty folder\n\n");
		summary_set_hide_read_msgs_menu(summaryview, FALSE);
		summary_clear_all(summaryview);
		summaryview->folder_item = item;
		gtk_clist_thaw(GTK_CLIST(ctree));
		summary_unlock(summaryview);
		inc_unlock();
		if (item && quicksearch_is_running(summaryview->quicksearch)) {
			main_window_cursor_wait(summaryview->mainwin);
			quicksearch_reset_cur_folder_item(summaryview->quicksearch);
			if (quicksearch_is_active(summaryview->quicksearch))
				quicksearch_search_subfolders(summaryview->quicksearch, 
					      summaryview->folderview,
					      summaryview->folder_item);
			main_window_cursor_normal(summaryview->mainwin);
		}			
		return TRUE;
	}
	g_free(buf);

	if (!is_refresh)
		messageview_clear(summaryview->messageview);

	summaryview->folder_item = item;
	item->opened = TRUE;

	buf = g_strdup_printf(_("Scanning folder (%s)..."), item->path);
	debug_print("%s\n", buf);
	STATUSBAR_PUSH(summaryview->mainwin, buf);
	g_free(buf);

	main_window_cursor_wait(summaryview->mainwin);

	mlist = folder_item_get_msg_list(item);

	if (summaryview->folder_item->hide_read_msgs &&
	    quicksearch_is_active(summaryview->quicksearch) == FALSE) {
		GSList *not_killed;
		
		summary_set_hide_read_msgs_menu(summaryview, TRUE);
		not_killed = NULL;
		for(cur = mlist ; cur != NULL && cur->data != NULL ; cur = g_slist_next(cur)) {
			MsgInfo * msginfo = (MsgInfo *) cur->data;
			
			if (!msginfo->hidden) {
				if (MSG_IS_UNREAD(msginfo->flags) &&
				    !MSG_IS_IGNORE_THREAD(msginfo->flags))
					not_killed = g_slist_prepend(not_killed, msginfo);
				else if (MSG_IS_MARKED(msginfo->flags) ||
					 MSG_IS_LOCKED(msginfo->flags))
					not_killed = g_slist_prepend(not_killed, msginfo);
				else if (is_refresh &&
					(msginfo->msgnum == selected_msgnum ||
					 msginfo->msgnum == displayed_msgnum))
					not_killed = g_slist_prepend(not_killed, msginfo);
				else
					procmsg_msginfo_free(msginfo);
			 } else
			 	procmsg_msginfo_free(msginfo);
		}
		hidden_removed = TRUE;
		g_slist_free(mlist);
		mlist = not_killed;
	} else {
		summary_set_hide_read_msgs_menu(summaryview, FALSE);
	}

	if (quicksearch_is_active(summaryview->quicksearch)) {
		GSList *not_killed;
		
		not_killed = NULL;
		for (cur = mlist ; cur != NULL && cur->data != NULL ; cur = g_slist_next(cur)) {
			MsgInfo * msginfo = (MsgInfo *) cur->data;

			if (!msginfo->hidden && quicksearch_match(summaryview->quicksearch, msginfo))
				not_killed = g_slist_prepend(not_killed, msginfo);
			else
				procmsg_msginfo_free(msginfo);
		}
		hidden_removed = TRUE;
		if (quicksearch_is_running(summaryview->quicksearch)) {
			/* only scan subfolders when quicksearch changed,
			 * not when search is the same and folder changed */
			main_window_cursor_wait(summaryview->mainwin);
			quicksearch_reset_cur_folder_item(summaryview->quicksearch);
			quicksearch_search_subfolders(summaryview->quicksearch, 
					      summaryview->folderview,
					      summaryview->folder_item);
			main_window_cursor_normal(summaryview->mainwin);
		}
		
		g_slist_free(mlist);
		mlist = not_killed;
	}

	if (!hidden_removed) {
        	not_killed = NULL;
        	for(cur = mlist ; cur != NULL && cur->data != NULL ; cur = g_slist_next(cur)) {
                	MsgInfo * msginfo = (MsgInfo *) cur->data;

                	if (!msginfo->hidden)
                        	not_killed = g_slist_prepend(not_killed, msginfo);
                	else
                        	procmsg_msginfo_free(msginfo);
        	}
		g_slist_free(mlist);
		mlist = not_killed;
	}

	STATUSBAR_POP(summaryview->mainwin);

	/* set ctree and hash table from the msginfo list, and
	   create the thread */
	summary_set_ctree_from_list(summaryview, mlist);

	g_slist_free(mlist);

	if (is_refresh) {
		summaryview->displayed =
			summary_find_msg_by_msgnum(summaryview,
						   displayed_msgnum);
		if (!summaryview->displayed)
			messageview_clear(summaryview->messageview);
		summary_unlock(summaryview);
		summary_select_by_msgnum(summaryview, selected_msgnum);
		summary_lock(summaryview);
		if (!summaryview->selected) {
			/* no selected message - select first unread
			   message, but do not display it */
			node = summary_find_next_flagged_msg(summaryview, NULL,
							     MSG_UNREAD, FALSE);
			if (node == NULL && GTK_CLIST(ctree)->row_list != NULL)
				node = gtk_ctree_node_nth
					(ctree,
					 item->sort_type == SORT_DESCENDING
					 ? 0 : GTK_CLIST(ctree)->rows - 1);
			summary_unlock(summaryview);
			summary_select_node(summaryview, node, FALSE, TRUE);
			summary_lock(summaryview);
		}
	} else {
 		switch (prefs_common.select_on_entry) {
 			case SELECTONENTRY_NEW:
				node = summary_find_next_flagged_msg(summaryview, NULL,
								     MSG_NEW, FALSE);
				if (node == NULL)
					node = summary_find_next_flagged_msg(summaryview, NULL,
								     MSG_UNREAD, FALSE);
				break;
 			case SELECTONENTRY_UNREAD:
				node = summary_find_next_flagged_msg(summaryview, NULL,
								     MSG_UNREAD, FALSE);
				if (node == NULL)
					node = summary_find_next_flagged_msg(summaryview, NULL,
								     MSG_NEW, FALSE);
				break;
 			default:
				node = NULL;
 		}

		if (node == NULL && GTK_CLIST(ctree)->row_list != NULL) {
			node = gtk_ctree_node_nth
				(ctree,
				 item->sort_type == SORT_DESCENDING
				 ? 0 : GTK_CLIST(ctree)->rows - 1);
		}
		summary_unlock(summaryview);
		summary_select_node(summaryview, node,
				    prefs_common.always_show_msg,
				    TRUE);
		summary_lock(summaryview);
	}

	summary_status_show(summaryview);
	summary_set_menu_sensitive(summaryview);
	toolbar_main_set_sensitive(summaryview->mainwin);
	
	gtk_clist_thaw(GTK_CLIST(ctree));

	debug_print("\n");
	STATUSBAR_PUSH(summaryview->mainwin, _("Done."));
	STATUSBAR_POP(summaryview->mainwin);
	main_window_cursor_normal(summaryview->mainwin);
	summary_unlock(summaryview);
	inc_unlock();

	return TRUE;
}

#undef CURRENTLY_DISPLAYED


void summary_clear_list(SummaryView *summaryview)
{
	GtkCList *clist = GTK_CLIST(summaryview->ctree);
	gint optimal_width;

	gtk_clist_freeze(clist);

	gtk_ctree_pre_recursive(GTK_CTREE(summaryview->ctree),
				NULL, summary_free_msginfo_func, NULL);

	if (summaryview->folder_item) {
		summaryview->folder_item->opened = FALSE;
		summaryview->folder_item = NULL;
	}

	summaryview->display_msg = FALSE;

	summaryview->selected = NULL;
	summaryview->displayed = NULL;
	summaryview->total_size = 0;
	summaryview->deleted = summaryview->moved = 0;
	summaryview->copied = 0;
	if (summaryview->msgid_table) {
		g_hash_table_destroy(summaryview->msgid_table);
		summaryview->msgid_table = NULL;
	}
	if (summaryview->subject_table) {
		g_hash_table_destroy(summaryview->subject_table);
		summaryview->subject_table = NULL;
	}
	summaryview->mlist = NULL;

	gtk_clist_clear(clist);
	if (summaryview->col_pos[S_COL_SUBJECT] == N_SUMMARY_COLS - 1) {
		optimal_width = gtk_clist_optimal_column_width
			(clist, summaryview->col_pos[S_COL_SUBJECT]);
		gtk_clist_set_column_width
			(clist, summaryview->col_pos[S_COL_SUBJECT],
			 optimal_width);
	}

	gtk_clist_thaw(clist);
}

void summary_clear_all(SummaryView *summaryview)
{
	messageview_clear(summaryview->messageview);
	summary_clear_list(summaryview);
	summary_set_menu_sensitive(summaryview);
	toolbar_main_set_sensitive(summaryview->mainwin);
	summary_status_show(summaryview);
}

void summary_lock(SummaryView *summaryview)
{
	summaryview->lock_count++;
}

void summary_unlock(SummaryView *summaryview)
{
	if (summaryview->lock_count)
		summaryview->lock_count--;
}

gboolean summary_is_locked(SummaryView *summaryview)
{
	return summaryview->lock_count > 0;
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

/*!
 *\return	MsgInfo	* Selected message if there's one selected;
 *		if multiple selected, or none, return NULL.
 */ 
MsgInfo *summary_get_selected_msg(SummaryView *summaryview)
{
	/* summaryview->selected may be valid when multiple 
	 * messages were selected */
	GList *sellist = GTK_CLIST(summaryview->ctree)->selection;

	if (sellist == NULL || sellist->next) 
		return NULL;
	
	return GTKUT_CTREE_NODE_GET_ROW_DATA(sellist->data);
}

GSList *summary_get_selected_msg_list(SummaryView *summaryview)
{
	GSList *mlist = NULL;
	GList *cur;
	MsgInfo *msginfo;

	for (cur = GTK_CLIST(summaryview->ctree)->selection; cur != NULL && cur->data != NULL;
	     cur = cur->next) {
		msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(cur->data);
		mlist = g_slist_prepend(mlist, msginfo);
	}

	mlist = g_slist_reverse(mlist);

	return mlist;
}

GSList *summary_get_msg_list(SummaryView *summaryview)
{
	GSList *mlist = NULL;
	GtkCTree *ctree;
	GtkCTreeNode *node;
	MsgInfo *msginfo;

	ctree = GTK_CTREE(summaryview->ctree);

	for (node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	     node != NULL; node = gtkut_ctree_node_next(ctree, node)) {
		msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);
		mlist = g_slist_prepend(mlist, msginfo);
	}

	mlist = g_slist_reverse(mlist);

	return mlist;
}

static void summary_set_menu_sensitive(SummaryView *summaryview)
{
	GtkItemFactory *ifactory = summaryview->popupfactory;
	SensitiveCond state;
	gboolean sensitive;
	GtkWidget *menuitem;
	gint i;

	static const struct {
		gchar *const entry;
		SensitiveCond cond;
	} entry[] = {
		{"/Reply"			, M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST},
		{"/Reply to"			, M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST},
		{"/Reply to/all"		, M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST},
		{"/Reply to/sender"             , M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST},
		{"/Reply to/mailing list"       , M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST},

		{"/Forward"			, M_HAVE_ACCOUNT|M_TARGET_EXIST},
		{"/Forward as attachment"	, M_HAVE_ACCOUNT|M_TARGET_EXIST},
        	{"/Redirect"			, M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST},

		{"/Move..."			, M_TARGET_EXIST|M_ALLOW_DELETE|M_NOT_NEWS},
		{"/Copy..."			, M_TARGET_EXIST|M_EXEC},
		{"/Move to trash"		, M_TARGET_EXIST|M_ALLOW_DELETE|M_NOT_NEWS},
		{"/Delete..."			, M_TARGET_EXIST|M_ALLOW_DELETE},

		{"/Mark"			, M_TARGET_EXIST},
		{"/Mark/Mark"   		, M_TARGET_EXIST},
		{"/Mark/Unmark"   		, M_TARGET_EXIST},
		{"/Mark/Mark as unread"   	, M_TARGET_EXIST},
		{"/Mark/Mark all read"   	, M_TARGET_EXIST},
		{"/Mark/Ignore thread"   	, M_TARGET_EXIST},
		{"/Mark/Lock"   		, M_TARGET_EXIST},
		{"/Mark/Unlock"   		, M_TARGET_EXIST},
		{"/Color label"			, M_TARGET_EXIST},

		{"/Add sender to address book"	, M_SINGLE_TARGET_EXIST},
		{"/Create filter rule"		, M_SINGLE_TARGET_EXIST|M_UNLOCKED},
		{"/Create processing rule"	, M_SINGLE_TARGET_EXIST|M_UNLOCKED},

		{"/View"			, M_SINGLE_TARGET_EXIST},
		{"/View/Open in new window"     , M_SINGLE_TARGET_EXIST},
		{"/View/Source"			, M_SINGLE_TARGET_EXIST},
		{"/View/All header"		, M_SINGLE_TARGET_EXIST},
		{"/Save as..."			, M_TARGET_EXIST},
		{"/Print..."			, M_TARGET_EXIST},
		{NULL, 0}
	};

	main_window_set_menu_sensitive(summaryview->mainwin);

	state = main_window_get_current_state(summaryview->mainwin);

	for (i = 0; entry[i].entry != NULL; i++) {
		sensitive = ((entry[i].cond & state) == entry[i].cond);
		menu_set_sensitive(ifactory, entry[i].entry, sensitive);
	}


	summary_lock(summaryview);
	menuitem = gtk_item_factory_get_widget(ifactory, "/View/All header");
	gtk_check_menu_item_set_active
		(GTK_CHECK_MENU_ITEM(menuitem),
		 summaryview->messageview->mimeview->textview->show_all_headers);
	summary_unlock(summaryview);
}

void summary_select_prev_unread(SummaryView *summaryview)
{
	GtkCTreeNode *node;
	gboolean skip_cur = FALSE;

	if (summaryview->displayed 
	&&  summaryview->selected == summaryview->displayed) {
		debug_print("skipping current\n");
		skip_cur = TRUE;
	}

	node = summary_find_prev_flagged_msg
		(summaryview, summaryview->selected, MSG_UNREAD, skip_cur);

	if (!node || node == summaryview->selected) {
		AlertValue val = 0;

 		switch (prefs_common.next_unread_msg_dialog) {
 			case NEXTUNREADMSGDIALOG_ALWAYS:
				val = alertpanel(_("No more unread messages"),
						 _("No unread message found. "
						   "Search from the end?"),
						 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
 				break;
 			case NEXTUNREADMSGDIALOG_ASSUME_YES:
 				val = G_ALERTDEFAULT;
 				break;
 			case NEXTUNREADMSGDIALOG_ASSUME_NO:
 				val = !G_ALERTDEFAULT;
 				break;
 			default:
 				debug_print(
 					_("Internal error: unexpected value for prefs_common.next_unread_msg_dialog\n"));
 		}
		if (val != G_ALERTDEFAULT) return;
		node = summary_find_prev_flagged_msg(summaryview, NULL,
						     MSG_UNREAD, FALSE);
	}

	if (!node)
		alertpanel_notice(_("No unread messages."));
	else
		summary_select_node(summaryview, node, TRUE, FALSE);
}

void summary_select_next_unread(SummaryView *summaryview)
{
	GtkCTreeNode *node = summaryview->selected;
	gboolean skip_cur = FALSE;
	
	if (summaryview->displayed 
	&&  summaryview->selected == summaryview->displayed) {
		debug_print("skipping cur (%p %p)\n",
			summaryview->displayed, summaryview->selected);
		skip_cur = TRUE;
	}


	node = summary_find_next_flagged_msg
		(summaryview, node, MSG_UNREAD, skip_cur);
	
	if (node)
		summary_select_node(summaryview, node, TRUE, FALSE);
	else {
		node = summary_find_next_flagged_msg
			(summaryview, NULL, MSG_UNREAD, FALSE);
		if (node == NULL || node == summaryview->selected) {
			AlertValue val = 0;

 			switch (prefs_common.next_unread_msg_dialog) {
 				case NEXTUNREADMSGDIALOG_ALWAYS:
					val = alertpanel(_("No more unread messages"),
							 _("No unread message found. "
							   "Go to next folder?"),
							 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
 					break;
 				case NEXTUNREADMSGDIALOG_ASSUME_YES:
 					val = G_ALERTDEFAULT;
 					break;
 				case NEXTUNREADMSGDIALOG_ASSUME_NO:
 					val = G_ALERTOTHER;
 					break;
 				default:
 					debug_print(
 						_("Internal error: unexpected value for prefs_common.next_unread_msg_dialog\n"));
 			}

			if (val == G_ALERTDEFAULT) {
				folderview_select_next_unread(summaryview->folderview);
				return;
			} 
			else
				return;
		} else
			summary_select_node(summaryview, node, TRUE, FALSE);

	}
}

void summary_select_prev_new(SummaryView *summaryview)
{
	GtkCTreeNode *node;
	gboolean skip_cur = FALSE;

	if (summaryview->displayed 
	&&  summaryview->selected == summaryview->displayed) {
		debug_print("skipping current\n");
		skip_cur = TRUE;
	}

	node = summary_find_prev_flagged_msg
		(summaryview, summaryview->selected, MSG_NEW, skip_cur);

	if (!node || node == summaryview->selected) {
		AlertValue val = 0;

 		switch (prefs_common.next_unread_msg_dialog) {
 			case NEXTUNREADMSGDIALOG_ALWAYS:
				val = alertpanel(_("No more new messages"),
						 _("No new message found. "
						   "Search from the end?"),
						 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
 				break;
 			case NEXTUNREADMSGDIALOG_ASSUME_YES:
 				val = G_ALERTDEFAULT;
 				break;
 			case NEXTUNREADMSGDIALOG_ASSUME_NO:
 				val = !G_ALERTDEFAULT;
 				break;
 			default:
 				debug_print(
 					_("Internal error: unexpected value for prefs_common.next_unread_msg_dialog\n"));
 		}
		if (val != G_ALERTDEFAULT) return;
		node = summary_find_prev_flagged_msg(summaryview, NULL,
						     MSG_NEW, FALSE);
	}

	if (!node)
		alertpanel_notice(_("No new messages."));
	else
		summary_select_node(summaryview, node, TRUE, FALSE);
}

void summary_select_next_new(SummaryView *summaryview)
{
	GtkCTreeNode *node = summaryview->selected;
	gboolean skip_cur = FALSE;
	
	if (summaryview->displayed 
	&&  summaryview->selected == summaryview->displayed) {
		debug_print("skipping cur (%p %p)\n",
			summaryview->displayed, summaryview->selected);
		skip_cur = TRUE;
	}


	node = summary_find_next_flagged_msg
		(summaryview, node, MSG_NEW, skip_cur);
	
	if (node)
		summary_select_node(summaryview, node, TRUE, FALSE);
	else {
		node = summary_find_next_flagged_msg
			(summaryview, NULL, MSG_NEW, FALSE);
		if (node == NULL || node == summaryview->selected) {
			AlertValue val = 0;

 			switch (prefs_common.next_unread_msg_dialog) {
 				case NEXTUNREADMSGDIALOG_ALWAYS:
					val = alertpanel(_("No more new messages"),
							 _("No new message found. "
							   "Go to next folder?"),
							 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
 					break;
 				case NEXTUNREADMSGDIALOG_ASSUME_YES:
 					val = G_ALERTDEFAULT;
 					break;
 				case NEXTUNREADMSGDIALOG_ASSUME_NO:
 					val = G_ALERTOTHER;
 					break;
 				default:
 					debug_print(
 						_("Internal error: unexpected value for prefs_common.next_unread_msg_dialog\n"));
 			}

			if (val == G_ALERTDEFAULT) {
				folderview_select_next_new(summaryview->folderview);
				return;
			} 
			else
				return;
		} else
			summary_select_node(summaryview, node, TRUE, FALSE);

	}
}

void summary_select_prev_marked(SummaryView *summaryview)
{
	GtkCTreeNode *node;

	node = summary_find_prev_flagged_msg
		(summaryview, summaryview->selected, MSG_MARKED, TRUE);

	if (!node) {
		AlertValue val;

		val = alertpanel(_("No more marked messages"),
				 _("No marked message found. "
				   "Search from the end?"),
				 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
		if (val != G_ALERTDEFAULT) return;
		node = summary_find_prev_flagged_msg(summaryview, NULL,
						     MSG_MARKED, TRUE);
	}

	if (!node)
		alertpanel_notice(_("No marked messages."));
	else
		summary_select_node(summaryview, node, TRUE, FALSE);
}

void summary_select_next_marked(SummaryView *summaryview)
{
	GtkCTreeNode *node;

	node = summary_find_next_flagged_msg
		(summaryview, summaryview->selected, MSG_MARKED, TRUE);

	if (!node) {
		AlertValue val;

		val = alertpanel(_("No more marked messages"),
				 _("No marked message found. "
				   "Search from the beginning?"),
				 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
		if (val != G_ALERTDEFAULT) return;
		node = summary_find_next_flagged_msg(summaryview, NULL,
						     MSG_MARKED, TRUE);
	}

	if (!node)
		alertpanel_notice(_("No marked messages."));
	else
		summary_select_node(summaryview, node, TRUE, FALSE);
}

void summary_select_prev_labeled(SummaryView *summaryview)
{
	GtkCTreeNode *node;

	node = summary_find_prev_flagged_msg
		(summaryview, summaryview->selected, MSG_CLABEL_FLAG_MASK, TRUE);

	if (!node) {
		AlertValue val;

		val = alertpanel(_("No more labeled messages"),
				 _("No labeled message found. "
				   "Search from the end?"),
				 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
		if (val != G_ALERTDEFAULT) return;
		node = summary_find_prev_flagged_msg(summaryview, NULL,
						     MSG_CLABEL_FLAG_MASK, TRUE);
	}

	if (!node)
		alertpanel_notice(_("No labeled messages."));
	else
		summary_select_node(summaryview, node, TRUE, FALSE);
}

void summary_select_next_labeled(SummaryView *summaryview)
{
	GtkCTreeNode *node;

	node = summary_find_next_flagged_msg
		(summaryview, summaryview->selected, MSG_CLABEL_FLAG_MASK, TRUE);

	if (!node) {
		AlertValue val;

		val = alertpanel(_("No more labeled messages"),
				 _("No labeled message found. "
				   "Search from the beginning?"),
				 GTK_STOCK_YES, GTK_STOCK_NO, NULL);
		if (val != G_ALERTDEFAULT) return;
		node = summary_find_next_flagged_msg(summaryview, NULL,
						     MSG_CLABEL_FLAG_MASK, TRUE);
	}

	if (!node)
		alertpanel_notice(_("No labeled messages."));
	else
		summary_select_node(summaryview, node, TRUE, FALSE);
}

void summary_select_by_msgnum(SummaryView *summaryview, guint msgnum)
{
	GtkCTreeNode *node;

	node = summary_find_msg_by_msgnum(summaryview, msgnum);
	summary_select_node(summaryview, node, FALSE, TRUE);
}

/**
 * summary_select_node:
 * @summaryview: Summary view.
 * @node: Summary tree node.
 * @display_msg: TRUE to display the selected message.
 * @do_refresh: TRUE to refresh the widget.
 *
 * Select @node (bringing it into view by scrolling and expanding its
 * thread, if necessary) and unselect all others.  If @display_msg is
 * TRUE, display the corresponding message in the message view.
 * If @do_refresh is TRUE, the widget is refreshed.
 **/
void summary_select_node(SummaryView *summaryview, GtkCTreeNode *node,
			 gboolean display_msg, gboolean do_refresh)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);

	if (node) {
		gtkut_ctree_expand_parent_all(ctree, node);
		if (do_refresh) {
			GTK_EVENTS_FLUSH();
			gtk_widget_grab_focus(GTK_WIDGET(ctree));
			gtk_ctree_node_moveto(ctree, node, -1, 0.5, 0);
		}
		summary_unselect_all(summaryview);
		if (display_msg && summaryview->displayed == node)
			summaryview->displayed = NULL;
		summaryview->display_msg = display_msg;
		gtk_sctree_select(GTK_SCTREE(ctree), node);
		if (summaryview->selected == NULL)
			summaryview->selected = node;
	}
}

static guint summary_get_msgnum(SummaryView *summaryview, GtkCTreeNode *node)
{
	GtkCTree *ctree =NULL;
	MsgInfo *msginfo;

	if (!summaryview)
		return 0;
	ctree = GTK_CTREE(summaryview->ctree);
	if (!node)
		return 0;
	msginfo = gtk_ctree_node_get_row_data(ctree, node);
	return msginfo->msgnum;
}

static GtkCTreeNode *summary_find_prev_msg(SummaryView *summaryview,
					   GtkCTreeNode *current_node)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	MsgInfo *msginfo;

	if (current_node)
		node = current_node;
	else
		node = gtk_ctree_node_nth(ctree, GTK_CLIST(ctree)->rows - 1);

	for (; node != NULL; node = GTK_CTREE_NODE_PREV(node)) {
		msginfo = gtk_ctree_node_get_row_data(ctree, node);
		if (msginfo && !MSG_IS_DELETED(msginfo->flags)) break;
	}

	return node;
}

static GtkCTreeNode *summary_find_next_msg(SummaryView *summaryview,
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
		if (msginfo && !MSG_IS_DELETED(msginfo->flags)) break;
	}

	return node;
}

static GtkCTreeNode *summary_find_prev_flagged_msg(SummaryView *summaryview,
						   GtkCTreeNode *current_node,
						   MsgPermFlags flags,
						   gboolean start_from_prev)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	MsgInfo *msginfo;

	if (current_node) {
		if (start_from_prev)
			node = GTK_CTREE_NODE_PREV(current_node);
		else
			node = current_node;
	} else
		node = gtk_ctree_node_nth(ctree, GTK_CLIST(ctree)->rows - 1);

	for (; node != NULL; node = GTK_CTREE_NODE_PREV(node)) {
		msginfo = gtk_ctree_node_get_row_data(ctree, node);
		if (msginfo && (msginfo->flags.perm_flags & flags) != 0) break;
	}

	return node;
}

static GtkCTreeNode *summary_find_next_flagged_msg(SummaryView *summaryview,
						   GtkCTreeNode *current_node,
						   MsgPermFlags flags,
						   gboolean start_from_next)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	MsgInfo *msginfo;

	if (current_node) {
		if (start_from_next)
			node = gtkut_ctree_node_next(ctree, current_node);
		else
			node = current_node;
	} else
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	for (; node != NULL; node = gtkut_ctree_node_next(ctree, node)) {
		msginfo = gtk_ctree_node_get_row_data(ctree, node);
		/* Find msg with matching flags but ignore messages with
		   ignore flags, if searching for new or unread messages */
		if (!(((flags & (MSG_NEW | MSG_UNREAD)) != 0) && MSG_IS_IGNORE_THREAD(msginfo->flags)) && 
			(msginfo && (msginfo->flags.perm_flags & flags) != 0))
			break;
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
		if (msginfo && msginfo->msgnum == msgnum) break;
	}

	return node;
}

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

	debug_print("Attracting messages by subject...");
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

	debug_print("done.\n");
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

 	if (msginfo->folder && msginfo->folder->folder &&
	    msginfo->folder->folder->klass->type == F_NEWS)
 		news_flag_crosspost(msginfo);

	if (MSG_IS_DELETED(msginfo->flags))
		summaryview->deleted++;

	summaryview->total_size += msginfo->size;

	summary_set_row_marks(summaryview, node);
}

static void summary_update_status(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	MsgInfo *msginfo;

	summaryview->total_size =
	summaryview->deleted = summaryview->moved = summaryview->copied = 0;

	for (node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	     node != NULL; node = gtkut_ctree_node_next(ctree, node)) {
		msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);

		if (MSG_IS_DELETED(msginfo->flags))
			summaryview->deleted++;
		if (MSG_IS_MOVE(msginfo->flags))
			summaryview->moved++;
		if (MSG_IS_COPY(msginfo->flags))
			summaryview->copied++;
		summaryview->total_size += msginfo->size;
	}
}

static void summary_status_show(SummaryView *summaryview)
{
	gchar *str;
	gchar *del, *mv, *cp;
	gchar *sel;
	gchar *spc;
	gchar *itstr;
	GList *rowlist, *cur;
	guint n_selected = 0;
	off_t sel_size = 0;
	MsgInfo *msginfo;
	gchar *name;
	
	if (!summaryview->folder_item) {
		gtk_label_set_text(GTK_LABEL(summaryview->statlabel_folder), "");
		gtk_label_set_text(GTK_LABEL(summaryview->statlabel_select), "");
		gtk_label_set_text(GTK_LABEL(summaryview->statlabel_msgs),   "");
		return;
	}

	rowlist = GTK_CLIST(summaryview->ctree)->selection;
	for (cur = rowlist; cur != NULL && cur->data != NULL; cur = cur->next) {
		msginfo = gtk_ctree_node_get_row_data
			(GTK_CTREE(summaryview->ctree),
			 GTK_CTREE_NODE(cur->data));
		if (!msginfo)
			g_warning("summary_status_show(): msginfo == NULL\n");
		else {
			sel_size += msginfo->size;
			n_selected++;
		}
	}

	name = folder_item_get_name(summaryview->folder_item);
	gtk_label_set_text(GTK_LABEL(summaryview->statlabel_folder), name);
	g_free(name);

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

	if (n_selected) {
		sel = g_strdup_printf(" (%s)", to_human_readable(sel_size));
		if (n_selected == 1)
			itstr = g_strdup(_(" item selected"));
		else
			itstr = g_strdup(_(" items selected"));
	} else {
		sel = g_strdup("");
		itstr = g_strdup("");
	}
		
	str = g_strconcat(n_selected ? itos(n_selected) : "",
					itstr, sel, spc, del, mv, cp, NULL);
	gtk_label_set_text(GTK_LABEL(summaryview->statlabel_select), str);
	g_free(str);
	g_free(sel);
	g_free(del);
	g_free(mv);
	g_free(cp);
	g_free(itstr);

	str = g_strdup_printf(_("%d new, %d unread, %d total (%s)"),

				      summaryview->folder_item->new_msgs,
				      summaryview->folder_item->unread_msgs,
				      summaryview->folder_item->total_msgs,
				      to_human_readable(summaryview->total_size));
	gtk_label_set_text(GTK_LABEL(summaryview->statlabel_msgs), str);
	g_free(str);
}

static void summary_set_column_titles(SummaryView *summaryview)
{
	GtkCList *clist = GTK_CLIST(summaryview->ctree);
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *arrow;
	gint pos;
	const gchar *title;
	SummaryColumnType type;
	gboolean single_char;
	GtkJustification justify;

	static FolderSortKey sort_by[N_SUMMARY_COLS] = {
		SORT_BY_MARK,
		SORT_BY_STATUS,
		SORT_BY_MIME,
		SORT_BY_SUBJECT,
		SORT_BY_FROM,
		SORT_BY_TO,
		SORT_BY_DATE,
		SORT_BY_SIZE,
		SORT_BY_NUMBER,
		SORT_BY_SCORE,
		SORT_BY_LOCKED
	};

	for (pos = 0; pos < N_SUMMARY_COLS; pos++) {
		type = summaryview->col_state[pos].type;

		/* CLAWS: mime and unread are single char headers */
		single_char = (type == S_COL_MIME || type == S_COL_STATUS);
		justify = (type == S_COL_NUMBER || type == S_COL_SIZE)
			? GTK_JUSTIFY_RIGHT : GTK_JUSTIFY_LEFT;

		switch (type) {
		case S_COL_SUBJECT:
		case S_COL_FROM:
		case S_COL_TO:
		case S_COL_DATE:
		case S_COL_NUMBER:
			if (prefs_common.trans_hdr)
				title = gettext(col_label[type]);
			else
				title = col_label[type];
			break;
		/* CLAWS: dummies for mark and locked headers */	
		case S_COL_MARK:	
		case S_COL_LOCKED:
			title = "";
			break;
		default:
			title = gettext(col_label[type]);
		}

		if (type == S_COL_MIME) {
			label = gtk_image_new_from_pixmap(clipxpm, clipxpmmask);
			gtk_widget_show(label);
			gtk_clist_set_column_widget(clist, pos, label);
			continue;
		} else if (single_char) {
			gtk_clist_set_column_title(clist, pos, title);
			continue;
		}

		/* CLAWS: changed so that locked and mark headers
		 * show a pixmap instead of single character */
		hbox  = gtk_hbox_new(FALSE, 4);
		
		if (type == S_COL_LOCKED)
			label = gtk_pixmap_new(lockedxpm, lockedxpmmask);
		else if (type == S_COL_MARK) 
			label = gtk_pixmap_new(markxpm, markxpmmask);
		else 
			label = gtk_label_new(title);
		
		if (justify == GTK_JUSTIFY_RIGHT)
			gtk_box_pack_end(GTK_BOX(hbox), label,
					 FALSE, FALSE, 0);
		else
			gtk_box_pack_start(GTK_BOX(hbox), label,
					   FALSE, FALSE, 0);

		if (summaryview->sort_key == sort_by[type]) {
			arrow = gtk_arrow_new
				(summaryview->sort_type == SORT_ASCENDING
				 ? GTK_ARROW_DOWN : GTK_ARROW_UP,
				 GTK_SHADOW_IN);
			if (justify == GTK_JUSTIFY_RIGHT)
				gtk_box_pack_start(GTK_BOX(hbox), arrow,
						   FALSE, FALSE, 0);
			else
				gtk_box_pack_end(GTK_BOX(hbox), arrow,
						 FALSE, FALSE, 0);
		}

		gtk_widget_show_all(hbox);
		gtk_clist_set_column_widget(clist, pos, hbox);
	}
}

void summary_reflect_prefs(void)
{
	static gchar *last_font = NULL;
	gboolean update_font = TRUE;
	SummaryView *summaryview = NULL;

	if (!mainwindow_get_mainwindow())
		return;
	summaryview = mainwindow_get_mainwindow()->summaryview;

	if (last_font && !strcmp(last_font, NORMAL_FONT))
		update_font = FALSE;

	if (last_font)
		g_free(last_font);
	
	last_font = g_strdup(NORMAL_FONT);

	if (update_font) {	
		bold_style = bold_marked_style = bold_deleted_style = 
			small_style = small_marked_style = small_deleted_style = NULL;
		summary_set_fonts(summaryview);
	}

	summary_set_column_titles(summaryview);
	summary_show(summaryview, summaryview->folder_item);
}

void summary_sort(SummaryView *summaryview,
		  FolderSortKey sort_key, FolderSortType sort_type)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCList *clist = GTK_CLIST(summaryview->ctree);
	GtkCListCompareFunc cmp_func = NULL;
	g_signal_handlers_block_by_func(G_OBJECT(summaryview->ctree),
				       G_CALLBACK(summary_tree_expanded), summaryview);
	gtk_clist_freeze(GTK_CLIST(summaryview->ctree));

	switch (sort_key) {
	case SORT_BY_MARK:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_mark;
		break;
	case SORT_BY_STATUS:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_status;
		break;
	case SORT_BY_MIME:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_mime;
		break;
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
		if (summaryview->simplify_subject_preg)
			cmp_func = (GtkCListCompareFunc)summary_cmp_by_simplified_subject;
		else
			cmp_func = (GtkCListCompareFunc)summary_cmp_by_subject;
		break;
	case SORT_BY_SCORE:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_score;
		break;
	case SORT_BY_LABEL:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_label;
		break;
	case SORT_BY_TO:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_to;
		break;
	case SORT_BY_LOCKED:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_locked;
		break;
	case SORT_BY_NONE:
		break;
	default:
		goto unlock;
	}

	summaryview->sort_key = sort_key;
	summaryview->sort_type = sort_type;

	summary_set_column_titles(summaryview);
	summary_set_menu_sensitive(summaryview);

	/* allow fallback to don't sort */
	if (summaryview->sort_key == SORT_BY_NONE)
		goto unlock;

	if (cmp_func != NULL) {
		debug_print("Sorting summary...");
		STATUSBAR_PUSH(summaryview->mainwin, _("Sorting summary..."));

		main_window_cursor_wait(summaryview->mainwin);

                gtk_clist_freeze(clist);
		gtk_clist_set_compare_func(clist, cmp_func);

		gtk_clist_set_sort_type(clist, (GtkSortType)sort_type);

		gtk_sctree_sort_recursive(ctree, NULL);

		gtk_ctree_node_moveto(ctree, summaryview->selected, -1, 0.5, 0);

		main_window_cursor_normal(summaryview->mainwin);
                gtk_clist_thaw(clist);

		debug_print("done.\n");
		STATUSBAR_POP(summaryview->mainwin);
	}
unlock:
	gtk_clist_thaw(GTK_CLIST(summaryview->ctree));
	g_signal_handlers_unblock_by_func(G_OBJECT(summaryview->ctree),
				       G_CALLBACK(summary_tree_expanded), summaryview);
}

gboolean summary_insert_gnode_func(GtkCTree *ctree, guint depth, GNode *gnode,
				   GtkCTreeNode *cnode, gpointer data)
{
	SummaryView *summaryview = (SummaryView *)data;
	MsgInfo *msginfo = (MsgInfo *)gnode->data;
	gchar *text[N_SUMMARY_COLS];
	gint *col_pos = summaryview->col_pos;
	const gchar *msgid = msginfo->msgid;
	GHashTable *msgid_table = summaryview->msgid_table;

	summary_set_header(summaryview, text, msginfo);

	gtk_ctree_set_node_info(ctree, cnode, text[col_pos[S_COL_SUBJECT]], 2,
				NULL, NULL, NULL, NULL, FALSE,
				gnode->parent->parent ? TRUE : FALSE);
#define SET_TEXT(col) \
	gtk_ctree_node_set_text(ctree, cnode, col_pos[col], \
				text[col_pos[col]])

	SET_TEXT(S_COL_NUMBER);
	SET_TEXT(S_COL_SCORE);
	SET_TEXT(S_COL_SIZE);
	SET_TEXT(S_COL_DATE);
	SET_TEXT(S_COL_FROM);
	SET_TEXT(S_COL_TO);
	/* SET_TEXT(S_COL_SUBJECT);  already set by node info */

#undef SET_TEXT

	GTKUT_CTREE_NODE_SET_ROW_DATA(cnode, msginfo);
	summary_set_marks_func(ctree, cnode, summaryview);

	if (msgid && msgid[0] != '\0')
		g_hash_table_insert(msgid_table, (gchar *)msgid, cnode);

	return TRUE;
}

static void summary_set_ctree_from_list(SummaryView *summaryview,
					GSList *mlist)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;
	GtkCTreeNode *node = NULL;
	GHashTable *msgid_table;
	GHashTable *subject_table;
	GSList * cur;
	START_TIMING("summary_set_ctree_from_list");
	
	if (!mlist) return;

	debug_print("\tSetting summary from message data...");
	STATUSBAR_PUSH(summaryview->mainwin,
		       _("Setting summary from message data..."));
	gdk_flush();

	g_signal_handlers_block_by_func(G_OBJECT(ctree),
				       G_CALLBACK(summary_tree_expanded), summaryview);

	msgid_table = g_hash_table_new(g_str_hash, g_str_equal);
	summaryview->msgid_table = msgid_table;
	subject_table = g_hash_table_new(g_str_hash, g_str_equal);
	summaryview->subject_table = subject_table;

	if (prefs_common.use_addr_book)
		start_address_completion();
	
	if (summaryview->threaded) {
		GNode *root, *gnode;

		root = procmsg_get_thread_tree(mlist);

		for (gnode = root->children; gnode != NULL;
		     gnode = gnode->next) {
			node = gtk_ctree_insert_gnode
				(ctree, NULL, node, gnode,
				 summary_insert_gnode_func, summaryview);
		}

		g_node_destroy(root);
                
		summary_thread_init(summaryview);
	} else {
		gchar *text[N_SUMMARY_COLS];
		cur = mlist;
		for (; mlist != NULL; mlist = mlist->next) {
			msginfo = (MsgInfo *)mlist->data;

			summary_set_header(summaryview, text, msginfo);

			node = gtk_ctree_insert_node
				(ctree, NULL, node, text, 2,
				 NULL, NULL, NULL, NULL, FALSE, FALSE);
			GTKUT_CTREE_NODE_SET_ROW_DATA(node, msginfo);
			summary_set_marks_func(ctree, node, summaryview);

			if (msginfo->msgid && msginfo->msgid[0] != '\0')
				g_hash_table_insert(msgid_table,
						    msginfo->msgid, node);

			subject_table_insert(subject_table,
					     msginfo->subject,
					     node);
		}
		mlist = cur;
	}

	if (prefs_common.enable_hscrollbar &&
	    summaryview->col_pos[S_COL_SUBJECT] == N_SUMMARY_COLS - 1) {
		gint optimal_width;

		optimal_width = gtk_clist_optimal_column_width
			(GTK_CLIST(ctree), summaryview->col_pos[S_COL_SUBJECT]);
		gtk_clist_set_column_width(GTK_CLIST(ctree),
					   summaryview->col_pos[S_COL_SUBJECT],
					   optimal_width);
	}

	if (prefs_common.use_addr_book)
		end_address_completion();

	debug_print("done.\n");
	STATUSBAR_POP(summaryview->mainwin);
	if (debug_get_mode()) {
		debug_print("\tmsgid hash table size = %d\n",
			    g_hash_table_size(msgid_table));
		debug_print("\tsubject hash table size = %d\n",
			    g_hash_table_size(subject_table));
	}

	summary_sort(summaryview, summaryview->sort_key, summaryview->sort_type);

	node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	if (prefs_common.bold_unread) {
		while (node) {
			GtkCTreeNode *next = GTK_CTREE_NODE_NEXT(node);
			if (GTK_CTREE_ROW(node)->children)
				summary_set_row_marks(summaryview, node);
			node = next;
		}
	}

	g_signal_handlers_unblock_by_func(G_OBJECT(ctree),
				       G_CALLBACK(summary_tree_expanded), summaryview);
	END_TIMING();
}

static gchar *summary_complete_address(const gchar *addr)
{
	gint count;
	gchar *res, *tmp, *email_addr;
	
	if (addr == NULL)
		return NULL;

	Xstrdup_a(email_addr, addr, return NULL);
	extract_address(email_addr);
	g_return_val_if_fail(*email_addr, NULL);

	/*
	 * completion stuff must be already initialized
	 */
	res = NULL;
	if (1 < (count = complete_address(email_addr))) {
		tmp = get_complete_address(1);
		res = procheader_get_fromname(tmp);
		g_free(tmp);
	}

	return res;
}

static void summary_set_header(SummaryView *summaryview, gchar *text[],
			       MsgInfo *msginfo)
{
	static gchar date_modified[80];
	static gchar col_score[11];
	static gchar buf[BUFFSIZE];
	gint *col_pos = summaryview->col_pos;
	gchar *from_text = NULL, *to_text = NULL;
	gboolean should_swap = FALSE;

	text[col_pos[S_COL_FROM]]   = "";
	text[col_pos[S_COL_TO]]     = "";
	text[col_pos[S_COL_SUBJECT]]= "";
	text[col_pos[S_COL_MARK]]   = "";
	text[col_pos[S_COL_STATUS]] = "";
	text[col_pos[S_COL_MIME]]   = "";
	text[col_pos[S_COL_LOCKED]] = "";
	text[col_pos[S_COL_DATE]]   = "";
	text[col_pos[S_COL_NUMBER]] = itos(msginfo->msgnum);
	text[col_pos[S_COL_SIZE]]   = to_human_readable(msginfo->size);
	text[col_pos[S_COL_SCORE]]  = itos_buf(col_score, msginfo->score);

	if (msginfo->date_t) {
		procheader_date_get_localtime(date_modified,
					      sizeof(date_modified),
					      msginfo->date_t);
		text[col_pos[S_COL_DATE]] = date_modified;
	} else if (msginfo->date)
		text[col_pos[S_COL_DATE]] = msginfo->date;
	else
		text[col_pos[S_COL_DATE]] = _("(No Date)");

	if (prefs_common.swap_from && msginfo->from && msginfo->to) {
		gchar *addr = NULL;
		
		addr = g_strdup(msginfo->from);

		if (addr) {
			extract_address(addr);
			if (account_find_from_address(addr)) {
				should_swap = TRUE;
			}
			g_free(addr);
		}
	}

	if (!prefs_common.use_addr_book) {
		from_text = msginfo->fromname ? 
				msginfo->fromname :
				_("(No From)");
	} else {
		gchar *tmp = summary_complete_address(msginfo->from);
		from_text = tmp ? tmp : (msginfo->fromname ?
					 msginfo->fromname: 
					 	_("(No From)"));
	}
	
	to_text = msginfo->to ? msginfo->to : 
		   (msginfo->cc ? msginfo->cc :
		     (msginfo->newsgroups ? msginfo->newsgroups : _("(No Recipient)")
		     )
		   );

	if (!should_swap) {
		text[col_pos[S_COL_FROM]] = from_text;
		text[col_pos[S_COL_TO]] = to_text;
	} else {
		gchar *tmp = NULL;
		tmp = g_strconcat("-->", to_text, NULL);
		text[col_pos[S_COL_FROM]] = tmp;
		tmp = g_strconcat("<--", from_text, NULL);
		text[col_pos[S_COL_TO]] = tmp;
	}
	
	if (summaryview->simplify_subject_preg != NULL)
		text[col_pos[S_COL_SUBJECT]] = msginfo->subject ? 
			string_remove_match(buf, BUFFSIZE, msginfo->subject, 
					summaryview->simplify_subject_preg) : 
			_("(No Subject)");
	else 
		text[col_pos[S_COL_SUBJECT]] = msginfo->subject ? msginfo->subject :
			_("(No Subject)");
}

static void summary_display_msg(SummaryView *summaryview, GtkCTreeNode *row)
{
	summary_display_msg_full(summaryview, row, FALSE, FALSE);
}

static gboolean defer_change(gpointer data);
typedef struct _ChangeData {
	MsgInfo *info;
	gint op; /* 0, 1, 2 for unset, set, change */
	MsgPermFlags set_flags;
	MsgTmpFlags  set_tmp_flags;
	MsgPermFlags unset_flags;
	MsgTmpFlags  unset_tmp_flags;
} ChangeData;

static void summary_msginfo_unset_flags(MsgInfo *msginfo, MsgPermFlags flags, MsgTmpFlags tmp_flags)
{
	if (!msginfo->folder || !msginfo->folder->processing_pending) {
		debug_print("flags: doing unset now\n");
		procmsg_msginfo_unset_flags(msginfo, flags, tmp_flags);
	} else {
		ChangeData *unset_data = g_new0(ChangeData, 1);
		unset_data->info = msginfo;
		unset_data->op = 0;
		unset_data->unset_flags = flags;
		unset_data->unset_tmp_flags = tmp_flags;
		debug_print("flags: deferring unset\n");
		g_timeout_add(100, defer_change, unset_data);
	}
}

static void summary_msginfo_set_flags(MsgInfo *msginfo, MsgPermFlags flags, MsgTmpFlags tmp_flags)
{
	if (!msginfo->folder || !msginfo->folder->processing_pending) {
		debug_print("flags: doing set now\n");
		procmsg_msginfo_set_flags(msginfo, flags, tmp_flags);
	} else {
		ChangeData *set_data = g_new0(ChangeData, 1);
		set_data->info = msginfo;
		set_data->op = 1;
		set_data->set_flags = flags;
		set_data->set_tmp_flags = tmp_flags;
		debug_print("flags: deferring set\n");
		g_timeout_add(100, defer_change, set_data);
	}
}

static void summary_msginfo_change_flags(MsgInfo *msginfo, 
		MsgPermFlags add_flags, MsgTmpFlags add_tmp_flags,
		MsgPermFlags rem_flags, MsgTmpFlags rem_tmp_flags)
{
	if (!msginfo->folder || !msginfo->folder->processing_pending) {
		debug_print("flags: doing change now\n");
		procmsg_msginfo_change_flags(msginfo, add_flags, add_tmp_flags,
			rem_flags, rem_tmp_flags);
	} else {
		ChangeData *change_data = g_new0(ChangeData, 1);
		change_data->info = msginfo;
		change_data->op = 2;
		change_data->set_flags = add_flags;
		change_data->set_tmp_flags = add_tmp_flags;
		change_data->unset_flags = rem_flags;
		change_data->unset_tmp_flags = rem_tmp_flags;
		debug_print("flags: deferring change\n");
		g_timeout_add(100, defer_change, change_data);
	}
}

gboolean defer_change(gpointer data)
{
	ChangeData *chg = (ChangeData *)data;
	if (chg->info->folder && chg->info->folder->processing_pending) {
		debug_print("flags: trying later\n");
		return TRUE; /* try again */
	} else {
		debug_print("flags: finally doing it\n");
		switch(chg->op) {
		case 0:
			procmsg_msginfo_unset_flags(chg->info, chg->unset_flags, chg->unset_tmp_flags);
			break;
		case 1:
			procmsg_msginfo_set_flags(chg->info, chg->set_flags, chg->set_tmp_flags);
			break;
		case 2:
			procmsg_msginfo_change_flags(chg->info, chg->set_flags, chg->set_tmp_flags,
				chg->unset_flags, chg->unset_tmp_flags);
			break;
		default:
			g_warning("shouldn't happen\n");
		}
		g_free(chg);
	}
	return FALSE;
}

static void msginfo_mark_as_read (SummaryView *summaryview, MsgInfo *msginfo,
				      GtkCTreeNode *row)
{
	g_return_if_fail(summaryview != NULL);
	g_return_if_fail(msginfo != NULL);
	g_return_if_fail(row != NULL);

	if (MSG_IS_NEW(msginfo->flags) || MSG_IS_UNREAD(msginfo->flags)) {
		summary_msginfo_unset_flags
			(msginfo, MSG_NEW | MSG_UNREAD, 0);
		summary_set_row_marks(summaryview, row);
		gtk_clist_thaw(GTK_CLIST(summaryview->ctree));
		summary_status_show(summaryview);
	}
}

typedef struct  {
	MsgInfo *msginfo;
	SummaryView *summaryview;
	GtkCTreeNode *row;
} MarkAsReadData;

static int msginfo_mark_as_read_timeout(void *data)
{
	MarkAsReadData *mdata = (MarkAsReadData *)data;
	if (!mdata)
		return FALSE;
	
	if (mdata->msginfo == summary_get_selected_msg(mdata->summaryview))
		msginfo_mark_as_read(mdata->summaryview, mdata->msginfo,
				     mdata->row); 

	g_free(mdata);

	return FALSE;	
}

static void summary_display_msg_full(SummaryView *summaryview,
				     GtkCTreeNode *row,
				     gboolean new_window, gboolean all_headers)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;
	gint val;

	if (!new_window) {
		if (summaryview->displayed == row)
			return;
		else
			summaryview->messageview->filtered = FALSE;
	}			
	
	g_return_if_fail(row != NULL);

	if (summary_is_locked(summaryview)) return;
	summary_lock(summaryview);

	STATUSBAR_POP(summaryview->mainwin);
	GTK_EVENTS_FLUSH();

	msginfo = gtk_ctree_node_get_row_data(ctree, row);

	if (new_window) {
		MessageView *msgview;

		msgview = messageview_create_with_new_window(summaryview->mainwin);
		val = messageview_show(msgview, msginfo, all_headers);
	} else {
		MessageView *msgview;

		msgview = summaryview->messageview;

		summaryview->displayed = row;
		if (!messageview_is_visible(msgview)) {
			main_window_toggle_message_view(summaryview->mainwin);
			GTK_EVENTS_FLUSH();
		}
		val = messageview_show(msgview, msginfo, all_headers);
		if (GTK_CLIST(msgview->mimeview->ctree)->row_list == NULL)
			gtk_widget_grab_focus(summaryview->ctree);
		gtkut_ctree_node_move_if_on_the_edge(ctree, row);
	}

	if (val == 0 && MSG_IS_UNREAD(msginfo->flags)) {
		if (prefs_common.mark_as_read_delay) {
			MarkAsReadData *data = g_new0(MarkAsReadData, 1);
			data->summaryview = summaryview;
			data->msginfo = msginfo;
			data->row = row;
			gtk_timeout_add(prefs_common.mark_as_read_delay * 1000,
				msginfo_mark_as_read_timeout, data);
		} else if (new_window || !prefs_common.mark_as_read_on_new_window) {
			msginfo_mark_as_read(summaryview, msginfo, row);
		}
	}

	summary_set_menu_sensitive(summaryview);
	toolbar_main_set_sensitive(summaryview->mainwin);
	messageview_set_menu_sensitive(summaryview->messageview);

	summary_unlock(summaryview);
}

void summary_display_msg_selected(SummaryView *summaryview,
				  gboolean all_headers)
{
	if (summary_is_locked(summaryview)) return;
	summaryview->displayed = NULL;
	summary_display_msg_full(summaryview, summaryview->selected, FALSE,
				 all_headers);
}

void summary_redisplay_msg(SummaryView *summaryview)
{
	GtkCTreeNode *node;

	if (summaryview->displayed) {
		node = summaryview->displayed;
		summaryview->displayed = NULL;
		summary_display_msg(summaryview, node);
	}
}

void summary_open_msg(SummaryView *summaryview)
{
	if (!summaryview->selected) return;
	
	/* CLAWS: if separate message view, don't open a new window
	 * but rather use the current separated message view */
	summary_display_msg_full(summaryview, summaryview->selected,
				 prefs_common.sep_msg ? FALSE : TRUE, 
				 FALSE);
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
	if (!summaryview->folder_item) return;
	if (!folder_has_parent_of_type(summaryview->folder_item, F_OUTBOX)
	&&  !folder_has_parent_of_type(summaryview->folder_item, F_DRAFT)
	&&  !folder_has_parent_of_type(summaryview->folder_item, F_QUEUE))
		return;

	msginfo = gtk_ctree_node_get_row_data(GTK_CTREE(summaryview->ctree),
					      summaryview->selected);
	if (!msginfo) return;

	compose_reedit(msginfo);
}

gboolean summary_step(SummaryView *summaryview, GtkScrollType type)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;

	if (summary_is_locked(summaryview)) return FALSE;
	if (type == GTK_SCROLL_STEP_FORWARD) {
		node = gtkut_ctree_node_next(ctree, summaryview->selected);
		if (node)
			gtkut_ctree_expand_parent_all(ctree, node);
		else
			return FALSE;
	} else {
		if (summaryview->selected) {
			node = GTK_CTREE_NODE_PREV(summaryview->selected);
			if (!node) return FALSE;
		}
	}

	if (messageview_is_visible(summaryview->messageview))
		summaryview->display_msg = TRUE;

	g_signal_emit_by_name(G_OBJECT(ctree), "scroll_vertical", type, 0.0);

	if (GTK_CLIST(ctree)->selection)
		gtk_sctree_set_anchor_row
			(GTK_SCTREE(ctree),
			 GTK_CTREE_NODE(GTK_CLIST(ctree)->selection->data));

	return TRUE;
}

void summary_toggle_view(SummaryView *summaryview)
{
	if (!messageview_is_visible(summaryview->messageview) &&
	    summaryview->selected)
		summary_display_msg(summaryview,
				    summaryview->selected);
	else
		main_window_toggle_message_view(summaryview->mainwin);
}

static gboolean summary_search_unread_recursive(GtkCTree *ctree,
						GtkCTreeNode *node)
{
	MsgInfo *msginfo;

	if (node) {
		msginfo = gtk_ctree_node_get_row_data(ctree, node);
		if (msginfo && MSG_IS_UNREAD(msginfo->flags) && !MSG_IS_IGNORE_THREAD(msginfo->flags))
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
	gint *col_pos = summaryview->col_pos;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	if (!msginfo) return;

	flags = msginfo->flags;

	gtk_ctree_node_set_foreground(ctree, row, NULL);

	/* set new/unread column */
	if (MSG_IS_IGNORE_THREAD(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_STATUS],
					  ignorethreadxpm, ignorethreadxpmmask);
	} else if (MSG_IS_NEW(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_STATUS],
					  newxpm, newxpmmask);
	} else if (MSG_IS_UNREAD(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_STATUS],
					  unreadxpm, unreadxpmmask);
	} else if (MSG_IS_REPLIED(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_STATUS],
					  repliedxpm, repliedxpmmask);
	} else if (MSG_IS_FORWARDED(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_STATUS],
					  forwardedxpm, forwardedxpmmask);
	} else {
		gtk_ctree_node_set_text(ctree, row, col_pos[S_COL_STATUS],
					"");
	}

	if (prefs_common.bold_unread &&
	    ((MSG_IS_UNREAD(flags) && !MSG_IS_IGNORE_THREAD(flags)) ||
	     (!GTK_CTREE_ROW(row)->expanded &&
	      GTK_CTREE_ROW(row)->children &&
	      summary_have_unread_children(summaryview, row))))
		style = bold_style;

	/* set mark column */
	if (MSG_IS_DELETED(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_MARK],
					  deletedxpm, deletedxpmmask);
		if (style)
			style = bold_deleted_style;
		else {
			style = small_deleted_style;
		}
			gtk_ctree_node_set_foreground
				(ctree, row, &summaryview->color_dim);
	} else if (MSG_IS_MARKED(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_MARK],
					  markxpm, markxpmmask);
	} else if (MSG_IS_MOVE(flags)) {
		gtk_ctree_node_set_text(ctree, row, col_pos[S_COL_MARK], "o");
		if (style)
			style = bold_marked_style;
		else {
			style = small_marked_style;
		}
			gtk_ctree_node_set_foreground
				(ctree, row, &summaryview->color_marked);
	} else if (MSG_IS_COPY(flags)) {
		gtk_ctree_node_set_text(ctree, row, col_pos[S_COL_MARK], "O");
		if (style)
			style = bold_marked_style;
		else {
			style = small_marked_style;
		}
			gtk_ctree_node_set_foreground
                        	(ctree, row, &summaryview->color_marked);
#if 0
	} else if ((global_scoring ||
		  summaryview->folder_item->prefs->scoring) &&
		 (msginfo->score >= summaryview->important_score) &&
		 (MSG_IS_MARKED(msginfo->flags) || MSG_IS_MOVE(msginfo->flags) || MSG_IS_COPY(msginfo->flags))) {
		gtk_ctree_node_set_text(ctree, row, S_COL_MARK, "!");
		gtk_ctree_node_set_foreground(ctree, row,
					      &summaryview->color_important);
#endif
	} else {
		gtk_ctree_node_set_text(ctree, row, col_pos[S_COL_MARK], "");
	}

	if (MSG_IS_LOCKED(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_LOCKED],
					  lockedxpm, lockedxpmmask);
	}
	else {
		gtk_ctree_node_set_text(ctree, row, col_pos[S_COL_LOCKED], "");
	}

	if (MSG_IS_WITH_ATTACHMENT(flags) && MSG_IS_SIGNED(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_MIME],
					  clipgpgsignedxpm, clipgpgsignedxpmmask);
	} else if (MSG_IS_SIGNED(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_MIME],
					  gpgsignedxpm, gpgsignedxpmmask);
	} else if (MSG_IS_WITH_ATTACHMENT(flags) && MSG_IS_ENCRYPTED(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_MIME],
					  clipkeyxpm, clipkeyxpmmask);
	} else if (MSG_IS_ENCRYPTED(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_MIME],
					  keyxpm, keyxpmmask);
	} else if (MSG_IS_WITH_ATTACHMENT(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_MIME],
					  clipxpm, clipxpmmask);
	} else {
		gtk_ctree_node_set_text(ctree, row, col_pos[S_COL_MIME], "");
	}
	if (!style)
		style = small_style;

	gtk_ctree_node_set_row_style(ctree, row, style);

	if (MSG_GET_COLORLABEL(flags))
		summary_set_colorlabel_color(ctree, row, MSG_GET_COLORLABEL_VALUE(flags));
}

static void summary_mark_row(SummaryView *summaryview, GtkCTreeNode *row)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	if (MSG_IS_DELETED(msginfo->flags))
		summaryview->deleted--;
	if (MSG_IS_MOVE(msginfo->flags))
		summaryview->moved--;
	if (MSG_IS_COPY(msginfo->flags))
		summaryview->copied--;

	procmsg_msginfo_set_to_folder(msginfo, NULL);
	summary_msginfo_change_flags(msginfo, MSG_MARKED, 0, MSG_DELETED, MSG_MOVE | MSG_COPY);
	summary_set_row_marks(summaryview, row);
	debug_print("Message %s/%d is marked\n", msginfo->folder->path, msginfo->msgnum);
}

static void summary_lock_row(SummaryView *summaryview, GtkCTreeNode *row)
{
	gboolean changed = FALSE;
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
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
	procmsg_msginfo_set_to_folder(msginfo, NULL);
	summary_msginfo_change_flags(msginfo, MSG_LOCKED, 0, MSG_DELETED, MSG_MOVE | MSG_COPY);
	
	summary_set_row_marks(summaryview, row);
	debug_print("Message %d is locked\n", msginfo->msgnum);
}

static void summary_unlock_row(SummaryView *summaryview, GtkCTreeNode *row)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	if (!MSG_IS_LOCKED(msginfo->flags))
		return;
	procmsg_msginfo_set_to_folder(msginfo, NULL);
	summary_msginfo_unset_flags(msginfo, MSG_LOCKED, 0);
	summary_set_row_marks(summaryview, row);
	debug_print("Message %d is unlocked\n", msginfo->msgnum);
}

void summary_mark(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	START_LONG_OPERATION(summaryview);
	folder_item_set_batch(summaryview->folder_item, TRUE);
	for (cur = GTK_CLIST(ctree)->selection; cur != NULL && cur->data != NULL; cur = cur->next)
		summary_mark_row(summaryview, GTK_CTREE_NODE(cur->data));
	folder_item_set_batch(summaryview->folder_item, FALSE);
	END_LONG_OPERATION(summaryview);

	summary_status_show(summaryview);
}

static void summary_mark_row_as_read(SummaryView *summaryview,
				     GtkCTreeNode *row)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);

	if(!(MSG_IS_NEW(msginfo->flags) || MSG_IS_UNREAD(msginfo->flags)))
		return;

	summary_msginfo_unset_flags(msginfo, MSG_NEW | MSG_UNREAD, 0);
	summary_set_row_marks(summaryview, row);
	debug_print("Message %d is marked as read\n",
		msginfo->msgnum);
}

void summary_mark_as_read(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	START_LONG_OPERATION(summaryview);
	folder_item_set_batch(summaryview->folder_item, TRUE);
	for (cur = GTK_CLIST(ctree)->selection; cur != NULL && cur->data != NULL; cur = cur->next)
		summary_mark_row_as_read(summaryview,
					 GTK_CTREE_NODE(cur->data));
	folder_item_set_batch(summaryview->folder_item, FALSE);
	END_LONG_OPERATION(summaryview);
	
	summary_status_show(summaryview);
}

void summary_msgs_lock(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	START_LONG_OPERATION(summaryview);
	for (cur = GTK_CLIST(ctree)->selection; cur != NULL && cur->data != NULL; cur = cur->next)
		summary_lock_row(summaryview,
					 GTK_CTREE_NODE(cur->data));
	END_LONG_OPERATION(summaryview);
	
	summary_status_show(summaryview);
}

void summary_msgs_unlock(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	START_LONG_OPERATION(summaryview);
	for (cur = GTK_CLIST(ctree)->selection; cur != NULL && cur->data != NULL; cur = cur->next)
		summary_unlock_row(summaryview,
				   GTK_CTREE_NODE(cur->data));
	END_LONG_OPERATION(summaryview);
	
	summary_status_show(summaryview);
}

void summary_mark_all_read(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;

	START_LONG_OPERATION(summaryview);
	folder_item_set_batch(summaryview->folder_item, TRUE);
	for (node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list); node != NULL;
	     node = gtkut_ctree_node_next(ctree, node))
		summary_mark_row_as_read(summaryview, node);
	folder_item_set_batch(summaryview->folder_item, FALSE);
	for (node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list); node != NULL;
	     node = gtkut_ctree_node_next(ctree, node)) {
		if (!GTK_CTREE_ROW(node)->expanded)
			summary_set_row_marks(summaryview, node);
	}
	END_LONG_OPERATION(summaryview);
	
	summary_status_show(summaryview);
}

static void summary_mark_row_as_unread(SummaryView *summaryview,
				       GtkCTreeNode *row)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	if (MSG_IS_DELETED(msginfo->flags)) {
		procmsg_msginfo_set_to_folder(msginfo, NULL);
		summary_msginfo_unset_flags(msginfo, MSG_DELETED, 0);
		summaryview->deleted--;
	}

	summary_msginfo_set_flags(msginfo, MSG_UNREAD, 0);
	debug_print("Message %d is marked as unread\n",
		msginfo->msgnum);

	summary_set_row_marks(summaryview, row);
}

void summary_mark_as_unread(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	START_LONG_OPERATION(summaryview);
	folder_item_set_batch(summaryview->folder_item, TRUE);
	for (cur = GTK_CLIST(ctree)->selection; cur != NULL && cur->data != NULL; 
		cur = cur->next)
		summary_mark_row_as_unread(summaryview,
					   GTK_CTREE_NODE(cur->data));
	folder_item_set_batch(summaryview->folder_item, FALSE);
	END_LONG_OPERATION(summaryview);
	
	summary_status_show(summaryview);
}

static gboolean check_permission(SummaryView *summaryview, MsgInfo * msginfo)
{
	GList * cur;
	gboolean found;

	switch (FOLDER_TYPE(summaryview->folder_item->folder)) {

	case F_NEWS:

		/*
		  security : checks if one the accounts correspond to
		  the author of the post
		*/

		found = FALSE;
		for(cur = account_get_list() ; cur != NULL ; cur = cur->next) {
			PrefsAccount * account;
			gchar * from_name;
			
			account = cur->data;
			if (account->name && *account->name)
				from_name =
					g_strdup_printf("%s <%s>",
							account->name,
							account->address);
			else
				from_name =
					g_strdup_printf("%s",
							account->address);
			
			if (g_utf8_collate(from_name, msginfo->from) == 0) {
				g_free(from_name);
				found = TRUE;
				break;
			}
			g_free(from_name);
		}

		if (!found) {
			alertpanel_error(_("You're not the author of the article.\n"));
		}
		
		return found;

	default:
		return TRUE;
	}
}

static void summary_delete_row(SummaryView *summaryview, GtkCTreeNode *row)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);

	if (MSG_IS_LOCKED(msginfo->flags)) return;

	if (MSG_IS_DELETED(msginfo->flags)) return;

	if (MSG_IS_MOVE(msginfo->flags))
		summaryview->moved--;
	if (MSG_IS_COPY(msginfo->flags))
		summaryview->copied--;

	procmsg_msginfo_set_to_folder(msginfo, NULL);
	summary_msginfo_change_flags(msginfo, MSG_DELETED, 0, MSG_MARKED, MSG_MOVE | MSG_COPY);
	summaryview->deleted++;

	if (!prefs_common.immediate_exec && 
	    !folder_has_parent_of_type(summaryview->folder_item, F_TRASH))
		summary_set_row_marks(summaryview, row);

	debug_print("Message %s/%d is set to delete\n",
		    msginfo->folder->path, msginfo->msgnum);
}

void summary_cancel(SummaryView *summaryview)
{
	MsgInfo * msginfo;
	GtkCList *clist = GTK_CLIST(summaryview->ctree);

	msginfo = gtk_ctree_node_get_row_data(GTK_CTREE(summaryview->ctree),
					      summaryview->selected);
	if (!msginfo) return;

	if (!check_permission(summaryview, msginfo))
		return;

	news_cancel_article(summaryview->folder_item->folder, msginfo);
	
	if (summary_is_locked(summaryview)) return;

	summary_lock(summaryview);

	gtk_clist_freeze(clist);

	summary_update_status(summaryview);
	summary_status_show(summaryview);

	gtk_clist_thaw(clist);

	summary_unlock(summaryview);
}

void summary_delete(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	FolderItem *item = summaryview->folder_item;
	GList *cur;
	GtkCTreeNode *sel_last = NULL;
	GtkCTreeNode *node;
	AlertValue aval;
	MsgInfo *msginfo;

	if (!item) return;

	if (summary_is_locked(summaryview)) return;

	if (!summaryview->folder_item) return;

	aval = alertpanel(_("Delete message(s)"),
			  _("Do you really want to delete selected message(s)?"),
			  GTK_STOCK_YES, GTK_STOCK_NO, NULL);
	if (aval != G_ALERTDEFAULT) return;

	for (cur = GTK_CLIST(ctree)->selection; cur != NULL && cur->data != NULL; 
	     cur = cur->next) {
		GtkCTreeNode *row = GTK_CTREE_NODE(cur->data);
		msginfo = gtk_ctree_node_get_row_data(ctree, row);
		if (msginfo->total_size != 0 && 
		    msginfo->size != (off_t)msginfo->total_size)
			partial_mark_for_delete(msginfo);
	}

	main_window_cursor_wait(summaryview->mainwin);

	/* next code sets current row focus right. We need to find a row
	 * that is not deleted. */
	START_LONG_OPERATION(summaryview);
	folder_item_set_batch(summaryview->folder_item, TRUE);
	for (cur = GTK_CLIST(ctree)->selection; cur != NULL && cur->data != NULL; cur = cur->next) {
		sel_last = GTK_CTREE_NODE(cur->data);
		summary_delete_row(summaryview, sel_last);
	}
	folder_item_set_batch(summaryview->folder_item, FALSE);
	END_LONG_OPERATION(summaryview);

	node = summary_find_next_msg(summaryview, sel_last);
	if (!node)
		node = summary_find_prev_msg(summaryview, sel_last);

	summary_select_node(summaryview, node, prefs_common.always_show_msg, TRUE);
	
	if (prefs_common.immediate_exec || folder_has_parent_of_type(item, F_TRASH)) {
		summary_execute(summaryview);
		/* after deleting, the anchor may be at an invalid row
		 * so reset it to the node we found earlier */
		gtk_sctree_set_anchor_row(GTK_SCTREE(ctree), node);
	} else
		summary_status_show(summaryview);

		
	main_window_cursor_normal(summaryview->mainwin);
}

void summary_delete_trash(SummaryView *summaryview)
{
	FolderItem *to_folder = NULL;
	PrefsAccount *ac;
	if (!summaryview->folder_item ||
	    FOLDER_TYPE(summaryview->folder_item->folder) == F_NEWS) return;
	
	if (NULL != (ac = account_find_from_item(summaryview->folder_item)))
		to_folder = account_get_special_folder(ac, F_TRASH);

	if (to_folder == NULL)
		to_folder = summaryview->folder_item->folder->trash;
	
	if (to_folder == NULL || to_folder == summaryview->folder_item
	    || folder_has_parent_of_type(summaryview->folder_item, F_TRASH))
		summary_delete(summaryview);
	else
		summary_move_selected_to(summaryview, to_folder);
}


static void summary_unmark_row(SummaryView *summaryview, GtkCTreeNode *row)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	if (MSG_IS_DELETED(msginfo->flags))
		summaryview->deleted--;
	if (MSG_IS_MOVE(msginfo->flags))
		summaryview->moved--;
	if (MSG_IS_COPY(msginfo->flags))
		summaryview->copied--;

	procmsg_msginfo_set_to_folder(msginfo, NULL);
	summary_msginfo_unset_flags(msginfo, MSG_MARKED | MSG_DELETED, MSG_MOVE | MSG_COPY);
	summary_set_row_marks(summaryview, row);

	debug_print("Message %s/%d is unmarked\n",
		    msginfo->folder->path, msginfo->msgnum);
}

void summary_unmark(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	START_LONG_OPERATION(summaryview);
	folder_item_set_batch(summaryview->folder_item, TRUE);
	for (cur = GTK_CLIST(ctree)->selection; cur != NULL && cur->data != NULL; cur = cur->next)
		summary_unmark_row(summaryview, GTK_CTREE_NODE(cur->data));
	folder_item_set_batch(summaryview->folder_item, FALSE);
	END_LONG_OPERATION(summaryview);
	
	summary_status_show(summaryview);
}

static void summary_move_row_to(SummaryView *summaryview, GtkCTreeNode *row,
				FolderItem *to_folder)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	g_return_if_fail(to_folder != NULL);

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	if (MSG_IS_LOCKED(msginfo->flags))
		return;

	procmsg_msginfo_set_to_folder(msginfo, to_folder);
	if (MSG_IS_DELETED(msginfo->flags))
		summaryview->deleted--;
	if (MSG_IS_COPY(msginfo->flags)) {
		summaryview->copied--;
	}
	if (!MSG_IS_MOVE(msginfo->flags)) {
		summary_msginfo_change_flags(msginfo, 0, MSG_MOVE, MSG_DELETED, MSG_COPY);
		summaryview->moved++;
	} else {
		summary_msginfo_unset_flags(msginfo, MSG_DELETED, MSG_COPY);
	}
	
	if (!prefs_common.immediate_exec) {
		summary_set_row_marks(summaryview, row);
	}

	debug_print("Message %d is set to move to %s\n",
		    msginfo->msgnum, to_folder->path);
}

void summary_move_selected_to(SummaryView *summaryview, FolderItem *to_folder)
{
	GList *cur;

	if (!to_folder) return;
	if (!summaryview->folder_item ||
	    FOLDER_TYPE(summaryview->folder_item->folder) == F_NEWS) return;

	if (summary_is_locked(summaryview)) return;

	if (summaryview->folder_item == to_folder) {
		alertpanel_error(_("Destination is same as current folder."));
		return;
	}

	START_LONG_OPERATION(summaryview);

	for (cur = GTK_CLIST(summaryview->ctree)->selection;
	     cur != NULL && cur->data != NULL; cur = cur->next)
		summary_move_row_to
			(summaryview, GTK_CTREE_NODE(cur->data), to_folder);

	END_LONG_OPERATION(summaryview);

	if (prefs_common.immediate_exec) {
		summary_execute(summaryview);
	} else {
		summary_status_show(summaryview);
	}
	
	if (!summaryview->selected) { /* this was the last message */
		GtkCTreeNode *node = gtk_ctree_node_nth (GTK_CTREE(summaryview->ctree), 
							 GTK_CLIST(summaryview->ctree)->rows - 1);
		if (node)
			summary_select_node(summaryview, node, prefs_common.always_show_msg, TRUE);
	}

}

void summary_move_to(SummaryView *summaryview)
{
	FolderItem *to_folder;

	if (!summaryview->folder_item ||
	    FOLDER_TYPE(summaryview->folder_item->folder) == F_NEWS) return;

	to_folder = foldersel_folder_sel(summaryview->folder_item->folder,
					 FOLDER_SEL_MOVE, NULL);
	summary_move_selected_to(summaryview, to_folder);
}

static void summary_copy_row_to(SummaryView *summaryview, GtkCTreeNode *row,
				FolderItem *to_folder)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	g_return_if_fail(to_folder != NULL);

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	procmsg_msginfo_set_to_folder(msginfo, to_folder);
	if (MSG_IS_DELETED(msginfo->flags))
		summaryview->deleted--;
	if (MSG_IS_MOVE(msginfo->flags)) {
		summaryview->moved--;
	}
	
	if (!MSG_IS_COPY(msginfo->flags)) {
		summary_msginfo_change_flags(msginfo, 0, MSG_COPY, MSG_DELETED, MSG_MOVE);
		summaryview->copied++;
	} else {
		summary_msginfo_unset_flags(msginfo, MSG_DELETED, MSG_MOVE);
	}
	if (!prefs_common.immediate_exec) {
		summary_set_row_marks(summaryview, row);
	}

	debug_print("Message %d is set to copy to %s\n",
		    msginfo->msgnum, to_folder->path);
}

void summary_copy_selected_to(SummaryView *summaryview, FolderItem *to_folder)
{
	GList *cur;

	if (!to_folder) return;
	if (!summaryview->folder_item) return;

	if (summary_is_locked(summaryview)) return;

	if (summaryview->folder_item == to_folder) {
		alertpanel_error
			(_("Destination to copy is same as current folder."));
		return;
	}

	START_LONG_OPERATION(summaryview);

	for (cur = GTK_CLIST(summaryview->ctree)->selection;
	     cur != NULL && cur->data != NULL; cur = cur->next)
		summary_copy_row_to
			(summaryview, GTK_CTREE_NODE(cur->data), to_folder);

	END_LONG_OPERATION(summaryview);

	if (prefs_common.immediate_exec)
		summary_execute(summaryview);
	else {
		summary_status_show(summaryview);
	}
}

void summary_copy_to(SummaryView *summaryview)
{
	FolderItem *to_folder;

	if (!summaryview->folder_item) return;

	to_folder = foldersel_folder_sel(summaryview->folder_item->folder,
					 FOLDER_SEL_COPY, NULL);
	summary_copy_selected_to(summaryview, to_folder);
}

void summary_add_address(SummaryView *summaryview)
{
	MsgInfo *msginfo;
	gchar *from;

	msginfo = gtk_ctree_node_get_row_data(GTK_CTREE(summaryview->ctree),
					      summaryview->selected);
	if (!msginfo) return;

	Xstrdup_a(from, msginfo->from, return);
	eliminate_address_comment(from);
	extract_address(from);
	addressbook_add_contact(msginfo->fromname, from, NULL);
}

void summary_select_all(SummaryView *summaryview)
{
	if (!summaryview->folder_item) return;

	summary_lock(summaryview);
	gtk_clist_select_all(GTK_CLIST(summaryview->ctree));
	summary_unlock(summaryview);
	summary_status_show(summaryview);
}

void summary_unselect_all(SummaryView *summaryview)
{
	summary_lock(summaryview);
	gtk_sctree_unselect_all(GTK_SCTREE(summaryview->ctree));
	summary_unlock(summaryview);
	summary_status_show(summaryview);
}

void summary_select_thread(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node = summaryview->selected;

	if (!node) return;

	while (GTK_CTREE_ROW(node)->parent != NULL)
		node = GTK_CTREE_ROW(node)->parent;

	if (node != summaryview->selected)
		summary_select_node
			(summaryview, node,
			 messageview_is_visible(summaryview->messageview),
			 FALSE);

	gtk_ctree_select_recursive(ctree, node);

	summary_status_show(summaryview);
}

void summary_save_as(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;
	gchar *filename = NULL;
	gchar *src, *dest;
	gchar *tmp;

	AlertValue aval = 0;

	if (!summaryview->selected) return;
	msginfo = gtk_ctree_node_get_row_data(ctree, summaryview->selected);
	if (!msginfo) return;

	if (msginfo->subject) {
		Xstrdup_a(filename, msginfo->subject, return);
		subst_for_filename(filename);
	}
	if (g_getenv ("G_BROKEN_FILENAMES") &&
	    filename && !g_utf8_validate(filename, -1, NULL)) {
		gchar *oldstr = filename;
		filename = conv_codeset_strdup(filename,
					       conv_get_locale_charset_str(),
					       CS_UTF_8);
		if (!filename) {
			g_warning("summary_save_as(): faild to convert character set.");
			filename = g_strdup(oldstr);
		}
		dest = filesel_select_file_save(_("Save as"), filename);
		g_free(filename);
	} else
		dest = filesel_select_file_save(_("Save as"), filename);
	filename = NULL;
	if (!dest) return;
	if (is_file_exist(dest)) {
		aval = alertpanel(_("Append or Overwrite"),
				  _("Append or overwrite existing file?"),
				  _("_Append"), _("_Overwrite"),
				  GTK_STOCK_CANCEL);
		if (aval != 0 && aval != 1)
			return;
	}

	src = procmsg_get_message_file(msginfo);
	tmp = g_path_get_basename(dest);

	if ( aval==0 ) { /* append */
		if (append_file(src, dest, TRUE) < 0) 
			alertpanel_error(_("Can't save the file '%s'."), tmp);
	} else { /* overwrite */
		if (copy_file(src, dest, TRUE) < 0)
			alertpanel_error(_("Can't save the file '%s'."), tmp);
	}
	g_free(src);
	
	/*
	 * If two or more msgs are selected,
	 * append them to the output file.
	 */
	if (GTK_CLIST(ctree)->selection->next) {
		GList *item;
		for (item = GTK_CLIST(ctree)->selection->next; item != NULL; item=item->next) {
			msginfo = gtk_ctree_node_get_row_data(ctree, (GtkCTreeNode*)item->data);
			if (!msginfo) break;
			src = procmsg_get_message_file(msginfo);
			if (append_file(src, dest, TRUE) < 0)
				alertpanel_error(_("Can't save the file '%s'."), tmp);
		}
		g_free(src);
	}
	g_free(dest);
	g_free(tmp);
}

#ifdef USE_GNOMEPRINT
static void print_mimeview(MimeView *mimeview) 
{
	if (!mimeview 
	||  !mimeview->textview
	||  !mimeview->textview->text)
		alertpanel_warning(_("Cannot print: the message doesn't "
				     "contain text."));
	else
		gedit_print(GTK_TEXT_VIEW(mimeview->textview->text));
}
#endif

void summary_print(SummaryView *summaryview)
{
	GtkCList *clist = GTK_CLIST(summaryview->ctree);
#ifndef USE_GNOMEPRINT
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;
	gchar *cmdline = NULL;
	gchar *p;
#endif
	GList *cur;

	if (clist->selection == NULL) return;
#ifndef USE_GNOMEPRINT
	cmdline = input_dialog(_("Print"),
			       _("Enter the print command line:\n"
				 "('%s' will be replaced with file name)"),
			       prefs_common.print_cmd);
	if (!cmdline) return;
	if (!(p = strchr(cmdline, '%')) || *(p + 1) != 's' ||
	    strchr(p + 2, '%')) {
		alertpanel_error(_("Print command line is invalid:\n'%s'"),
				 cmdline);
		g_free(cmdline);
		return;
	}
	for (cur = clist->selection; 
	     cur != NULL && cur->data != NULL; 
	     cur = cur->next) {
		msginfo = gtk_ctree_node_get_row_data
			(ctree, GTK_CTREE_NODE(cur->data));
		if (msginfo) 
			procmsg_print_message(msginfo, cmdline);
	}

	g_free(cmdline);
#else
	for (cur = clist->selection; 
	     cur != NULL && cur->data != NULL; 
	     cur = cur->next) {
		GtkCTreeNode *node = GTK_CTREE_NODE(cur->data);
		if (node != summaryview->displayed) {
			MessageView *tmpview = messageview_create(
						summaryview->mainwin);
			MsgInfo *msginfo = gtk_ctree_node_get_row_data(
						GTK_CTREE(summaryview->ctree),
						node);

			messageview_init(tmpview);
			tmpview->all_headers = summaryview->messageview->all_headers;
			if (msginfo && messageview_show(tmpview, msginfo, 
				tmpview->all_headers) >= 0) {
					print_mimeview(tmpview->mimeview);
			}
			messageview_destroy(tmpview);
		} else {
			print_mimeview(summaryview->messageview->mimeview);
		}
	}
#endif
}

gboolean summary_execute(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCList *clist = GTK_CLIST(summaryview->ctree);
	GtkCTreeNode *node, *next;
	GtkCTreeNode *new_selected = NULL;
	gint move_val = -1;

	if (!summaryview->folder_item) return FALSE;

	if (summary_is_locked(summaryview)) return FALSE;
	summary_lock(summaryview);

	gtk_clist_freeze(clist);

	if (summaryview->threaded)
		summary_unthread_for_exec(summaryview);

	folder_item_update_freeze();
	move_val = summary_execute_move(summaryview);
	summary_execute_copy(summaryview);
	summary_execute_delete(summaryview);
	
	node = GTK_CTREE_NODE(clist->row_list);
	for (; node != NULL; node = next) {
		next = gtkut_ctree_node_next(ctree, node);
		if (gtk_ctree_node_get_row_data(ctree, node) != NULL) continue;

		if (node == summaryview->displayed) {
			messageview_clear(summaryview->messageview);
			summaryview->displayed = NULL;
		}
		if (GTK_CTREE_ROW(node)->children != NULL) {
			g_warning("summary_execute(): children != NULL\n");
			continue;
		}

		if (!new_selected &&
		    gtkut_ctree_node_is_selected(ctree, node)) {
			summary_unselect_all(summaryview);
			new_selected = summary_find_next_msg(summaryview, node);
			if (!new_selected)
				new_selected = summary_find_prev_msg
					(summaryview, node);
		}

		gtk_ctree_remove_node(ctree, node);
	}

	folder_item_update_thaw();
	gtk_clist_thaw(GTK_CLIST(summaryview->ctree));

	if (new_selected) {
		summary_unlock(summaryview);
		gtk_sctree_select
			(GTK_SCTREE(ctree),
			 new_selected);
		summary_lock(summaryview);
	}

	if (summaryview->threaded) {
		gtk_clist_freeze(GTK_CLIST(summaryview->ctree));
		summary_thread_build(summaryview);
		summary_thread_init(summaryview);
		gtk_clist_thaw(GTK_CLIST(summaryview->ctree));
	}

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

	gtk_ctree_node_moveto(ctree, summaryview->selected, -1, 0.5, 0);

	summary_unlock(summaryview);
	
	if (move_val < 0) 
		summary_show(summaryview, summaryview->folder_item);
	return TRUE;
}

static gint summary_execute_move(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GSList *cur;
	gint val = -1;
	/* search moving messages and execute */
	gtk_ctree_pre_recursive(ctree, NULL, summary_execute_move_func,
				summaryview);

	if (summaryview->mlist) {
		val = procmsg_move_messages(summaryview->mlist);

		for (cur = summaryview->mlist; cur != NULL && cur->data != NULL; cur = cur->next)
			procmsg_msginfo_free((MsgInfo *)cur->data);
		g_slist_free(summaryview->mlist);
		summaryview->mlist = NULL;
	}
	return val;
}

static void summary_execute_move_func(GtkCTree *ctree, GtkCTreeNode *node,
				      gpointer data)
{
	SummaryView *summaryview = data;
	MsgInfo *msginfo;

	msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);

	if (msginfo && MSG_IS_MOVE(msginfo->flags) && msginfo->to_folder) {
		summaryview->mlist =
			g_slist_prepend(summaryview->mlist, msginfo);
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

	/* search copying messages and execute */
	gtk_ctree_pre_recursive(ctree, NULL, summary_execute_copy_func,
				summaryview);

	if (summaryview->mlist) {
		summaryview->mlist = g_slist_reverse(summaryview->mlist);
		procmsg_copy_messages(summaryview->mlist);

		g_slist_free(summaryview->mlist);
		summaryview->mlist = NULL;
	}
}

static void summary_execute_copy_func(GtkCTree *ctree, GtkCTreeNode *node,
				      gpointer data)
{
	SummaryView *summaryview = data;
	MsgInfo *msginfo;

	msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);

	if (msginfo && MSG_IS_COPY(msginfo->flags) && msginfo->to_folder) {
		summaryview->mlist =
			g_slist_prepend(summaryview->mlist, msginfo);

		summary_msginfo_unset_flags(msginfo, 0, MSG_COPY);
		summary_set_row_marks(summaryview, node);
	}
}

static void summary_execute_delete(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GSList *cur;

	/* search deleting messages and execute */
	gtk_ctree_pre_recursive
		(ctree, NULL, summary_execute_delete_func, summaryview);

	if (!summaryview->mlist) return;

	folder_item_remove_msgs(summaryview->folder_item,
				summaryview->mlist);

	for (cur = summaryview->mlist; cur != NULL && cur->data != NULL; cur = cur->next)
		procmsg_msginfo_free((MsgInfo *)cur->data);

	g_slist_free(summaryview->mlist);
	summaryview->mlist = NULL;
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
						msginfo->msgid)) {
			g_hash_table_remove(summaryview->msgid_table,
					    msginfo->msgid);
		}	
		if (msginfo->subject && *msginfo->subject && 
		    node == subject_table_lookup(summaryview->subject_table,
						 msginfo->subject)) {
			subject_table_remove(summaryview->subject_table,
					     msginfo->subject);
		}					    
	}
}

/* thread functions */

void summary_thread_build(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	GtkCTreeNode *next;
	GtkCTreeNode *parent;
	MsgInfo *msginfo;
        GSList *reflist;

	summary_lock(summaryview);

	debug_print("Building threads...");
	STATUSBAR_PUSH(summaryview->mainwin, _("Building threads..."));
	main_window_cursor_wait(summaryview->mainwin);

	g_signal_handlers_block_by_func(G_OBJECT(ctree),
				       G_CALLBACK(summary_tree_expanded), summaryview);
	gtk_clist_freeze(GTK_CLIST(ctree));

	node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	while (node) {
		next = GTK_CTREE_ROW(node)->sibling;

		msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);

		parent = NULL;

		if (msginfo && msginfo->inreplyto) {
			parent = g_hash_table_lookup(summaryview->msgid_table,
						     msginfo->inreplyto);
                                                     
			if (!parent && msginfo->references) {
				for (reflist = msginfo->references;
				     reflist != NULL; reflist = reflist->next)
					if ((parent = g_hash_table_lookup
						(summaryview->msgid_table,
						 reflist->data)))
						break;
			}
		}

		if (prefs_common.thread_by_subject && parent == NULL) {
			parent = subject_table_lookup
				(summaryview->subject_table,
				 msginfo->subject);
		}

		if (parent && parent != node) {
			gtk_ctree_move(ctree, node, parent, NULL);
		}

		node = next;
	}

	gtkut_ctree_set_focus_row(ctree, summaryview->selected);

	gtk_clist_thaw(GTK_CLIST(ctree));
	g_signal_handlers_unblock_by_func(G_OBJECT(ctree),
					 G_CALLBACK(summary_tree_expanded), summaryview);

	debug_print("done.\n");
	STATUSBAR_POP(summaryview->mainwin);
	main_window_cursor_normal(summaryview->mainwin);

	summaryview->threaded = TRUE;

	summary_unlock(summaryview);
}

static void summary_thread_init(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	GtkCTreeNode *next;

	if (!summaryview->thread_collapsed) {
		g_signal_handlers_block_by_func(G_OBJECT(ctree),
				       G_CALLBACK(summary_tree_expanded), summaryview);
		while (node) {
			next = GTK_CTREE_ROW(node)->sibling;
			if (GTK_CTREE_ROW(node)->children)
				gtk_ctree_expand_recursive(ctree, node);
			node = next;
		}
		g_signal_handlers_unblock_by_func(G_OBJECT(ctree),
				       G_CALLBACK(summary_tree_expanded), summaryview);
	} 
}

void summary_unthread(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	GtkCTreeNode *child;
	GtkCTreeNode *sibling;
	GtkCTreeNode *next_child;

	summary_lock(summaryview);

	debug_print("Unthreading...");
	STATUSBAR_PUSH(summaryview->mainwin, _("Unthreading..."));
	main_window_cursor_wait(summaryview->mainwin);
	
	g_signal_handlers_block_by_func(G_OBJECT(ctree),
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

	/* CLAWS: and sort it */
	gtk_sctree_sort_recursive(ctree, NULL);	

	gtk_clist_thaw(GTK_CLIST(ctree));
	g_signal_handlers_unblock_by_func(G_OBJECT(ctree),
					   G_CALLBACK(summary_tree_collapsed), summaryview);

	debug_print("done.\n");
	STATUSBAR_POP(summaryview->mainwin);
	main_window_cursor_normal(summaryview->mainwin);

	summaryview->threaded = FALSE;

	summary_unlock(summaryview);
}

static void summary_unthread_for_exec(SummaryView *summaryview)
{
	GtkCTreeNode *node;
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);

	debug_print("Unthreading for execution...");

	START_LONG_OPERATION(summaryview);

	for (node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	     node != NULL; node = GTK_CTREE_NODE_NEXT(node)) {
		summary_unthread_for_exec_func(ctree, node, NULL);
	}

	END_LONG_OPERATION(summaryview);

	debug_print("done.\n");
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

void summary_expand_threads(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	g_signal_handlers_block_by_func(G_OBJECT(ctree),
				       G_CALLBACK(summary_tree_expanded), summaryview);
	gtk_clist_freeze(GTK_CLIST(ctree));

	while (node) {
		if (GTK_CTREE_ROW(node)->children) {
			gtk_ctree_expand(ctree, node);
			summary_set_row_marks(summaryview, node);
		}
		node = GTK_CTREE_NODE_NEXT(node);
	}

	gtk_clist_thaw(GTK_CLIST(ctree));
	g_signal_handlers_unblock_by_func(G_OBJECT(ctree),
					 G_CALLBACK(summary_tree_expanded), summaryview);

	summaryview->thread_collapsed = FALSE;

	gtk_ctree_node_moveto(ctree, summaryview->selected, -1, 0.5, 0);
}

void summary_collapse_threads(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	gtk_clist_freeze(GTK_CLIST(ctree));

	while (node) {
		if (GTK_CTREE_ROW(node)->children)
			gtk_ctree_collapse(ctree, node);
		node = GTK_CTREE_ROW(node)->sibling;
	}

	gtk_clist_thaw(GTK_CLIST(ctree));
	
	summaryview->thread_collapsed = TRUE;

	gtk_ctree_node_moveto(ctree, summaryview->selected, -1, 0.5, 0);
}

void summary_filter(SummaryView *summaryview, gboolean selected_only)
{
	summary_lock(summaryview);

	folder_item_update_freeze();
	
	debug_print("filtering...");
	STATUSBAR_PUSH(summaryview->mainwin, _("Filtering..."));
	main_window_cursor_wait(summaryview->mainwin);

	gtk_clist_freeze(GTK_CLIST(summaryview->ctree));

	if (selected_only) {
		GList *cur;

		for (cur = GTK_CLIST(summaryview->ctree)->selection;
	     	     cur != NULL && cur->data != NULL; cur = cur->next) {
			summary_filter_func(GTK_CTREE(summaryview->ctree),
				    	    GTK_CTREE_NODE(cur->data),
				    	    summaryview);
		}
	} else {
		gtk_ctree_pre_recursive(GTK_CTREE(summaryview->ctree), NULL,
					GTK_CTREE_FUNC(summary_filter_func),
					summaryview);
	}
	gtk_clist_thaw(GTK_CLIST(summaryview->ctree));

	folder_item_update_thaw();
	debug_print("done.\n");
	STATUSBAR_POP(summaryview->mainwin);
	main_window_cursor_normal(summaryview->mainwin);

	summary_unlock(summaryview);

	/* 
	 * CLAWS: summary_show() only valid after having a lock. ideally
	 * we want the lock to be context aware...  
	 */
	summary_show(summaryview, summaryview->folder_item);
}

static void summary_filter_func(GtkCTree *ctree, GtkCTreeNode *node,
				gpointer data)
{
	MailFilteringData mail_filtering_data;
	MsgInfo *msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);

	mail_filtering_data.msginfo = msginfo;
	if (hooks_invoke(MAIL_MANUAL_FILTERING_HOOKLIST, &mail_filtering_data))
		return;

	filter_message_by_msginfo(filtering_rules, msginfo);
}

void summary_msginfo_filter_open(FolderItem * item, MsgInfo *msginfo,
				 PrefsFilterType type, gint processing_rule)
{
	gchar *header = NULL;
	gchar *key = NULL;

	procmsg_get_filter_keyword(msginfo, &header, &key, type);
	
	if (processing_rule) {
		if (item == NULL)
			prefs_filtering_open(&pre_global_processing,
					     _("Processing rules to apply before folder rules"),
					     header, key);
		else
			prefs_filtering_open(&item->prefs->processing,
					     _("Processing configuration"),
					     header, key);
	}
	else {
		prefs_filtering_open(&filtering_rules,
				     _("Filtering configuration"),
				       header, key);
	}
	
	g_free(header);
	g_free(key);
}

void summary_filter_open(SummaryView *summaryview, PrefsFilterType type,
			 gint processing_rule)
{
	MsgInfo *msginfo;
	FolderItem * item;
	
	if (!summaryview->selected) return;

	msginfo = gtk_ctree_node_get_row_data(GTK_CTREE(summaryview->ctree),
					      summaryview->selected);
	if (!msginfo) return;
	
	item = summaryview->folder_item;
	summary_msginfo_filter_open(item, msginfo, type, processing_rule);
}

/* color label */

#define N_COLOR_LABELS colorlabel_get_color_count()

static void summary_colorlabel_menu_item_activate_cb(GtkWidget *widget,
						     gpointer data)
{
	guint color = GPOINTER_TO_UINT(data);
	SummaryView *summaryview;

	summaryview = g_object_get_data(G_OBJECT(widget), "summaryview");
	g_return_if_fail(summaryview != NULL);

	/* "dont_toggle" state set? */
	if (g_object_get_data(G_OBJECT(summaryview->colorlabel_menu),
				"dont_toggle"))
		return;

	summary_set_colorlabel(summaryview, color, NULL);
}

/* summary_set_colorlabel_color() - labelcolor parameter is the color *flag*
 * for the messsage; not the color index */
void summary_set_colorlabel_color(GtkCTree *ctree, GtkCTreeNode *node,
				  guint labelcolor)
{
	GdkColor color;
	GtkStyle *style, *prev_style, *ctree_style;
	MsgInfo *msginfo;
	gint color_index;

	msginfo = gtk_ctree_node_get_row_data(ctree, node);

	color_index = labelcolor == 0 ? -1 : (gint)labelcolor - 1;
	ctree_style = gtk_widget_get_style(GTK_WIDGET(ctree));
	prev_style = gtk_ctree_node_get_row_style(ctree, node);

	if (color_index < 0 || color_index >= N_COLOR_LABELS) {
		if (!prev_style) return;
		style = gtk_style_copy(prev_style);
		color = ctree_style->fg[GTK_STATE_NORMAL];
		style->fg[GTK_STATE_NORMAL] = color;
		color = ctree_style->fg[GTK_STATE_SELECTED];
		style->fg[GTK_STATE_SELECTED] = color;
	} else {
		if (prev_style)
			style = gtk_style_copy(prev_style);
		else
			style = gtk_style_copy(ctree_style);
		color = colorlabel_get_color(color_index);
		style->fg[GTK_STATE_NORMAL] = color;
		/* get the average of label color and selected fg color
		   for visibility */
		style->fg[GTK_STATE_SELECTED].red   = (color.red   + ctree_style->fg[GTK_STATE_SELECTED].red  ) / 2;
		style->fg[GTK_STATE_SELECTED].green = (color.green + ctree_style->fg[GTK_STATE_SELECTED].green) / 2;
		style->fg[GTK_STATE_SELECTED].blue  = (color.blue  + ctree_style->fg[GTK_STATE_SELECTED].blue ) / 2;
	}

	gtk_ctree_node_set_row_style(ctree, node, style);
}

static void summary_set_row_colorlabel(SummaryView *summaryview, GtkCTreeNode *row, guint labelcolor)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);

	summary_msginfo_change_flags(msginfo, MSG_COLORLABEL_TO_FLAGS(labelcolor), 0, 
					MSG_CLABEL_FLAG_MASK, 0);
}

void summary_set_colorlabel(SummaryView *summaryview, guint labelcolor,
			    GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	START_LONG_OPERATION(summaryview);

	for (cur = GTK_CLIST(ctree)->selection; cur != NULL && cur->data != NULL; cur = cur->next)
		summary_set_row_colorlabel(summaryview,
					   GTK_CTREE_NODE(cur->data), labelcolor);
	END_LONG_OPERATION(summaryview);
}

static void summary_colorlabel_menu_item_activate_item_cb(GtkMenuItem *menu_item,
							  gpointer data)
{
	SummaryView *summaryview;
	GtkMenuShell *menu;
	GtkCheckMenuItem **items;
	gint n;
	GList *cur, *sel;

	summaryview = (SummaryView *)data;
	g_return_if_fail(summaryview != NULL);

	sel = GTK_CLIST(summaryview->ctree)->selection;
	if (!sel) return;

	menu = GTK_MENU_SHELL(summaryview->colorlabel_menu);
	g_return_if_fail(menu != NULL);

	Xalloca(items, (N_COLOR_LABELS + 1) * sizeof(GtkWidget *), return);

	/* NOTE: don't return prematurely because we set the "dont_toggle"
	 * state for check menu items */
	g_object_set_data(G_OBJECT(menu), "dont_toggle",
			  GINT_TO_POINTER(1));

	/* clear items. get item pointers. */
	for (n = 0, cur = menu->children; cur != NULL && cur->data != NULL; cur = cur->next) {
		if (GTK_IS_CHECK_MENU_ITEM(cur->data)) {
			gtk_check_menu_item_set_active
				(GTK_CHECK_MENU_ITEM(cur->data), FALSE);
			items[n] = GTK_CHECK_MENU_ITEM(cur->data);
			n++;
		}
	}

	if (n == (N_COLOR_LABELS + 1)) {
		/* iterate all messages and set the state of the appropriate
		 * items */
		for (; sel != NULL; sel = sel->next) {
			MsgInfo *msginfo;
			gint clabel;

			msginfo = gtk_ctree_node_get_row_data
				(GTK_CTREE(summaryview->ctree),
				 GTK_CTREE_NODE(sel->data));
			if (msginfo) {
				clabel = MSG_GET_COLORLABEL_VALUE(msginfo->flags);
				if (!items[clabel]->active)
					gtk_check_menu_item_set_active
						(items[clabel], TRUE);
			}
		}
	} else
		g_warning("invalid number of color elements (%d)\n", n);

	/* reset "dont_toggle" state */
	g_object_set_data(G_OBJECT(menu), "dont_toggle",
			  GINT_TO_POINTER(0));
}

static void summary_colorlabel_menu_create(SummaryView *summaryview)
{
	GtkWidget *label_menuitem;
	GtkWidget *menu;
	GtkWidget *item;
	gint i;

	label_menuitem = gtk_item_factory_get_item(summaryview->popupfactory,
						   "/Color label");
	g_signal_connect(G_OBJECT(label_menuitem), "activate",
			 G_CALLBACK(summary_colorlabel_menu_item_activate_item_cb),
			   summaryview);
	gtk_widget_show(label_menuitem);

	menu = gtk_menu_new();

	/* create sub items. for the menu item activation callback we pass the
	 * index of label_colors[] as data parameter. for the None color we
	 * pass an invalid (high) value. also we attach a data pointer so we
	 * can always get back the SummaryView pointer. */

	item = gtk_check_menu_item_new_with_label(_("None"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_signal_connect(G_OBJECT(item), "activate",
			 G_CALLBACK(summary_colorlabel_menu_item_activate_cb),
			   GUINT_TO_POINTER(0));
	g_object_set_data(G_OBJECT(item), "summaryview", summaryview);
	gtk_widget_show(item);

	item = gtk_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	/* create pixmap/label menu items */
	for (i = 0; i < N_COLOR_LABELS; i++) {
		item = colorlabel_create_check_color_menu_item(i);
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		g_signal_connect(G_OBJECT(item), "activate",
				 G_CALLBACK(summary_colorlabel_menu_item_activate_cb),
				 GUINT_TO_POINTER(i + 1));
		g_object_set_data(G_OBJECT(item), "summaryview",
				  summaryview);
		gtk_widget_show(item);
	}

	gtk_widget_show(menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(label_menuitem), menu);
	summaryview->colorlabel_menu = menu;
}

static GtkWidget *summary_ctree_create(SummaryView *summaryview)
{
	GtkWidget *ctree;
	gint *col_pos = summaryview->col_pos;
	SummaryColumnState *col_state;
	gchar *titles[N_SUMMARY_COLS];
	SummaryColumnType type;
	gint pos;

	memset(titles, 0, sizeof(titles));

	col_state = prefs_summary_column_get_config();
	for (pos = 0; pos < N_SUMMARY_COLS; pos++) {
		summaryview->col_state[pos] = col_state[pos];
		type = col_state[pos].type;
		col_pos[type] = pos;
		titles[pos] = "dummy";
	}
	col_state = summaryview->col_state;

	ctree = gtk_sctree_new_with_titles
		(N_SUMMARY_COLS, col_pos[S_COL_SUBJECT], titles);

	gtk_clist_set_selection_mode(GTK_CLIST(ctree), GTK_SELECTION_EXTENDED);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), col_pos[S_COL_MARK],
					   GTK_JUSTIFY_CENTER);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), col_pos[S_COL_STATUS],
					   GTK_JUSTIFY_CENTER);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), col_pos[S_COL_LOCKED],
					   GTK_JUSTIFY_CENTER);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), col_pos[S_COL_MIME],
					   GTK_JUSTIFY_CENTER);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), col_pos[S_COL_SIZE],
					   GTK_JUSTIFY_RIGHT);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), col_pos[S_COL_NUMBER],
					   GTK_JUSTIFY_RIGHT);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), col_pos[S_COL_SCORE],
					   GTK_JUSTIFY_RIGHT);
	gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[S_COL_MARK],
				   SUMMARY_COL_MARK_WIDTH);
	gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[S_COL_STATUS],
				   SUMMARY_COL_STATUS_WIDTH);
	gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[S_COL_LOCKED],
				   SUMMARY_COL_LOCKED_WIDTH);
	gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[S_COL_MIME],
				   SUMMARY_COL_MIME_WIDTH);
	gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[S_COL_SUBJECT],
				   prefs_common.summary_col_size[S_COL_SUBJECT]);
	gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[S_COL_FROM],
				   prefs_common.summary_col_size[S_COL_FROM]);
	gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[S_COL_TO],
				   prefs_common.summary_col_size[S_COL_TO]);
	gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[S_COL_DATE],
				   prefs_common.summary_col_size[S_COL_DATE]);
	gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[S_COL_SIZE],
				   prefs_common.summary_col_size[S_COL_SIZE]);
	gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[S_COL_NUMBER],
				   prefs_common.summary_col_size[S_COL_NUMBER]);
	gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[S_COL_SCORE],
				   prefs_common.summary_col_size[S_COL_SCORE]);
	gtk_ctree_set_line_style(GTK_CTREE(ctree), GTK_CTREE_LINES_DOTTED);
	gtk_ctree_set_expander_style(GTK_CTREE(ctree),
				     GTK_CTREE_EXPANDER_SQUARE);
#if 0
	gtk_ctree_set_line_style(GTK_CTREE(ctree), GTK_CTREE_LINES_NONE);
	gtk_ctree_set_expander_style(GTK_CTREE(ctree),
				     GTK_CTREE_EXPANDER_TRIANGLE);
#endif
	gtk_ctree_set_indent(GTK_CTREE(ctree), 12);
	g_object_set_data(G_OBJECT(ctree), "user_data", summaryview);

	for (pos = 0; pos < N_SUMMARY_COLS; pos++) {
		GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(ctree)->column[pos].button,
				       GTK_CAN_FOCUS);
		gtk_clist_set_column_visibility
			(GTK_CLIST(ctree), pos, col_state[pos].visible);
	}

	/* connect signal to the buttons for sorting */
#define CLIST_BUTTON_SIGNAL_CONNECT(col, func) \
	g_signal_connect \
		(G_OBJECT(GTK_CLIST(ctree)->column[col_pos[col]].button), \
		 "clicked", \
		 G_CALLBACK(func), \
		 summaryview)

	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_MARK   , summary_mark_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_STATUS , summary_status_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_MIME   , summary_mime_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_NUMBER , summary_num_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_SIZE   , summary_size_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_DATE   , summary_date_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_FROM   , summary_from_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_TO     , summary_to_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_SUBJECT, summary_subject_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_SCORE,   summary_score_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_LOCKED,  summary_locked_clicked);

#undef CLIST_BUTTON_SIGNAL_CONNECT

	g_signal_connect(G_OBJECT(ctree), "tree_select_row",
			 G_CALLBACK(summary_selected), summaryview);
	g_signal_connect(G_OBJECT(ctree), "button_press_event",
			 G_CALLBACK(summary_button_pressed),
			 summaryview);
	g_signal_connect(G_OBJECT(ctree), "button_release_event",
			 G_CALLBACK(summary_button_released),
			 summaryview);
	g_signal_connect(G_OBJECT(ctree), "key_press_event",
			 G_CALLBACK(summary_key_pressed), summaryview);
	g_signal_connect(G_OBJECT(ctree), "resize_column",
			 G_CALLBACK(summary_col_resized), summaryview);
        g_signal_connect(G_OBJECT(ctree), "open_row",
			 G_CALLBACK(summary_open_row), summaryview);

	g_signal_connect_after(G_OBJECT(ctree), "tree_expand",
			       G_CALLBACK(summary_tree_expanded),
			       summaryview);
	g_signal_connect_after(G_OBJECT(ctree), "tree_collapse",
			       G_CALLBACK(summary_tree_collapsed),
			       summaryview);

	g_signal_connect(G_OBJECT(ctree), "start_drag",
			 G_CALLBACK(summary_start_drag),
			 summaryview);
	g_signal_connect(G_OBJECT(ctree), "drag_data_get",
			 G_CALLBACK(summary_drag_data_get),
			 summaryview);

	return ctree;
}

void summary_set_column_order(SummaryView *summaryview)
{
	GtkWidget *ctree;
	GtkWidget *scrolledwin = summaryview->scrolledwin;
	GtkWidget *pixmap;
	FolderItem *item;
	guint selected_msgnum = summary_get_msgnum(summaryview, summaryview->selected);
	guint displayed_msgnum = summary_get_msgnum(summaryview, summaryview->displayed);

	item = summaryview->folder_item;

	summary_clear_all(summaryview);
	gtk_widget_destroy(summaryview->ctree);

	summaryview->ctree = ctree = summary_ctree_create(summaryview);
	summary_set_fonts(summaryview);
	pixmap = gtk_image_new_from_pixmap(clipxpm, clipxpmmask);
	gtk_clist_set_column_widget(GTK_CLIST(ctree),
				    summaryview->col_pos[S_COL_MIME], pixmap);
	gtk_widget_show(pixmap);
	gtk_scrolled_window_set_hadjustment(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_CLIST(ctree)->hadjustment);
	gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_CLIST(ctree)->vadjustment);
	gtk_container_add(GTK_CONTAINER(scrolledwin), ctree);
	gtk_widget_show(ctree);

	summary_show(summaryview, item);

	summary_select_by_msgnum(summaryview, selected_msgnum);

	summaryview->displayed = summary_find_msg_by_msgnum(summaryview, displayed_msgnum);
	if (!summaryview->displayed)
		messageview_clear(summaryview->messageview);
	else
		summary_redisplay_msg(summaryview);
}


/* callback functions */

static gint summary_toggle_pressed(GtkWidget *eventbox, GdkEventButton *event,
				   SummaryView *summaryview)
{
	if (event)  
		summary_toggle_view(summaryview);
	return TRUE;
}

static gboolean summary_button_pressed(GtkWidget *ctree, GdkEventButton *event,
				       SummaryView *summaryview)
{
	if (!event) return FALSE;

	if (event->button == 3) {
		summaryview->display_msg = messageview_is_visible(summaryview->messageview);
		/* right clicked */
		gtk_menu_popup(GTK_MENU(summaryview->popupmenu), NULL, NULL,
			       NULL, NULL, event->button, event->time);
	} else if (event->button == 2) {
		summaryview->display_msg = messageview_is_visible(summaryview->messageview);
	} else if (event->button == 1) {
		if (!prefs_common.emulate_emacs &&
		    messageview_is_visible(summaryview->messageview))
			summaryview->display_msg = TRUE;
	}

	return FALSE;
}

static gboolean summary_button_released(GtkWidget *ctree, GdkEventButton *event,
					SummaryView *summaryview)
{
	return FALSE;
}

void summary_pass_key_press_event(SummaryView *summaryview, GdkEventKey *event)
{
	summary_key_pressed(summaryview->ctree, event, summaryview);
}

#define BREAK_ON_MODIFIER_KEY() \
	if ((event->state & (GDK_MOD1_MASK|GDK_CONTROL_MASK)) != 0) break

static gboolean summary_key_pressed(GtkWidget *widget, GdkEventKey *event,
				    SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(widget);
	GtkCTreeNode *node;
	MessageView *messageview;
	TextView *textview;
	GtkAdjustment *adj;
	gboolean mod_pressed;

	if (summary_is_locked(summaryview)) return TRUE;
	if (!event) return TRUE;

	if (quicksearch_has_focus(summaryview->quicksearch))
		return FALSE;

	switch (event->keyval) {
	case GDK_Left:		/* Move focus */
		adj = gtk_scrolled_window_get_hadjustment
			(GTK_SCROLLED_WINDOW(summaryview->scrolledwin));
		if (adj->lower != adj->value)
			break;
		/* FALLTHROUGH */	
	case GDK_Escape:
		gtk_widget_grab_focus(summaryview->folderview->ctree);
		return TRUE;
	case GDK_Home:
	case GDK_End:
		if ((node = summaryview->selected) != NULL) {
			GtkCTreeNode *next = NULL;
			next = (event->keyval == GDK_Home)
					? gtk_ctree_node_nth(ctree, 0)
					: gtk_ctree_node_nth(ctree, 
						g_list_length(GTK_CLIST(ctree)->row_list)-1);
			if (next) {
				gtk_sctree_select_with_state
					(GTK_SCTREE(ctree), next, event->state);

				/* Deprecated - what are the non-deprecated equivalents? */
				if (gtk_ctree_node_is_visible(GTK_CTREE(ctree), next) != GTK_VISIBILITY_FULL)
					gtk_ctree_node_moveto(GTK_CTREE(ctree), next, 0, 0, 0);
				summaryview->selected = next;
			}
		}
		return TRUE;
	default:
		break;
	}

	if (!summaryview->selected) {
		node = gtk_ctree_node_nth(ctree, 0);
		if (node)
			gtk_sctree_select(GTK_SCTREE(ctree), node);
		else
			return TRUE;
	}

	messageview = summaryview->messageview;
	textview = messageview->mimeview->textview;

	mod_pressed =
		((event->state & (GDK_SHIFT_MASK|GDK_MOD1_MASK)) != 0);

	switch (event->keyval) {
	case GDK_space:		/* Page down or go to the next */
		if (event->state & GDK_SHIFT_MASK) 
			textview_scroll_page(textview, TRUE);
		else {
			if (summaryview->displayed != summaryview->selected) {
				summary_display_msg(summaryview,
						    summaryview->selected);
				break;
			}
			if (mod_pressed) {
				if (!textview_scroll_page(textview, TRUE))
					summary_select_prev_unread(summaryview);
			} else {
				if (!textview_scroll_page(textview, FALSE))
					summary_select_next_unread(summaryview);
			}				
		}
		break;
	case GDK_BackSpace:	/* Page up */
		textview_scroll_page(textview, TRUE);
		break;
	case GDK_Return:	/* Scroll up/down one line */
		if (summaryview->displayed != summaryview->selected) {
			summary_display_msg(summaryview,
					    summaryview->selected);
			break;
		}
		textview_scroll_one_line(textview, mod_pressed);
		break;
	case GDK_Delete:
		BREAK_ON_MODIFIER_KEY();
		summary_delete_trash(summaryview);
		break;
	case GDK_y:
	case GDK_t:
	case GDK_l:
	case GDK_c:
		if ((event->state & (GDK_MOD1_MASK|GDK_CONTROL_MASK)) == 0) {
			mimeview_pass_key_press_event(messageview->mimeview,
						      event);
			break;
		}
	default:
		break;
	}
	return FALSE;
}

static void quicksearch_execute_cb(QuickSearch *quicksearch, gpointer data)
{
	SummaryView *summaryview = data;

	summary_show(summaryview, summaryview->folder_item);
}

static void tog_searchbar_cb(GtkWidget *w, gpointer data)
{
	SummaryView *summaryview = (SummaryView *)data;

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
		prefs_common.show_searchbar = TRUE;
 		quicksearch_show(summaryview->quicksearch);
	} else {
		prefs_common.show_searchbar = FALSE;
 		quicksearch_hide(summaryview->quicksearch);
	}
}

void summaryview_activate_quicksearch(SummaryView *summaryview) 
{
	gtk_toggle_button_set_active(
		GTK_TOGGLE_BUTTON(summaryview->toggle_search), 
		TRUE);
	quicksearch_show(summaryview->quicksearch);
}

static void summary_open_row(GtkSCTree *sctree, SummaryView *summaryview)
{
	if (folder_has_parent_of_type(summaryview->folder_item, F_OUTBOX)
	||  folder_has_parent_of_type(summaryview->folder_item, F_DRAFT)
	||  folder_has_parent_of_type(summaryview->folder_item, F_QUEUE))
		summary_reedit(summaryview);
	else
		summary_open_msg(summaryview);

	summaryview->display_msg = FALSE;
}

static void summary_tree_expanded(GtkCTree *ctree, GtkCTreeNode *node,
				  SummaryView *summaryview)
{
	summary_set_row_marks(summaryview, node);
	if (prefs_common.bold_unread) {
		while (node) {
			GtkCTreeNode *next = GTK_CTREE_NODE_NEXT(node);
			if (GTK_CTREE_ROW(node)->children)
				summary_set_row_marks(summaryview, node);
			node = next;
		}
	}
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
	gboolean marked_unread = FALSE;

	if (summary_is_locked(summaryview)
	||  GTK_SCTREE(ctree)->selecting_range) {
		return;
	}

	summary_status_show(summaryview);

	if (GTK_CLIST(ctree)->selection &&
	    GTK_CLIST(ctree)->selection->next) {
		summaryview->display_msg = FALSE;
		summary_set_menu_sensitive(summaryview);
		toolbar_main_set_sensitive(summaryview->mainwin);
		return;
	}

	summaryview->selected = row;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	g_return_if_fail(msginfo != NULL);

	switch (column < 0 ? column : summaryview->col_state[column].type) {
	case S_COL_MARK:
		if (!MSG_IS_DELETED(msginfo->flags) &&
		    !MSG_IS_MOVE(msginfo->flags) &&
		    !MSG_IS_COPY(msginfo->flags)) {
			if (MSG_IS_MARKED(msginfo->flags)) {
				summary_unmark_row(summaryview, row);
			} else {
				summary_mark_row(summaryview, row);
			}
		}
		break;
	case S_COL_STATUS:
		if (MSG_IS_UNREAD(msginfo->flags)) {
			summary_mark_row_as_read(summaryview, row);
			summary_status_show(summaryview);
		} else if (!MSG_IS_REPLIED(msginfo->flags) &&
			 !MSG_IS_FORWARDED(msginfo->flags)) {
			marked_unread = TRUE;
		} else if (MSG_IS_REPLIED(msginfo->flags)) {
			summary_find_answers(summaryview, msginfo);
			return;
		} 
		break;
	case S_COL_LOCKED:
		if (MSG_IS_LOCKED(msginfo->flags)) {
			summary_msginfo_unset_flags(msginfo, MSG_LOCKED, 0);
			summary_set_row_marks(summaryview, row);
		}
		else
			summary_lock_row(summaryview, row);
		break;
	default:
		break;
	}

	if (summaryview->display_msg ||
	    (prefs_common.always_show_msg &&
	     messageview_is_visible(summaryview->messageview))) {
		summaryview->display_msg = FALSE;
		if (summaryview->displayed != row) {
			summary_display_msg(summaryview, row);
			if (marked_unread) {
				summary_mark_row_as_unread(summaryview, row);
				summary_status_show(summaryview);
			} 
			return;
		}
	}
	
	if (marked_unread) {
		summary_mark_row_as_unread(summaryview, row);
		summary_status_show(summaryview);
	} 

	summary_set_menu_sensitive(summaryview);
	toolbar_main_set_sensitive(summaryview->mainwin);
}

static void summary_col_resized(GtkCList *clist, gint column, gint width,
				SummaryView *summaryview)
{
	SummaryColumnType type = summaryview->col_state[column].type;

	prefs_common.summary_col_size[type] = width;
}


/*
 * \brief get List of msginfo selected in SummaryView
 *
 * \param summaryview
 *
 * \return GSList holding MsgInfo
 */
GSList *summary_get_selection(SummaryView *summaryview)
{
	GList *sel = NULL;
	GSList *msginfo_list = NULL;
	
	g_return_val_if_fail(summaryview != NULL, NULL);

	sel = GTK_CLIST(summaryview->ctree)->selection;

	g_return_val_if_fail(sel != NULL, NULL);

	for ( ; sel != NULL; sel = sel->next)
		msginfo_list = 
			g_slist_append(msginfo_list, 
				       gtk_ctree_node_get_row_data(GTK_CTREE(summaryview->ctree),
								   GTK_CTREE_NODE(sel->data)));
	return msginfo_list;
}

static void summary_reply_cb(SummaryView *summaryview, guint action,
			     GtkWidget *widget)
{
	MessageView *msgview = (MessageView*)summaryview->messageview;
	GSList *msginfo_list;

	g_return_if_fail(msgview != NULL);

	msginfo_list = summary_get_selection(summaryview);
	g_return_if_fail(msginfo_list != NULL);
	compose_reply_from_messageview(msgview, msginfo_list, action);
	g_slist_free(msginfo_list);
}

static void summary_show_all_header_cb(SummaryView *summaryview,
				       guint action, GtkWidget *widget)
{
	summaryview->messageview->all_headers =
			GTK_CHECK_MENU_ITEM(widget)->active;
	summary_display_msg_selected(summaryview,
				     GTK_CHECK_MENU_ITEM(widget)->active);
}

static void summary_add_address_cb(SummaryView *summaryview,
				   guint action, GtkWidget *widget)
{
	summary_add_address(summaryview);
}

static void summary_create_filter_cb(SummaryView *summaryview,
				     guint action, GtkWidget *widget)
{
	summary_filter_open(summaryview, (PrefsFilterType)action, 0);
}

static void summary_create_processing_cb(SummaryView *summaryview,
					 guint action, GtkWidget *widget)
{
	summary_filter_open(summaryview, (PrefsFilterType)action, 1);
}

static void summary_sort_by_column_click(SummaryView *summaryview,
					 FolderSortKey sort_key)
{
	GtkCTreeNode *node = NULL;
	START_TIMING("summary_sort_by_column_click");
	if (summaryview->sort_key == sort_key)
		summary_sort(summaryview, sort_key,
			     summaryview->sort_type == SORT_ASCENDING
			     ? SORT_DESCENDING : SORT_ASCENDING);
	else
		summary_sort(summaryview, sort_key, SORT_ASCENDING);

	node = GTK_CTREE_NODE(GTK_CLIST(summaryview->ctree)->row_list);

	if (prefs_common.bold_unread) {
		while (node) {
			GtkCTreeNode *next = GTK_CTREE_NODE_NEXT(node);
			if (GTK_CTREE_ROW(node)->children)
				summary_set_row_marks(summaryview, node);
			node = next;
		}
	}
	END_TIMING();
}

static void summary_mark_clicked(GtkWidget *button, SummaryView *summaryview)
{
	summary_sort_by_column_click(summaryview, SORT_BY_MARK);
}

static void summary_status_clicked(GtkWidget *button, SummaryView *summaryview)
{
	summary_sort_by_column_click(summaryview, SORT_BY_STATUS);
}

static void summary_mime_clicked(GtkWidget *button, SummaryView *summaryview)
{
	summary_sort_by_column_click(summaryview, SORT_BY_MIME);
}

static void summary_num_clicked(GtkWidget *button, SummaryView *summaryview)
{
	summary_sort_by_column_click(summaryview, SORT_BY_NUMBER);
}

static void summary_size_clicked(GtkWidget *button, SummaryView *summaryview)
{
	summary_sort_by_column_click(summaryview, SORT_BY_SIZE);
}

static void summary_date_clicked(GtkWidget *button, SummaryView *summaryview)
{
	summary_sort_by_column_click(summaryview, SORT_BY_DATE);
}

static void summary_from_clicked(GtkWidget *button, SummaryView *summaryview)
{
	summary_sort_by_column_click(summaryview, SORT_BY_FROM);
}

static void summary_to_clicked(GtkWidget *button, SummaryView *summaryview)
{
	summary_sort_by_column_click(summaryview, SORT_BY_TO);
}

static void summary_subject_clicked(GtkWidget *button,
				    SummaryView *summaryview)
{
	summary_sort_by_column_click(summaryview, SORT_BY_SUBJECT);
}

static void summary_score_clicked(GtkWidget *button,
				  SummaryView *summaryview)
{
	summary_sort_by_column_click(summaryview, SORT_BY_SCORE);
}

static void summary_locked_clicked(GtkWidget *button,
				   SummaryView *summaryview)
{
	summary_sort_by_column_click(summaryview, SORT_BY_LOCKED);
}

static void summary_start_drag(GtkWidget *widget, gint button, GdkEvent *event,
			       SummaryView *summaryview)
{
	GdkDragContext *context;

	g_return_if_fail(summaryview != NULL);
	g_return_if_fail(summaryview->folder_item != NULL);
	g_return_if_fail(summaryview->folder_item->folder != NULL);
	if (summaryview->selected == NULL) return;

	context = gtk_drag_begin(widget, summaryview->target_list,
				 GDK_ACTION_MOVE|GDK_ACTION_COPY|GDK_ACTION_DEFAULT, button, event);
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
		     cur != NULL && cur->data != NULL; cur = cur->next) {
			msginfo = gtk_ctree_node_get_row_data
				(ctree, GTK_CTREE_NODE(cur->data));
			tmp2 = procmsg_get_message_file(msginfo);
			if (!tmp2) continue;
			tmp1 = g_strconcat("file://", tmp2, NULL);
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
					       "Dummy-Summaryview", 
					       strlen("Dummy-Summaryview")+1);
	}
}


/* custom compare functions for sorting */

#define CMP_FUNC_DEF(func_name, val)					 \
static gint func_name(GtkCList *clist,					 \
		      gconstpointer ptr1, gconstpointer ptr2)		 \
{									 \
	MsgInfo *msginfo1 = ((GtkCListRow *)ptr1)->data;		 \
	MsgInfo *msginfo2 = ((GtkCListRow *)ptr2)->data;		 \
									 \
	if (!msginfo1 || !msginfo2)					 \
		return -1;						 \
									 \
	return (val);							 \
}

CMP_FUNC_DEF(summary_cmp_by_mark,
	     MSG_IS_MARKED(msginfo1->flags) - MSG_IS_MARKED(msginfo2->flags))
CMP_FUNC_DEF(summary_cmp_by_status,
	     MSG_IS_UNREAD(msginfo1->flags) - MSG_IS_UNREAD(msginfo2->flags))
CMP_FUNC_DEF(summary_cmp_by_mime,
	     MSG_IS_WITH_ATTACHMENT(msginfo1->flags) - MSG_IS_WITH_ATTACHMENT(msginfo2->flags))
CMP_FUNC_DEF(summary_cmp_by_label,
	     MSG_GET_COLORLABEL(msginfo1->flags) -
	     MSG_GET_COLORLABEL(msginfo2->flags))
CMP_FUNC_DEF(summary_cmp_by_locked,
	     MSG_IS_LOCKED(msginfo1->flags) - MSG_IS_LOCKED(msginfo2->flags))

CMP_FUNC_DEF(summary_cmp_by_num, msginfo1->msgnum - msginfo2->msgnum)
CMP_FUNC_DEF(summary_cmp_by_size, msginfo1->size - msginfo2->size)
CMP_FUNC_DEF(summary_cmp_by_date, msginfo1->date_t - msginfo2->date_t)

#undef CMP_FUNC_DEF

static gint summary_cmp_by_subject(GtkCList *clist,
				   gconstpointer ptr1,
				   gconstpointer ptr2)
{
	MsgInfo *msginfo1 = ((GtkCListRow *)ptr1)->data;
	MsgInfo *msginfo2 = ((GtkCListRow *)ptr2)->data;

	if (!msginfo1->subject)
		return (msginfo2->subject != NULL);
	if (!msginfo2->subject)
		return -1;

	return subject_compare_for_sort
		(msginfo1->subject, msginfo2->subject);
}

static gint summary_cmp_by_from(GtkCList *clist, gconstpointer ptr1,
				gconstpointer ptr2)
{
	const gchar *str1, *str2;
	const GtkCListRow *r1 = (const GtkCListRow *) ptr1;
	const GtkCListRow *r2 = (const GtkCListRow *) ptr2;
	const SummaryView *sv = g_object_get_data(G_OBJECT(clist), "summaryview");
	
	g_return_val_if_fail(sv, -1);
	
	str1 = GTK_CELL_TEXT(r1->cell[sv->col_pos[S_COL_FROM]])->text;
	str2 = GTK_CELL_TEXT(r2->cell[sv->col_pos[S_COL_FROM]])->text;

	if (!str1)
		return str2 != NULL;
 
	if (!str2)
 		return -1;
 
	return g_utf8_collate(str1, str2);
}
 
static gint summary_cmp_by_to(GtkCList *clist, gconstpointer ptr1,
				gconstpointer ptr2)
{
	const gchar *str1, *str2;
	const GtkCListRow *r1 = (const GtkCListRow *) ptr1;
	const GtkCListRow *r2 = (const GtkCListRow *) ptr2;
	const SummaryView *sv = g_object_get_data(G_OBJECT(clist), "summaryview");
	
	g_return_val_if_fail(sv, -1);
	
	str1 = GTK_CELL_TEXT(r1->cell[sv->col_pos[S_COL_TO]])->text;
	str2 = GTK_CELL_TEXT(r2->cell[sv->col_pos[S_COL_TO]])->text;

	if (!str1)
		return str2 != NULL;
 
	if (!str2)
 		return -1;
 
	return g_utf8_collate(str1, str2);
}
 
static gint summary_cmp_by_simplified_subject
	(GtkCList *clist, gconstpointer ptr1, gconstpointer ptr2)
{
	const FolderItemPrefs *prefs;
	const gchar *str1, *str2;
	const GtkCListRow *r1 = (const GtkCListRow *) ptr1;
	const GtkCListRow *r2 = (const GtkCListRow *) ptr2;
	const MsgInfo *msginfo1 = r1->data;
	const MsgInfo *msginfo2 = r2->data;
	const SummaryView *sv = g_object_get_data(G_OBJECT(clist), "summaryview");
	
	g_return_val_if_fail(sv, -1);
	g_return_val_if_fail(msginfo1 != NULL && msginfo2 != NULL, -1);
	
	str1 = GTK_CELL_TEXT(r1->cell[sv->col_pos[S_COL_SUBJECT]])->text;
	str2 = GTK_CELL_TEXT(r2->cell[sv->col_pos[S_COL_SUBJECT]])->text;

	if (!str1)
		return str2 != NULL;

	if (!str2)
		return -1;

	prefs = msginfo1->folder->prefs;
	if (!prefs)
		prefs = msginfo2->folder->prefs;
	if (!prefs)
		return -1;
	
	return subject_compare_for_sort(str1, str2);
}

static gint summary_cmp_by_score(GtkCList *clist,
				 gconstpointer ptr1, gconstpointer ptr2)
{
	MsgInfo *msginfo1 = ((GtkCListRow *)ptr1)->data;
	MsgInfo *msginfo2 = ((GtkCListRow *)ptr2)->data;
	int diff;

	/* if score are equal, sort by date */

	diff = msginfo1->score - msginfo2->score;
	if (diff != 0)
		return diff;
	else
		return summary_cmp_by_date(clist, ptr1, ptr2);
}

static void news_flag_crosspost(MsgInfo *msginfo)
{
	GString *line;
	gpointer key;
	gpointer value;
	Folder *mff;

	g_return_if_fail(msginfo != NULL);
	g_return_if_fail(msginfo->folder != NULL);
	g_return_if_fail(msginfo->folder->folder != NULL);
	mff = msginfo->folder->folder;
	g_return_if_fail(mff->klass->type == F_NEWS);

	if (mff->account->mark_crosspost_read) {
		line = g_string_sized_new(128);
		g_string_printf(line, "%s:%d", msginfo->folder->path, msginfo->msgnum);
		debug_print("nfcp: checking <%s>", line->str);
		if (mff->newsart && 
		    g_hash_table_lookup_extended(mff->newsart, line->str, &key, &value)) {
			debug_print(" <%s>", (gchar *)value);
			if (MSG_IS_NEW(msginfo->flags) || MSG_IS_UNREAD(msginfo->flags)) {
				summary_msginfo_change_flags(msginfo, 
					mff->account->crosspost_col, 0, MSG_NEW | MSG_UNREAD, 0);
			}
			g_hash_table_remove(mff->newsart, key);
			g_free(key);
		}
		g_string_free(line, TRUE);
		debug_print("\n");
	}
}

static void summary_ignore_thread_func(GtkCTree *ctree, GtkCTreeNode *row, gpointer data)
{
	SummaryView *summaryview = (SummaryView *) data;
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);

	summary_msginfo_change_flags(msginfo, MSG_IGNORE_THREAD, 0, MSG_NEW | MSG_UNREAD, 0);

	summary_set_row_marks(summaryview, row);
	debug_print("Message %d is marked as ignore thread\n",
	    msginfo->msgnum);
}

static void summary_ignore_thread(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	START_LONG_OPERATION(summaryview);
	for (cur = GTK_CLIST(ctree)->selection; cur != NULL && cur->data != NULL; cur = cur->next)
		gtk_ctree_pre_recursive(ctree, GTK_CTREE_NODE(cur->data), 
					GTK_CTREE_FUNC(summary_ignore_thread_func), 
					summaryview);

	END_LONG_OPERATION(summaryview);

	summary_status_show(summaryview);
}

static void summary_unignore_thread_func(GtkCTree *ctree, GtkCTreeNode *row, gpointer data)
{
	SummaryView *summaryview = (SummaryView *) data;
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);

	summary_msginfo_unset_flags(msginfo, MSG_IGNORE_THREAD, 0);

	summary_set_row_marks(summaryview, row);
	debug_print("Message %d is marked as unignore thread\n",
	    msginfo->msgnum);
}

static void summary_unignore_thread(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	START_LONG_OPERATION(summaryview);
	for (cur = GTK_CLIST(ctree)->selection; cur != NULL && cur->data != NULL; cur = cur->next)
		gtk_ctree_pre_recursive(ctree, GTK_CTREE_NODE(cur->data), 
					GTK_CTREE_FUNC(summary_unignore_thread_func), 
					summaryview);

	END_LONG_OPERATION(summaryview);

	summary_status_show(summaryview);
}

static void summary_check_ignore_thread_func
		(GtkCTree *ctree, GtkCTreeNode *row, gpointer data)
{
	MsgInfo *msginfo;
	gint *found_ignore = (gint *) data;

	if (*found_ignore) return;
	else {
		msginfo = gtk_ctree_node_get_row_data(ctree, row);
		*found_ignore = MSG_IS_IGNORE_THREAD(msginfo->flags);
	}		
}

void summary_toggle_ignore_thread(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;
	gint found_ignore = 0;

	for (cur = GTK_CLIST(ctree)->selection; cur != NULL && cur->data != NULL; cur = cur->next)
		gtk_ctree_pre_recursive(ctree, GTK_CTREE_NODE(cur->data),
					GTK_CTREE_FUNC(summary_check_ignore_thread_func),
					&found_ignore);

	if (found_ignore) 
		summary_unignore_thread(summaryview);
	else 
		summary_ignore_thread(summaryview);
}

#if 0 /* OLD PROCESSING */
static gboolean processing_apply_func(GNode *node, gpointer data)
{
	FolderItem *item;
	GSList * processing;
	SummaryView * summaryview = (SummaryView *) data;
	
	if (node == NULL)
		return FALSE;

	item = node->data;
	/* prevent from the warning */
	if (item->path == NULL)
		return FALSE;
	processing = item->prefs->processing;


	if (processing != NULL) {
		gchar * buf;
		GSList * mlist;
		GSList * cur;

		buf = g_strdup_printf(_("Processing (%s)..."), item->path);
		debug_print(buf);
		STATUSBAR_PUSH(summaryview->mainwin, buf);
		g_free(buf);

		mlist = folder_item_get_msg_list(item);
		for(cur = mlist ; cur != NULL && cur->data != NULL ; cur = cur->next) {
			MsgInfo * msginfo;
			
			msginfo = (MsgInfo *) cur->data;
			filter_message_by_msginfo(processing, msginfo, NULL);
			procmsg_msginfo_free(msginfo);
		}

		g_slist_free(mlist);
		
		STATUSBAR_POP(summaryview->mainwin);
	}


	return FALSE;
}

void processing_apply(SummaryView * summaryview)
{
	GList * cur;

	for (cur = folder_get_list() ; cur != NULL && cur->data != NULL ; cur = g_list_next(cur)) {
		Folder *folder;

		folder = (Folder *) cur->data;
		g_node_traverse(folder->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				processing_apply_func, summaryview);
	}
}
#endif

void summary_toggle_show_read_messages(SummaryView *summaryview)
{
	FolderItemUpdateData source;
	if (summaryview->folder_item->hide_read_msgs)
 		summaryview->folder_item->hide_read_msgs = 0;
 	else
 		summaryview->folder_item->hide_read_msgs = 1;

	source.item = summaryview->folder_item;
	source.update_flags = F_ITEM_UPDATE_NAME;
	hooks_invoke(FOLDER_ITEM_UPDATE_HOOKLIST, &source);
 	summary_show(summaryview, summaryview->folder_item);
}
 
static void summary_set_hide_read_msgs_menu (SummaryView *summaryview,
 					     guint action)
{
 	GtkWidget *widget;
 
 	widget = gtk_item_factory_get_item(gtk_item_factory_from_widget(summaryview->mainwin->menubar),
 					   "/View/Hide read messages");
 	g_object_set_data(G_OBJECT(widget), "dont_toggle",
 			  GINT_TO_POINTER(1));
 	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(widget), action);
 	g_object_set_data(G_OBJECT(widget), "dont_toggle",
 			  GINT_TO_POINTER(0));
}

void summary_reflect_prefs_pixmap_theme(SummaryView *summaryview)
{
	GtkWidget *ctree = summaryview->ctree;
	GtkWidget *pixmap; 

	gtk_widget_destroy(summaryview->folder_pixmap);

	stock_pixmap_gdk(ctree, STOCK_PIXMAP_MARK, &markxpm, &markxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_DELETED, &deletedxpm, &deletedxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_NEW, &newxpm, &newxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_UNREAD, &unreadxpm, &unreadxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_REPLIED, &repliedxpm, &repliedxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_FORWARDED, &forwardedxpm, &forwardedxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_CLIP, &clipxpm, &clipxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_LOCKED, &lockedxpm, &lockedxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_IGNORETHREAD, &ignorethreadxpm, &ignorethreadxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_CLIP_KEY, &clipkeyxpm, &clipkeyxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_KEY, &keyxpm, &keyxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_GPG_SIGNED, &gpgsignedxpm, &gpgsignedxpmmask);
	stock_pixmap_gdk(ctree, STOCK_PIXMAP_CLIP_GPG_SIGNED, &clipgpgsignedxpm, &clipgpgsignedxpmmask);

	pixmap = stock_pixmap_widget(summaryview->hbox, STOCK_PIXMAP_DIR_OPEN);
	gtk_box_pack_start(GTK_BOX(summaryview->hbox), pixmap, FALSE, FALSE, 4);
	gtk_box_reorder_child(GTK_BOX(summaryview->hbox), pixmap, 1); /* search_toggle before */
	gtk_widget_show(pixmap);
	summaryview->folder_pixmap = pixmap; 

	pixmap = stock_pixmap_widget(summaryview->hbox, STOCK_PIXMAP_QUICKSEARCH);
	gtk_container_remove (GTK_CONTAINER(summaryview->toggle_search), 
			      summaryview->quick_search_pixmap);
	gtk_container_add(GTK_CONTAINER(summaryview->toggle_search), pixmap);
	gtk_widget_show(pixmap);
	summaryview->quick_search_pixmap = pixmap;

	folderview_unselect(summaryview->folderview);
	folderview_select(summaryview->folderview, summaryview->folder_item);
	summary_set_column_titles(summaryview);
}

/*
 * Harvest addresses for selected messages in summary view.
 */
void summary_harvest_address(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE( summaryview->ctree );
	GList *cur;
	GList *msgList;
	MsgInfo *msginfo;

	msgList = NULL;
	for( cur = GTK_CLIST(ctree)->selection; cur != NULL && cur->data != NULL; cur = cur->next ) {
		msginfo = gtk_ctree_node_get_row_data( ctree, GTK_CTREE_NODE(cur->data) );
		msgList = g_list_append( msgList, GUINT_TO_POINTER( msginfo->msgnum ) );
	}
	addressbook_harvest( summaryview->folder_item, TRUE, msgList );
	g_list_free( msgList );
}

static regex_t *summary_compile_simplify_regexp(gchar *simplify_subject_regexp)
{
	int err;
	gchar buf[BUFFSIZE];
	regex_t *preg = NULL;
	
	preg = g_new0(regex_t, 1);

	err = string_match_precompile(simplify_subject_regexp, 
				      preg, REG_EXTENDED);
	if (err) {
		regerror(err, preg, buf, BUFFSIZE);
		alertpanel_error(_("Regular expression (regexp) error:\n%s"), buf);
		g_free(preg);
		preg = NULL;
	}
	
	return preg;
}

void summary_set_prefs_from_folderitem(SummaryView *summaryview, FolderItem *item)
{
	g_return_if_fail(summaryview != NULL);
	g_return_if_fail(item != NULL);

	/* Subject simplification */
	if(summaryview->simplify_subject_preg) {
		regfree(summaryview->simplify_subject_preg);
		g_free(summaryview->simplify_subject_preg);
		summaryview->simplify_subject_preg = NULL;
	}
	if(item->prefs && item->prefs->simplify_subject_regexp && 
	   item->prefs->simplify_subject_regexp[0] && item->prefs->enable_simplify_subject)
		summaryview->simplify_subject_preg = summary_compile_simplify_regexp(item->prefs->simplify_subject_regexp);

	/* Sorting */
	summaryview->sort_key = item->sort_key;
	summaryview->sort_type = item->sort_type;

	/* Threading */
	summaryview->threaded = item->threaded;
	summaryview->thread_collapsed = item->thread_collapsed;

	/* Scoring */
#if 0
	if (global_scoring || item->prefs->scoring) {
		summaryview->important_score = prefs_common.important_score;
		if (item->prefs->important_score >
		    summaryview->important_score)
			summaryview->important_score =
				item->prefs->important_score;
	}
#endif
}

void summary_save_prefs_to_folderitem(SummaryView *summaryview, FolderItem *item)
{
	/* Sorting */
	item->sort_key = summaryview->sort_key;
	item->sort_type = summaryview->sort_type;

	/* Threading */
	item->threaded = summaryview->threaded;
	item->thread_collapsed = summaryview->thread_collapsed;
}

static gboolean summary_update_msg(gpointer source, gpointer data) 
{
	MsgInfoUpdate *msginfo_update = (MsgInfoUpdate *) source;
	SummaryView *summaryview = (SummaryView *)data;
	GtkCTreeNode *node;

	g_return_val_if_fail(msginfo_update != NULL, TRUE);
	g_return_val_if_fail(summaryview != NULL, FALSE);

	if (msginfo_update->flags & MSGINFO_UPDATE_FLAGS) {
		node = gtk_ctree_find_by_row_data(
				GTK_CTREE(summaryview->ctree), NULL, 
				msginfo_update->msginfo);

		if (node) 
			summary_set_row_marks(summaryview, node);
	}

	return FALSE;
}

/*!
 *\brief	change summaryview to display your answer(s) to a message
 *
 *\param	summaryview The SummaryView ;)
 *\param	msginfo The message for which answers are searched
 *
 */
static void summary_find_answers (SummaryView *summaryview, MsgInfo *msg)
{
	FolderItem *sent_folder = NULL;
	PrefsAccount *account = NULL;
	GtkCTreeNode *node = NULL;
	char *buf = NULL;
	if (msg == NULL || msg->msgid == NULL)
		return;
	
	account = account_get_reply_account(msg, prefs_common.reply_account_autosel);
	if (account == NULL) 
		return;
	sent_folder = account_get_special_folder
				(account, F_OUTBOX);
	
	buf = g_strdup_printf("inreplyto matchcase \"%s\"", msg->msgid);

	if (sent_folder != summaryview->folder_item) {
		folderview_select(summaryview->mainwin->folderview, sent_folder);
	}
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(summaryview->toggle_search), TRUE);

	quicksearch_set(summaryview->quicksearch, QUICK_SEARCH_EXTENDED, buf);
	g_free(buf);

	node = gtk_ctree_node_nth(GTK_CTREE(summaryview->ctree), 0);
	if (node)
		summary_select_node(summaryview, node, TRUE, TRUE);
}

void summaryview_export_mbox_list(SummaryView *summaryview)
{
	GSList *list = summary_get_selected_msg_list(summaryview);
	gchar *mbox = filesel_select_file_save(_("Export to mbox file"), NULL);
	
	if (mbox == NULL || list == NULL)
		return;
		
	export_list_to_mbox(list, mbox);
	
	g_slist_free(list);
	g_free(mbox);
	
}

void summaryview_lock(SummaryView *summaryview, FolderItem *item)
{
	if (!summaryview || !summaryview->folder_item || !item) {
		return;
	}

	if (summaryview->folder_item->folder == item->folder) {
		gtk_widget_set_sensitive(summaryview->ctree, FALSE);
	}
}
void summaryview_unlock(SummaryView *summaryview, FolderItem *item)
{
	gtk_widget_set_sensitive(summaryview->ctree, TRUE);
}
