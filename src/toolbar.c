/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2001 Hiroyuki Yamamoto
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

/*
 * General functions for accessing address book files.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <math.h>
#include <setjmp.h>

#include "intl.h"
#include "utils.h"
#include "xml.h"
#include "mgutils.h"
#include "prefs.h"
#include "codeconv.h"
#include "stock_pixmap.h"
#include "mainwindow.h"
#include "prefs_common.h"
#include "menu.h"
#include "prefs_actions.h"
#include "manage_window.h"

#include "toolbar.h"
#include "prefs_toolbar.h"

/* elements */
#define TOOLBAR_TAG_INDEX        "toolbar"
#define TOOLBAR_TAG_ITEM         "item"
#define TOOLBAR_TAG_SEPARATOR    SEPARATOR

jmp_buf    jumper;

GSList *toolbar_list;

MainWindow *mwin;

#define TOOLBAR_ICON_FILE   "file"    
#define TOOLBAR_ICON_TEXT   "text"     
#define TOOLBAR_ICON_ACTION "action"    

gboolean      toolbar_is_duplicate           (gint action);
static void   toolbar_parse_item             (XMLFile *file);
static gint   toolbar_ret_val_from_text      (gchar *text);


/* callback functions */
static void toolbar_inc_cb		(GtkWidget	*widget,
				         gpointer	 data);

static void toolbar_inc_all_cb	        (GtkWidget	*widget,
				         gpointer	 data);

static void toolbar_send_cb	        (GtkWidget	*widget,
				         gpointer	 data);

static void toolbar_compose_cb	        (GtkWidget	*widget,
				         gpointer	 data);

static void toolbar_reply_cb	        (GtkWidget	*widget,
				         gpointer	 data);

static void toolbar_reply_to_all_cb	(GtkWidget	*widget,
				         gpointer	 data);

static void toolbar_reply_to_sender_cb	(GtkWidget	*widget,
					 gpointer	 data);

static void toolbar_forward_cb	        (GtkWidget	*widget,
				         gpointer	 data);

static void toolbar_delete_cb	        (GtkWidget	*widget,
					 gpointer	 data);

static void toolbar_exec_cb	        (GtkWidget	*widget,
					 gpointer	 data);

static void toolbar_next_unread_cb	(GtkWidget	*widget,
				 	 gpointer	 data);

static void toolbar_actions_execute_cb	(GtkWidget	*widget,
					 gpointer	 data);

static void toolbar_reply_popup_cb	       (GtkWidget	*widget,
						GdkEventButton  *event,
						gpointer	 data);
static void toolbar_reply_popup_closed_cb      (GtkMenuShell	*menu_shell,
						gpointer	 data);

static void toolbar_reply_to_all_popup_cb      (GtkWidget	*widget,
						GdkEventButton  *event,
						gpointer	 data);

static void toolbar_reply_to_all_popup_closed_cb
					(GtkMenuShell	*menu_shell,
					 gpointer	 data);

static void toolbar_reply_to_sender_popup_cb(GtkWidget	*widget,
					 GdkEventButton *event,
					 gpointer	 data);
static void toolbar_reply_to_sender_popup_closed_cb
					(GtkMenuShell	*menu_shell,
					 gpointer	 data);

static void toolbar_forward_popup_cb	 (GtkWidget	 *widget,
					 GdkEventButton    *event,
					 gpointer	 data);

static void toolbar_forward_popup_closed_cb   		
					(GtkMenuShell	*menu_shell,
					 gpointer	 data);

static void activate_compose_button     (MainToolbar       *toolbar,
					 ToolbarStyle      style,
					 ComposeButtonType type);
static ToolbarAction t_action[] = 
{
	{ "A_RECEIVE_ALL",   N_("Receive Mail on all Accounts"),    toolbar_inc_all_cb        },
	{ "A_RECEIVE_CUR",   N_("Receive Mail on current Account"), toolbar_inc_cb            },
	{ "A_SEND_QUEUED",   N_("Send Queued Message(s)"),          toolbar_send_cb           },
	{ "A_COMPOSE_EMAIL", N_("Compose Email"),                   toolbar_compose_cb        },
	{ "A_REPLY_MESSAGE", N_("Reply to Message"),                toolbar_reply_cb          },
	{ "A_REPLY_SENDER",  N_("Reply to Sender"),                 toolbar_reply_to_sender_cb},
	{ "A_REPLY_ALL",     N_("Reply to All"),                    toolbar_reply_to_all_cb   },
	{ "A_FORWARD",       N_("Forward Message"),                 toolbar_forward_cb        },
	{ "A_DELETE",        N_("Delete Message"),                  toolbar_delete_cb         },
	{ "A_EXECUTE",       N_("Execute"),                         toolbar_exec_cb           },
	{ "A_GOTO_NEXT",     N_("Goto Next Message"),               toolbar_next_unread_cb    },
	{ "A_SYL_ACTIONS",   N_("Sylpheed Actions Feature"),        toolbar_actions_execute_cb},

	{ "A_COMPOSE_NEWS",  N_("Compose News"),                    toolbar_compose_cb        },    
	{ "A_SEPARATOR",     SEPARATOR,                             NULL                      }
};

