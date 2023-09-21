/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 *  Copyright (C) 1999-2023 the Claws Mail team and Hiroyuki Yamamoto
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

#ifndef __MAINWINDOW_H__
#define __MAINWINDOW_H__

#include <glib.h>

typedef struct _MainWindow  MainWindow;

#include "viewtypes.h"
#include "logwindow.h"
#include "procmsg.h"
#include "toolbar.h"

#define OFFLINE_SWITCH_HOOKLIST "offline_switch"
#define ACCOUNT_LIST_CHANGED_HOOKLIST "account_list_changed"
#define MAIN_WINDOW_CLOSE "mainwindow_close"
#define MAIN_WINDOW_GOT_ICONIFIED "mainwindow_iconified"
#define THEME_CHANGED_HOOKLIST "theme_changed"
typedef enum
{
	M_UNLOCKED,
	M_MSG_EXIST,
	M_TARGET_EXIST,
	M_SINGLE_TARGET_EXIST,
	M_EXEC,
	M_ALLOW_REEDIT,
	M_HAVE_ACCOUNT,
	M_THREADED,
	M_UNTHREADED,
	M_ALLOW_DELETE,
	M_INC_ACTIVE,
	M_SEND_ACTIVE,
	M_NEWS,
	M_HAVE_NEWS_ACCOUNT,
	M_HIDE_READ_MSG,
	M_DELAY_EXEC,
	M_NOT_NEWS,
	M_CAN_LEARN_SPAM,
	M_ACTIONS_EXIST,
	M_HAVE_QUEUED_MAILS,
	M_WANT_SYNC,
	M_TAGS_EXIST,
	M_HAVE_PROCESSING,
	M_SUMMARY_ISLIST,
	M_IN_MSGLIST,
	M_HAVE_MULTI_ACCOUNT,
	M_FOLDER_SELECTED,
	M_SESSION_PASSWORDS,
	M_MASTER_PASSPHRASE,
	M_DELETED_EXISTS,
	M_NOT_TRASH,
	M_HIDE_READ_THREADS,
	M_HAVE_RETRIEVABLE_ACCOUNT,
	M_HAVE_ANY_RETRIEVABLE_ACCOUNT,
	M_NOT_DRAFT,
	M_MSG_SELECTED,

/* reserved */
    M_MAX_RESERVED
} SensitiveCond;

typedef guint64 SensitiveCondMask;

typedef enum
{
	NORMAL_LAYOUT       = 0,
	VERTICAL_LAYOUT     = 1 << 0,
	WIDE_LAYOUT         = 1 << 1,
	WIDE_MSGLIST_LAYOUT = 1 << 2,
	SMALL_LAYOUT
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
	LogWindow	*filtering_debugwin;

	gint	progressindicator_hook;
	
	GtkWidget 	*colorlabel_menu;
	GtkWidget	*warning_btn;
	GtkWidget 	*tags_menu;
	
	gboolean	 in_folder;
	GtkActionGroup	*action_group;
	GtkUIManager	*ui_manager;

#ifdef HAVE_LIBSM
	gpointer smc_conn;
#endif
};

MainWindow *main_window_create		(void);

void main_window_update_actions_menu	(MainWindow	*mainwin);

void main_window_cursor_wait		(MainWindow	*mainwin);
void main_window_cursor_normal		(MainWindow	*mainwin);

void main_window_lock			(MainWindow	*mainwin);
void main_window_unlock			(MainWindow	*mainwin);

void main_window_reflect_prefs_all_real	(gboolean 	 pixmap_theme_changed);
void main_window_reflect_prefs_all	(void);
void main_window_reflect_prefs_all_now	(void);
void main_window_reflect_prefs_custom_colors(MainWindow 	*mainwindow);
void main_window_reflect_tags_changes(MainWindow 	*mainwindow);
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
gboolean main_window_empty_trash	(MainWindow	*mainwin,
					 gboolean	 confirm,
					 gboolean 	 for_quit);

SensitiveCondMask main_window_get_mask(SensitiveCond cond, ...);

void main_window_set_menu_sensitive	(MainWindow	*mainwin);


void main_window_show			(MainWindow 	*mainwin);
void main_window_hide			(MainWindow 	*mainwin);
void main_window_popup			(MainWindow	*mainwin);

SensitiveCondMask main_window_get_current_state   (MainWindow *mainwin);

void toolbar_set_compose_button               (Toolbar		 *toolbar, 
					       ComposeButtonType  compose_btn_type);
void main_window_destroy_all                  (void);

void main_window_toggle_work_offline          (MainWindow        *mainwin, 
                                               gboolean           offline,
					       gboolean		  ask_sync);

MainWindow *mainwindow_get_mainwindow 	      (void);
void mainwindow_learn			      (MainWindow *mainwin,
					       gboolean is_spam);
void mainwindow_jump_to			      (const gchar 	 *target,
					       gboolean popup);
void mainwindow_show_error		      (void);
void mainwindow_clear_error		      (MainWindow *mainwin);
void mainwindow_delete_duplicated     (MainWindow *mainwin);
void mainwindow_delete_duplicated_all (MainWindow *mainwin);
gboolean mainwindow_is_obscured		      (void);
void mainwindow_exit_folder		      (MainWindow *mainwin);
void mainwindow_enter_folder		      (MainWindow *mainwin);
void mainwindow_reset_paned		      (GtkPaned *paned);

void mainwin_accel_changed_cb (GtkAccelGroup *accelgroup, guint keyval, GdkModifierType modifier,
				  GClosure *closure, GtkMenuItem *item);

#endif /* __MAINWINDOW_H__ */
