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

#ifndef __CUSTOM_TOOLBAR_H__
#define __CUSTOM_TOOLBAR_H__

#define SEPARATOR        "separator"
#define TOOLBAR_FILE     "toolbar.xml"

#define SEPARATOR_PIXMAP "---"

typedef enum 
{
	A_RECEIVE_ALL = 0,
	A_RECEIVE_CUR,
	A_SEND_QUEUED,
	A_COMPOSE_EMAIL,
	A_REPLY_MESSAGE,
	A_REPLY_SENDER,
	A_REPLY_ALL,
	A_FORWARD,
	A_DELETE,
	A_EXECUTE,
	A_GOTO_NEXT,
	A_SYL_ACTIONS,
	
	N_ACTION_VAL
} CTActionVal;

#define A_COMPOSE_NEWS N_ACTION_VAL + 1
#define A_SEPARATOR    N_ACTION_VAL + 2

typedef struct _ToolbarAction ToolbarAction;
struct _ToolbarAction
{
	gchar *action_text;
	gchar *descr;
	void (*func)(GtkWidget *widget, gpointer data);
};

typedef struct _ToolbarItem ToolbarItem;
struct _ToolbarItem 
{
	gchar *file;
	gchar *text;
	gint  action;
};

typedef struct _ToolbarSylpheedActions ToolbarSylpheedActions;
struct _ToolbarSylpheedActions
{
	GtkWidget *widget;
	gchar     *name;
};

typedef enum 
{
	COMPOSEBUTTON_MAIL,
	COMPOSEBUTTON_NEWS
} ComposeButtonType;

typedef struct _MainToolbar MainToolbar;

struct _MainToolbar {

	GtkWidget *toolbar;

	GtkWidget *get_btn;
	GtkWidget *getall_btn;
	GtkWidget *sel_down;
	GtkWidget *sel_down_all;
	GtkWidget *sel_down_cur;
	GtkWidget *send_btn;

	GtkWidget *compose_mail_btn;
	GtkWidget *compose_news_btn;

	GtkWidget *reply_btn;
	GtkWidget *replysender_btn;
	GtkWidget *replyall_btn;

	GtkWidget *fwd_btn;

	GtkWidget *delete_btn;
	GtkWidget *next_btn;
	GtkWidget *exec_btn;

	GSList    *syl_action;
	GtkWidget *separator;

	/* for the reply buttons */
	GtkWidget *reply_popup;
	GtkWidget *replyall_popup;
	GtkWidget *replysender_popup;
	
	/* the forward button similar to the reply buttons*/
	GtkWidget *fwd_popup;

	ComposeButtonType compose_btn_type;
};

extern GSList *toolbar_list;

void      toolbar_actions_cb               (GtkWidget          *widget, 
					    ToolbarItem        *toolbar_item);

GList    *toolbar_get_action_items         (void);
void      toolbar_save_config_file         (void);
void      toolbar_read_config_file         (void);
void      toolbar_set_default_toolbar      (void);
void      toolbar_clear_list               (void);
void      toolbar_update                   (void);
void      toolbar_destroy                  (MainWindow         *mainwin);

gint      toolbar_ret_val_from_descr       (gchar              *descr);
gchar    *toolbar_ret_descr_from_val       (gint               val);
gchar    *toolbar_ret_text_from_val        (gint               val);
void      toolbar_create                   (MainWindow         *mainwin,
					    GtkWidget          *container);

void      toolbar_set_sensitive            (MainWindow         *mainwin);
void      toolbar_set_compose_button       (MainToolbar        *toolbar, 
					    ComposeButtonType  compose_btn_type);
#endif /* __CUSTOM_TOOLBAR_H__ */