static GtkItemFactoryEntry reply_popup_entries[] =
{
	{N_("/Reply with _quote"), NULL, reply_cb, COMPOSE_REPLY_WITH_QUOTE, NULL},
	{N_("/_Reply without quote"), NULL, reply_cb, COMPOSE_REPLY_WITHOUT_QUOTE, NULL}
};
static GtkItemFactoryEntry replyall_popup_entries[] =
{
	{N_("/Reply to all with _quote"), "<shift>A", reply_cb, COMPOSE_REPLY_TO_ALL_WITH_QUOTE, NULL},
	{N_("/_Reply to all without quote"), "a", reply_cb, COMPOSE_REPLY_TO_ALL_WITHOUT_QUOTE, NULL}
};
static GtkItemFactoryEntry replysender_popup_entries[] =
{
	{N_("/Reply to sender with _quote"), NULL, reply_cb, COMPOSE_REPLY_TO_SENDER_WITH_QUOTE, NULL},
	{N_("/_Reply to sender without quote"), NULL, reply_cb, COMPOSE_REPLY_TO_SENDER_WITHOUT_QUOTE, NULL}
};
static GtkItemFactoryEntry fwd_popup_entries[] =
{
	{N_("/_Forward message (inline style)"), "f", reply_cb, COMPOSE_FORWARD_INLINE, NULL},
	{N_("/Forward message as _attachment"), "<shift>F", reply_cb, COMPOSE_FORWARD_AS_ATTACH, NULL}
};


void toolbar_actions_cb(GtkWidget *widget, ToolbarItem *toolbar_item)
{
	if (toolbar_item->action < sizeof(t_action) / sizeof(t_action[0]))
		t_action[toolbar_item->action].func(widget, mwin);
}

gint toolbar_ret_val_from_descr(gchar *descr)
{
	gint i;
	
	for (i = 0; i < sizeof(t_action) / sizeof(t_action[0]); i++) {
		if (g_strcasecmp(t_action[i].descr, descr) == 0)
				return i;
	}
	
	return -1;
}

gchar *toolbar_ret_descr_from_val(gint val)
{
	g_return_val_if_fail(val >=0 && val <= sizeof(t_action) / sizeof(t_action[0]), NULL);

	return t_action[val].descr;

}

static gint toolbar_ret_val_from_text(gchar *text)
{
	gint i;
	
	for (i = 0; i < sizeof(t_action) / sizeof(t_action[0]); i++) {
		if (g_strcasecmp(t_action[i].action_text, text) == 0)
				return i;
	}
	
	return -1;
}

gchar *toolbar_ret_text_from_val(gint val)
{
	g_return_val_if_fail(val >=0 && val <= sizeof(t_action) / sizeof(t_action[0]), NULL);

	return t_action[val].action_text;
}

gboolean toolbar_is_duplicate(gint action)
{
	GSList *cur;

	if ((action == A_SEPARATOR) || (action == A_SYL_ACTIONS)) 
		return FALSE;

	for (cur = toolbar_list; cur != NULL; cur = cur->next) {
		ToolbarItem *item = (ToolbarItem*) cur->data;
		
		if (item->action == action)
			return TRUE;
	}
	return FALSE;
}

GList *toolbar_get_action_items(void)
{
	GList *items = NULL;
	gint i;
	
	for (i = 0; i < N_ACTION_VAL; i++) {
		items = g_list_append(items, t_action[i].descr);
	}

	return items;
}

