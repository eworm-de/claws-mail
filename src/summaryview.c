/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto
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
#include <regex.h>

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
#include "scoring.h"
#include "prefs_folder_item.h"
#include "filtering.h"
#include "string_match.h"
#include "toolbar.h"
#include "news.h"

#define SUMMARY_COL_MARK_WIDTH		10
#define SUMMARY_COL_UNREAD_WIDTH	13
#define SUMMARY_COL_LOCKED_WIDTH	13
#define SUMMARY_COL_MIME_WIDTH		11

static GdkFont *boldfont;
static GdkFont *smallfont;

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

static void summary_thread_init		(SummaryView		*summaryview);
static void summary_ignore_thread	(SummaryView  		*summaryview);
static void summary_unignore_thread	(SummaryView 		*summaryview);

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
static void summary_searchbar_pressed	(GtkWidget		*ctree,
					 GdkEventKey		*event,
					 SummaryView		*summaryview);
static void summary_searchtype_changed	(GtkMenuItem 		*widget, 
					 gpointer 		 data);
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
static void summary_execute_cb		(SummaryView		*summaryview,
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

static void summary_mark_clicked	(GtkWidget		*button,
					 SummaryView		*summaryview);
static void summary_unread_clicked	(GtkWidget		*button,
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
static gint summary_cmp_by_unread	(GtkCList		*clist,
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
static gint summary_cmp_by_to		(GtkCList		*clist,
					 gconstpointer		 ptr1, 
					 gconstpointer		 ptr2);
static gint summary_cmp_by_subject	(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);
static gint summary_cmp_by_simplified_subject
	(GtkCList *clist, gconstpointer ptr1, gconstpointer ptr2);
static gint summary_cmp_by_score	(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);
static gint summary_cmp_by_locked	(GtkCList *clist,
				         gconstpointer ptr1, gconstpointer ptr2);
static gint summary_cmp_by_label	(GtkCList		*clist,
					 gconstpointer		 ptr1,
					 gconstpointer		 ptr2);

static void news_flag_crosspost		(MsgInfo *msginfo);

static void tog_searchbar_cb		(GtkWidget	*w,
					 gpointer	 data);


GtkTargetEntry summary_drag_types[1] =
{
	{"text/plain", GTK_TARGET_SAME_APP, TARGET_DUMMY}
};

static GtkItemFactoryEntry summary_popup_entries[] =
{
	{N_("/_Reply"),			NULL, summary_reply_cb,	COMPOSE_REPLY, NULL},
	{N_("/Repl_y to"),		NULL, NULL,		0, "<Branch>"},
	{N_("/Repl_y to/_all"),		NULL, summary_reply_cb,	COMPOSE_REPLY_TO_ALL, NULL},
	{N_("/Repl_y to/_sender"),	NULL, summary_reply_cb,	COMPOSE_REPLY_TO_SENDER, NULL},
	{N_("/Repl_y to/mailing _list"),
					NULL, summary_reply_cb,	COMPOSE_REPLY_TO_LIST, NULL},
	{N_("/Follow-up and reply to"),	NULL, summary_reply_cb,	COMPOSE_FOLLOWUP_AND_REPLY_TO, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/_Forward"),		NULL, summary_reply_cb, COMPOSE_FORWARD, NULL},
	{N_("/Redirect"),	        NULL, summary_reply_cb, COMPOSE_REDIRECT, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/Re-_edit"),		NULL, summary_reedit,   0, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/M_ove..."),		NULL, summary_move_to,	0, NULL},
	{N_("/_Copy..."),		NULL, summary_copy_to,	0, NULL},
	{N_("/_Delete"),		NULL, summary_delete,	0, NULL},
	{N_("/Cancel a news message"),	NULL, summary_cancel,	0, NULL},
	{N_("/E_xecute"),		NULL, summary_execute_cb,	0, NULL},
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
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/_View"),			NULL, NULL,		0, "<Branch>"},
	{N_("/_View/Open in new _window"),
					NULL, summary_open_msg,	0, NULL},
	{N_("/_View/_Source"),		NULL, summary_view_source, 0, NULL},
	{N_("/_View/All _header"),	NULL, summary_show_all_header_cb, 0, "<ToggleItem>"},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/_Save as..."),		NULL, summary_save_as,	0, NULL},
	{N_("/_Print..."),		NULL, summary_print,	0, NULL},
	{N_("/---"),			NULL, NULL,		0, "<Separator>"},
	{N_("/Select _all"),		NULL, summary_select_all, 0, NULL},
	{N_("/Select t_hread"),		NULL, summary_select_thread, 0, NULL}
};

static const gchar *const col_label[N_SUMMARY_COLS] = {
	N_("M"),	/* S_COL_MARK    */
	N_("U"),	/* S_COL_UNREAD  */
	"",		/* S_COL_MIME    */
	N_("Subject"),	/* S_COL_SUBJECT */
	N_("From"),	/* S_COL_FROM    */
	N_("Date"),	/* S_COL_DATE    */
	N_("Size"),	/* S_COL_SIZE    */
	N_("No."),	/* S_COL_NUMBER  */
	N_("Score"),	/* S_COL_SCORE   */
	N_("L")		/* S_COL_LOCKED	 */
};

SummaryView *summary_create(void)
{
	SummaryView *summaryview;
	GtkWidget *vbox;
	GtkWidget *scrolledwin;
	GtkWidget *ctree;
	GtkWidget *hbox;
	GtkWidget *hbox_l;
	GtkWidget *hbox_search;
	GtkWidget *statlabel_folder;
	GtkWidget *statlabel_select;
	GtkWidget *statlabel_msgs;
	GtkWidget *hbox_spc;
	GtkWidget *toggle_eventbox;
	GtkWidget *toggle_arrow;
	GtkWidget *popupmenu;
	GtkWidget *search_type_opt;
	GtkWidget *search_type;
	GtkWidget *search_string;
	GtkWidget *menuitem;
	GtkWidget *toggle_search;
	GtkTooltips *search_tip;
	GtkItemFactory *popupfactory;
	gint n_entries;

	debug_print("Creating summary view...\n");
	summaryview = g_new0(SummaryView, 1);

	vbox = gtk_vbox_new(FALSE, 3);
	
	/* create status label */
	hbox = gtk_hbox_new(FALSE, 0);
	
	search_tip = gtk_tooltips_new();
	toggle_search = gtk_toggle_button_new();

	gtk_tooltips_set_tip(GTK_TOOLTIPS(search_tip),
			     toggle_search,
			     _("Toggle quick-search bar"), NULL);
	
	gtk_box_pack_start(GTK_BOX(hbox), toggle_search, FALSE, FALSE, 2);	

	hbox_l = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), hbox_l, TRUE, TRUE, 0);
 
	statlabel_folder = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox_l), statlabel_folder, FALSE, FALSE, 2);
	statlabel_select = gtk_label_new("");
	gtk_box_pack_start(GTK_BOX(hbox_l), statlabel_select, FALSE, FALSE, 12);
 
	/* toggle view button */
	toggle_eventbox = gtk_event_box_new();
	gtk_box_pack_end(GTK_BOX(hbox), toggle_eventbox, FALSE, FALSE, 4);
	toggle_arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_container_add(GTK_CONTAINER(toggle_eventbox), toggle_arrow);
	gtk_signal_connect(GTK_OBJECT(toggle_eventbox), "button_press_event",
			   GTK_SIGNAL_FUNC(summary_toggle_pressed),
			   summaryview);
	
	
	statlabel_msgs = gtk_label_new("");
	gtk_box_pack_end(GTK_BOX(hbox), statlabel_msgs, FALSE, FALSE, 4);

	hbox_spc = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), hbox_spc, FALSE, FALSE, 6);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(vbox), scrolledwin, TRUE, TRUE, 0);
	gtk_widget_set_usize(vbox,
			     prefs_common.summaryview_width,
			     prefs_common.summaryview_height);

	ctree = summary_ctree_create(summaryview);

	gtk_scrolled_window_set_hadjustment(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_CLIST(ctree)->hadjustment);
	gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_CLIST(ctree)->vadjustment);
	gtk_container_add(GTK_CONTAINER(scrolledwin), ctree);

	/* status label */
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	/* quick search */
	hbox_search = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_search, FALSE, FALSE, 0);

	search_type_opt = gtk_option_menu_new();
	gtk_widget_show(search_type_opt);
	gtk_box_pack_start(GTK_BOX(hbox_search), search_type_opt, FALSE, FALSE, 0);

	search_type = gtk_menu_new();
	MENUITEM_ADD (search_type, menuitem, _("Subject"), S_SEARCH_SUBJECT);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(summary_searchtype_changed),
			   summaryview);
	MENUITEM_ADD (search_type, menuitem, _("From"), S_SEARCH_FROM);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(summary_searchtype_changed),
			   summaryview);
	MENUITEM_ADD (search_type, menuitem, _("To"), S_SEARCH_TO);
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
			   GTK_SIGNAL_FUNC(summary_searchtype_changed),
			   summaryview);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(search_type_opt), search_type);
	gtk_widget_show(search_type);
	
	search_string = gtk_entry_new();
	
	gtk_box_pack_start(GTK_BOX(hbox_search), search_string, FALSE, FALSE, 2);
	
	gtk_widget_show(search_string);
	gtk_widget_show(hbox_search);

	gtk_signal_connect(GTK_OBJECT(search_string), "key_press_event",
			   GTK_SIGNAL_FUNC(summary_searchbar_pressed),
			   summaryview);

  	gtk_signal_connect (GTK_OBJECT(toggle_search), "toggled",
                        GTK_SIGNAL_FUNC(tog_searchbar_cb), hbox_search);

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
	summaryview->hbox_search = hbox_search;
	summaryview->statlabel_folder = statlabel_folder;
	summaryview->statlabel_select = statlabel_select;
	summaryview->statlabel_msgs = statlabel_msgs;
	summaryview->toggle_eventbox = toggle_eventbox;
	summaryview->toggle_arrow = toggle_arrow;
	summaryview->toggle_search = toggle_search;
	summaryview->popupmenu = popupmenu;
	summaryview->popupfactory = popupfactory;
	summaryview->lock_count = 0;
	summaryview->search_type_opt = search_type_opt;
	summaryview->search_type = search_type;
	summaryview->search_string = search_string;

	/* CLAWS: need this to get the SummaryView * from
	 * the CList */
	gtk_object_set_data(GTK_OBJECT(ctree), "summaryview", (gpointer)summaryview); 

	gtk_widget_show_all(vbox);

	return summaryview;
}

