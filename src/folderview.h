/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999,2000 Hiroyuki Yamamoto
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

#ifndef __FOLDERVIEW_H__
#define __FOLDERVIEW_H__

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkctree.h>

#include "folder.h"

typedef struct _FolderView	FolderView;

#include "mainwindow.h"
#include "summaryview.h"

struct _FolderView
{
	GtkWidget *scrolledwin;
	GtkWidget *ctree;
	GtkWidget *mail_popup;
	GtkWidget *imap_popup;
	GtkWidget *news_popup;

	GtkItemFactory *mail_factory;
	GtkItemFactory *imap_factory;
	GtkItemFactory *news_factory;

	GtkCTreeNode *selected;
	GtkCTreeNode *opened;

	gboolean open_folder;

	GdkColor color_new;
	GdkColor color_normal;

	MainWindow   *mainwin;
	SummaryView  *summaryview;
};

FolderView *folderview_create		(void);
void folderview_init			(FolderView	*folderview);
void folderview_set			(FolderView	*folderview);
void folderview_set_imap_account	(FolderView	*folderview);
void folderview_select			(FolderView	*folderview,
					 FolderItem	*item);
void folderview_unselect		(FolderView	*folderview);
void folderview_select_next_unread	(FolderView	*folderview);
void folderview_update_msg_num		(FolderView	*folderview,
					 GtkCTreeNode	*row,
					 gint		 new,
					 gint		 unread,
					 gint		 total);
void folderview_update_all		(void);

void folderview_update_item		(FolderItem	*item,
					 gboolean	 update_summary);
void folderview_update_item_foreach	(GHashTable	*table);

void folderview_new_folder		(FolderView	*folderview);
void folderview_rename_folder		(FolderView	*folderview);
void folderview_delete_folder		(FolderView	*folderview);

#endif /* __FOLDERVIEW_H__ */