static void toolbar_parse_item(XMLFile *file)
{
	GList *attr;
	gchar *name, *value;
	ToolbarItem *item = NULL;

	attr = xml_get_current_tag_attr(file);
	item = g_new0(ToolbarItem, 1);
	while( attr ) {
		name = ((XMLAttr *)attr->data)->name;
		value = ((XMLAttr *)attr->data)->value;
		
		if (g_strcasecmp(name, TOOLBAR_ICON_FILE) == 0) 
			item->file = g_strdup (value);
		else if (g_strcasecmp(name, TOOLBAR_ICON_TEXT) == 0)
			item->text = g_strdup (value);
		else if (g_strcasecmp(name, TOOLBAR_ICON_ACTION) == 0)
			item->action = toolbar_ret_val_from_text(value);

		attr = g_list_next(attr);
	}
	if (item->action != -1) {
		
		if ( !toolbar_is_duplicate(item->action)) 
			toolbar_list = g_slist_append(toolbar_list, item);
	}
}

void toolbar_set_default_toolbar(void)
{
	gint i;
	/* create default toolbar */
	struct _default_toolbar {
		gint action;
		gint icon;
		gchar *text;
	} default_toolbar[] = {
		{ A_RECEIVE_CUR,   STOCK_PIXMAP_MAIL_RECEIVE,         _("Get")     },
		{ A_RECEIVE_ALL,   STOCK_PIXMAP_MAIL_RECEIVE_ALL,     _("Get All") },
		{ A_SEPARATOR,     0,                                 ("")         }, 
		{ A_SEND_QUEUED,   STOCK_PIXMAP_MAIL_SEND_QUEUE,      _("Send")    },
		{ A_COMPOSE_EMAIL, STOCK_PIXMAP_MAIL_COMPOSE,         _("Email")   },
		{ A_SEPARATOR,     0,                                 ("")         },
		{ A_REPLY_MESSAGE, STOCK_PIXMAP_MAIL_REPLY,           _("Reply")   }, 
		{ A_REPLY_ALL,     STOCK_PIXMAP_MAIL_REPLY_TO_ALL,    _("All")     },
		{ A_REPLY_SENDER,  STOCK_PIXMAP_MAIL_REPLY_TO_AUTHOR, _("Sender")  },
		{ A_FORWARD,       STOCK_PIXMAP_MAIL_FORWARD,         _("Forward") },
		{ A_SEPARATOR,     0,                                 ("")         },
		{ A_DELETE,        STOCK_PIXMAP_CLOSE,                _("Delete")  },
		{ A_EXECUTE,       STOCK_PIXMAP_EXEC,                 _("Execute") },
		{ A_GOTO_NEXT,     STOCK_PIXMAP_DOWN_ARROW,           _("Next")    },
	};
		
	for (i = 0; i < sizeof(default_toolbar) / sizeof(default_toolbar[0]); i++) {
		
		ToolbarItem *toolbar_item = g_new0(ToolbarItem, 1);
		
		if (default_toolbar[i].action != A_SEPARATOR) {
			
			gchar *file = stock_pixmap_get_name((StockPixmap)default_toolbar[i].icon);
			
			toolbar_item->file   = g_strdup(file);
			toolbar_item->action = default_toolbar[i].action;
			toolbar_item->text   = g_strdup(default_toolbar[i].text);
		} else {

			toolbar_item->file   = g_strdup(SEPARATOR);
			toolbar_item->action = A_SEPARATOR;
		}
		
		if (toolbar_item->action != -1) {
			
			if ( !toolbar_is_duplicate(toolbar_item->action)) 
				toolbar_list = g_slist_append(toolbar_list, toolbar_item);
		}	
	}
}

void toolbar_save_config_file()
{
	GSList *cur;
	FILE *fp;
	PrefFile *pfile;
	gchar *fileSpec = NULL;

	debug_print("save Toolbar Configuration\n");

	fileSpec = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, TOOLBAR_FILE, NULL );
	pfile = prefs_write_open(fileSpec);
	g_free( fileSpec );
	if( pfile ) {
		fp = pfile->fp;
		fprintf(fp, "<?xml version=\"1.0\" encoding=\"%s\" ?>\n",
			conv_get_current_charset_str());

		fprintf(fp, "<%s>\n", TOOLBAR_TAG_INDEX);

		for (cur = toolbar_list; cur != NULL; cur = cur->next) {
			ToolbarItem *toolbar_item = (ToolbarItem*) cur->data;
			
			if (g_strcasecmp(toolbar_item->file, SEPARATOR) != 0) 
				fprintf(fp, "\t<%s %s=\"%s\" %s=\"%s\" %s=\"%s\"/>\n",
					TOOLBAR_TAG_ITEM, 
					TOOLBAR_ICON_FILE, toolbar_item->file,
					TOOLBAR_ICON_TEXT, toolbar_item->text,
					TOOLBAR_ICON_ACTION, 
					toolbar_ret_text_from_val(toolbar_item->action));
			else 
				fprintf(fp, "\t<%s/>\n", TOOLBAR_TAG_SEPARATOR); 
		}

		fprintf(fp, "</%s>\n", TOOLBAR_TAG_INDEX);	
	
		if (prefs_write_close (pfile) < 0 ) 
			g_warning("failed to write toolbar configuration to file\n");
	} else
		g_warning("failed to open toolbar configuration file for writing\n");
}