void summary_init(SummaryView *summaryview)
{
	GtkStyle *style;
	GtkWidget *pixmap;

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

	if (!small_style) {
		small_style = gtk_style_copy
			(gtk_widget_get_style(summaryview->ctree));
		if (!smallfont)
			smallfont = gtkut_font_load(SMALL_FONT);
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
			boldfont = gtkut_font_load(BOLD_FONT);
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

#define CURRENTLY_DISPLAYED(m) \
( (m->msgnum == displayed_msgnum) \
  && (!g_strcasecmp(m->folder->name,item->name)) )

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

	if (summary_is_locked(summaryview)) return FALSE;

	inc_lock();
	summary_lock(summaryview);

	if (item != summaryview->folder_item) {
		/* changing folder, reset search */
		gtk_entry_set_text(GTK_ENTRY(summaryview->search_string), "");
	}
	
	STATUSBAR_POP(summaryview->mainwin);

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

		val = alertpanel(_("Process mark"),
				 _("Some marks are left. Process it?"),
				 _("Yes"), _("No"), _("Cancel"));
		if (G_ALERTDEFAULT == val) {
			summary_unlock(summaryview);
			summary_execute(summaryview);
			summary_lock(summaryview);
		} else if (G_ALERTALTERNATE == val) {
			/* DO NOTHING */
		} else {
			summary_unlock(summaryview);
			inc_unlock();
			return FALSE;
		}
   		folder_update_op_count();
	}
	
	gtk_clist_freeze(GTK_CLIST(ctree));

	summary_clear_list(summaryview);
	summary_set_column_titles(summaryview);
	if (!is_refresh)
		messageview_clear(summaryview->messageview);

	buf = NULL;
	if (!item || !item->path || !item->parent || item->no_select ||
	    (item->folder->type == F_MH &&
	     ((buf = folder_item_get_path(item)) == NULL ||
	      change_dir(buf) < 0))) {
		g_free(buf);
		debug_print("empty folder\n\n");
		summary_set_hide_read_msgs_menu(summaryview, FALSE);
		if (is_refresh)
			messageview_clear(summaryview->messageview);
		summary_clear_all(summaryview);
		summaryview->folder_item = item;
		gtk_clist_thaw(GTK_CLIST(ctree));
		summary_unlock(summaryview);
		inc_unlock();
		return TRUE;
	}
	g_free(buf);

	summaryview->folder_item = item;
	item->opened = TRUE;

	gtk_signal_handler_block_by_data(GTK_OBJECT(ctree), summaryview);

	buf = g_strdup_printf(_("Scanning folder (%s)..."), item->path);
	debug_print("%s\n", buf);
	STATUSBAR_PUSH(summaryview->mainwin, buf);
	g_free(buf);

	main_window_cursor_wait(summaryview->mainwin);

/*
	mlist = item->folder->get_msg_list(item->folder, item, !update_cache);

	!!! NEW !!!
	USE LIST FROM CACHE, WILL NOT DISPLAY ANY MESSAGES DROPED
	BY OTHER PROGRAMS TO THE FOLDER
*/
	mlist = folder_item_get_msg_list(item);
#if 0
	summary_processing(summaryview, mlist);
#endif
	for(cur = mlist ; cur != NULL ; cur = g_slist_next(cur)) {
		MsgInfo * msginfo = (MsgInfo *) cur->data;

		msginfo->score = score_message(global_scoring, msginfo);
		if (msginfo->score != MAX_SCORE &&
		    msginfo->score != MIN_SCORE) {
			msginfo->score += score_message(item->prefs->scoring,
							msginfo);
		}
	}

	if (summaryview->folder_item->hide_read_msgs) {
		GSList *not_killed;
		
		summary_set_hide_read_msgs_menu(summaryview, TRUE);
		not_killed = NULL;
		for(cur = mlist ; cur != NULL ; cur = g_slist_next(cur)) {
			MsgInfo * msginfo = (MsgInfo *) cur->data;
			
			if ((MSG_IS_UNREAD(msginfo->flags)
			     || MSG_IS_MARKED(msginfo->flags)
			     || MSG_IS_LOCKED(msginfo->flags)
			     || CURRENTLY_DISPLAYED(msginfo))
			     && !MSG_IS_IGNORE_THREAD(msginfo->flags))
			    not_killed = g_slist_append(not_killed, msginfo);
			else
				procmsg_msginfo_free(msginfo);
		}
		g_slist_free(mlist);
		mlist = not_killed;
	} else {
		summary_set_hide_read_msgs_menu(summaryview, FALSE);
	}

	if (strlen(gtk_entry_get_text(GTK_ENTRY(summaryview->search_string))) > 0) {
		GSList *not_killed;
		gint search_type = GPOINTER_TO_INT(gtk_object_get_user_data(
				   GTK_OBJECT(GTK_MENU_ITEM(gtk_menu_get_active(
				   GTK_MENU(summaryview->search_type))))));
		gchar *search_string = gtk_entry_get_text(GTK_ENTRY(summaryview->search_string));
		gchar *searched_header = NULL;
		
		not_killed = NULL;
		for (cur = mlist ; cur != NULL ; cur = g_slist_next(cur)) {
			MsgInfo * msginfo = (MsgInfo *) cur->data;

			switch (search_type) {
			case S_SEARCH_SUBJECT:
				searched_header = msginfo->subject;
				break;
			case S_SEARCH_FROM:
				searched_header = msginfo->from;
				break;
			case S_SEARCH_TO:
				searched_header = msginfo->to;
				break;
			default:
				debug_print("unknown search type (%d)\n", search_type);
				break;
			}
			if (searched_header && strcasestr(searched_header, search_string) != NULL)
				not_killed = g_slist_append(not_killed, msginfo);
			else
				procmsg_msginfo_free(msginfo);
		}
		g_slist_free(mlist);
		mlist = not_killed;
	}
	
	if ((global_scoring || item->prefs->scoring)) {
		GSList *not_killed;
		gint kill_score;

		not_killed = NULL;
		kill_score = prefs_common.kill_score;
		if (item->prefs->kill_score > kill_score)
			kill_score = item->prefs->kill_score;
		for(cur = mlist ; cur != NULL ; cur = g_slist_next(cur)) {
			MsgInfo * msginfo = (MsgInfo *) cur->data;

			if (msginfo->score > kill_score)
				not_killed = g_slist_append(not_killed, msginfo);
			else
				procmsg_msginfo_free(msginfo);
		}
		g_slist_free(mlist);
		mlist = not_killed;
	}

	STATUSBAR_POP(summaryview->mainwin);

	/* set ctree and hash table from the msginfo list
	   creating thread, and count the number of messages */
	summary_set_ctree_from_list(summaryview, mlist);

	g_slist_free(mlist);

	if (summaryview->sort_key != SORT_BY_NONE)
		summary_sort(summaryview, summaryview->sort_key, summaryview->sort_type);

	gtk_signal_handler_unblock_by_data(GTK_OBJECT(ctree), summaryview);

	gtk_clist_thaw(GTK_CLIST(ctree));

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
			node = summary_find_next_flagged_msg(summaryview, NULL,
							     MSG_UNREAD, FALSE);
			if (node == NULL && GTK_CLIST(ctree)->row_list != NULL)
				node = gtk_ctree_node_nth
					(ctree,
					 item->sort_type == SORT_DESCENDING
					 ? 0 : GTK_CLIST(ctree)->rows - 1);
			summary_select_node(summaryview, node, FALSE, TRUE);
		}
	} else {
		/* select first unread message */
		if (summaryview->sort_key == SORT_BY_SCORE)
			node = summary_find_next_important_score(summaryview,
								 NULL);
		else
		node = summary_find_next_flagged_msg(summaryview, NULL,
						     MSG_UNREAD, FALSE);
		if (node == NULL && GTK_CLIST(ctree)->row_list != NULL) {
			node = gtk_ctree_node_nth
				(ctree,
				 item->sort_type == SORT_DESCENDING
				 ? 0 : GTK_CLIST(ctree)->rows - 1);
		}
		if (prefs_common.open_unread_on_enter) {
			summary_unlock(summaryview);
			summary_select_node(summaryview, node, TRUE, TRUE);
			summary_lock(summaryview);
		} else
			summary_select_node(summaryview, node, FALSE, TRUE);
	}

	summary_status_show(summaryview);
	summary_set_menu_sensitive(summaryview);
	toolbar_set_sensitive(summaryview->mainwin);

	debug_print("\n");
	STATUSBAR_PUSH(summaryview->mainwin, _("Done."));

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
	summary_clear_list(summaryview);
	summary_set_menu_sensitive(summaryview);
	toolbar_set_sensitive(summaryview->mainwin);
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

static void summary_set_menu_sensitive(SummaryView *summaryview)
{
	GtkItemFactory *ifactory = summaryview->popupfactory;
	SummarySelection selection;
	GtkWidget *menuitem;
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

	if (summaryview->folder_item->folder->type != F_NEWS)
		menu_set_sensitive(ifactory, "/Move...", TRUE);
	else
		menu_set_sensitive(ifactory, "/Move...", FALSE);

#if 0
	menu_set_sensitive(ifactory, "/Delete", TRUE);
#endif
	menu_set_sensitive(ifactory, "/Copy...", TRUE);
	menu_set_sensitive(ifactory, "/Execute", TRUE);

	menu_set_sensitive(ifactory, "/Mark", TRUE);
	menu_set_sensitive(ifactory, "/Mark/Mark",   TRUE);
	menu_set_sensitive(ifactory, "/Mark/Unmark", TRUE);

	menu_set_sensitive(ifactory, "/Mark/Mark as unread", TRUE);
	menu_set_sensitive(ifactory, "/Mark/Mark as read",   TRUE);
	menu_set_sensitive(ifactory, "/Mark/Mark all read", TRUE);
	menu_set_sensitive(ifactory, "/Mark/Ignore thread",   TRUE);
	menu_set_sensitive(ifactory, "/Mark/Unignore thread", TRUE);

	menu_set_sensitive(ifactory, "/Color label", TRUE);

	sens = (selection == SUMMARY_SELECTED_MULTIPLE) ? FALSE : TRUE;
	menu_set_sensitive(ifactory, "/Reply",			  sens);
	menu_set_sensitive(ifactory, "/Reply to",		  sens);
	menu_set_sensitive(ifactory, "/Reply to/all",		  sens);
	menu_set_sensitive(ifactory, "/Reply to/sender",	  sens);
	menu_set_sensitive(ifactory, "/Reply to/mailing list",	  sens);
	menu_set_sensitive(ifactory, "/Forward",		  TRUE);
	menu_set_sensitive(ifactory, "/Redirect",		  sens);

	menu_set_sensitive(ifactory, "/Add sender to address book", sens);
	menu_set_sensitive(ifactory, "/Create filter rule",         sens);

	menu_set_sensitive(ifactory, "/View", sens);
	menu_set_sensitive(ifactory, "/View/Open in new window", sens);
	menu_set_sensitive(ifactory, "/View/Source", sens);
	menu_set_sensitive(ifactory, "/View/All header", sens);
	if (summaryview->folder_item->stype == F_OUTBOX ||
	    summaryview->folder_item->stype == F_DRAFT  ||
	    summaryview->folder_item->stype == F_QUEUE)
		menu_set_sensitive(ifactory, "/Re-edit", sens);
	else
		menu_set_sensitive(ifactory, "/Re-edit", FALSE);

	menu_set_sensitive(ifactory, "/Save as...", TRUE);
	menu_set_sensitive(ifactory, "/Print...",   TRUE);

	menu_set_sensitive(ifactory, "/Select all", TRUE);
	menu_set_sensitive(ifactory, "/Select thread", sens);
	if (summaryview->folder_item->folder->account)
		sens = summaryview->folder_item->folder->account->protocol
			== A_NNTP;
	else
		sens = FALSE;
	menu_set_sensitive(ifactory, "/Follow-up and reply to",	sens);
	menu_set_sensitive(ifactory, "/Cancel a news message", sens);
	menu_set_sensitive(ifactory, "/Delete", !sens);

	summary_lock(summaryview);
	menuitem = gtk_item_factory_get_widget(ifactory, "/View/All header");
	gtk_check_menu_item_set_active
		(GTK_CHECK_MENU_ITEM(menuitem),
		 summaryview->messageview->textview->show_all_headers);
	summary_unlock(summaryview);
}

