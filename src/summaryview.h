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

#ifndef __SUMMARY_H__
#define __SUMMARY_H__

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkctree.h>
#include <gtk/gtkdnd.h>

typedef struct _SummaryView	SummaryView;

#include "mainwindow.h"
#include "folderview.h"
#include "headerview.h"
#include "messageview.h"
#include "headerwindow.h"
#include "folder.h"
#include "gtksctree.h"

typedef enum
{
	S_COL_MARK	= 0,
	S_COL_UNREAD	= 1,
	S_COL_MIME	= 2,
	S_COL_NUMBER	= 3,
	S_COL_SCORE     = 4,
	S_COL_SIZE	= 5,
	S_COL_DATE	= 6,
	S_COL_FROM	= 7,
	S_COL_SUBJECT	= 8
} SummaryColumnPos;

#define N_SUMMARY_COLS	9

typedef enum
{
	SORT_BY_NONE,
	SORT_BY_NUMBER,
	SORT_BY_SIZE,
	SORT_BY_DATE,
	SORT_BY_FROM,
	SORT_BY_SUBJECT,
	SORT_BY_SCORE,
	SORT_BY_LABEL
} SummarySortType;

typedef enum
{
	SUMMARY_NONE,
	SUMMARY_SELECTED_NONE,
	SUMMARY_SELECTED_SINGLE,
	SUMMARY_SELECTED_MULTIPLE
} SummarySelection;

typedef enum
{
	TARGET_MAIL_URI_LIST,
	TARGET_DUMMY
} TargetInfo;

extern GtkTargetEntry summary_drag_types[1];

struct _SummaryView
{
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
	GtkWidget *label_menu_item; /* label menu item */
	GtkWidget *label_menu;	    /* label menu itself */

	GtkItemFactory *popupfactory;

	GtkWidget *window;

	GtkCTreeNode *selected;
	GtkCTreeNode *displayed;

	gboolean msg_is_toggled_on;
	gboolean display_msg;

	GdkColor color_important;
	GdkColor color_marked;
	GdkColor color_dim;

	MainWindow   *mainwin;
	FolderView   *folderview;
	HeaderView   *headerview;
	MessageView  *messageview;
	HeaderWindow *headerwin;

	FolderItem *folder_item;

	GSList * killed_messages;
	gint important_score;

	/* current message status */
	gint   newmsgs;
	gint   unread;
	gint   messages;
	off_t  total_size;
	gint   deleted;
	gint   moved;
	gint   copied;

/*
private:
*/
	/* table for looking up message-id */
	GHashTable *msgid_table;
	GHashTable *subject_table;

	/* list for moving/deleting messages */
	GSList *mlist;
	/* table for updating folder tree */
	GHashTable *folder_table;

	/* current sorting state */
	SummarySortType sort_mode;
	GtkSortType sort_type;
};

SummaryView	*summary_create(void);

void summary_init		  (SummaryView		*summaryview);
gboolean summary_show		  (SummaryView		*summaryview,
				   FolderItem		*fitem,
				   gboolean		 update_cache);
void summary_clear_list		  (SummaryView		*summaryview);
void summary_clear_all		  (SummaryView		*summaryview);

void summary_select_next_unread	  (SummaryView		*summaryview);
void summary_select_next_marked   (SummaryView		*summaryview);
void summary_select_prev_marked   (SummaryView		*summaryview);
void summary_select_by_msgnum	  (SummaryView		*summaryview,
				   guint		 msgnum);
guint summary_get_current_msgnum  (SummaryView		*summaryview);
void summary_thread_build	  (SummaryView		*summaryview);
void summary_unthread		  (SummaryView		*summaryview);
void summary_filter		  (SummaryView		*summaryview);
void summary_sort		  (SummaryView		*summaryview,
				   SummarySortType	 type);

void summary_delete		  (SummaryView		*summaryview);
void summary_delete_duplicated	  (SummaryView		*summaryview);
void summary_execute		  (SummaryView		*summaryview);
void summary_attract_by_subject	  (SummaryView		*summaryview);
gint summary_write_cache	  (SummaryView		*summaryview);
void summary_pass_key_press_event (SummaryView		*summaryview,
				   GdkEventKey		*event);
void summary_change_display_item  (SummaryView		*summaryview);
void summary_redisplay_msg	  (SummaryView		*summaryview);
void summary_open_msg		  (SummaryView		*summaryview);
void summary_view_source	  (SummaryView		*summaryview);
void summary_reedit		  (SummaryView		*summaryview);
void summary_step		  (SummaryView		*summaryview,
				   GtkScrollType	 type);
void summary_set_marks_selected	  (SummaryView		*summaryview);

void summary_move_selected_to	  (SummaryView		*summaryview,
				   FolderItem		*to_folder);
void summary_move_to		  (SummaryView		*summaryview);
void summary_copy_selected_to	  (SummaryView		*summaryview,
				   FolderItem		*to_folder);
void summary_copy_to		  (SummaryView		*summaryview);
void summary_save_as		  (SummaryView		*summaryview);
void summary_print		  (SummaryView		*summaryview);
void summary_mark		  (SummaryView		*summaryview);
void summary_unmark		  (SummaryView		*summaryview);
void summary_mark_as_unread	  (SummaryView		*summaryview);
void summary_mark_as_read	  (SummaryView		*summaryview);
void summary_select_all		  (SummaryView		*summaryview);
void summary_unselect_all	  (SummaryView		*summaryview);
void summary_set_label		  (SummaryView		*summaryview, guint labelcolor, GtkWidget *widget);
void summary_set_label_color	  (GtkCTree *ctree, GtkCTreeNode *node, guint labelcolor);

#endif /* __SUMMARY_H__ */