void toolbar_clear_list()
{
	while (toolbar_list != NULL) {
		ToolbarItem *item = (ToolbarItem*) toolbar_list->data;
		
		toolbar_list = g_slist_remove(toolbar_list, item);
		if (item->file)
			g_free(item->file);
		if (item->text)
			g_free(item->text);
		g_free(item);	
	}
	g_slist_free(toolbar_list);
}

void toolbar_read_config_file(void)
{
	XMLFile *file   = NULL;
	gchar *fileSpec = NULL;
	GList *attr;
	gboolean retVal;

	debug_print("read Toolbar Configuration\n");

	fileSpec = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, TOOLBAR_FILE, NULL );
	file = xml_open_file(fileSpec);
	g_free(fileSpec);

	toolbar_clear_list();

	if (file) {
		if ((setjmp(jumper))
		|| (xml_get_dtd(file))
		|| (xml_parse_next_tag(file))
		|| (!xml_compare_tag(file, TOOLBAR_TAG_INDEX))) {
			xml_close_file(file);
			return;
		}

		attr = xml_get_current_tag_attr(file);
		
		retVal = TRUE;
		for (;;) {
			if (!file->level) 
				break;
			/* Get item tag */
			if (xml_parse_next_tag(file)) 
				longjmp(jumper, 1);

			/* Get next tag (icon, icon_text or icon_action) */
			if (xml_compare_tag(file, TOOLBAR_TAG_ITEM)) {
				toolbar_parse_item(file);
			} else if (xml_compare_tag(file, TOOLBAR_TAG_SEPARATOR)) {
				ToolbarItem *item = g_new0(ToolbarItem, 1);
			
				item->file   = g_strdup(SEPARATOR);
				item->action = A_SEPARATOR;
				toolbar_list = g_slist_append(toolbar_list, item);
			}

		}
		xml_close_file(file);
	}
	else {
		/* save default toolbar */
		toolbar_set_default_toolbar();
		toolbar_save_config_file();
	}
}

static void toolbar_actions_execute_cb(GtkWidget *widget,
				       gpointer	 data)
{
	gint i = 0;
	GSList *cur, *lop;
	MainWindow *mainwin = (MainWindow*)data;
	gchar *action, *action_p;
	gboolean found = FALSE;

	for (cur = mainwin->toolbar->syl_action; cur != NULL;  cur = cur->next) {
		ToolbarSylpheedActions *act = (ToolbarSylpheedActions*)cur->data;

		if (widget == act->widget) {
			
			for (lop = prefs_common.actions_list; lop != NULL; lop = lop->next) {
				action = g_strdup((gchar*)lop->data);

				action_p = strstr(action, ": ");
				action_p[0] = 0x00;
				if (g_strcasecmp(act->name, action) == 0) {
					found = TRUE;
					g_free(action);
					break;
				} else 
					i++;
				g_free(action);
			}
			if (found) 
				break;
		}
	}

	if (found)
		actions_execute(mwin, i, widget);
	else
		g_warning ("Error: did not find Sylpheed Action to execute");
}

static void toolbar_inc_cb	         (GtkWidget	*widget,
					  gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	inc_mail_cb(mainwin, 0, NULL);
}

static void toolbar_inc_all_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	inc_all_account_mail_cb(mainwin, 0, NULL);
}

static void toolbar_send_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	send_queue_cb(mainwin, 0, NULL);
}

static void toolbar_compose_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (mainwin->toolbar->compose_btn_type == COMPOSEBUTTON_NEWS) 
		compose_news_cb(mainwin, 0, NULL);
	else
		compose_mail_cb(mainwin, 0, NULL);
}

static void toolbar_reply_cb(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	reply_cb(mainwin, 
		 prefs_common.reply_with_quote ? COMPOSE_REPLY_WITH_QUOTE 
		 : COMPOSE_REPLY_WITHOUT_QUOTE,
		 NULL);
}