void summary_select_prev_unread(SummaryView *summaryview)
{
	GtkCTreeNode *node;

	node = summary_find_prev_flagged_msg
		(summaryview, summaryview->selected, MSG_UNREAD, FALSE);

	if (!node) {
		AlertValue val = 0;

 		switch (prefs_common.next_unread_msg_dialog) {
 			case NEXTUNREADMSGDIALOG_ALWAYS:
				val = alertpanel(_("No more unread messages"),
						 _("No unread message found. "
						   "Search from the end?"),
						 _("Yes"), _("No"), NULL);
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
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);

	node = summary_find_next_flagged_msg
		(summaryview, node, MSG_UNREAD, FALSE);
	
	if (node)
		summary_select_node(summaryview, node, TRUE, FALSE);
	else {
		node = summary_find_next_flagged_msg
			(summaryview, NULL, MSG_UNREAD, FALSE);
		if (node == NULL) {
			AlertValue val = 0;

 			switch (prefs_common.next_unread_msg_dialog) {
 				case NEXTUNREADMSGDIALOG_ALWAYS:
					val = alertpanel(_("No more unread messages"),
							 _("No unread message found. "
							   "Go to next folder?"),
							 _("Yes"), _("No"), NULL);
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
				if (gtk_signal_n_emissions_by_name
					(GTK_OBJECT(ctree), "key_press_event") > 0)
						gtk_signal_emit_stop_by_name
							(GTK_OBJECT(ctree),
							 "key_press_event");
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

	node = summary_find_prev_flagged_msg
		(summaryview, summaryview->selected, MSG_NEW, FALSE);

	if (!node) {
		AlertValue val;

		val = alertpanel(_("No more new messages"),
				 _("No new message found. "
				   "Search from the end?"),
				 _("Yes"), _("No"), NULL);
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
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);

	while ((node = summary_find_next_flagged_msg
		(summaryview, node, MSG_NEW, FALSE)) == NULL) {
		AlertValue val;

		val = alertpanel(_("No more new messages"),
				 _("No new message found. "
				   "Go to next folder?"),
				 _("Yes"), _("Search again"), _("No"));
		if (val == G_ALERTDEFAULT) {
			if (gtk_signal_n_emissions_by_name
				(GTK_OBJECT(ctree), "key_press_event") > 0)
					gtk_signal_emit_stop_by_name
						(GTK_OBJECT(ctree),
						 "key_press_event");
			folderview_select_next_unread(summaryview->folderview);
			return;
		} else if (val == G_ALERTALTERNATE)
			node = NULL;
		else
			return;
	}

	if (node)
		summary_select_node(summaryview, node, TRUE, FALSE);
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
				 _("Yes"), _("No"), NULL);
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
				 _("Yes"), _("No"), NULL);
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
				 _("Yes"), _("No"), NULL);
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
				 _("Yes"), _("No"), NULL);
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
		gtk_sctree_unselect_all(GTK_SCTREE(ctree));
		if (display_msg && summaryview->displayed == node)
			summaryview->displayed = NULL;
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
		if (!MSG_IS_DELETED(msginfo->flags)) break;
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
		if (!MSG_IS_DELETED(msginfo->flags)) break;
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
		if ((msginfo->flags.perm_flags & flags) != 0) break;
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
			((msginfo->flags.perm_flags & flags) != 0))
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
		if (msginfo->msgnum == msgnum) break;
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

 	if (MSG_IS_NEWS(msginfo->flags))
 		news_flag_crosspost(msginfo);

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
	gchar *itstr;
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

	if (summaryview->folder_item->folder->type == F_NEWS &&
	    prefs_common.ng_abbrev_len < strlen(summaryview->folder_item->path)) {
		gchar *group;
		group = get_abbrev_newsgroup_name
			(g_basename(summaryview->folder_item->path), prefs_common.ng_abbrev_len);
		gtk_label_set(GTK_LABEL(summaryview->statlabel_folder), group);
		g_free(group);
	} else {
		gtk_label_set(GTK_LABEL(summaryview->statlabel_folder),
			      summaryview->folder_item->path);
	}

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
	gtk_label_set(GTK_LABEL(summaryview->statlabel_select), str);
	g_free(str);
	g_free(sel);
	g_free(del);
	g_free(mv);
	g_free(cp);
	g_free(itstr);

	if (FOLDER_IS_LOCAL(summaryview->folder_item->folder)) {
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
		SORT_BY_UNREAD,
		SORT_BY_MIME,
		SORT_BY_SUBJECT,
		SORT_BY_FROM,
		SORT_BY_DATE,
		SORT_BY_SIZE,
		SORT_BY_NUMBER,
		SORT_BY_SCORE,
		SORT_BY_LOCKED
	};

	for (pos = 0; pos < N_SUMMARY_COLS; pos++) {
		type = summaryview->col_state[pos].type;

		/* CLAWS: mime and unread are single char headers */
		single_char = (type == S_COL_MIME || type == S_COL_UNREAD);
		justify = (type == S_COL_NUMBER || type == S_COL_SIZE)
			? GTK_JUSTIFY_RIGHT : GTK_JUSTIFY_LEFT;

		switch (type) {
		case S_COL_SUBJECT:
		case S_COL_FROM:
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
			label = gtk_pixmap_new(clipxpm, clipxpmmask);
			gtk_widget_show(label);
			gtk_clist_set_column_widget(clist, pos, label);
			continue;
		}
		if (single_char) {
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

void summary_sort(SummaryView *summaryview,
		  FolderSortKey sort_key, FolderSortType sort_type)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCList *clist = GTK_CLIST(summaryview->ctree);
	GtkCListCompareFunc cmp_func = NULL;

	switch (sort_key) {
	case SORT_BY_MARK:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_mark;
		break;
	case SORT_BY_UNREAD:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_unread;
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
		if (summaryview->folder_item && summaryview->folder_item->stype != F_OUTBOX)
			cmp_func = (GtkCListCompareFunc) summary_cmp_by_from;
		else			
			cmp_func = (GtkCListCompareFunc) summary_cmp_by_to;
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
	case SORT_BY_LOCKED:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_locked;
		break;
	case SORT_BY_LABEL:
		cmp_func = (GtkCListCompareFunc)summary_cmp_by_label;
		break;
	case SORT_BY_NONE:
		cmp_func = NULL;
		return;
	default:
		return;
	}

	summaryview->sort_key = sort_key;
	summaryview->sort_type = sort_type;

	summary_set_column_titles(summaryview);
	summary_set_menu_sensitive(summaryview);
	if(cmp_func != NULL) {
		debug_print("Sorting summary...");
		STATUSBAR_PUSH(summaryview->mainwin, _("Sorting summary..."));

		main_window_cursor_wait(summaryview->mainwin);

		gtk_clist_set_compare_func(clist, cmp_func);

		gtk_clist_set_sort_type(clist, (GtkSortType)sort_type);

		gtk_sctree_sort_recursive(ctree, NULL);

		gtk_ctree_node_moveto(ctree, summaryview->selected, -1, 0.5, 0);

		main_window_cursor_normal(summaryview->mainwin);

		debug_print("done.\n");
		STATUSBAR_POP(summaryview->mainwin);
	}
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
	SET_TEXT(S_COL_SUBJECT);

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
	
	if (!mlist) return;

	debug_print("\tSetting summary from message data...");
	STATUSBAR_PUSH(summaryview->mainwin,
		       _("Setting summary from message data..."));
	gdk_flush();

	msgid_table = g_hash_table_new(g_str_hash, g_str_equal);
	summaryview->msgid_table = msgid_table;
	subject_table = g_hash_table_new(g_str_hash, g_str_equal);
	summaryview->subject_table = subject_table;

	if (prefs_common.use_addr_book)
		start_address_completion();
	
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

		folder_update_items_when_required(FALSE);

		summary_thread_init(summaryview);
	} else {
		gchar *text[N_SUMMARY_COLS];

		mlist = g_slist_reverse(mlist);
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
		mlist = g_slist_reverse(mlist);
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
	if (debug_mode) {
		debug_print("\tmsgid hash table size = %d\n",
			    g_hash_table_size(msgid_table));
		debug_print("\tsubject hash table size = %d\n",
			    g_hash_table_size(subject_table));
	}
}

static gchar *summary_complete_address(const gchar *addr)
{
	gint count;
	gchar *res, *tmp, *email_addr;

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
	static gchar *to = NULL;
	static gchar col_score[11];
	static gchar buf[BUFFSIZE];
	gint *col_pos = summaryview->col_pos;

	text[col_pos[S_COL_MARK]]   = NULL;
	text[col_pos[S_COL_UNREAD]] = NULL;
	text[col_pos[S_COL_MIME]]   = NULL;
	text[col_pos[S_COL_LOCKED]] = NULL;
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

	text[col_pos[S_COL_FROM]] = msginfo->fromname ? msginfo->fromname :
		_("(No From)");
	if (prefs_common.swap_from && msginfo->from && msginfo->to &&
	    !MSG_IS_NEWS(msginfo->flags)) {
		gchar *addr = NULL;

		Xstrdup_a(addr, msginfo->from, return);
		extract_address(addr);

		if (prefs_common.use_addr_book) {
			if (account_find_from_address(addr)) {
				addr = summary_complete_address(msginfo->to);
				g_free(to);
				to   = g_strconcat("-->", addr == NULL ? msginfo->to : addr, NULL);
				text[col_pos[S_COL_FROM]] = to;
				g_free(addr);
			}
		} else {
			if (cur_account && cur_account->address && !strcmp( addr, cur_account->address)) {
				g_free(to);
				to = g_strconcat("-->", msginfo->to, NULL);
				text[col_pos[S_COL_FROM]] = to;
			}
		}
	}

	/*
	 * CLAWS: note that the "text[col_pos[S_COL_FROM]] != to" is really a hack, 
	 * checking whether the above block (which handles the special case of
	 * the --> in sent boxes) was executed.
	 */
	if (text[col_pos[S_COL_FROM]] != to && prefs_common.use_addr_book && msginfo->from) {
		gchar *from = summary_complete_address(msginfo->from);
		if (from) {
			g_free(to);
			to = from;
			text[col_pos[S_COL_FROM]] = to;
		}			
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

static void summary_display_msg_full(SummaryView *summaryview,
				     GtkCTreeNode *row,
				     gboolean new_window, gboolean all_headers)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;
	MsgFlags flags;
	gchar *filename;

	if (!new_window && summaryview->displayed == row) return;
	g_return_if_fail(row != NULL);

	if (summary_is_locked(summaryview)) return;
	summary_lock(summaryview);

	STATUSBAR_POP(summaryview->mainwin);
	GTK_EVENTS_FLUSH();

	msginfo = gtk_ctree_node_get_row_data(ctree, row);

	filename = procmsg_get_message_file(msginfo);
	if (!filename) {
		summary_unlock(summaryview);
		return;
	}
	g_free(filename);

	if (new_window || !prefs_common.mark_as_read_on_new_window) {
		if (MSG_IS_NEW(msginfo->flags) && !MSG_IS_IGNORE_THREAD(msginfo->flags))
			summaryview->newmsgs--;
		if (MSG_IS_UNREAD(msginfo->flags) && !MSG_IS_IGNORE_THREAD(msginfo->flags))
			summaryview->unread--;
		if (MSG_IS_NEW(msginfo->flags) || MSG_IS_UNREAD(msginfo->flags)) {
			procmsg_msginfo_unset_flags
				(msginfo, MSG_NEW | MSG_UNREAD, 0);
			folder_update_item(msginfo->folder, FALSE);
			summary_set_row_marks(summaryview, row);
			gtk_clist_thaw(GTK_CLIST(ctree));
			summary_status_show(summaryview);
			
			flags = msginfo->flags;
		}
	}

	if (new_window) {
		MessageView *msgview;

		msgview = messageview_create_with_new_window(summaryview->mainwin);
		messageview_show(msgview, msginfo, all_headers);
	} else {
		MessageView *msgview;

		msgview = summaryview->messageview;

		summaryview->displayed = row;
		if (!messageview_is_visible(msgview))
			main_window_toggle_message_view(summaryview->mainwin);
		messageview_show(msgview, msginfo, all_headers);
		if (msgview->type == MVIEW_TEXT ||
		    (msgview->type == MVIEW_MIME &&
		     (GTK_CLIST(msgview->mimeview->ctree)->row_list == NULL ||
		      gtk_notebook_get_current_page
			(GTK_NOTEBOOK(msgview->mimeview->notebook)) == 0)))
			gtk_widget_grab_focus(summaryview->ctree);
		GTK_EVENTS_FLUSH();
		gtkut_ctree_node_move_if_on_the_edge(ctree, row);
	}

	summary_set_menu_sensitive(summaryview);
	toolbar_set_sensitive(summaryview->mainwin);

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

	summary_display_msg_full(summaryview, summaryview->selected,
				 TRUE, FALSE);
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
	if (summaryview->folder_item->stype != F_OUTBOX &&
	    summaryview->folder_item->stype != F_DRAFT  &&
	    summaryview->folder_item->stype != F_QUEUE) return;

	msginfo = gtk_ctree_node_get_row_data(GTK_CTREE(summaryview->ctree),
					      summaryview->selected);
	if (!msginfo) return;

	compose_reedit(msginfo);
}

void summary_step(SummaryView *summaryview, GtkScrollType type)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;

	if (summary_is_locked(summaryview)) return;

	if (type == GTK_SCROLL_STEP_FORWARD) {
		node = gtkut_ctree_node_next(ctree, summaryview->selected);
		if (node)
			gtkut_ctree_expand_parent_all(ctree, node);
		else
			return;
	} else {
		if (summaryview->selected) {
			node = GTK_CTREE_NODE_PREV(summaryview->selected);
			if (!node) return;
		}
	}

	if (messageview_is_visible(summaryview->messageview))
		summaryview->display_msg = TRUE;

	gtk_signal_emit_by_name(GTK_OBJECT(ctree), "scroll_vertical",
				type, 0.0);

	if (GTK_CLIST(ctree)->selection)
		gtk_sctree_set_anchor_row
			(GTK_SCTREE(ctree),
			 GTK_CTREE_NODE(GTK_CLIST(ctree)->selection->data));

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
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_UNREAD],
					  ignorethreadxpm, ignorethreadxpmmask);
	} else if (MSG_IS_NEW(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_UNREAD],
					  newxpm, newxpmmask);
	} else if (MSG_IS_UNREAD(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_UNREAD],
					  unreadxpm, unreadxpmmask);
	} else if (MSG_IS_REPLIED(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_UNREAD],
					  repliedxpm, repliedxpmmask);
	} else if (MSG_IS_FORWARDED(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_UNREAD],
					  forwardedxpm, forwardedxpmmask);
	} else {
		gtk_ctree_node_set_text(ctree, row, col_pos[S_COL_UNREAD],
					NULL);
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
	}
	else if ((global_scoring ||
		  summaryview->folder_item->prefs->scoring) &&
		 (msginfo->score >= summaryview->important_score) &&
		 (MSG_IS_MARKED(msginfo->flags) || MSG_IS_MOVE(msginfo->flags) || MSG_IS_COPY(msginfo->flags))) {
		gtk_ctree_node_set_text(ctree, row, S_COL_MARK, "!");
		gtk_ctree_node_set_foreground(ctree, row,
					      &summaryview->color_important);
	} else {
		gtk_ctree_node_set_text(ctree, row, col_pos[S_COL_MARK], NULL);
	}

	if (MSG_IS_LOCKED(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_LOCKED],
					  lockedxpm, lockedxpmmask);
	}
	else {
		gtk_ctree_node_set_text(ctree, row, col_pos[S_COL_LOCKED], NULL);
	}

	if (MSG_IS_MIME(flags) && MSG_IS_ENCRYPTED(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_MIME],
					  clipkeyxpm, clipkeyxpmmask);
	} else if (MSG_IS_ENCRYPTED(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_MIME],
					  keyxpm, keyxpmmask);
	} else if (MSG_IS_MIME(flags)) {
		gtk_ctree_node_set_pixmap(ctree, row, col_pos[S_COL_MIME],
					  clipxpm, clipxpmmask);
	} else {
		gtk_ctree_node_set_text(ctree, row, col_pos[S_COL_MIME], NULL);
	}
        if (!style)
		style = small_style;

	gtk_ctree_node_set_row_style(ctree, row, style);

	if (MSG_GET_COLORLABEL(flags))
		summary_set_colorlabel_color(ctree, row, MSG_GET_COLORLABEL_VALUE(flags));
}

