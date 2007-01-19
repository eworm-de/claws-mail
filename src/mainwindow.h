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

#define OFFLINE_SWITCH_HOOKLIST "offline_switch"
#define ACCOUNT_LIST_CHANGED_HOOKLIST "account_list_changed"

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
	M_DELAY_EXEC	      = 1 << 14,
	M_NOT_NEWS	      = 1 << 15,
	M_CAN_LEARN_SPAM      = 1 << 16,
	M_ACTIONS_EXIST       = 1 << 17,
	M_HAVE_QUEUED_MAILS   = 1 << 18,
	M_WANT_SYNC	      = 1 << 19
} SensitiveCond;

typedef enum
{
	NORMAL_LAYOUT	 = 0,
	VERTICAL_LAYOUT	 = 1 << 0,
	WIDE_LAYOUT = 1 << 1
} LayoutType;

typedef enum
{	
	TOOLBAR_NONE		= 0,
	TOOLBAR_ICON		= 1,
	TOOLBAR_TEXT		= 2,
	TOOLBAR_BOTH		= 3,
	TOOLBAR_BOTH_HORIZ	= 4
} ToolbarStyle;

struct _MainWindow
{
	GtkWidget *hpaned;
	GtkWidget *vpaned;

	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *menubar;

	GtkItemFactory *menu_factory;

	/* Toolbar handlebox */
	GtkWidget *handlebox;
	Toolbar *toolbar;

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
	GtkWidget *online_pixmap;
	GtkWidget *offline_pixmap;

	/* context IDs for status bar */
	gint mainwin_cid;
	gint folderview_cid;
	gint summaryview_cid;
	gint messageview_cid;

	ToolbarStyle toolbar_style;

	guint lock_count;
	guint menu_lock_count;
	guint cursor_count;

	FolderView	*folderview;
	SummaryView	*summaryview;
	MessageView	*messageview;
	LogWindow	*logwin;

	gint	progressindicator_hook;
	
	GtkWidget 	*colorlabel_menu;
	GtkWidget	*warning_btn;
	
#ifdef HAVE_LIBSM
	gpointer smc_conn;
#endif
};

MainWindow *main_window_create		(void);

void main_window_destroy                (MainWindow *mainwin);

void main_window_update_actions_menu	(MainWindow	*mainwin);

void main_window_cursor_wait		(MainWindow	*mainwin);
void main_window_cursor_normal		(MainWindow	*mainwin);

void main_window_lock			(MainWindow	*mainwin);
void main_window_unlock			(MainWindow	*mainwin);

void main_window_reflect_prefs_all_real	(gboolean 	 pixmap_theme_changed);
void main_window_reflect_prefs_all	(void);
void main_window_reflect_prefs_all_now	(void);
void main_window_reflect_prefs_custom_colors(MainWindow 	*mainwindow);
void main_window_set_summary_column	(void);
void main_window_set_folder_column	(void);
void main_window_set_account_menu	(GList		*account_list);
void main_window_set_account_menu_only_toolbar	(GList		*account_list);

/* Mailing list support */
void main_create_mailing_list_menu 	(MainWindow *mainwin, MsgInfo *msginfo);
gint mailing_list_get_list_post_mailto 	(gchar **url, gchar *mailto, gint maxlen);

void main_window_toggle_message_view	(MainWindow *mainwin);

void main_window_get_size		(MainWindow	*mainwin);
void main_window_get_position		(MainWindow	*mainwin);

void main_window_progress_on		(MainWindow	*mainwin);
void main_window_progress_off		(MainWindow	*mainwin);
void main_window_progress_set		(MainWindow	*mainwin,
					 gint		 cur,
					 gint		 total);

void main_window_empty_trash		(MainWindow	*mainwin,
					 gboolean	 confirm);
void main_window_add_mailbox		(MainWindow	*mainwin);

void main_window_set_menu_sensitive	(MainWindow	*mainwin);


void main_window_show			(MainWindow 	*mainwin);
void main_window_hide			(MainWindow 	*mainwin);
void main_window_popup			(MainWindow	*mainwin);

void main_window_toolbar_set_compose_button	(MainWindow *mainwin, 
						 ComposeButtonType compose_btn_type);

SensitiveCond main_window_get_current_state   (MainWindow *mainwin);

void toolbar_set_compose_button               (Toolbar		 *toolbar, 
					       ComposeButtonType  compose_btn_type);
void main_window_destroy_all                  (void);

void main_window_toggle_work_offline          (MainWindow        *mainwin, 
                                               gboolean           offline,
					       gboolean		  ask_sync);

/* public so it can be disabled from summaryview */
gboolean mainwindow_key_pressed		      (GtkWidget 	 *widget, 
					       GdkEventKey 	 *event,
				   	       gpointer 	  data);
MainWindow *mainwindow_get_mainwindow 	      (void);
void mainwindow_learn			      (MainWindow *mainwin,
					       gboolean is_spam);
void mainwindow_jump_to			      (const gchar 	 *target);
void mainwindow_show_error		      (void);
void mainwindow_clear_error		      (MainWindow *mainwin);
#endif /* __MAINWINDOW_H__ */
