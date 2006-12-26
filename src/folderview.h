/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto and the Claws Mail team
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

#ifndef __FOLDERVIEW_H__
#define __FOLDERVIEW_H__

typedef struct _FolderView	FolderView;
typedef struct _FolderViewPopup	FolderViewPopup;
typedef struct _FolderColumnState	FolderColumnState;

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkctree.h>

#include "mainwindow.h"
#include "summaryview.h"
#include "folder.h"

typedef enum
{
	F_COL_FOLDER,
	F_COL_NEW,
	F_COL_UNREAD,
	F_COL_TOTAL
} FolderColumnType;

#define N_FOLDER_COLS	4

struct _FolderColumnState
{
	FolderColumnType type;
	gboolean visible;
};

struct _FolderView
{
	GtkWidget *scrolledwin;
	GtkWidget *ctree;

	GHashTable *popups;

	GtkCTreeNode *selected;
	GtkCTreeNode *opened;

	gboolean open_folder;

	GdkColor color_new;
	GdkColor color_op;

	MainWindow   *mainwin;
	SummaryView  *summaryview;

	gint folder_update_callback_id;
	gint folder_item_update_callback_id;
	
	/* DND states */
	GSList *nodes_to_recollapse;
	guint   drag_timer;		/* timer id */
	FolderItem *drag_item;		/* dragged item */
	GtkCTreeNode *drag_node;	/* drag node */
	
	GtkTargetList *target_list; /* DnD */
	FolderColumnState col_state[N_FOLDER_COLS];
	gint col_pos[N_FOLDER_COLS];
};

struct _FolderViewPopup
{
	gchar		 *klass;
	gchar		 *path;
	GSList		 *entries;
	void		(*set_sensitivity)	(GtkItemFactory *menu, FolderItem *item);
};

void folderview_initialize		(void);
FolderView *folderview_create		(void);
void folderview_init			(FolderView	*folderview);

void folderview_set			(FolderView	*folderview);
void folderview_set_all			(void);

void folderview_select			(FolderView	*folderview,
					 FolderItem	*item);
void folderview_unselect		(FolderView	*folderview);
void folderview_select_next_unread	(FolderView	*folderview, 
					 gboolean 	 force_open);
void folderview_select_next_new		(FolderView	*folderview);
void folderview_select_next_marked	(FolderView	*folderview);

FolderItem *folderview_get_selected_item(FolderView	*folderview);

void folderview_update_msg_num		(FolderView	*folderview,
					 GtkCTreeNode	*row);

void folderview_append_item		(FolderItem	*item);

void folderview_rescan_tree		(Folder		*folder,
					 gboolean	 rebuild);
void folderview_rescan_all		(void);
gint folderview_check_new		(Folder		*folder);
void folderview_check_new_all		(void);

void folderview_update_item_foreach	(GHashTable	*table,
					 gboolean	 update_summary);
void folderview_update_all_updated	(gboolean	 update_summary);

void folderview_move_folder		(FolderView 	*folderview,
					 FolderItem 	*from_folder,
					 FolderItem 	*to_folder,
					 gboolean	 copy);

void folderview_set_target_folder_color (gint		color_op);

void folderview_reflect_prefs_pixmap_theme	(FolderView *folderview);

void folderview_reflect_prefs		(void);
void folderview_register_popup		(FolderViewPopup	*fpopup);
void folderview_unregister_popup	(FolderViewPopup	*fpopup);
void folderview_update_search_icon	(FolderItem 		*item, 	
					 gboolean 		 matches);
void folderview_set_column_order	(FolderView		*folderview);
void folderview_finish_dnd		(const gchar 		*data, 
					 GdkDragContext 	*drag_context,
			   		 guint 			 time, 
					 FolderItem 		*item);

#endif /* __FOLDERVIEW_H__ */