void summary_set_marks_selected(SummaryView *summaryview)
{
	GList *cur;

	for (cur = GTK_CLIST(summaryview->ctree)->selection; cur != NULL;
	     cur = cur->next)
		summary_set_row_marks(summaryview, GTK_CTREE_NODE(cur->data));
}

static void summary_mark_row(SummaryView *summaryview, GtkCTreeNode *row)
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
	if (changed && !prefs_common.immediate_exec) {
		msginfo->to_folder->op_count--;
		if (msginfo->to_folder->op_count == 0)
			folder_update_item(msginfo->to_folder, FALSE);
	}
	msginfo->to_folder = NULL;
	procmsg_msginfo_unset_flags(msginfo, MSG_DELETED, MSG_MOVE | MSG_COPY);
	procmsg_msginfo_set_flags(msginfo, MSG_MARKED, 0);
	summary_set_row_marks(summaryview, row);
	debug_print("Message %s/%d is marked\n", msginfo->folder->path, msginfo->msgnum);
}

static void summary_lock_row(SummaryView *summaryview, GtkCTreeNode *row)
{
	/* almost verbatim summary_mark_row(); may want a menu action? */
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
	if (changed && !prefs_common.immediate_exec) {
		msginfo->to_folder->op_count--;
		if (msginfo->to_folder->op_count == 0)
			folder_update_item(msginfo->to_folder, FALSE);
	}
	msginfo->to_folder = NULL;
	procmsg_msginfo_unset_flags(msginfo, MSG_DELETED, MSG_MOVE | MSG_COPY);
	procmsg_msginfo_set_flags(msginfo, MSG_LOCKED, 0);
	summary_set_row_marks(summaryview, row);
	debug_print("Message %d is locked\n", msginfo->msgnum);
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

	if(!MSG_IS_NEW(msginfo->flags) && !MSG_IS_UNREAD(msginfo->flags))
		return;

	if (MSG_IS_NEW(msginfo->flags) && !MSG_IS_IGNORE_THREAD(msginfo->flags))
		summaryview->newmsgs--;
	if (MSG_IS_UNREAD(msginfo->flags) && !MSG_IS_IGNORE_THREAD(msginfo->flags))
		summaryview->unread--;

	procmsg_msginfo_unset_flags(msginfo, MSG_NEW | MSG_UNREAD, 0);
	summary_set_row_marks(summaryview, row);
	debug_print("Message %d is marked as read\n",
		msginfo->msgnum);
}

void summary_mark_as_read(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	for (cur = GTK_CLIST(ctree)->selection; cur != NULL; cur = cur->next)
		summary_mark_row_as_read(summaryview,
					 GTK_CTREE_NODE(cur->data));
	folder_update_items_when_required(FALSE);

	summary_status_show(summaryview);
}

void summary_mark_all_read(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCList *clist = GTK_CLIST(summaryview->ctree);
	GtkCTreeNode *node;

	gtk_clist_freeze(clist);
	for (node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list); node != NULL;
	     node = gtkut_ctree_node_next(ctree, node))
		summary_mark_row_as_read(summaryview, node);
	for (node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list); node != NULL;
	     node = gtkut_ctree_node_next(ctree, node)) {
		if (!GTK_CTREE_ROW(node)->expanded)
			summary_set_row_marks(summaryview, node);
	}
	gtk_clist_thaw(clist);
	folder_update_items_when_required(FALSE);

	summary_status_show(summaryview);
}

static void summary_mark_row_as_unread(SummaryView *summaryview,
				       GtkCTreeNode *row)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);
	if (MSG_IS_DELETED(msginfo->flags)) {
		msginfo->to_folder = NULL;
		procmsg_msginfo_unset_flags(msginfo, MSG_DELETED, 0);
		summaryview->deleted--;
	}

	if (!MSG_IS_UNREAD(msginfo->flags) && !MSG_IS_IGNORE_THREAD(msginfo->flags))
		summaryview->unread++;

	procmsg_msginfo_unset_flags(msginfo, MSG_REPLIED | MSG_FORWARDED, 0);
	procmsg_msginfo_set_flags(msginfo, MSG_UNREAD, 0);
	debug_print("Message %d is marked as unread\n",
		msginfo->msgnum);

	summary_set_row_marks(summaryview, row);
}

void summary_mark_as_unread(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	for (cur = GTK_CLIST(ctree)->selection; cur != NULL; cur = cur->next)
		summary_mark_row_as_unread(summaryview,
					   GTK_CTREE_NODE(cur->data));
	folder_update_items_when_required(FALSE);

	summary_status_show(summaryview);
}

static gboolean check_permission(SummaryView *summaryview, MsgInfo * msginfo)
{
	GList * cur;
	gboolean found;

	switch (summaryview->folder_item->folder->type) {

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
			
			if (g_strcasecmp(from_name, msginfo->from) == 0) {
				g_free(from_name);
				found = TRUE;
				break;
			}
			g_free(from_name);
		}

		if (!found) {
			alertpanel_error(_("You're not the author of the article\n"));
		}
		
		return found;

	default:
		return TRUE;
	}
}

