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

#define SEPARATOR            "separator"
#define SEPARATOR_PIXMAP     "---"

typedef enum {
	TOOLBAR_MAIN,	
	TOOLBAR_COMPOSE,
} Toolbar;

#define TOOLBAR_DESTROY_ITEMS(t_item_list) \
{ \
        ToolbarItem *t_item; \
	while (t_item_list != NULL) { \
		t_item = (ToolbarItem*)t_item_list->data; \
		t_item_list = g_slist_remove(t_item_list, t_item); \
		if (t_item->file) \
			g_free(t_item->file); \
		if (t_item->text) \
			g_free(t_item->text);\
		g_free(t_item);\
	}\
	g_slist_free(t_item_list);\
}

#define TOOLBAR_DESTROY_ACTIONS(t_action_list) \
{ \
	ToolbarSylpheedActions *t_action; \
	while (t_action_list != NULL) { \
		t_action = (ToolbarSylpheedActions*)t_action_list->data;\
		t_action_list = \
			g_slist_remove(t_action_list, t_action);\
		if (t_action->name) \
			g_free(t_action->name); \
		g_free(t_action); \
	} \
	g_slist_free(t_action_list); \
}

typedef struct _ToolbarConfig ToolbarConfig;
struct _ToolbarConfig {
	const gchar  *conf_file;
	GSList       *item_list;
};


/* enum holds available actions for both 
   Compose Toolbar and Main Toolbar 
*/
enum {
	/* main toolbar */
	A_RECEIVE_ALL = 0,
	A_RECEIVE_CUR,
	A_SEND_QUEUED,
	A_COMPOSE_EMAIL,
	A_COMPOSE_NEWS,
	A_REPLY_MESSAGE,
	A_REPLY_SENDER,
	A_REPLY_ALL,
	A_REPLY_ML,
	A_FORWARD,
	A_DELETE,
	A_EXECUTE,
	A_GOTO_NEXT,

	/* compose toolbar */
	A_SEND,
	A_SENDL,
	A_DRAFT,
	A_INSERT,
	A_ATTACH,
	A_SIG,
	A_EXTEDITOR,
	A_LINEWRAP,
	A_ADDRBOOK,

	/* common items */
	A_SYL_ACTIONS,
	A_SEPARATOR,

	N_ACTION_VAL
};

typedef struct _ToolbarText ToolbarText;
struct _ToolbarText 
{
	gchar *index_str;
	gchar *descr;
};

typedef struct _ToolbarItem ToolbarItem;
struct _ToolbarItem 
{
	gint      index;
	gchar    *file;
	gchar    *text;
	gpointer  parent;
};

typedef struct _ToolbarSylpheedActions ToolbarSylpheedActions;
struct _ToolbarSylpheedActions
{
	GtkWidget *widget;
	gchar     *name;
};


void      toolbar_action_execute           (GtkWidget           *widget,
					    GSList              *action_list, 
					    gpointer            data,
					    gint                source);

GList    *toolbar_get_action_items         (Toolbar            source);

void      toolbar_save_config_file         (Toolbar            source);
void      toolbar_read_config_file         (Toolbar            source);

void      toolbar_set_default              (Toolbar            source);
void      toolbar_clear_list               (Toolbar            source);

GSList   *toolbar_get_list                 (Toolbar            source);
void      toolbar_set_list_item            (ToolbarItem        *t_item, 
					    Toolbar            source);

gint      toolbar_ret_val_from_descr       (const gchar        *descr);
gchar    *toolbar_ret_descr_from_val       (gint                val);

void      toolbar_destroy_items            (GSList             *t_item_list);
#endif /* __CUSTOM_TOOLBAR_H__ */