static void toolbar_reply_to_all_cb(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	reply_cb(mainwin, 
		 prefs_common.reply_with_quote ? COMPOSE_REPLY_TO_ALL_WITH_QUOTE 
		 : COMPOSE_REPLY_TO_ALL_WITHOUT_QUOTE, 
		 NULL);
}


static void toolbar_reply_to_sender_cb(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	reply_cb(mainwin, 
		 prefs_common.reply_with_quote ? COMPOSE_REPLY_TO_SENDER_WITH_QUOTE 
		 : COMPOSE_REPLY_TO_SENDER_WITHOUT_QUOTE, 
		 NULL);
}

static void toolbar_forward_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (prefs_common.forward_as_attachment)
		reply_cb(mainwin, COMPOSE_FORWARD_AS_ATTACH, NULL);
	else
		reply_cb(mainwin, COMPOSE_FORWARD, NULL);
}

static void toolbar_delete_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	summary_delete(mainwin->summaryview);
}

static void toolbar_exec_cb	(GtkWidget	*widget,
				 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	summary_execute(mainwin->summaryview);
}

static void toolbar_next_unread_cb	(GtkWidget	*widget,
					 gpointer	 data)
{
	MainWindow *mainwin = (MainWindow *)data;

	next_unread_cb(mainwin, 0, NULL);
}

/* popup callback functions */
static void toolbar_reply_popup_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	MainWindow *mainwindow = (MainWindow *) data;
	
	if (!event) return;

	if (event->button == 3) {
		gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NORMAL);
		gtk_menu_popup(GTK_MENU(mainwindow->toolbar->reply_popup), NULL, NULL,
		       menu_button_position, widget,
		       event->button, event->time);
	}
}

static void toolbar_reply_popup_closed_cb(GtkMenuShell *menu_shell, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	gtk_button_set_relief(GTK_BUTTON(mainwin->toolbar->reply_btn), GTK_RELIEF_NONE);
	manage_window_focus_in(mainwin->window, NULL, NULL);
}

static void toolbar_reply_to_all_popup_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	MainWindow *mainwindow = (MainWindow *) data;
	
	if (!event) return;

	if (event->button == 3) {
		gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NORMAL);
		gtk_menu_popup(GTK_MENU(mainwindow->toolbar->replyall_popup), NULL, NULL,
		       menu_button_position, widget,
		       event->button, event->time);
	}
}

static void toolbar_reply_to_all_popup_closed_cb(GtkMenuShell *menu_shell, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	gtk_button_set_relief(GTK_BUTTON(mainwin->toolbar->replyall_btn), GTK_RELIEF_NONE);
	manage_window_focus_in(mainwin->window, NULL, NULL);
}

static void toolbar_reply_to_sender_popup_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	MainWindow *mainwindow = (MainWindow *) data;

	if (!event) return;

	if (event->button == 3) {
		gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NORMAL);
		gtk_menu_popup(GTK_MENU(mainwindow->toolbar->replysender_popup), NULL, NULL,
		       menu_button_position, widget,
		       event->button, event->time);
	}
}

static void toolbar_reply_to_sender_popup_closed_cb(GtkMenuShell *menu_shell, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	gtk_button_set_relief(GTK_BUTTON(mainwin->toolbar->replysender_btn), GTK_RELIEF_NONE);
	manage_window_focus_in(mainwin->window, NULL, NULL);
}

static void toolbar_forward_popup_cb(GtkWidget *widget, GdkEventButton *event, gpointer data)
{
	MainWindow *mainwindow = (MainWindow *) data;
	
	if (!event) return;

	if (event->button == 3) {
		gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NORMAL);
		gtk_menu_popup(GTK_MENU(mainwindow->toolbar->fwd_popup), NULL, NULL,
		       menu_button_position, widget,
		       event->button, event->time);
	}
}

static void toolbar_forward_popup_closed_cb (GtkMenuShell *menu_shell, 
					     gpointer     data)
{
	MainWindow *mainwin = (MainWindow *)data;

	gtk_button_set_relief(GTK_BUTTON(mainwin->toolbar->fwd_btn), GTK_RELIEF_NONE);
	manage_window_focus_in(mainwin->window, NULL, NULL);
}