static void summary_delete_row(SummaryView *summaryview, GtkCTreeNode *row)
{
	gboolean changed = FALSE;
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	MsgInfo *msginfo;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);

	if (!check_permission(summaryview, msginfo))
		return;

	if (MSG_IS_LOCKED(msginfo->flags)) return;

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
			folder_update_item(msginfo->to_folder, FALSE);
	}
	msginfo->to_folder = NULL;
	procmsg_msginfo_unset_flags(msginfo, MSG_MARKED, MSG_MOVE | MSG_COPY);
	procmsg_msginfo_set_flags(msginfo, MSG_DELETED, 0);
	summaryview->deleted++;

	if (!prefs_common.immediate_exec && 
	    summaryview->folder_item->stype != F_TRASH)
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

	if (!item) return;
#if 0
	if (!item || item->folder->type == F_NEWS) return;
#endif

	if (summary_is_locked(summaryview)) return;

	/* if current folder is trash, ask for confirmation */
	if (item->stype == F_TRASH) {
		AlertValue aval;

		aval = alertpanel(_("Delete message(s)"),
				  _("Do you really want to delete message(s) from the trash?"),
				  _("Yes"), _("No"), NULL);
		if (aval != G_ALERTDEFAULT) return;
	}

	/* next code sets current row focus right. We need to find a row
	 * that is not deleted. */
	for (cur = GTK_CLIST(ctree)->selection; cur != NULL; cur = cur->next) {
		sel_last = GTK_CTREE_NODE(cur->data);
		summary_delete_row(summaryview, sel_last);
	}

	node = summary_find_next_msg(summaryview, sel_last);
	if (!node)
		node = summary_find_prev_msg(summaryview, sel_last);

	if (node) {
		if (sel_last && node == gtkut_ctree_node_next(ctree, sel_last))
			summary_step(summaryview, GTK_SCROLL_STEP_FORWARD);
		else if (sel_last && node == GTK_CTREE_NODE_PREV(sel_last))
			summary_step(summaryview, GTK_SCROLL_STEP_BACKWARD);
		else
			summary_select_node
				(summaryview, node,
				 messageview_is_visible(summaryview->messageview),
				 FALSE);
	}

	if (prefs_common.immediate_exec || item->stype == F_TRASH)
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
	debug_print("Deleting duplicated messages...");
	STATUSBAR_PUSH(summaryview->mainwin,
		       _("Deleting duplicated messages..."));

	gtk_ctree_pre_recursive(GTK_CTREE(summaryview->ctree), NULL,
				GTK_CTREE_FUNC(summary_delete_duplicated_func),
				summaryview);

	if (prefs_common.immediate_exec)
		summary_execute(summaryview);
	else
		summary_status_show(summaryview);

	debug_print("done.\n");
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
			folder_update_item(msginfo->to_folder, FALSE);
	}
	msginfo->to_folder = NULL;
	procmsg_msginfo_unset_flags(msginfo, MSG_MARKED | MSG_DELETED, MSG_MOVE | MSG_COPY);
	summary_set_row_marks(summaryview, row);

	debug_print("Message %s/%d is unmarked\n",
		    msginfo->folder->path, msginfo->msgnum);
}

void summary_unmark(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	for (cur = GTK_CLIST(ctree)->selection; cur != NULL; cur = cur->next)
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
				folder_update_item(msginfo->to_folder, FALSE);
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
	procmsg_msginfo_unset_flags(msginfo, MSG_MARKED | MSG_DELETED, MSG_COPY);
	if (!MSG_IS_MOVE(msginfo->flags)) {
		procmsg_msginfo_set_flags(msginfo, 0, MSG_MOVE);
		summaryview->moved++;
		changed = TRUE;
	}
	if (!prefs_common.immediate_exec) {
		summary_set_row_marks(summaryview, row);
		if (changed) {
			msginfo->to_folder->op_count++;
			if (msginfo->to_folder->op_count == 1)
				folder_update_item(msginfo->to_folder, FALSE);
		}
	}

	debug_print("Message %d is set to move to %s\n",
		    msginfo->msgnum, to_folder->path);
}

void summary_move_selected_to(SummaryView *summaryview, FolderItem *to_folder)
{
	GList *cur;

	if (!to_folder) return;
	if (!summaryview->folder_item ||
	    summaryview->folder_item->folder->type == F_NEWS) return;

	if (summary_is_locked(summaryview)) return;

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

		folder_update_item(to_folder, FALSE);
	}
	
	if (!summaryview->selected) { /* this was the last message */
		GtkCTreeNode *node = gtk_ctree_node_nth (GTK_CTREE(summaryview->ctree), 
							 GTK_CLIST(summaryview->ctree)->rows - 1);
		if (node)
			summary_select_node(summaryview, node, TRUE, TRUE);
	}

}

void summary_move_to(SummaryView *summaryview)
{
	FolderItem *to_folder;

	if (!summaryview->folder_item ||
	    summaryview->folder_item->folder->type == F_NEWS) return;

	to_folder = foldersel_folder_sel(summaryview->folder_item->folder,
					 FOLDER_SEL_MOVE, NULL);
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
				folder_update_item(msginfo->to_folder, FALSE);
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
	procmsg_msginfo_unset_flags(msginfo, MSG_MARKED | MSG_DELETED, MSG_MOVE);
	if (!MSG_IS_COPY(msginfo->flags)) {
		procmsg_msginfo_set_flags(msginfo, 0, MSG_COPY);
		summaryview->copied++;
		changed = TRUE;
	}
	if (!prefs_common.immediate_exec) {
		summary_set_row_marks(summaryview, row);
		if (changed) {
			msginfo->to_folder->op_count++;
			if (msginfo->to_folder->op_count == 1)
				folder_update_item(msginfo->to_folder, FALSE);
		}
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

		folder_update_item(to_folder, FALSE);
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

	AlertValue aval;

	if (!summaryview->selected) return;
	msginfo = gtk_ctree_node_get_row_data(ctree, summaryview->selected);
	if (!msginfo) return;

	if (msginfo->subject) {
		Xstrdup_a(filename, msginfo->subject, return);
		subst_for_filename(filename);
	}
	dest = filesel_select_file(_("Save as"), filename);
	if (!dest) return;
	if (is_file_exist(dest)) {
		aval = alertpanel(_("Append or Overwrite"),
				  _("Append or overwrite existing file?"),
				  _("Append"), _("Overwrite"), _("Cancel"));
		if (aval!=0 && aval!=1) return;
	}

	src = procmsg_get_message_file(msginfo);
	if ( aval==0 ) { /* append */
		if (append_file(src, dest, TRUE) < 0) 
			alertpanel_error(_("Can't save the file `%s'."),
					 g_basename(dest));
	} else { /* overwrite */
		if (copy_file(src, dest, TRUE) < 0)
			alertpanel_error(_("Can't save the file `%s'."),
					 g_basename(dest));
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
				alertpanel_error(_("Can't save the file `%s'."),
						 g_basename(dest));
		}
		g_free(src);
	}
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

gboolean summary_execute(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCList *clist = GTK_CLIST(summaryview->ctree);
	GtkCTreeNode *node, *next;

	if (!summaryview->folder_item) return FALSE;

	if (summary_is_locked(summaryview)) return FALSE;
	summary_lock(summaryview);

	gtk_clist_freeze(clist);

	if (summaryview->threaded)
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

	if (summaryview->threaded)
		summary_thread_build(summaryview);

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

	gtk_clist_thaw(clist);

	summary_unlock(summaryview);
	return TRUE;
}

static void summary_execute_move(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GSList *cur;

	/* search moving messages and execute */
	gtk_ctree_pre_recursive(ctree, NULL, summary_execute_move_func,
				summaryview);

	if (summaryview->mlist) {
		procmsg_move_messages(summaryview->mlist);

		folder_update_items_when_required(FALSE);

		for (cur = summaryview->mlist; cur != NULL; cur = cur->next)
			procmsg_msginfo_free((MsgInfo *)cur->data);
		g_slist_free(summaryview->mlist);
		summaryview->mlist = NULL;
	}

	folder_update_item(summaryview->folder_item, FALSE);
}

static void summary_execute_move_func(GtkCTree *ctree, GtkCTreeNode *node,
				      gpointer data)
{
	SummaryView *summaryview = data;
	MsgInfo *msginfo;

	msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);

	if (msginfo && MSG_IS_MOVE(msginfo->flags) && msginfo->to_folder) {
		if (!prefs_common.immediate_exec &&
		    msginfo->to_folder->op_count > 0)
                	msginfo->to_folder->op_count--;

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

	/* search copying messages and execute */
	gtk_ctree_pre_recursive(ctree, NULL, summary_execute_copy_func,
				summaryview);

	if (summaryview->mlist) {
		procmsg_copy_messages(summaryview->mlist);

		folder_update_items_when_required(FALSE);

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
		if (!prefs_common.immediate_exec &&
		    msginfo->to_folder->op_count > 0)
                	msginfo->to_folder->op_count--;

		summaryview->mlist =
			g_slist_append(summaryview->mlist, msginfo);

		procmsg_msginfo_unset_flags(msginfo, 0, MSG_COPY);
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

	/* search deleting messages and execute */
	gtk_ctree_pre_recursive
		(ctree, NULL, summary_execute_delete_func, summaryview);

	if (!summaryview->mlist) return;

	if (trash == NULL || summaryview->folder_item == trash)
		folder_item_remove_msgs(summaryview->folder_item,
					summaryview->mlist);
	else
		folder_item_move_msgs_with_dest(trash, summaryview->mlist);

	for (cur = summaryview->mlist; cur != NULL; cur = cur->next)
		procmsg_msginfo_free((MsgInfo *)cur->data);

	g_slist_free(summaryview->mlist);
	summaryview->mlist = NULL;

	if ((summaryview->folder_item != trash) && (trash != NULL)) {
		folder_update_item(trash, FALSE);
	}
	folder_update_item(summaryview->folder_item, FALSE);
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

	summary_lock(summaryview);

	debug_print("Building threads...");
	STATUSBAR_PUSH(summaryview->mainwin, _("Building threads..."));
	main_window_cursor_wait(summaryview->mainwin);

	gtk_signal_handler_block_by_func(GTK_OBJECT(ctree),
					 summary_tree_expanded, summaryview);
	gtk_clist_freeze(GTK_CLIST(ctree));

	node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	while (node) {
		next = GTK_CTREE_ROW(node)->sibling;

		msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);

		parent = NULL;

		/* alfons - claws seems to prefer subject threading before
		 * inreplyto threading. we should look more deeply in this,
		 * because inreplyto should have precedence... */
		if (msginfo && msginfo->inreplyto) {
			parent = g_hash_table_lookup(summaryview->msgid_table,
						     msginfo->inreplyto);
		}

		if (parent == NULL) {
			parent = subject_table_lookup
				(summaryview->subject_table,
				 msginfo->subject);
		}

		if (parent && parent != node) {
			gtk_ctree_move(ctree, node, parent, NULL);
			gtk_ctree_expand(ctree, node);
		}

		node = next;
	}

	node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

	while (node) {
		next = GTK_CTREE_NODE_NEXT(node);
		if (prefs_common.expand_thread)
			gtk_ctree_expand(ctree, node);
		if (prefs_common.bold_unread &&
		    GTK_CTREE_ROW(node)->children)
			summary_set_row_marks(summaryview, node);
		node = next;
	}

	gtk_clist_thaw(GTK_CLIST(ctree));
	gtk_signal_handler_unblock_by_func(GTK_OBJECT(ctree),
					   summary_tree_expanded, summaryview);

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
				summary_set_row_marks(summaryview, node);
			node = next;
		}
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

	/* CLAWS: and sort it */
	gtk_sctree_sort_recursive(ctree, NULL);	

	gtk_clist_thaw(GTK_CLIST(ctree));
	gtk_signal_handler_unblock_by_func(GTK_OBJECT(ctree),
					   summary_tree_collapsed, summaryview);

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

	summary_lock(summaryview);

	debug_print("Unthreading for execution...");

	gtk_clist_freeze(GTK_CLIST(ctree));

	for (node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	     node != NULL; node = GTK_CTREE_NODE_NEXT(node)) {
		summary_unthread_for_exec_func(ctree, node, NULL);
	}

	gtk_clist_thaw(GTK_CLIST(ctree));

	debug_print("done.\n");

	summary_unlock(summaryview);
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

	gtk_clist_freeze(GTK_CLIST(ctree));

	while (node) {
		if (GTK_CTREE_ROW(node)->children)
			gtk_ctree_expand(ctree, node);
		node = GTK_CTREE_NODE_NEXT(node);
	}

	gtk_clist_thaw(GTK_CLIST(ctree));

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

	gtk_ctree_node_moveto(ctree, summaryview->selected, -1, 0.5, 0);
}

