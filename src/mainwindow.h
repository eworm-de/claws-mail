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

#ifndef __MAINWINDOW_H__
#define __MAINWINDOW_H__

#include <glib.h>

typedef struct _MainWindow  MainWindow;

#include "folderview.h"
#include "summaryview.h"
#include "headerview.h"
#include "messageview.h"
#include "logwindow.h"
#include "toolbar.h"

typedef enum
{
	M_UNLOCKED            = 1 << 0,
	M_MSG_EXIST           = 1 << 1,
	M_TARGET_EXIST        = 1 << 2,
	M_SINGLE_TARGET_EXIST = 1 << 3,
	M_EXEC                = 1 << 4,
	M_ALLOW_REEDIT        = 1 << 5,
	M_HAVE_ACCOUNT        = 1 << 6,
	M_THREADED	      = 1 << 7,
	M_UNTHREADED	      = 1 << 8,
	M_ALLOW_DELETE	      = 1 << 9,
	M_INC_ACTIVE	      = 1 << 10,
	M_NEWS                = 1 << 11,
	M_HAVE_NEWS_ACCOUNT   = 1 << 12,
	M_HIDE_READ_MSG	      = 1 << 13,
	M_DELAY_EXEC	      = 1 << 14
} SensitiveCond;

typedef enum
{
	SEPARATE_NONE	 = 0,
	SEPARATE_FOLDER	 = 1 << 0,
	SEPARATE_MESSAGE = 1 << 1,
	SEPARATE_BOTH	 = (SEPARATE_FOLDER | SEPARATE_MESSAGE)
} SeparateType;

typedef enum
{
	TOOLBAR_NONE	= 0,
	TOOLBAR_ICON	= 1,
	TOOLBAR_TEXT	= 2,
	TOOLBAR_BOTH	= 3
} ToolbarStyle;

struct _MainWindow
{
	SeparateType type;

	union CompositeWin {
		struct 
		{
			GtkWidget *hpaned;
			GtkWidget *vpaned;
		} sep_none;
		struct {
			GtkWidget *folderwin;
			GtkWidget *vpaned;
		} sep_folder;
		struct {
			GtkWidget *messagewin;
			GtkWidget *hpaned;
		} sep_message;
		struct {
			GtkWidget *folderwin;
			GtkWidget *messagewin;
		} sep_both;
	} win;

	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *menubar;

	GtkItemFactory *menu_factory;
	
	/* Toolbar handlebox */
	GtkWidget *handlebox;
	
	MainToolbar *toolbar;

	/* body */
	GtkWidget *vbox_body;
	GtkWidget *hbox_stat;
	GtkWidget *statusbar;
	GtkWidget *progressbar;
	GtkWidget *statuslabel;
	GtkWidget *ac_button;
	GtkWidget *ac_label;
	GtkWidget *ac_menu;
	GtkWidget *online_switch;
	GtkWidget *offline_switch;

	/* context IDs for status bar */
	gint mainwin_cid;
	gint folderview_cid;
	gint summaryview_cid;

	ToolbarStyle toolbar_style;

	guint lock_count;
	guint menu_lock_count;
	guint cursor_count;

	FolderView	*folderview;
	SummaryView	*summaryview;
	MessageView	*messageview;
	LogWindow	*logwin;
};

MainWindow *main_window_create		(SeparateType	 type);

void main_window_cursor_wait		(MainWindow	*mainwin);
void main_window_cursor_normal		(MainWindow	*mainwin);

void main_window_lock			(MainWindow	*mainwin);
void main_window_unlock			(MainWindow	*mainwin);

void main_window_reflect_prefs_all_real		(gboolean pixmap_theme_changed);
void main_window_reflect_prefs_all	(void);
void main_window_set_summary_column	(void);
void main_window_set_account_menu	(GList		*account_list);
void main_window_separation_change	(MainWindow	*mainwin,
					 SeparateType	 type);

void main_window_toggle_message_view	(MainWindow *mainwin);

void main_window_get_size		(MainWindow	*mainwin);
void main_window_get_position		(MainWindow	*mainwin);

void main_window_empty_trash		(MainWindow	*mainwin,
					 gboolean	 confirm);
void main_window_add_mailbox		(MainWindow	*mainwin);

void main_window_set_menu_sensitive	(MainWindow	*mainwin);


void main_window_popup			(MainWindow	*mainwin);

void main_window_toolbar_set_compose_button	(MainWindow *mainwin, 
					 ComposeButtonType compose_btn_type);

SensitiveCond main_window_get_current_state   (MainWindow *mainwin);

void reply_cb			          (MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

void inc_mail_cb			          (MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

void inc_all_account_mail_cb	          (MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

void send_queue_cb		          (MainWindow	*mainwin,
					 guint		 action,
					 GtkWidget	*widget);

void compose_mail_cb                          (MainWindow       *mainwin, 
					 guint action,
			                   GtkWidget         *widget);

void compose_news_cb                          (MainWindow       *mainwin, 
					 guint action,
			                   GtkWidget         *widget);

void next_unread_cb	                   (MainWindow	*mainwin,
				          guint		 action,
				          GtkWidget	*widget);

#endif /* __MAINWINDOW_H__ */