static void activate_compose_button (MainToolbar       *toolbar,
				     ToolbarStyle      style,
				     ComposeButtonType type)
{
	if ((!toolbar->compose_mail_btn) || (!toolbar->compose_news_btn))
		return;

	gtk_widget_hide(type == COMPOSEBUTTON_NEWS ? toolbar->compose_mail_btn 
			: toolbar->compose_news_btn);
	gtk_widget_show(type == COMPOSEBUTTON_NEWS ? toolbar->compose_news_btn
			: toolbar->compose_mail_btn);
	toolbar->compose_btn_type = type;	
}

void toolbar_set_compose_button (MainToolbar       *toolbar, 
				 ComposeButtonType compose_btn_type)
{
	if (toolbar->compose_btn_type != compose_btn_type)
		activate_compose_button(toolbar, 
					prefs_common.toolbar_style,
					compose_btn_type);
}

void toolbar_set_sensitive(MainWindow *mainwin)
{
	SensitiveCond state;
	gboolean sensitive;
	MainToolbar *toolbar = mainwin->toolbar;
	GSList *cur;
	GSList *entry_list = NULL;
	
	typedef struct _Entry Entry;
	struct _Entry {
		GtkWidget *widget;
		SensitiveCond cond;
		gboolean empty;
	};

#define SET_WIDGET_COND(w, c)     \
{ \
	Entry *e = g_new0(Entry, 1); \
	e->widget = w; \
	e->cond   = c; \
	entry_list = g_slist_append(entry_list, e); \
}

	SET_WIDGET_COND(toolbar->get_btn, M_HAVE_ACCOUNT|M_UNLOCKED);
	SET_WIDGET_COND(toolbar->getall_btn, M_HAVE_ACCOUNT|M_UNLOCKED);
	SET_WIDGET_COND(toolbar->compose_news_btn, M_HAVE_ACCOUNT);
	SET_WIDGET_COND(toolbar->reply_btn,
			M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST);
	SET_WIDGET_COND(toolbar->replyall_btn,
			M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST);
	SET_WIDGET_COND(toolbar->replysender_btn,
			M_HAVE_ACCOUNT|M_SINGLE_TARGET_EXIST);
	SET_WIDGET_COND(toolbar->fwd_btn, M_HAVE_ACCOUNT|M_TARGET_EXIST);

	SET_WIDGET_COND(toolbar->next_btn, M_MSG_EXIST);
	SET_WIDGET_COND(toolbar->delete_btn,
			M_TARGET_EXIST|M_ALLOW_DELETE|M_UNLOCKED);
	SET_WIDGET_COND(toolbar->exec_btn, M_DELAY_EXEC);

	for (cur = toolbar->syl_action; cur != NULL;  cur = cur->next) {
		ToolbarSylpheedActions *act = (ToolbarSylpheedActions*)cur->data;
		
		SET_WIDGET_COND(act->widget, M_TARGET_EXIST|M_UNLOCKED);
	}

#undef SET_WIDGET_COND

	state = main_window_get_current_state(mainwin);

	for (cur = entry_list; cur != NULL; cur = cur->next) {
		Entry *e = (Entry*) cur->data;

		if (e->widget != NULL) {
			sensitive = ((e->cond & state) == e->cond);
			gtk_widget_set_sensitive(e->widget, sensitive);	
		}
	}
	
	while (entry_list != NULL) {
		Entry *e = (Entry*) entry_list->data;

		if (e)
			g_free(e);
		entry_list = g_slist_remove(entry_list, e);
	}

	g_slist_free(entry_list);

	activate_compose_button(toolbar, 
				prefs_common.toolbar_style,
				toolbar->compose_btn_type);
}

void toolbar_update(void)
{
	gtk_container_remove(GTK_CONTAINER(mwin->handlebox), 
			     GTK_WIDGET(mwin->toolbar->toolbar));
	
	mwin->toolbar->toolbar    = NULL;
	mwin->toolbar->get_btn    = NULL;
	mwin->toolbar->getall_btn = NULL;
	mwin->toolbar->send_btn   = NULL;
	mwin->toolbar->compose_mail_btn = NULL;
	mwin->toolbar->compose_news_btn = NULL;
	mwin->toolbar->reply_btn        = NULL;	
	mwin->toolbar->replyall_btn     = NULL;	
	mwin->toolbar->replysender_btn  = NULL;	
	mwin->toolbar->fwd_btn    = NULL;	
	mwin->toolbar->delete_btn = NULL;	
	mwin->toolbar->next_btn   = NULL;	
	mwin->toolbar->exec_btn   = NULL;

	toolbar_destroy(mwin);
	toolbar_create(mwin, mwin->handlebox);
	toolbar_set_sensitive(mwin);

}