void summary_filter(SummaryView *summaryview)
{
	if (!global_processing) {
		alertpanel_error(_("No filter rules defined."));
		return;
	}

	summary_lock(summaryview);

	debug_print("filtering...");
	STATUSBAR_PUSH(summaryview->mainwin, _("Filtering..."));
	main_window_cursor_wait(summaryview->mainwin);

	gtk_clist_freeze(GTK_CLIST(summaryview->ctree));

	if (global_processing == NULL) {
		gtk_ctree_pre_recursive(GTK_CTREE(summaryview->ctree), NULL,
					GTK_CTREE_FUNC(summary_filter_func),
					summaryview);
		
		gtk_clist_thaw(GTK_CLIST(summaryview->ctree));
		
		if (prefs_common.immediate_exec) {
			summary_unlock(summaryview);
			summary_execute(summaryview);
			summary_lock(summaryview);
		} else
			summary_status_show(summaryview);
	}
	else {
		gtk_ctree_pre_recursive(GTK_CTREE(summaryview->ctree), NULL,
					GTK_CTREE_FUNC(summary_filter_func),
					summaryview);

		gtk_clist_thaw(GTK_CLIST(summaryview->ctree));

		folder_update_items_when_required(FALSE);
	}

	debug_print("done.\n");
	STATUSBAR_POP(summaryview->mainwin);
	main_window_cursor_normal(summaryview->mainwin);

	summary_unlock(summaryview);

	/* 
	 * CLAWS: summary_show() only valid after having a lock. ideally
	 * we want the lock to be context aware...  
	 */
	if (global_processing) {
		summary_show(summaryview, summaryview->folder_item);
	}		
}

static void summary_filter_func(GtkCTree *ctree, GtkCTreeNode *node,
				gpointer data)
{
	MsgInfo *msginfo = GTKUT_CTREE_NODE_GET_ROW_DATA(node);
	SummaryView *summaryview = data;
	gchar *file;
	FolderItem *dest;

	filter_message_by_msginfo(global_processing, msginfo);
}

void summary_filter_open(SummaryView *summaryview, PrefsFilterType type)
{
	static HeaderEntry hentry[] = {{"X-BeenThere:",    NULL, FALSE},
				       {"X-ML-Name:",      NULL, FALSE},
				       {"X-List:",         NULL, FALSE},
				       {"X-Mailing-list:", NULL, FALSE},
				       {"List-Id:",        NULL, FALSE},
				       {NULL,              NULL, FALSE}};

	static gchar *header_strs[] = {"From", "from", "To", "to", "Subject", "subject"};

	static gchar *hentry_strs[]   = {"X-BeenThere", "X-ML-Name", "X-List",
					 "X-Mailing-List", "List-Id",
					 "header \"X-BeenThere\"", "header \"X-ML-Name\"",
					 "header \"X-List\"", "header \"X-Mailing-List\"",
					 "header \"List-Id\""};
	enum
	{
		H_X_BEENTHERE	 = 0,
		H_X_ML_NAME      = 1,
		H_X_LIST         = 2,
		H_X_MAILING_LIST = 3,
		H_LIST_ID	 = 4
	};

	enum
	{
		H_FROM    = 0,
		H_TO      = 2,
		H_SUBJECT = 4
	};

	MsgInfo *msginfo;
	gchar *header = NULL;
	gchar *key = NULL;
	FILE *fp;
	int header_offset;
	int hentry_offset;

	if (!summaryview->selected) return;

	msginfo = gtk_ctree_node_get_row_data(GTK_CTREE(summaryview->ctree),
					      summaryview->selected);
	if (!msginfo) return;

	header_offset = 1;
	hentry_offset = 5;

	switch (type) {
	case FILTER_BY_NONE:
		break;
	case FILTER_BY_AUTO:
		if ((fp = procmsg_open_message(msginfo)) == NULL) return;
		procheader_get_header_fields(fp, hentry);
		fclose(fp);

		if (hentry[H_X_BEENTHERE].body != NULL) {
			header = hentry_strs[H_X_BEENTHERE + hentry_offset];
			Xstrdup_a(key, hentry[H_X_BEENTHERE].body, );
		} else if (hentry[H_X_ML_NAME].body != NULL) {
			header = hentry_strs[H_X_ML_NAME + hentry_offset];
			Xstrdup_a(key, hentry[H_X_ML_NAME].body, );
		} else if (hentry[H_X_LIST].body != NULL) {
			header = hentry_strs[H_X_LIST + hentry_offset];
			Xstrdup_a(key, hentry[H_X_LIST].body, );
		} else if (hentry[H_X_MAILING_LIST].body != NULL) {
			header = hentry_strs[H_X_MAILING_LIST + hentry_offset];
			Xstrdup_a(key, hentry[H_X_MAILING_LIST].body, );
		} else if (hentry[H_LIST_ID].body != NULL) {
			header = hentry_strs[H_LIST_ID + hentry_offset];
			Xstrdup_a(key, hentry[H_LIST_ID].body, );
		} else if (msginfo->subject) {
			header = header_strs[H_SUBJECT + header_offset];
			key = msginfo->subject;
		}

		g_free(hentry[H_X_BEENTHERE].body);
		hentry[H_X_BEENTHERE].body = NULL;
		g_free(hentry[H_X_ML_NAME].body);
		hentry[H_X_ML_NAME].body = NULL;
		g_free(hentry[H_X_LIST].body);
		hentry[H_X_LIST].body = NULL;
		g_free(hentry[H_X_MAILING_LIST].body);
		hentry[H_X_MAILING_LIST].body = NULL;
		g_free(hentry[H_LIST_ID].body);
		hentry[H_LIST_ID].body = NULL;

		break;
	case FILTER_BY_FROM:
		header = header_strs[H_FROM + header_offset];
		key = msginfo->from;
		break;
	case FILTER_BY_TO:
		header = header_strs[H_TO + header_offset];
		key = msginfo->to;
		break;
	case FILTER_BY_SUBJECT:
		header = header_strs[H_SUBJECT + header_offset];
		key = msginfo->subject;
		break;
	default:
		break;
	}

	/*
	 * NOTE: key may be allocated on the stack, so 
	 * prefs_filter[ing]_open() should have completed 
	 * and have set entries. Otherwise we're hosed.  
	 */

	prefs_filtering_open(NULL, header, key);
}

void summary_reply(SummaryView *summaryview, ComposeMode mode)
{
	GList *sel = GTK_CLIST(summaryview->ctree)->selection;
	MsgInfo *msginfo;
	gchar *text;

	msginfo = gtk_ctree_node_get_row_data(GTK_CTREE(summaryview->ctree),
					      summaryview->selected);
	if (!msginfo) return;

	text = gtkut_editable_get_selection
		(GTK_EDITABLE(summaryview->messageview->textview->text));

	if (!text && summaryview->messageview->type == MVIEW_MIME
	    && summaryview->messageview->mimeview->type == MIMEVIEW_TEXT
	    && summaryview->messageview->mimeview->textview
	    && !summaryview->messageview->mimeview->textview->default_text) {
	 	text = gtkut_editable_get_selection (GTK_EDITABLE 
			 (summaryview->messageview->mimeview->textview->text));   
	}
	
	switch (mode) {
	case COMPOSE_REPLY:
		if (prefs_common.default_reply_list)
			compose_reply(msginfo, prefs_common.reply_with_quote,
			    	      FALSE, TRUE, FALSE, text);
		else
			compose_reply(msginfo, prefs_common.reply_with_quote,
				      FALSE, FALSE, FALSE, text);
		break;
	case COMPOSE_REPLY_WITH_QUOTE:
		if (prefs_common.default_reply_list)
			compose_reply(msginfo, TRUE, FALSE, TRUE, FALSE, text);
		else
			compose_reply(msginfo, TRUE, FALSE, FALSE, FALSE, text);
		break;
	case COMPOSE_REPLY_WITHOUT_QUOTE:
		if (prefs_common.default_reply_list)
			compose_reply(msginfo, FALSE, FALSE, TRUE, FALSE, NULL);
		else
			compose_reply(msginfo, FALSE, FALSE, FALSE, FALSE, NULL);
		break;
	case COMPOSE_REPLY_TO_SENDER:
		compose_reply(msginfo, prefs_common.reply_with_quote,
			      FALSE, FALSE, TRUE, text);
		break;
	case COMPOSE_FOLLOWUP_AND_REPLY_TO:
		compose_followup_and_reply_to(msginfo,
					      prefs_common.reply_with_quote,
					      FALSE, TRUE, text);
		break;
	case COMPOSE_REPLY_TO_SENDER_WITH_QUOTE:
		compose_reply(msginfo, TRUE, FALSE, FALSE, TRUE, text);
		break;
	case COMPOSE_REPLY_TO_SENDER_WITHOUT_QUOTE:
		compose_reply(msginfo, FALSE, FALSE, FALSE, TRUE, NULL);
		break;
	case COMPOSE_REPLY_TO_ALL:
		compose_reply(msginfo, prefs_common.reply_with_quote,
			      TRUE, FALSE, FALSE, text);
		break;
	case COMPOSE_REPLY_TO_ALL_WITH_QUOTE:
		compose_reply(msginfo, TRUE, TRUE, FALSE, FALSE, text);
		break;
	case COMPOSE_REPLY_TO_ALL_WITHOUT_QUOTE:
		compose_reply(msginfo, FALSE, TRUE, FALSE, FALSE, NULL);
		break;
	case COMPOSE_REPLY_TO_LIST:
		compose_reply(msginfo, prefs_common.reply_with_quote,
			      FALSE, TRUE, FALSE, text);
		break;
	case COMPOSE_REPLY_TO_LIST_WITH_QUOTE:
		compose_reply(msginfo, TRUE, FALSE, TRUE, FALSE, text);
		break;
	case COMPOSE_REPLY_TO_LIST_WITHOUT_QUOTE:
		compose_reply(msginfo, FALSE, FALSE, TRUE, FALSE, NULL);
		break;
	case COMPOSE_FORWARD:
		if (prefs_common.forward_as_attachment) {
			summary_reply_cb(summaryview, COMPOSE_FORWARD_AS_ATTACH, NULL);
			return;
		} else {
			summary_reply_cb(summaryview, COMPOSE_FORWARD_INLINE, NULL);
			return;
		}
		break;
	case COMPOSE_FORWARD_INLINE:
		if (!sel->next) {
			compose_forward(NULL, msginfo, FALSE, text);
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
	case COMPOSE_REDIRECT:
		compose_redirect(NULL, msginfo);
		break;
	default:
		g_warning("summary_reply_cb(): invalid action: %d\n", mode);
	}

	summary_set_marks_selected(summaryview);
	g_free(text);
}

/* color label */

#define N_COLOR_LABELS colorlabel_get_color_count()

static void summary_colorlabel_menu_item_activate_cb(GtkWidget *widget,
						     gpointer data)
{
	guint color = GPOINTER_TO_UINT(data);
	SummaryView *summaryview;

	summaryview = gtk_object_get_data(GTK_OBJECT(widget), "summaryview");
	g_return_if_fail(summaryview != NULL);

	/* "dont_toggle" state set? */
	if (gtk_object_get_data(GTK_OBJECT(summaryview->colorlabel_menu),
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
	procmsg_msginfo_unset_flags(msginfo, MSG_CLABEL_FLAG_MASK, 0);
	procmsg_msginfo_set_flags(msginfo, MSG_COLORLABEL_TO_FLAGS(labelcolor), 0);

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

void summary_set_colorlabel(SummaryView *summaryview, guint labelcolor,
			    GtkWidget *widget)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCList *clist = GTK_CLIST(summaryview->ctree);
	GList *cur;

	for (cur = clist->selection; cur != NULL; cur = cur->next)
		summary_set_colorlabel_color(ctree, GTK_CTREE_NODE(cur->data),
					     labelcolor);
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
	gtk_object_set_data(GTK_OBJECT(menu), "dont_toggle",
			    GINT_TO_POINTER(1));

	/* clear items. get item pointers. */
	for (n = 0, cur = menu->children; cur != NULL; cur = cur->next) {
		if (GTK_IS_CHECK_MENU_ITEM(cur->data)) {
			gtk_check_menu_item_set_state
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
					gtk_check_menu_item_set_state
						(items[clabel], TRUE);
			}
		}
	} else
		g_warning("invalid number of color elements (%d)\n", n);

	/* reset "dont_toggle" state */
	gtk_object_set_data(GTK_OBJECT(menu), "dont_toggle",
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
	gtk_signal_connect(GTK_OBJECT(label_menuitem), "activate",
			   GTK_SIGNAL_FUNC(summary_colorlabel_menu_item_activate_item_cb),
			   summaryview);
	gtk_widget_show(label_menuitem);

	menu = gtk_menu_new();

	/* create sub items. for the menu item activation callback we pass the
	 * index of label_colors[] as data parameter. for the None color we
	 * pass an invalid (high) value. also we attach a data pointer so we
	 * can always get back the SummaryView pointer. */

	item = gtk_check_menu_item_new_with_label(_("None"));
	gtk_menu_append(GTK_MENU(menu), item);
	gtk_signal_connect(GTK_OBJECT(item), "activate",
			   GTK_SIGNAL_FUNC(summary_colorlabel_menu_item_activate_cb),
			   GUINT_TO_POINTER(0));
	gtk_object_set_data(GTK_OBJECT(item), "summaryview", summaryview);
	gtk_widget_show(item);

	item = gtk_menu_item_new();
	gtk_menu_append(GTK_MENU(menu), item);
	gtk_widget_show(item);

	/* create pixmap/label menu items */
	for (i = 0; i < N_COLOR_LABELS; i++) {
		item = colorlabel_create_check_color_menu_item(i);
		gtk_menu_append(GTK_MENU(menu), item);
		gtk_signal_connect(GTK_OBJECT(item), "activate",
				   GTK_SIGNAL_FUNC(summary_colorlabel_menu_item_activate_cb),
				   GUINT_TO_POINTER(i + 1));
		gtk_object_set_data(GTK_OBJECT(item), "summaryview",
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
	}
	col_state = summaryview->col_state;

	ctree = gtk_sctree_new_with_titles
		(N_SUMMARY_COLS, col_pos[S_COL_SUBJECT], titles);

	gtk_clist_set_selection_mode(GTK_CLIST(ctree), GTK_SELECTION_EXTENDED);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), col_pos[S_COL_MARK],
					   GTK_JUSTIFY_CENTER);
	gtk_clist_set_column_justification(GTK_CLIST(ctree), col_pos[S_COL_UNREAD],
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
	gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[S_COL_UNREAD],
				   SUMMARY_COL_UNREAD_WIDTH);
	gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[S_COL_LOCKED],
				   SUMMARY_COL_LOCKED_WIDTH);
	gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[S_COL_MIME],
				   SUMMARY_COL_MIME_WIDTH);
	gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[S_COL_SUBJECT],
				   prefs_common.summary_col_size[S_COL_SUBJECT]);
	gtk_clist_set_column_width(GTK_CLIST(ctree), col_pos[S_COL_FROM],
				   prefs_common.summary_col_size[S_COL_FROM]);
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
	gtk_ctree_set_indent(GTK_CTREE(ctree), 16);
	gtk_object_set_user_data(GTK_OBJECT(ctree), summaryview);

	for (pos = 0; pos < N_SUMMARY_COLS; pos++) {
		GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(ctree)->column[pos].button,
				       GTK_CAN_FOCUS);
		gtk_clist_set_column_visibility
			(GTK_CLIST(ctree), pos, col_state[pos].visible);
	}

	/* connect signal to the buttons for sorting */
#define CLIST_BUTTON_SIGNAL_CONNECT(col, func) \
	gtk_signal_connect \
		(GTK_OBJECT(GTK_CLIST(ctree)->column[col_pos[col]].button), \
		 "clicked", \
		 GTK_SIGNAL_FUNC(func), \
		 summaryview)

	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_MARK   , summary_mark_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_UNREAD , summary_unread_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_MIME   , summary_mime_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_NUMBER , summary_num_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_SIZE   , summary_size_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_DATE   , summary_date_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_FROM   , summary_from_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_SUBJECT, summary_subject_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_SCORE,   summary_score_clicked);
	CLIST_BUTTON_SIGNAL_CONNECT(S_COL_LOCKED,  summary_locked_clicked);

#undef CLIST_BUTTON_SIGNAL_CONNECT

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
	pixmap = gtk_pixmap_new(clipxpm, clipxpmmask);
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

static void summary_toggle_pressed(GtkWidget *eventbox, GdkEventButton *event,
				   SummaryView *summaryview)
{
	if (!event) return;

	summary_toggle_view(summaryview);
}

static void summary_button_pressed(GtkWidget *ctree, GdkEventButton *event,
				   SummaryView *summaryview)
{
	if (!event) return;

	if (event->button == 3) {
		/* right clicked */
		gtk_menu_popup(GTK_MENU(summaryview->popupmenu), NULL, NULL,
			       NULL, NULL, event->button, event->time);
	} else if (event->button == 2) {
		summaryview->display_msg = TRUE;
	} else if (event->button == 1) {
		if (!prefs_common.emulate_emacs &&
		    messageview_is_visible(summaryview->messageview))
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

static void summary_key_pressed(GtkWidget *widget, GdkEventKey *event,
				SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(widget);
	GtkCTreeNode *node;
	MessageView *messageview;
	TextView *textview;

	if (summary_is_locked(summaryview)) return;
	if (!event) return;

	switch (event->keyval) {
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
			gtk_sctree_select(GTK_SCTREE(ctree), node);
		else
			return;
	}

	messageview = summaryview->messageview;
	if (messageview->type == MVIEW_MIME &&
	    gtk_notebook_get_current_page
		(GTK_NOTEBOOK(messageview->mimeview->notebook)) == 1)
		textview = messageview->mimeview->textview;
	else
		textview = messageview->textview;

	switch (event->keyval) {
	case GDK_space:		/* Page down or go to the next */
		if (summaryview->displayed != summaryview->selected) {
			summary_display_msg(summaryview,
					    summaryview->selected);
			break;
		}
		if (!textview_scroll_page(textview, FALSE))
			summary_select_next_unread(summaryview);
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
		textview_scroll_one_line
			(textview, (event->state & GDK_MOD1_MASK) != 0);
		break;
	case GDK_asterisk:	/* Mark */
		summary_mark(summaryview);
		break;
	case GDK_exclam:	/* Mark as unread */
		summary_mark_as_unread(summaryview);
		break;
	case GDK_Delete:
		RETURN_IF_LOCKED();
		BREAK_ON_MODIFIER_KEY();
		summary_delete(summaryview);
		break;
	default:
		break;
	}
}

static void summary_searchbar_pressed(GtkWidget *widget, GdkEventKey *event,
				SummaryView *summaryview)
{
	if (event != NULL && event->keyval == GDK_Return)
	 	summary_show(summaryview, summaryview->folder_item);
}

static void summary_searchtype_changed(GtkMenuItem *widget, gpointer data)
{
	SummaryView *sw = (SummaryView *)data;
	if (gtk_entry_get_text(GTK_ENTRY(sw->search_string)))
	 	summary_show(sw, sw->folder_item);
}

static void tog_searchbar_cb(GtkWidget *w, gpointer data)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w))) {
		prefs_common.show_searchbar = TRUE;
 		gtk_widget_show(GTK_WIDGET(data));
	} else {
		prefs_common.show_searchbar = FALSE;
		gtk_widget_hide(GTK_WIDGET(data));
	}
}