void toolbar_destroy(MainWindow *mainwin)
{
	ToolbarSylpheedActions *syl_action;

	toolbar_clear_list();
	
	while (mainwin->toolbar->syl_action != NULL) {
		syl_action = (ToolbarSylpheedActions*)mainwin->toolbar->syl_action->data;

		mainwin->toolbar->syl_action = g_slist_remove(mainwin->toolbar->syl_action, syl_action);
		if (syl_action->name)
			g_free(syl_action->name);
		g_free(syl_action);
	}

	g_slist_free(mainwin->toolbar->syl_action);
}

void toolbar_create(MainWindow *mainwin,
		    GtkWidget *container)
{
	ToolbarItem *toolbar_item;

	GtkWidget *toolbar;
	GtkWidget *icon_wid  = NULL;
	GtkWidget *icon_news = NULL;
	GtkWidget *item_news = NULL;
	GtkWidget *item;
	GtkTooltips *toolbar_tips;
	ToolbarSylpheedActions *syl_action;
	GSList *cur;

	guint n_menu_entries;
	GtkWidget *reply_popup;
	GtkWidget *replyall_popup;
	GtkWidget *replysender_popup;
	GtkWidget *fwd_popup;

 	toolbar_tips = gtk_tooltips_new();

	/* store mainwin localy */
	mwin = mainwin;

	if (mainwin->toolbar != NULL) {
		toolbar_destroy(mainwin);
		g_free(mainwin->toolbar);
	}

	toolbar_read_config_file();

	mainwin->toolbar = g_new0(MainToolbar, 1); 

	toolbar = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL,
				  GTK_TOOLBAR_BOTH);
	gtk_container_add(GTK_CONTAINER(container), toolbar);
	gtk_container_set_border_width(GTK_CONTAINER(container), 2);
	gtk_toolbar_set_button_relief(GTK_TOOLBAR(toolbar), GTK_RELIEF_NONE);
	gtk_toolbar_set_space_style(GTK_TOOLBAR(toolbar),
				    GTK_TOOLBAR_SPACE_LINE);
	
	for (cur = toolbar_list; cur != NULL; cur = cur->next) {
		toolbar_item  = (ToolbarItem*) cur->data;
		

		if (g_strcasecmp(toolbar_item->file, SEPARATOR) == 0) {
			gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
			continue;
		}

		icon_wid = stock_pixmap_widget(container, stock_pixmap_get_icon(toolbar_item->file));
		item  = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
						toolbar_item->text,
						(""),
						(""),
						icon_wid, toolbar_actions_cb, 
						toolbar_item);
		
		switch (toolbar_item->action) {
		case A_RECEIVE_ALL:
			mainwin->toolbar->getall_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     mainwin->toolbar->getall_btn, 
					   _("Receive Mail on all Accounts"), NULL);
			break;
		case A_RECEIVE_CUR:
			mainwin->toolbar->get_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     mainwin->toolbar->get_btn,
					   _("Receive Mail on current Account"), NULL);
			break;
		case A_SEND_QUEUED:
			mainwin->toolbar->send_btn = item; 
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     mainwin->toolbar->send_btn,
					   _("Send Queued Message(s)"), NULL);
			break;
		case A_COMPOSE_EMAIL:
			icon_news = stock_pixmap_widget(container, STOCK_PIXMAP_NEWS_COMPOSE);
			item_news = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
							    _("News"),
							    toolbar_ret_descr_from_val(A_COMPOSE_NEWS),
							    (""),
							    icon_news, toolbar_actions_cb, 
							    toolbar_item);
			mainwin->toolbar->compose_mail_btn = item; 
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     mainwin->toolbar->compose_mail_btn,
					   _("Compose Email"), NULL);
			mainwin->toolbar->compose_news_btn = item_news;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     mainwin->toolbar->compose_news_btn,
					   _("Compose News"), NULL);
			break;
		case A_REPLY_MESSAGE:
			mainwin->toolbar->reply_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     mainwin->toolbar->reply_btn,
					   _("Reply to Message"), NULL);
			gtk_signal_connect(GTK_OBJECT(mainwin->toolbar->reply_btn), 
					   "button_press_event",
					   GTK_SIGNAL_FUNC(toolbar_reply_popup_cb),
					   mainwin);
			n_menu_entries = sizeof(reply_popup_entries) /
				sizeof(reply_popup_entries[0]);
			reply_popup = popupmenu_create(mainwin->window, reply_popup_entries, n_menu_entries,
						       "<ReplyPopup>", mainwin);
			gtk_signal_connect(GTK_OBJECT(reply_popup), "selection_done",
					   GTK_SIGNAL_FUNC(toolbar_reply_popup_closed_cb), mainwin);
			mainwin->toolbar->reply_popup       = reply_popup;
			break;
		case A_REPLY_SENDER:
			mainwin->toolbar->replysender_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     mainwin->toolbar->replysender_btn,
					   _("Reply to Sender"), NULL);
			gtk_signal_connect(GTK_OBJECT(mainwin->toolbar->replysender_btn), 
					   "button_press_event",
					   GTK_SIGNAL_FUNC(toolbar_reply_to_sender_popup_cb),
					   mainwin);
			n_menu_entries = sizeof(replysender_popup_entries) /
				sizeof(replysender_popup_entries[0]);
			replysender_popup = popupmenu_create(mainwin->window, 
							     replysender_popup_entries, n_menu_entries,
							     "<ReplySenderPopup>", mainwin);
			gtk_signal_connect(GTK_OBJECT(replysender_popup), "selection_done",
					   GTK_SIGNAL_FUNC(toolbar_reply_to_sender_popup_closed_cb), mainwin);
			mainwin->toolbar->replysender_popup = replysender_popup;
			break;
		case A_REPLY_ALL:
			mainwin->toolbar->replyall_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     mainwin->toolbar->replyall_btn,
					   _("Reply to All"), NULL);
			gtk_signal_connect(GTK_OBJECT(mainwin->toolbar->replyall_btn), 
					   "button_press_event",
					   GTK_SIGNAL_FUNC(toolbar_reply_to_all_popup_cb),
					   mainwin);
			n_menu_entries = sizeof(replyall_popup_entries) /
				sizeof(replyall_popup_entries[0]);
			replyall_popup = popupmenu_create(mainwin->window, 
							  replyall_popup_entries, n_menu_entries,
							  "<ReplyAllPopup>", mainwin);
			gtk_signal_connect(GTK_OBJECT(replyall_popup), "selection_done",
					   GTK_SIGNAL_FUNC(toolbar_reply_to_all_popup_closed_cb), mainwin);
			mainwin->toolbar->replyall_popup    = replyall_popup;
			break;
		case A_FORWARD:
			mainwin->toolbar->fwd_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     mainwin->toolbar->fwd_btn,
					   _("Forward Message"), NULL);
			gtk_signal_connect(GTK_OBJECT(mainwin->toolbar->fwd_btn), 
					   "button_press_event",
					   GTK_SIGNAL_FUNC(toolbar_forward_popup_cb),
					   mainwin);
			n_menu_entries = sizeof(fwd_popup_entries) /
				sizeof(fwd_popup_entries[0]);
			fwd_popup = popupmenu_create(mainwin->window, 
						     fwd_popup_entries, n_menu_entries,
						     "<ForwardPopup>", mainwin);
			gtk_signal_connect(GTK_OBJECT(fwd_popup), "selection_done",
					   GTK_SIGNAL_FUNC(toolbar_forward_popup_closed_cb), mainwin);
			mainwin->toolbar->fwd_popup         = fwd_popup;
			break;
		case A_DELETE:
			mainwin->toolbar->delete_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     mainwin->toolbar->delete_btn,
					   _("Delete Message"), NULL);
			break;
		case A_EXECUTE:
			mainwin->toolbar->exec_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     mainwin->toolbar->exec_btn,
					   _("Execute"), NULL);
			break;
		case A_GOTO_NEXT:
			mainwin->toolbar->next_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     mainwin->toolbar->next_btn,
					   _("Goto Next Message"), NULL);
			break;
		case A_SYL_ACTIONS:
			syl_action = g_new0(ToolbarSylpheedActions, 1);
			syl_action->widget = item;
			syl_action->name   = g_strdup(toolbar_item->text);

			mainwin->toolbar->syl_action = g_slist_append(mainwin->toolbar->syl_action,
								      syl_action);
			gtk_widget_show(item);
			break;
		default:
			break;
		}
	}

	mainwin->toolbar->toolbar = toolbar;

	activate_compose_button(mainwin->toolbar, 
				prefs_common.toolbar_style, 
				mainwin->toolbar->compose_btn_type);

	gtk_widget_show_all(toolbar);
}