static void summary_open_row(GtkSCTree *sctree, SummaryView *summaryview)
{
	if (summaryview->folder_item->stype == F_OUTBOX ||
	    summaryview->folder_item->stype == F_DRAFT  ||
	    summaryview->folder_item->stype == F_QUEUE)
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

	if (GTK_CLIST(ctree)->selection &&
	     GTK_CLIST(ctree)->selection->next) {
		summaryview->display_msg = FALSE;
		summary_set_menu_sensitive(summaryview);
		toolbar_set_sensitive(summaryview->mainwin);
		return;
	}

	summaryview->selected = row;

	msginfo = gtk_ctree_node_get_row_data(ctree, row);

	switch (column < 0 ? column : summaryview->col_state[column].type) {
	case S_COL_MARK:
		if (MSG_IS_MARKED(msginfo->flags)) {
			procmsg_msginfo_unset_flags(msginfo, MSG_MARKED, 0);
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
	case S_COL_LOCKED:
		if (MSG_IS_LOCKED(msginfo->flags)) {
			procmsg_msginfo_unset_flags(msginfo, MSG_LOCKED, 0);
			summary_set_row_marks(summaryview, row);
		}
		else
			summary_lock_row(summaryview, row);
		break;
	default:
		break;
	}

	if (summaryview->display_msg ||
	    (prefs_common.show_msg_with_cursor_key &&
	     messageview_is_visible(summaryview->messageview))) {
		summary_display_msg(summaryview, row);
		summaryview->display_msg = FALSE;
	} else {
		summary_set_menu_sensitive(summaryview);
		toolbar_set_sensitive(summaryview->mainwin);
	}
}

static void summary_col_resized(GtkCList *clist, gint column, gint width,
				SummaryView *summaryview)
{
	SummaryColumnType type = summaryview->col_state[column].type;

	prefs_common.summary_col_size[type] = width;
}

static void summary_reply_cb(SummaryView *summaryview, guint action,
			     GtkWidget *widget)
{
	summary_reply(summaryview, (ComposeMode)action);
}

static void summary_execute_cb(SummaryView *summaryview, guint action,
			       GtkWidget *widget)
{
	summary_execute(summaryview);
}

static void summary_show_all_header_cb(SummaryView *summaryview,
				       guint action, GtkWidget *widget)
{
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
	summary_filter_open(summaryview, (PrefsFilterType)action);
}

static void summary_sort_by_column_click(SummaryView *summaryview,
					 FolderSortKey sort_key)
{
	if (summaryview->sort_key == sort_key)
		summary_sort(summaryview, sort_key,
			     summaryview->sort_type == SORT_ASCENDING
			     ? SORT_DESCENDING : SORT_ASCENDING);
	else
		summary_sort(summaryview, sort_key, SORT_ASCENDING);
}

static void summary_mark_clicked(GtkWidget *button, SummaryView *summaryview)
{
	summary_sort_by_column_click(summaryview, SORT_BY_MARK);
}

static void summary_unread_clicked(GtkWidget *button, SummaryView *summaryview)
{
	summary_sort_by_column_click(summaryview, SORT_BY_UNREAD);
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
	GtkTargetList *list;
	GdkDragContext *context;

	g_return_if_fail(summaryview != NULL);
	g_return_if_fail(summaryview->folder_item != NULL);
	g_return_if_fail(summaryview->folder_item->folder != NULL);
	if (summaryview->selected == NULL) return;

	list = gtk_target_list_new(summary_drag_types, 1);

	context = gtk_drag_begin(widget, list,
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
		     cur != NULL; cur = cur->next) {
			msginfo = gtk_ctree_node_get_row_data
				(ctree, GTK_CTREE_NODE(cur->data));
			tmp2 = procmsg_get_message_file(msginfo);
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

static gint summary_cmp_by_mark(GtkCList *clist,
				gconstpointer ptr1, gconstpointer ptr2)
{
	MsgInfo *msginfo1 = ((GtkCListRow *)ptr1)->data;
	MsgInfo *msginfo2 = ((GtkCListRow *)ptr2)->data;

	return MSG_IS_MARKED(msginfo1->flags) - MSG_IS_MARKED(msginfo2->flags);
}

static gint summary_cmp_by_unread(GtkCList *clist,
				  gconstpointer ptr1, gconstpointer ptr2)
{
	MsgInfo *msginfo1 = ((GtkCListRow *)ptr1)->data;
	MsgInfo *msginfo2 = ((GtkCListRow *)ptr2)->data;

	return MSG_IS_UNREAD(msginfo1->flags) - MSG_IS_UNREAD(msginfo2->flags);
}

static gint summary_cmp_by_mime(GtkCList *clist,
				gconstpointer ptr1, gconstpointer ptr2)
{
	MsgInfo *msginfo1 = ((GtkCListRow *)ptr1)->data;
	MsgInfo *msginfo2 = ((GtkCListRow *)ptr2)->data;

	return MSG_IS_MIME(msginfo1->flags) - MSG_IS_MIME(msginfo2->flags);
}

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

static gint summary_cmp_by_to(GtkCList *clist,
			      gconstpointer ptr1, gconstpointer ptr2)
{
	const gchar *str1, *str2;
	const GtkCListRow *r1 = (const GtkCListRow *) ptr1;
	const GtkCListRow *r2 = (const GtkCListRow *) ptr2;
	const SummaryView *sv = gtk_object_get_data(GTK_OBJECT(clist), "summaryview");
	
	g_return_val_if_fail(sv, -1);
	
	str1 = GTK_CELL_TEXT(r1->cell[sv->col_pos[S_COL_FROM]])->text;
	str2 = GTK_CELL_TEXT(r2->cell[sv->col_pos[S_COL_FROM]])->text;

	if (!str1)
		return str2 != NULL;

	if (!str2)
		return -1;

	if (g_strncasecmp(str1, "-->", 3) == 0)
		str1 += 3;
	if (g_strncasecmp(str2, "-->", 3) == 0)
		str2 += 3;

	return strcasecmp(str1, str2);
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

static gint summary_cmp_by_simplified_subject
	(GtkCList *clist, gconstpointer ptr1, gconstpointer ptr2)
{
	const PrefsFolderItem *prefs;
	const gchar *str1, *str2;
	const GtkCListRow *r1 = (const GtkCListRow *) ptr1;
	const GtkCListRow *r2 = (const GtkCListRow *) ptr2;
	const MsgInfo *msginfo1 = r1->data;
	const MsgInfo *msginfo2 = r2->data;
	const SummaryView *sv = gtk_object_get_data(GTK_OBJECT(clist), "summaryview");
	
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
	
	return strcasecmp(str1, str2);
}

static gint summary_cmp_by_label(GtkCList *clist,
				 gconstpointer ptr1, gconstpointer ptr2)
{
	MsgInfo *msginfo1 = ((GtkCListRow *)ptr1)->data;
	MsgInfo *msginfo2 = ((GtkCListRow *)ptr2)->data;

	return MSG_GET_COLORLABEL(msginfo1->flags) -
		MSG_GET_COLORLABEL(msginfo2->flags);
}

static void news_flag_crosspost(MsgInfo *msginfo)
{
	GString *line;
	gpointer key;
	gpointer value;
	Folder *mff = msginfo->folder->folder;

	if (mff->account->mark_crosspost_read && MSG_IS_NEWS(msginfo->flags)) {
		line = g_string_sized_new(128);
		g_string_sprintf(line, "%s:%d", msginfo->folder->path, msginfo->msgnum);
		debug_print("nfcp: checking <%s>", line->str);
		if (mff->newsart && 
		    g_hash_table_lookup_extended(mff->newsart, line->str, &key, &value)) {
			debug_print(" <%s>", (gchar *)value);
			if (MSG_IS_NEW(msginfo->flags) || MSG_IS_UNREAD(msginfo->flags)) {
				procmsg_msginfo_unset_flags(msginfo, MSG_NEW | MSG_UNREAD, 0);
				folder_update_item(msginfo->folder, FALSE);
				procmsg_msginfo_set_flags(msginfo, mff->account->crosspost_col, 0);
			}
			g_hash_table_remove(mff->newsart, key);
			g_free(key);
		}
		g_string_free(line, TRUE);
		debug_print("\n");
	}
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

static gint summary_cmp_by_locked(GtkCList *clist,
				  gconstpointer ptr1, gconstpointer ptr2)
{
	MsgInfo *msginfo1 = ((GtkCListRow *)ptr1)->data;
	MsgInfo *msginfo2 = ((GtkCListRow *)ptr2)->data;

	return MSG_IS_LOCKED(msginfo1->flags) - MSG_IS_LOCKED(msginfo2->flags);
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

	procmsg_msginfo_set_flags(msginfo, MSG_IGNORE_THREAD, 0);

	summary_set_row_marks(summaryview, row);
	debug_print("Message %d is marked as ignore thread\n",
	    msginfo->msgnum);
}

static void summary_ignore_thread(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	for (cur = GTK_CLIST(ctree)->selection; cur != NULL; cur = cur->next) {
		gtk_ctree_pre_recursive(ctree, GTK_CTREE_NODE(cur->data), GTK_CTREE_FUNC(summary_ignore_thread_func), summaryview);
	}
	folder_update_items_when_required(FALSE);

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

	procmsg_msginfo_unset_flags(msginfo, MSG_IGNORE_THREAD, 0);

	summary_set_row_marks(summaryview, row);
	debug_print("Message %d is marked as unignore thread\n",
	    msginfo->msgnum);
}

static void summary_unignore_thread(SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;

	for (cur = GTK_CLIST(ctree)->selection; cur != NULL; cur = cur->next) {
		gtk_ctree_pre_recursive(ctree, GTK_CTREE_NODE(cur->data), GTK_CTREE_FUNC(summary_unignore_thread_func), summaryview);
	}
	folder_update_items_when_required(FALSE);

	summary_status_show(summaryview);
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

/*
		mlist = item->folder->get_msg_list(item->folder, item,
						   TRUE);
*/		
		mlist = folder_item_get_msg_list(item);
		for(cur = mlist ; cur != NULL ; cur = cur->next) {
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

	for (cur = folder_get_list() ; cur != NULL ; cur = g_list_next(cur)) {
		Folder *folder;

		folder = (Folder *) cur->data;
		g_node_traverse(folder->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				processing_apply_func, summaryview);
	}
}
#endif

void summary_toggle_show_read_messages(SummaryView *summaryview)
{
 	if (summaryview->folder_item->hide_read_msgs)
 		summaryview->folder_item->hide_read_msgs = 0;
 	else
 		summaryview->folder_item->hide_read_msgs = 1;
 	summary_show(summaryview, summaryview->folder_item);
}
 
static void summary_set_hide_read_msgs_menu (SummaryView *summaryview,
 					     guint action)
{
 	GtkWidget *widget;
 
 	widget = gtk_item_factory_get_item(gtk_item_factory_from_widget(summaryview->mainwin->menubar),
 					   "/View/Hide read messages");
 	gtk_object_set_data(GTK_OBJECT(widget), "dont_toggle",
 			    GINT_TO_POINTER(1));
 	gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(widget), action);
 	gtk_object_set_data(GTK_OBJECT(widget), "dont_toggle",
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
	for( cur = GTK_CLIST(ctree)->selection; cur != NULL; cur = cur->next ) {
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
}

void summary_save_prefs_to_folderitem(SummaryView *summaryview, FolderItem *item)
{
	/* Sorting */
	item->sort_key = summaryview->sort_key;
	item->sort_type = summaryview->sort_type;

	/* Threading */
	item->threaded = summaryview->threaded;
}

/*
 * End of Source.
 */
