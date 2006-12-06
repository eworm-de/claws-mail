/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2001-2006 Hiroyuki Yamamoto and the Claws Mail team
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

/*
 * General functions for accessing address book files.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <math.h>
#include <setjmp.h>

#include "mainwindow.h"
#include "summaryview.h"
#include "compose.h"
#include "utils.h"
#include "xml.h"
#include "mgutils.h"
#include "prefs_gtk.h"
#include "codeconv.h"
#include "stock_pixmap.h"
#include "manage_window.h"
#include "gtkutils.h"
#include "toolbar.h"
#include "menu.h"
#include "inc.h"
#include "action.h"
#include "prefs_actions.h"
#include "prefs_common.h"
#include "prefs_toolbar.h"
#include "alertpanel.h"

/* elements */
#define TOOLBAR_TAG_INDEX        "toolbar"
#define TOOLBAR_TAG_ITEM         "item"
#define TOOLBAR_TAG_SEPARATOR    "separator"

#define TOOLBAR_ICON_FILE   "file"    
#define TOOLBAR_ICON_TEXT   "text"     
#define TOOLBAR_ICON_ACTION "action"    

gboolean      toolbar_is_duplicate		(gint           action,
					      	 ToolbarType	source);
static void   toolbar_parse_item		(XMLFile        *file,
					      	 ToolbarType	source);

static gint   toolbar_ret_val_from_text		(const gchar	*text);
static gchar *toolbar_ret_text_from_val		(gint           val);

static void   toolbar_set_default_main		(void);
static void   toolbar_set_default_compose	(void);
static void   toolbar_set_default_msgview	(void);

static void	toolbar_style			(ToolbarType 	 type, 
						 guint 		 action, 
						 gpointer 	 data);

static MainWindow *get_mainwin			(gpointer data);
static void activate_compose_button 		(Toolbar	*toolbar,
				     		 ToolbarStyle	 style,
				     		 ComposeButtonType type);

/* toolbar callbacks */
static void toolbar_reply			(gpointer 	 data, 
						 guint 		 action);
static void toolbar_delete_cb			(GtkWidget	*widget,
					 	 gpointer        data);
static void toolbar_trash_cb			(GtkWidget	*widget,
					 	 gpointer        data);

static void toolbar_compose_cb			(GtkWidget	*widget,
					    	 gpointer	 data);

static void toolbar_learn_cb			(GtkWidget	*widget,
					    	 gpointer	 data);

static void toolbar_reply_cb		   	(GtkWidget	*widget,
					    	 gpointer	 data);

static void toolbar_reply_to_all_cb	   	(GtkWidget	*widget,
					    	 gpointer	 data);

static void toolbar_reply_to_list_cb	   	(GtkWidget	*widget,
					    	 gpointer	 data);

static void toolbar_reply_to_sender_cb	   	(GtkWidget	*widget,
					    	 gpointer 	 data);

static void toolbar_forward_cb		   	(GtkWidget	*widget,
					    	 gpointer 	 data);

static void toolbar_prev_unread_cb	   	(GtkWidget	*widget,
					    	 gpointer 	 data);
static void toolbar_next_unread_cb	   	(GtkWidget	*widget,
					    	 gpointer 	 data);

static void toolbar_ignore_thread_cb	   	(GtkWidget	*widget,
					    	 gpointer 	 data);

static void toolbar_print_cb			(GtkWidget	*widget,
					    	 gpointer 	 data);

static void toolbar_actions_execute_cb	   	(GtkWidget     	*widget,
				  	    	 gpointer      	 data);


static void toolbar_send_cb			(GtkWidget	*widget,
					 	 gpointer	 data);
static void toolbar_send_later_cb		(GtkWidget	*widget,
					 	 gpointer	 data);
static void toolbar_draft_cb			(GtkWidget	*widget,
					 	 gpointer	 data);
static void toolbar_insert_cb			(GtkWidget	*widget,
					 	 gpointer	 data);
static void toolbar_attach_cb			(GtkWidget	*widget,
					 	 gpointer	 data);
static void toolbar_sig_cb			(GtkWidget	*widget,
					 	 gpointer	 data);
static void toolbar_ext_editor_cb		(GtkWidget	*widget,
					 	 gpointer	 data);
static void toolbar_linewrap_current_cb		(GtkWidget	*widget,
					 	 gpointer	 data);
static void toolbar_linewrap_all_cb		(GtkWidget	*widget,
					 	 gpointer	 data);
static void toolbar_addrbook_cb   		(GtkWidget   	*widget, 
					 	 gpointer     	 data);
#ifdef USE_ASPELL
static void toolbar_check_spelling_cb  		(GtkWidget   	*widget, 
					 	 gpointer     	 data);
#endif

struct {
	gchar *index_str;
	const gchar *descr;
} toolbar_text [] = {
	{ "A_RECEIVE_ALL",   	N_("Receive Mail on all Accounts")         },
	{ "A_RECEIVE_CUR",   	N_("Receive Mail on current Account")      },
	{ "A_SEND_QUEUED",   	N_("Send Queued Messages")                 },
	{ "A_COMPOSE_EMAIL", 	N_("Compose Email")                        },
	{ "A_COMPOSE_NEWS",  	N_("Compose News")                         },
	{ "A_REPLY_MESSAGE", 	N_("Reply to Message")                     },
	{ "A_REPLY_SENDER",  	N_("Reply to Sender")                      },
	{ "A_REPLY_ALL",     	N_("Reply to All")                         },
	{ "A_REPLY_ML",      	N_("Reply to Mailing-list")                },
	{ "A_FORWARD",       	N_("Forward Message")                      }, 
	{ "A_TRASH",        	N_("Trash Message")   	                   },
	{ "A_DELETE_REAL",    	N_("Delete Message")                       },
	{ "A_EXECUTE",       	N_("Execute")                              },
	{ "A_GOTO_PREV",     	N_("Go to Previous Unread Message")        },
	{ "A_GOTO_NEXT",     	N_("Go to Next Unread Message")            },
	{ "A_IGNORE_THREAD", 	N_("Ignore thread")			   },
	{ "A_PRINT",	     	N_("Print")				   },
	{ "A_LEARN_SPAM",	N_("Learn Spam or Ham")			   },

	{ "A_SEND",          	N_("Send Message")                         },
	{ "A_SENDL",         	N_("Put into queue folder and send later") },
	{ "A_DRAFT",         	N_("Save to draft folder")                 },
	{ "A_INSERT",        	N_("Insert file")                          },   
	{ "A_ATTACH",        	N_("Attach file")                          },
	{ "A_SIG",           	N_("Insert signature")                     },
	{ "A_EXTEDITOR",     	N_("Edit with external editor")            },
	{ "A_LINEWRAP_CURRENT",	N_("Wrap long lines of current paragraph") }, 
	{ "A_LINEWRAP_ALL",     N_("Wrap all long lines")                  }, 
	{ "A_ADDRBOOK",      	N_("Address book")                         },
#ifdef USE_ASPELL
	{ "A_CHECK_SPELLING",	N_("Check spelling")                       },
#endif
	{ "A_SYL_ACTIONS",   	N_("Claws Mail Actions Feature")	   }, 
	{ "A_SEPARATOR",     	"Separator"				}
};

/* struct holds configuration files and a list of
 * currently active toolbar items 
 * TOOLBAR_MAIN, TOOLBAR_COMPOSE and TOOLBAR_MSGVIEW
 * give us an index
 */
struct {
	const gchar  *conf_file;
	GSList       *item_list;
} toolbar_config[3] = {
	{ "toolbar_main.xml",    NULL},
	{ "toolbar_compose.xml", NULL}, 
  	{ "toolbar_msgview.xml", NULL}
};

static GtkItemFactoryEntry reply_entries[] =
{
	{N_("/Reply with _quote"), NULL,    toolbar_reply, COMPOSE_REPLY_WITH_QUOTE, NULL},
	{N_("/_Reply without quote"), NULL, toolbar_reply, COMPOSE_REPLY_WITHOUT_QUOTE, NULL}
};
static GtkItemFactoryEntry replyall_entries[] =
{
	{N_("/Reply to all with _quote"), "<shift>A", toolbar_reply, COMPOSE_REPLY_TO_ALL_WITH_QUOTE, NULL},
	{N_("/_Reply to all without quote"), "a",     toolbar_reply, COMPOSE_REPLY_TO_ALL_WITHOUT_QUOTE, NULL}
};
static GtkItemFactoryEntry replylist_entries[] =
{
	{N_("/Reply to list with _quote"),    NULL, toolbar_reply, COMPOSE_REPLY_TO_LIST_WITH_QUOTE, NULL},
	{N_("/_Reply to list without quote"), NULL, toolbar_reply, COMPOSE_REPLY_TO_LIST_WITHOUT_QUOTE, NULL}
};
static GtkItemFactoryEntry replysender_entries[] =
{
	{N_("/Reply to sender with _quote"),    NULL, toolbar_reply, COMPOSE_REPLY_TO_SENDER_WITH_QUOTE, NULL},
	{N_("/_Reply to sender without quote"), NULL, toolbar_reply, COMPOSE_REPLY_TO_SENDER_WITHOUT_QUOTE, NULL}
};
static GtkItemFactoryEntry forward_entries[] =
{
	{N_("/_Forward"),		"f", 	    toolbar_reply, COMPOSE_FORWARD_INLINE, NULL},
	{N_("/For_ward as attachment"), "<shift>F", toolbar_reply, COMPOSE_FORWARD_AS_ATTACH, NULL},
	{N_("/Redirec_t"),		NULL, 	    toolbar_reply, COMPOSE_REDIRECT, NULL}
};


gint toolbar_ret_val_from_descr(const gchar *descr)
{
	gint i;

	for (i = 0; i < N_ACTION_VAL; i++) {
		if (g_utf8_collate(gettext(toolbar_text[i].descr), descr) == 0)
				return i;
	}
	
	return -1;
}

gchar *toolbar_ret_descr_from_val(gint val)
{
	g_return_val_if_fail(val >=0 && val < N_ACTION_VAL, NULL);

	return gettext(toolbar_text[val].descr);
}

static gint toolbar_ret_val_from_text(const gchar *text)
{
	gint i;
	
	for (i = 0; i < N_ACTION_VAL; i++) {
		if (g_utf8_collate(toolbar_text[i].index_str, text) == 0)
				return i;
	}

	return -1;
}

static gchar *toolbar_ret_text_from_val(gint val)
{
	g_return_val_if_fail(val >=0 && val < N_ACTION_VAL, NULL);

	return toolbar_text[val].index_str;
}

gboolean toolbar_is_duplicate(gint action, ToolbarType source)
{
	GSList *cur;

	if ((action == A_SEPARATOR) || (action == A_SYL_ACTIONS)) 
		return FALSE;

	for (cur = toolbar_config[source].item_list; cur != NULL; cur = cur->next) {
		ToolbarItem *item = (ToolbarItem*) cur->data;
		
		if (item->index == action)
			return TRUE;
	}
	return FALSE;
}

/* depending on toolbar type this function 
   returns a list of available toolbar events being 
   displayed by prefs_toolbar
*/
GList *toolbar_get_action_items(ToolbarType source)
{
	GList *items = NULL;
	gint i = 0;
	
	if (source == TOOLBAR_MAIN) {
		gint main_items[]   = { A_RECEIVE_ALL,   A_RECEIVE_CUR,   A_SEND_QUEUED,
					A_COMPOSE_EMAIL, A_REPLY_MESSAGE, A_REPLY_SENDER, 
					A_REPLY_ALL,     A_REPLY_ML,      A_FORWARD, 
					A_TRASH , A_DELETE_REAL,       A_EXECUTE,       A_GOTO_PREV, 
					A_GOTO_NEXT,	A_IGNORE_THREAD,  A_PRINT,
					A_ADDRBOOK, 	A_LEARN_SPAM, A_SYL_ACTIONS };

		for (i = 0; i < sizeof main_items / sizeof main_items[0]; i++)  {
			items = g_list_append(items, gettext(toolbar_text[main_items[i]].descr));
		}	
	}
	else if (source == TOOLBAR_COMPOSE) {
		gint comp_items[] =   {	A_SEND,          A_SENDL,        A_DRAFT,
					A_INSERT,        A_ATTACH,       A_SIG,
					A_EXTEDITOR,     A_LINEWRAP_CURRENT,     
					A_LINEWRAP_ALL,  A_ADDRBOOK,
#ifdef USE_ASPELL
					A_CHECK_SPELLING, 
#endif
					A_SYL_ACTIONS };	

		for (i = 0; i < sizeof comp_items / sizeof comp_items[0]; i++) 
			items = g_list_append(items, gettext(toolbar_text[comp_items[i]].descr));
	}
	else if (source == TOOLBAR_MSGVIEW) {
		gint msgv_items[] =   { A_COMPOSE_EMAIL, A_REPLY_MESSAGE, A_REPLY_SENDER,
				        A_REPLY_ALL,     A_REPLY_ML,      A_FORWARD,
				        A_TRASH, A_DELETE_REAL,       A_GOTO_PREV,	  A_GOTO_NEXT,
					A_ADDRBOOK,	 A_LEARN_SPAM, A_SYL_ACTIONS };	

		for (i = 0; i < sizeof msgv_items / sizeof msgv_items[0]; i++) 
			items = g_list_append(items, gettext(toolbar_text[msgv_items[i]].descr));
	}

	return items;
}

static void toolbar_parse_item(XMLFile *file, ToolbarType source)
{
	GList *attr;
	gchar *name, *value;
	ToolbarItem *item = NULL;
	gboolean rewrite = FALSE;

	attr = xml_get_current_tag_attr(file);
	item = g_new0(ToolbarItem, 1);
	while( attr ) {
		name = ((XMLAttr *)attr->data)->name;
		value = ((XMLAttr *)attr->data)->value;
		
		if (g_utf8_collate(name, TOOLBAR_ICON_FILE) == 0) 
			item->file = g_strdup (value);
		else if (g_utf8_collate(name, TOOLBAR_ICON_TEXT) == 0)
			item->text = g_strdup (value);
		else if (g_utf8_collate(name, TOOLBAR_ICON_ACTION) == 0)
			item->index = toolbar_ret_val_from_text(value);
		if (item->index == -1 && !strcmp(value, "A_DELETE")) {
			/* switch button */
			item->index = A_TRASH;
			g_free(item->file);
			item->file = g_strdup("trash_btn");
			g_free(item->text);
			item->text = g_strdup(_("Trash"));
			rewrite = TRUE;
		}
		attr = g_list_next(attr);
	}
	if (item->index != -1) {
		
		if (!toolbar_is_duplicate(item->index, source)) 
			toolbar_config[source].item_list = g_slist_append(toolbar_config[source].item_list,
									 item);
	}
	if (rewrite) {
		toolbar_save_config_file(source);
	}
}

static void toolbar_set_default_main(void) 
{
	struct {
		gint action;
		gint icon;
		gchar *text;
	} default_toolbar[] = {
		{ A_RECEIVE_ALL,   STOCK_PIXMAP_MAIL_RECEIVE_ALL,     _("Get Mail") },
		{ A_SEPARATOR,     0,                                 ("")         }, 
		{ A_SEND_QUEUED,   STOCK_PIXMAP_MAIL_SEND_QUEUE,      _("Send")    },
		{ A_COMPOSE_EMAIL, STOCK_PIXMAP_MAIL_COMPOSE,
			(gchar*)Q_("Toolbar|Compose") },
		{ A_SEPARATOR,     0,                                 ("")         },
		{ A_REPLY_MESSAGE, STOCK_PIXMAP_MAIL_REPLY,           _("Reply")   }, 
		{ A_REPLY_ALL,     STOCK_PIXMAP_MAIL_REPLY_TO_ALL,    _("All")     },
		{ A_REPLY_SENDER,  STOCK_PIXMAP_MAIL_REPLY_TO_AUTHOR, _("Sender")  },
		{ A_FORWARD,       STOCK_PIXMAP_MAIL_FORWARD,         _("Forward") },
		{ A_SEPARATOR,     0,                                 ("")         },
		{ A_TRASH,         STOCK_PIXMAP_TRASH,                _("Trash")   },
#if (defined(USE_SPAMASSASSIN_PLUGIN) || defined(USE_BOGOFILTER_PLUGIN))
		{ A_LEARN_SPAM,	   STOCK_PIXMAP_SPAM_BTN,             _("Spam")    },
#endif
		{ A_SEPARATOR,     0,                                 ("")         },
		{ A_GOTO_NEXT,     STOCK_PIXMAP_DOWN_ARROW,           _("Next")    }
	};
	
	gint i;
	
	for (i = 0; i < sizeof(default_toolbar) / sizeof(default_toolbar[0]); i++) {
		
		ToolbarItem *toolbar_item = g_new0(ToolbarItem, 1);
		
		if (default_toolbar[i].action != A_SEPARATOR) {
			
			gchar *file = stock_pixmap_get_name((StockPixmap)default_toolbar[i].icon);
			
			toolbar_item->file  = g_strdup(file);
			toolbar_item->index = default_toolbar[i].action;
			toolbar_item->text  = g_strdup(default_toolbar[i].text);
		} else {

			toolbar_item->file  = g_strdup(TOOLBAR_TAG_SEPARATOR);
			toolbar_item->index = A_SEPARATOR;
		}
		
		if (toolbar_item->index != -1) {
			if ( !toolbar_is_duplicate(toolbar_item->index, TOOLBAR_MAIN)) 
				toolbar_config[TOOLBAR_MAIN].item_list = 
					g_slist_append(toolbar_config[TOOLBAR_MAIN].item_list, toolbar_item);
		}	
	}
}

static void toolbar_set_default_compose(void)
{
	struct {
		gint action;
		gint icon;
		gchar *text;
	} default_toolbar[] = {
		{ A_SEND,      		STOCK_PIXMAP_MAIL_SEND,         _("Send")       	},
		{ A_SENDL,     		STOCK_PIXMAP_MAIL_SEND_QUEUE,   _("Send later") 	},
		{ A_DRAFT,     		STOCK_PIXMAP_MAIL,              _("Draft")      	},
		{ A_SEPARATOR, 		0,                               ("")           	}, 
		{ A_INSERT,    		STOCK_PIXMAP_INSERT_FILE,       _("Insert")     	},
		{ A_ATTACH,    		STOCK_PIXMAP_MAIL_ATTACH,       _("Attach")     	},
		{ A_SEPARATOR, 		0,                               ("")           	},
		{ A_ADDRBOOK,  		STOCK_PIXMAP_ADDRESS_BOOK,      _("Address")    	}
	};
	
	gint i;

	for (i = 0; i < sizeof(default_toolbar) / sizeof(default_toolbar[0]); i++) {
		
		ToolbarItem *toolbar_item = g_new0(ToolbarItem, 1);
		
		if (default_toolbar[i].action != A_SEPARATOR) {
			
			gchar *file = stock_pixmap_get_name((StockPixmap)default_toolbar[i].icon);
			
			toolbar_item->file  = g_strdup(file);
			toolbar_item->index = default_toolbar[i].action;
			toolbar_item->text  = g_strdup(default_toolbar[i].text);
		} else {

			toolbar_item->file  = g_strdup(TOOLBAR_TAG_SEPARATOR);
			toolbar_item->index = A_SEPARATOR;
		}
		
		if (toolbar_item->index != -1) {
			if ( !toolbar_is_duplicate(toolbar_item->index, TOOLBAR_COMPOSE)) 
				toolbar_config[TOOLBAR_COMPOSE].item_list = 
					g_slist_append(toolbar_config[TOOLBAR_COMPOSE].item_list, toolbar_item);
		}	
	}
}

static void toolbar_set_default_msgview(void)
{
	struct {
		gint action;
		gint icon;
		gchar *text;
	} default_toolbar[] = {
		{ A_REPLY_MESSAGE, STOCK_PIXMAP_MAIL_REPLY,           _("Reply")   }, 
		{ A_REPLY_ALL,     STOCK_PIXMAP_MAIL_REPLY_TO_ALL,    _("All")     },
		{ A_REPLY_SENDER,  STOCK_PIXMAP_MAIL_REPLY_TO_AUTHOR, _("Sender")  },
		{ A_FORWARD,       STOCK_PIXMAP_MAIL_FORWARD,         _("Forward") },
		{ A_SEPARATOR,     0,                                 ("")         },
		{ A_TRASH,         STOCK_PIXMAP_TRASH,                _("Trash")   },
#if (defined(USE_SPAMASSASSIN_PLUGIN) || defined(USE_BOGOFILTER_PLUGIN))
		{ A_LEARN_SPAM,	   STOCK_PIXMAP_SPAM_BTN,             _("Spam")    },
#endif
		{ A_GOTO_NEXT,     STOCK_PIXMAP_DOWN_ARROW,           _("Next")    }
	};
	
	gint i;

	for (i = 0; i < sizeof(default_toolbar) / sizeof(default_toolbar[0]); i++) {
		
		ToolbarItem *toolbar_item = g_new0(ToolbarItem, 1);
		
		if (default_toolbar[i].action != A_SEPARATOR) {
			
			gchar *file = stock_pixmap_get_name((StockPixmap)default_toolbar[i].icon);
			
			toolbar_item->file  = g_strdup(file);
			toolbar_item->index = default_toolbar[i].action;
			toolbar_item->text  = g_strdup(default_toolbar[i].text);
		} else {

			toolbar_item->file  = g_strdup(TOOLBAR_TAG_SEPARATOR);
			toolbar_item->index = A_SEPARATOR;
		}
		
		if (toolbar_item->index != -1) {
			if ( !toolbar_is_duplicate(toolbar_item->index, TOOLBAR_MSGVIEW)) 
				toolbar_config[TOOLBAR_MSGVIEW].item_list = 
					g_slist_append(toolbar_config[TOOLBAR_MSGVIEW].item_list, toolbar_item);
		}	
	}
}

void toolbar_set_default(ToolbarType source)
{
	if (source == TOOLBAR_MAIN)
		toolbar_set_default_main();
	else if  (source == TOOLBAR_COMPOSE)
		toolbar_set_default_compose();
	else if  (source == TOOLBAR_MSGVIEW)
		toolbar_set_default_msgview();
}

void toolbar_save_config_file(ToolbarType source)
{
	GSList *cur;
	FILE *fp;
	PrefFile *pfile;
	gchar *fileSpec = NULL;

	debug_print("save Toolbar Configuration to %s\n", toolbar_config[source].conf_file);

	fileSpec = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, toolbar_config[source].conf_file, NULL );
	pfile = prefs_write_open(fileSpec);
	g_free( fileSpec );
	if( pfile ) {
		fp = pfile->fp;
		fprintf(fp, "<?xml version=\"1.0\" encoding=\"%s\" ?>\n", CS_INTERNAL);

		fprintf(fp, "<%s>\n", TOOLBAR_TAG_INDEX);

		for (cur = toolbar_config[source].item_list; cur != NULL; cur = cur->next) {
			ToolbarItem *toolbar_item = (ToolbarItem*) cur->data;
			
			if (toolbar_item->index != A_SEPARATOR) {
				fprintf(fp, "\t<%s %s=\"%s\" %s=\"",
					TOOLBAR_TAG_ITEM, 
					TOOLBAR_ICON_FILE, toolbar_item->file,
					TOOLBAR_ICON_TEXT);
				xml_file_put_escape_str(fp, toolbar_item->text);
				fprintf(fp, "\" %s=\"%s\"/>\n",
					TOOLBAR_ICON_ACTION, 
					toolbar_ret_text_from_val(toolbar_item->index));
			} else {
				fprintf(fp, "\t<%s/>\n", TOOLBAR_TAG_SEPARATOR); 
			}
		}

		fprintf(fp, "</%s>\n", TOOLBAR_TAG_INDEX);	
	
		if (prefs_file_close (pfile) < 0 ) 
			g_warning("failed to write toolbar configuration to file\n");
	} else
		g_warning("failed to open toolbar configuration file for writing\n");
}

void toolbar_read_config_file(ToolbarType source)
{
	XMLFile *file   = NULL;
	gchar *fileSpec = NULL;
	GList *attr;
	gboolean retVal;
	jmp_buf    jumper;

	debug_print("read Toolbar Configuration from %s\n", toolbar_config[source].conf_file);

	fileSpec = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, toolbar_config[source].conf_file, NULL );
	file = xml_open_file(fileSpec);
	g_free(fileSpec);

	toolbar_clear_list(source);

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
				toolbar_parse_item(file, source);
			} else if (xml_compare_tag(file, TOOLBAR_TAG_SEPARATOR)) {
				ToolbarItem *item = g_new0(ToolbarItem, 1);
			
				item->file   = g_strdup(toolbar_ret_descr_from_val(A_SEPARATOR));
				item->index  = A_SEPARATOR;
				toolbar_config[source].item_list = 
					g_slist_append(toolbar_config[source].item_list, item);
			}

		}
		xml_close_file(file);
	}

	if ((!file) || (g_slist_length(toolbar_config[source].item_list) == 0)) {

		if (source == TOOLBAR_MAIN) 
			toolbar_set_default(TOOLBAR_MAIN);
		else if (source == TOOLBAR_COMPOSE) 
			toolbar_set_default(TOOLBAR_COMPOSE);
		else if (source == TOOLBAR_MSGVIEW) 
			toolbar_set_default(TOOLBAR_MSGVIEW);
		else {		
			g_warning("failed to write Toolbar Configuration to %s\n", toolbar_config[source].conf_file);
			return;
		}

		toolbar_save_config_file(source);
	}
}

/*
 * clears list of toolbar items read from configuration files
 */
void toolbar_clear_list(ToolbarType source)
{
	while (toolbar_config[source].item_list != NULL) {
		ToolbarItem *item = (ToolbarItem*) toolbar_config[source].item_list->data;
		
		toolbar_config[source].item_list = 
			g_slist_remove(toolbar_config[source].item_list, item);

		g_free(item->file);
		g_free(item->text);
		g_free(item);	
	}
	g_slist_free(toolbar_config[source].item_list);
}


/* 
 * return list of Toolbar items
 */
GSList *toolbar_get_list(ToolbarType source)
{
	GSList *list = NULL;

	if ((source == TOOLBAR_MAIN) || (source == TOOLBAR_COMPOSE) || (source == TOOLBAR_MSGVIEW))
		list = toolbar_config[source].item_list;

	return list;
}

void toolbar_set_list_item(ToolbarItem *t_item, ToolbarType source)
{
	ToolbarItem *toolbar_item = g_new0(ToolbarItem, 1);

	toolbar_item->file  = g_strdup(t_item->file);
	toolbar_item->text  = g_strdup(t_item->text);
	toolbar_item->index = t_item->index;
	
	toolbar_config[source].item_list = 
		g_slist_append(toolbar_config[source].item_list,
			       toolbar_item);
}

void toolbar_action_execute(GtkWidget    *widget,
			    GSList       *action_list, 
			    gpointer     data,
			    gint         source) 
{
	GSList *cur, *lop;
	gchar *action, *action_p;
	gboolean found = FALSE;
	gint i = 0;

	for (cur = action_list; cur != NULL;  cur = cur->next) {
		ToolbarSylpheedActions *act = (ToolbarSylpheedActions*)cur->data;

		if (widget == act->widget) {
			
			for (lop = prefs_common.actions_list; lop != NULL; lop = lop->next) {
				action = g_strdup((gchar*)lop->data);

				action_p = strstr(action, ": ");
				action_p[0] = 0x00;
				if (g_utf8_collate(act->name, action) == 0) {
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
		actions_execute(data, i, widget, source);
	else
		g_warning ("Error: did not find Claws Action to execute");
}

static void activate_compose_button (Toolbar           *toolbar,
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

void toolbar_set_compose_button(Toolbar            *toolbar, 
				ComposeButtonType  compose_btn_type)
{
	if (toolbar->compose_btn_type != compose_btn_type)
		activate_compose_button(toolbar, 
					prefs_common.toolbar_style,
					compose_btn_type);
}

static void activate_learn_button (Toolbar           *toolbar,
				     ToolbarStyle      style,
				     LearnButtonType type)
{
	if ((!toolbar->learn_spam_btn) || (!toolbar->learn_ham_btn))
		return;

	gtk_widget_hide(type == LEARN_SPAM ? toolbar->learn_ham_btn 
			: toolbar->learn_spam_btn);
	gtk_widget_show(type == LEARN_SPAM ? toolbar->learn_spam_btn
			: toolbar->learn_ham_btn);
	toolbar->learn_btn_type = type;	
}

void toolbar_set_learn_button(Toolbar            *toolbar, 
				LearnButtonType  learn_btn_type)
{
	if (toolbar->learn_btn_type != learn_btn_type)
		activate_learn_button(toolbar, 
					prefs_common.toolbar_style,
					learn_btn_type);
}

void toolbar_toggle(guint action, gpointer data)
{
	MainWindow *mainwin = (MainWindow*)data;
	GList *list;
	GList *cur;

	g_return_if_fail(mainwin != NULL);

	toolbar_style(TOOLBAR_MAIN, action, mainwin);

	list = compose_get_compose_list();
	for (cur = list; cur != NULL; cur = cur->next) {
		toolbar_style(TOOLBAR_COMPOSE, action, cur->data);
	}
	list = messageview_get_msgview_list();
	for (cur = list; cur != NULL; cur = cur->next) {
		toolbar_style(TOOLBAR_MSGVIEW, action, cur->data);
	}
	
}

void toolbar_set_style(GtkWidget *toolbar_wid, GtkWidget *handlebox_wid, guint action)
{
	switch ((ToolbarStyle)action) {
	case TOOLBAR_NONE:
		gtk_widget_hide(handlebox_wid);
		break;
	case TOOLBAR_ICON:
		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar_wid),
				      GTK_TOOLBAR_ICONS);
		break;
	case TOOLBAR_TEXT:
		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar_wid),
				      GTK_TOOLBAR_TEXT);
		break;
	case TOOLBAR_BOTH:
		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar_wid),
				      GTK_TOOLBAR_BOTH);
		break;
	case TOOLBAR_BOTH_HORIZ:
		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar_wid),
				      GTK_TOOLBAR_BOTH_HORIZ);
		break;
	default:
		return;
	}

	prefs_common.toolbar_style = (ToolbarStyle)action;
	gtk_widget_set_size_request(handlebox_wid, 1, -1);
	
	if (prefs_common.toolbar_style != TOOLBAR_NONE) {
		gtk_widget_show(handlebox_wid);
		gtk_widget_queue_resize(handlebox_wid);
	}
}
/*
 * Change the style of toolbar
 */
static void toolbar_style(ToolbarType type, guint action, gpointer data)
{
	GtkWidget  *handlebox_wid;
	GtkWidget  *toolbar_wid;
	MainWindow *mainwin = (MainWindow*)data;
	Compose    *compose = (Compose*)data;
	MessageView *msgview = (MessageView*)data;
	
	g_return_if_fail(data != NULL);
	
	switch (type) {
	case TOOLBAR_MAIN:
		handlebox_wid = mainwin->handlebox;
		toolbar_wid = mainwin->toolbar->toolbar;
		break;
	case TOOLBAR_COMPOSE:
		handlebox_wid = compose->handlebox;
		toolbar_wid = compose->toolbar->toolbar;
		break;
	case TOOLBAR_MSGVIEW: 
		handlebox_wid = msgview->handlebox;
		toolbar_wid = msgview->toolbar->toolbar;
		break;
	default:

		return;
	}
	toolbar_set_style(toolbar_wid, handlebox_wid, action);
}

/* Toolbar handling */
static void toolbar_inc_cb(GtkWidget	*widget,
			   gpointer	 data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;

	g_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)toolbar_item->parent;	
		inc_mail_cb(mainwin, 0, NULL);
		break;
	default:
		break;
	}
}

static void toolbar_inc_all_cb(GtkWidget	*widget,
			       gpointer	 	 data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;

	g_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)toolbar_item->parent;
		inc_all_account_mail_cb(mainwin, 0, NULL);
		break;
	default:
		break;
	}
}

static void toolbar_send_queued_cb(GtkWidget *widget,gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;

	g_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)toolbar_item->parent;
		send_queue_cb(mainwin, 0, NULL);
		break;
	default:
		break;
	}
}

static void toolbar_exec_cb(GtkWidget	*widget,
			    gpointer	 data)
{
	MainWindow *mainwin = get_mainwin(data);

	g_return_if_fail(mainwin != NULL);
	summary_execute(mainwin->summaryview);
}

/*
 * Delete current/selected(s) message(s)
 */
static void toolbar_trash_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;

	g_return_if_fail(toolbar_item != NULL);
	g_return_if_fail(toolbar_item->parent);
	
	switch (toolbar_item->type) {
	case TOOLBAR_MSGVIEW:
		messageview_delete((MessageView *)toolbar_item->parent);
        	break;
        case TOOLBAR_MAIN:
		mainwin = (MainWindow *)toolbar_item->parent;
        	summary_delete_trash(mainwin->summaryview);
        	break;
        default: 
        	debug_print("toolbar event not supported\n");
        	break;
	}
}

/*
 * Delete current/selected(s) message(s)
 */
static void toolbar_delete_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;

	g_return_if_fail(toolbar_item != NULL);
	g_return_if_fail(toolbar_item->parent);
	
	switch (toolbar_item->type) {
	case TOOLBAR_MSGVIEW:
		messageview_delete((MessageView *)toolbar_item->parent);
        	break;
        case TOOLBAR_MAIN:
		mainwin = (MainWindow *)toolbar_item->parent;
        	summary_delete(mainwin->summaryview);
        	break;
        default: 
        	debug_print("toolbar event not supported\n");
        	break;
	}
}


/*
 * Compose new message
 */
static void toolbar_compose_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;
	MessageView *msgview;

	g_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)toolbar_item->parent;
		if (mainwin->toolbar->compose_btn_type == COMPOSEBUTTON_NEWS) 
			compose_news_cb(mainwin, 0, NULL);
		else
			compose_mail_cb(mainwin, 0, NULL);
		break;
	case TOOLBAR_MSGVIEW:
		msgview = (MessageView*)toolbar_item->parent;
		compose_new_with_folderitem(NULL, 
					    msgview->msginfo->folder, NULL);
		break;	
	default:
		debug_print("toolbar event not supported\n");
	}
}

static void toolbar_learn_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;
	MessageView *msgview;

	g_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)toolbar_item->parent;
		if (mainwin->toolbar->learn_btn_type == LEARN_SPAM) 
			mainwindow_learn(mainwin, TRUE);
		else
			mainwindow_learn(mainwin, FALSE);
		break;
	case TOOLBAR_MSGVIEW:
		msgview = (MessageView*)toolbar_item->parent;
		if (msgview->toolbar->learn_btn_type == LEARN_SPAM) 
			messageview_learn(msgview, TRUE);
		else
			messageview_learn(msgview, FALSE);
		break;
	default:
		debug_print("toolbar event not supported\n");
	}
}


/*
 * Reply Message
 */
static void toolbar_reply_cb(GtkWidget *widget, gpointer data)
{
	toolbar_reply(data, prefs_common.reply_with_quote ? 
		      COMPOSE_REPLY_WITH_QUOTE : COMPOSE_REPLY_WITHOUT_QUOTE);
}


/*
 * Reply message to Sender and All recipients
 */
static void toolbar_reply_to_all_cb(GtkWidget *widget, gpointer data)
{
	toolbar_reply(data,
		      prefs_common.reply_with_quote ? COMPOSE_REPLY_TO_ALL_WITH_QUOTE 
		      : COMPOSE_REPLY_TO_ALL_WITHOUT_QUOTE);
}


/*
 * Reply to Mailing List
 */
static void toolbar_reply_to_list_cb(GtkWidget *widget, gpointer data)
{
	toolbar_reply(data, 
		      prefs_common.reply_with_quote ? COMPOSE_REPLY_TO_LIST_WITH_QUOTE 
		      : COMPOSE_REPLY_TO_LIST_WITHOUT_QUOTE);
}


/*
 * Reply to sender of message
 */ 
static void toolbar_reply_to_sender_cb(GtkWidget *widget, gpointer data)
{
	toolbar_reply(data, 
		      prefs_common.reply_with_quote ? COMPOSE_REPLY_TO_SENDER_WITH_QUOTE 
		      : COMPOSE_REPLY_TO_SENDER_WITHOUT_QUOTE);
}

/*
 * Open addressbook
 */ 
static void toolbar_addrbook_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	Compose *compose;

	g_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
	case TOOLBAR_MSGVIEW:
		compose = NULL;
		break;
	case TOOLBAR_COMPOSE:
		compose = (Compose *)toolbar_item->parent;
		break;
	default:
		return;
	}
	addressbook_open(compose);
}


/*
 * Forward current/selected(s) message(s)
 */
static void toolbar_forward_cb(GtkWidget *widget, gpointer data)
{
	toolbar_reply(data, COMPOSE_FORWARD);
}

/*
 * Goto Prev Unread Message
 */
static void toolbar_prev_unread_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;
	MessageView *msgview;

	g_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)toolbar_item->parent;
		summary_select_prev_unread(mainwin->summaryview);
		break;
		
	case TOOLBAR_MSGVIEW:
		msgview = (MessageView*)toolbar_item->parent;
		msgview->updating = TRUE;
		summary_select_prev_unread(msgview->mainwin->summaryview);
		msgview->updating = FALSE;

		if (msgview->deferred_destroy) {
			debug_print("messageview got away!\n");
			messageview_destroy(msgview);
			return;
		}
		
		/* Now we need to update the messageview window */
		if (msgview->mainwin->summaryview->selected) {
			MsgInfo * msginfo = summary_get_selected_msg(msgview->mainwin->summaryview);
		       
			if (msginfo)
				messageview_show(msgview, msginfo, 
					 msgview->all_headers);
		} else {
			gtk_widget_destroy(msgview->window);
		}
		break;
	default:
		debug_print("toolbar event not supported\n");
	}
}

/*
 * Goto Next Unread Message
 */
static void toolbar_next_unread_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;
	MessageView *msgview;

	g_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)toolbar_item->parent;
		summary_select_next_unread(mainwin->summaryview);
		break;
		
	case TOOLBAR_MSGVIEW:
		msgview = (MessageView*)toolbar_item->parent;
		msgview->updating = TRUE;
		summary_select_next_unread(msgview->mainwin->summaryview);
		msgview->updating = FALSE;

		if (msgview->deferred_destroy) {
			debug_print("messageview got away!\n");
			messageview_destroy(msgview);
			return;
		}

		/* Now we need to update the messageview window */
		if (msgview->mainwin->summaryview->selected) {
			MsgInfo * msginfo = summary_get_selected_msg(msgview->mainwin->summaryview);
			
			if (msginfo)
				messageview_show(msgview, msginfo, 
					 msgview->all_headers);
		} else {
			gtk_widget_destroy(msgview->window);
		}
		break;
	default:
		debug_print("toolbar event not supported\n");
	}
}

static void toolbar_ignore_thread_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;

	g_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow *) toolbar_item->parent;
		summary_toggle_ignore_thread(mainwin->summaryview);
		break;
	case TOOLBAR_MSGVIEW:
		/* TODO: see toolbar_next_unread_cb() if you need
		 * this in the message view */
		break;
	default:
		debug_print("toolbar event not supported\n");
		break;
	}
}

static void toolbar_print_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;

	g_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow *) toolbar_item->parent;
		summary_print(mainwin->summaryview);
		break;
	case TOOLBAR_MSGVIEW:
		/* TODO: see toolbar_next_unread_cb() if you need
		 * this in the message view */
		break;
	default:
		debug_print("toolbar event not supported\n");
		break;
	}
}

static void toolbar_send_cb(GtkWidget *widget, gpointer data)
{
	compose_toolbar_cb(A_SEND, data);
}

static void toolbar_send_later_cb(GtkWidget *widget, gpointer data)
{
	compose_toolbar_cb(A_SENDL, data);
}

static void toolbar_draft_cb(GtkWidget *widget, gpointer data)
{
	compose_toolbar_cb(A_DRAFT, data);
}

static void toolbar_insert_cb(GtkWidget *widget, gpointer data)
{
	compose_toolbar_cb(A_INSERT, data);
}

static void toolbar_attach_cb(GtkWidget *widget, gpointer data)
{
	compose_toolbar_cb(A_ATTACH, data);
}

static void toolbar_sig_cb(GtkWidget *widget, gpointer data)
{
	compose_toolbar_cb(A_SIG, data);
}

static void toolbar_ext_editor_cb(GtkWidget *widget, gpointer data)
{
	compose_toolbar_cb(A_EXTEDITOR, data);
}

static void toolbar_linewrap_current_cb(GtkWidget *widget, gpointer data)
{
	compose_toolbar_cb(A_LINEWRAP_CURRENT, data);
}

static void toolbar_linewrap_all_cb(GtkWidget *widget, gpointer data)
{
	compose_toolbar_cb(A_LINEWRAP_ALL, data);
}

#ifdef USE_ASPELL
static void toolbar_check_spelling_cb(GtkWidget *widget, gpointer data)
{
	compose_toolbar_cb(A_CHECK_SPELLING, data);
}
#endif
/*
 * Execute actions from toolbar
 */
static void toolbar_actions_execute_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	GSList *action_list;
	MainWindow *mainwin;
	Compose *compose;
	MessageView *msgview;
	gpointer parent = toolbar_item->parent;

	g_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)parent;
		action_list = mainwin->toolbar->action_list;
		break;
	case TOOLBAR_COMPOSE:
		compose = (Compose*)parent;
		action_list = compose->toolbar->action_list;
		break;
	case TOOLBAR_MSGVIEW:
		msgview = (MessageView*)parent;
		action_list = msgview->toolbar->action_list;
		break;
	default:
		debug_print("toolbar event not supported\n");
		return;
	}
	toolbar_action_execute(widget, action_list, parent, toolbar_item->type);	
}

static MainWindow *get_mainwin(gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin = NULL;
	MessageView *msgview;

	g_return_val_if_fail(toolbar_item != NULL, NULL);

	switch(toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)toolbar_item->parent;
		break;
	case TOOLBAR_MSGVIEW:
		msgview = (MessageView*)toolbar_item->parent;
		mainwin = (MainWindow*)msgview->mainwin;
		break;
	default:
		break;
	}

	return mainwin;
}

static void toolbar_buttons_cb(GtkWidget   *widget, 
			       ToolbarItem *item)
{
	gint num_items;
	gint i;
	struct {
		gint   index;
		void (*func)(GtkWidget *widget, gpointer data);
	} callbacks[] = {
		{ A_RECEIVE_ALL,	toolbar_inc_all_cb		},
		{ A_RECEIVE_CUR,	toolbar_inc_cb			},
		{ A_SEND_QUEUED,	toolbar_send_queued_cb		},
		{ A_COMPOSE_EMAIL,	toolbar_compose_cb		},
		{ A_COMPOSE_NEWS,	toolbar_compose_cb		},
		{ A_REPLY_MESSAGE,	toolbar_reply_cb		},
		{ A_REPLY_SENDER,	toolbar_reply_to_sender_cb	},
		{ A_REPLY_ALL,		toolbar_reply_to_all_cb		},
		{ A_REPLY_ML,		toolbar_reply_to_list_cb	},
		{ A_FORWARD,		toolbar_forward_cb		},
		{ A_TRASH,       	toolbar_trash_cb		},
		{ A_DELETE_REAL,       	toolbar_delete_cb		},
		{ A_EXECUTE,        	toolbar_exec_cb			},
		{ A_GOTO_PREV,      	toolbar_prev_unread_cb		},
		{ A_GOTO_NEXT,      	toolbar_next_unread_cb		},
		{ A_IGNORE_THREAD,	toolbar_ignore_thread_cb	},
		{ A_PRINT,		toolbar_print_cb		},
		{ A_LEARN_SPAM,		toolbar_learn_cb		},

		{ A_SEND,		toolbar_send_cb       		},
		{ A_SENDL,		toolbar_send_later_cb 		},
		{ A_DRAFT,		toolbar_draft_cb      		},
		{ A_INSERT,		toolbar_insert_cb     		},
		{ A_ATTACH,		toolbar_attach_cb     		},
		{ A_SIG,		toolbar_sig_cb	      		},
		{ A_EXTEDITOR,		toolbar_ext_editor_cb 		},
		{ A_LINEWRAP_CURRENT,	toolbar_linewrap_current_cb   	},
		{ A_LINEWRAP_ALL,	toolbar_linewrap_all_cb   	},
		{ A_ADDRBOOK,		toolbar_addrbook_cb		},
#ifdef USE_ASPELL
		{ A_CHECK_SPELLING,     toolbar_check_spelling_cb       },
#endif
		{ A_SYL_ACTIONS,	toolbar_actions_execute_cb	}
	};

	num_items = sizeof(callbacks)/sizeof(callbacks[0]);

	for (i = 0; i < num_items; i++) {
		if (callbacks[i].index == item->index) {
			callbacks[i].func(widget, item);
			return;
		}
	}
}

/**
 * Create a new toolbar with specified type
 * if a callback list is passed it will be used before the 
 * common callback list
 **/
Toolbar *toolbar_create(ToolbarType 	 type, 
	  		GtkWidget 	*container,
			gpointer 	 data)
{
	ToolbarItem *toolbar_item;

	GtkWidget *toolbar;
	GtkWidget *icon_wid = NULL;
	GtkWidget *icon_news;
	GtkWidget *icon_ham;
	GtkWidget *item;
	GtkWidget *item_news;
	GtkWidget *item_ham;
	guint n_menu_entries;
	ComboButton *getall_combo;
	ComboButton *reply_combo;
	ComboButton *replyall_combo;
	ComboButton *replylist_combo;
	ComboButton *replysender_combo;
	ComboButton *fwd_combo;
	ComboButton *compose_combo;

	GtkTooltips *toolbar_tips;
	ToolbarSylpheedActions *action_item;
	GSList *cur;
	GSList *toolbar_list;
	Toolbar *toolbar_data;

	
 	toolbar_tips = gtk_tooltips_new();
	
	toolbar_read_config_file(type);
	toolbar_list = toolbar_get_list(type);

	toolbar_data = g_new0(Toolbar, 1); 

	toolbar = gtk_toolbar_new();
	gtk_container_add(GTK_CONTAINER(container), toolbar);
	gtk_container_set_border_width(GTK_CONTAINER(container), 2);
	gtk_toolbar_set_orientation(GTK_TOOLBAR(toolbar), GTK_ORIENTATION_HORIZONTAL);
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH);
	
	for (cur = toolbar_list; cur != NULL; cur = cur->next) {

		if (g_ascii_strcasecmp(((ToolbarItem*)cur->data)->file, TOOLBAR_TAG_SEPARATOR) == 0) {
			gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
			continue;
		}
		
		toolbar_item = g_new0(ToolbarItem, 1); 
		toolbar_item->index = ((ToolbarItem*)cur->data)->index;
		toolbar_item->file = g_strdup(((ToolbarItem*)cur->data)->file);
		toolbar_item->text = g_strdup(((ToolbarItem*)cur->data)->text);
		toolbar_item->parent = data;
		toolbar_item->type = type;

		/* collect toolbar items in list to keep track */
		toolbar_data->item_list = 
			g_slist_append(toolbar_data->item_list, 
				       toolbar_item);
		icon_wid = stock_pixmap_widget(container, stock_pixmap_get_icon(toolbar_item->file));
		item  = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
						toolbar_item->text,
						(""),
						(""),
						icon_wid, G_CALLBACK(toolbar_buttons_cb), 
						toolbar_item);
		
		switch (toolbar_item->index) {

		case A_RECEIVE_ALL:
			toolbar_data->getall_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->getall_btn, 
					   _("Receive Mail on all Accounts"), NULL);
			getall_combo = gtkut_combo_button_create(toolbar_data->getall_btn, NULL, 0,
					"<GetAll>", (gpointer)toolbar_item);
			gtk_button_set_relief(GTK_BUTTON(getall_combo->arrow),
					      GTK_RELIEF_NONE);
			gtk_toolbar_append_widget(GTK_TOOLBAR(toolbar),
				  		  GTK_WIDGET_PTR(getall_combo),
				 		  _("Receive Mail on selected Account"), "Reply");
			toolbar_data->getall_combo = getall_combo;
			break;
		case A_RECEIVE_CUR:
			toolbar_data->get_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->get_btn,
					   _("Receive Mail on current Account"), NULL);
			break;
		case A_SEND_QUEUED:
			toolbar_data->send_btn = item; 
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->send_btn,
					   _("Send Queued Messages"), NULL);
			break;
		case A_COMPOSE_EMAIL:
			icon_news = stock_pixmap_widget(container, STOCK_PIXMAP_NEWS_COMPOSE);
			item_news = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
							    _("Compose"),
							    (""),
							    (""),
							    icon_news, G_CALLBACK(toolbar_buttons_cb), 
							    toolbar_item);
			toolbar_data->compose_mail_btn = item; 
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->compose_mail_btn,
					   _("Compose Email"), NULL);
			toolbar_data->compose_news_btn = item_news;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->compose_news_btn,
					   _("Compose News"), NULL);
			compose_combo = gtkut_combo_button_create(toolbar_data->compose_mail_btn, NULL, 0,
					"<Compose>", (gpointer)toolbar_item);
			gtk_button_set_relief(GTK_BUTTON(compose_combo->arrow),
					      GTK_RELIEF_NONE);
			gtk_toolbar_append_widget(GTK_TOOLBAR(toolbar),
				  		  GTK_WIDGET_PTR(compose_combo),
				 		  _("Compose with selected Account"), "Compose");
			toolbar_data->compose_combo = compose_combo;
			break;
		case A_LEARN_SPAM:
			icon_ham = stock_pixmap_widget(container, STOCK_PIXMAP_HAM_BTN);
			item_ham = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
							    _("Ham"),
							    (""),
							    (""),
							    icon_ham, G_CALLBACK(toolbar_buttons_cb), 
							    toolbar_item);
			toolbar_data->learn_spam_btn = item; 
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->learn_spam_btn,
					   _("Learn Spam"), NULL);
			toolbar_data->learn_ham_btn = item_ham;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->learn_ham_btn,
					   _("Learn Ham"), NULL);			
			break;
		case A_REPLY_MESSAGE:
			toolbar_data->reply_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->reply_btn,
					   _("Reply to Message"), NULL);
			n_menu_entries = sizeof(reply_entries) / 
				sizeof(reply_entries[0]);
			reply_combo = gtkut_combo_button_create(toolbar_data->reply_btn,
					      reply_entries, n_menu_entries,
					      "<Reply>", (gpointer)toolbar_item);
			gtk_button_set_relief(GTK_BUTTON(reply_combo->arrow),
					      GTK_RELIEF_NONE);
			gtk_toolbar_append_widget(GTK_TOOLBAR(toolbar),
				  		  GTK_WIDGET_PTR(reply_combo),
				 		  _("Reply to Message"), "Reply");
			toolbar_data->reply_combo = reply_combo;
			break;
		case A_REPLY_SENDER:
			toolbar_data->replysender_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->replysender_btn,
					   _("Reply to Sender"), NULL);
			n_menu_entries = sizeof(replysender_entries) / 
				sizeof(replysender_entries[0]);
			replysender_combo = gtkut_combo_button_create(toolbar_data->replysender_btn,
					      replysender_entries, n_menu_entries,
					      "<ReplySender>", (gpointer)toolbar_item);
			gtk_button_set_relief(GTK_BUTTON(replysender_combo->arrow),
					      GTK_RELIEF_NONE);
			gtk_toolbar_append_widget(GTK_TOOLBAR(toolbar),
				  		  GTK_WIDGET_PTR(replysender_combo),
				 		  _("Reply to Sender"), "ReplySender");
			toolbar_data->replysender_combo = replysender_combo;
			break;
		case A_REPLY_ALL:
			toolbar_data->replyall_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->replyall_btn,
					   _("Reply to All"), NULL);
			n_menu_entries = sizeof(replyall_entries) / 
				sizeof(replyall_entries[0]);
			replyall_combo = gtkut_combo_button_create(toolbar_data->replyall_btn,
					      replyall_entries, n_menu_entries,
					      "<ReplyAll>", (gpointer)toolbar_item);
			gtk_button_set_relief(GTK_BUTTON(replyall_combo->arrow),
					      GTK_RELIEF_NONE);
			gtk_toolbar_append_widget(GTK_TOOLBAR(toolbar),
				  		  GTK_WIDGET_PTR(replyall_combo),
				 		  _("Reply to All"), "ReplyAll");
			toolbar_data->replyall_combo = replyall_combo;
			break;
		case A_REPLY_ML:
			toolbar_data->replylist_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->replylist_btn,
					   _("Reply to Mailing-list"), NULL);
			n_menu_entries = sizeof(replylist_entries) / 
				sizeof(replylist_entries[0]);
			replylist_combo = gtkut_combo_button_create(toolbar_data->replylist_btn,
					      replylist_entries, n_menu_entries,
					      "<ReplyList>", (gpointer)toolbar_item);
			gtk_button_set_relief(GTK_BUTTON(replylist_combo->arrow),
					      GTK_RELIEF_NONE);
			gtk_toolbar_append_widget(GTK_TOOLBAR(toolbar),
				  		  GTK_WIDGET_PTR(replylist_combo),
				 		  _("Reply to Mailing-list"), "ReplyList");
			toolbar_data->replylist_combo = replylist_combo;
			break;
		case A_FORWARD:
			toolbar_data->fwd_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->fwd_btn,
					     _("Forward Message"), NULL);
			n_menu_entries = sizeof(forward_entries) / 
				sizeof(forward_entries[0]);
			fwd_combo = gtkut_combo_button_create(toolbar_data->fwd_btn,
					      forward_entries, n_menu_entries,
					      "<Forward>", (gpointer)toolbar_item);
			gtk_button_set_relief(GTK_BUTTON(fwd_combo->arrow),
					      GTK_RELIEF_NONE);
			gtk_toolbar_append_widget(GTK_TOOLBAR(toolbar),
				  		  GTK_WIDGET_PTR(fwd_combo),
				 		  _("Forward Message"), "Fwd");
			toolbar_data->fwd_combo = fwd_combo;
			break;
		case A_TRASH:
			toolbar_data->trash_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->trash_btn,
					     _("Trash Message"), NULL);
			break;
		case A_DELETE_REAL:
			toolbar_data->delete_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->delete_btn,
					     _("Delete Message"), NULL);
			break;
		case A_EXECUTE:
			toolbar_data->exec_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->exec_btn,
					   _("Execute"), NULL);
			break;
		case A_GOTO_PREV:
			toolbar_data->prev_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->prev_btn,
					     _("Go to Previous Unread Message"),
					     NULL);
			break;
		case A_GOTO_NEXT:
			toolbar_data->next_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->next_btn,
					     _("Go to Next Unread Message"),
					     NULL);
			break;
		
		/* Compose Toolbar */
		case A_SEND:
			toolbar_data->send_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->send_btn, 
					     _("Send Message"), NULL);
			break;
		case A_SENDL:
			toolbar_data->sendl_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->sendl_btn,
					     _("Put into queue folder and send later"), NULL);
			break;
		case A_DRAFT:
			toolbar_data->draft_btn = item; 
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->draft_btn,
					     _("Save to draft folder"), NULL);
			break;
		case A_INSERT:
			toolbar_data->insert_btn = item; 
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->insert_btn,
					     _("Insert file"), NULL);
			break;
		case A_ATTACH:
			toolbar_data->attach_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->attach_btn,
					     _("Attach file"), NULL);
			break;
		case A_SIG:
			toolbar_data->sig_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->sig_btn,
					     _("Insert signature"), NULL);
			break;
		case A_EXTEDITOR:
			toolbar_data->exteditor_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->exteditor_btn,
					     _("Edit with external editor"), NULL);
			break;
		case A_LINEWRAP_CURRENT:
			toolbar_data->linewrap_current_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->linewrap_current_btn,
					     _("Wrap long lines of current paragraph"), NULL);
			break;
		case A_LINEWRAP_ALL:
			toolbar_data->linewrap_all_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->linewrap_all_btn,
					     _("Wrap all long lines"), NULL);
			break;
		case A_ADDRBOOK:
			toolbar_data->addrbook_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->addrbook_btn,
					     _("Address book"), NULL);
			break;
#ifdef USE_ASPELL
		case A_CHECK_SPELLING:
			toolbar_data->spellcheck_btn = item;
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     toolbar_data->spellcheck_btn,
					     _("Check spelling"), NULL);
			break;
#endif

		case A_SYL_ACTIONS:
			action_item = g_new0(ToolbarSylpheedActions, 1);
			action_item->widget = item;
			action_item->name   = g_strdup(toolbar_item->text);

			toolbar_data->action_list = 
				g_slist_append(toolbar_data->action_list,
					       action_item);

			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips), 
					     item,
					     action_item->name, NULL);

			gtk_widget_show(item);
			break;
		default:
			/* find and set the tool tip text */
			gtk_tooltips_set_tip(GTK_TOOLTIPS(toolbar_tips),
					     item,
					     toolbar_ret_descr_from_val
						(toolbar_item->index),
					     NULL);
			break;
		}

	}
	toolbar_data->toolbar = toolbar;
	if (type == TOOLBAR_MAIN)
		activate_compose_button(toolbar_data, 
					prefs_common.toolbar_style, 
					toolbar_data->compose_btn_type);
	if (type != TOOLBAR_COMPOSE)
		activate_learn_button(toolbar_data, prefs_common.toolbar_style,
				LEARN_SPAM);
	
	gtk_widget_show_all(toolbar);
	
	return toolbar_data; 
}

/**
 * Free toolbar structures
 */ 
void toolbar_destroy(Toolbar * toolbar) {

	TOOLBAR_DESTROY_ITEMS(toolbar->item_list);	
	TOOLBAR_DESTROY_ACTIONS(toolbar->action_list);
}

void toolbar_update(ToolbarType type, gpointer data)
{
	Toolbar *toolbar_data;
	GtkWidget *handlebox;
	MainWindow *mainwin = (MainWindow*)data;
	Compose    *compose = (Compose*)data;
	MessageView *msgview = (MessageView*)data;

	switch(type) {
	case TOOLBAR_MAIN:
		toolbar_data = mainwin->toolbar;
		handlebox    = mainwin->handlebox;
		break;
	case TOOLBAR_COMPOSE:
		toolbar_data = compose->toolbar;
		handlebox    = compose->handlebox;
		break;
	case TOOLBAR_MSGVIEW:
		toolbar_data = msgview->toolbar;
		handlebox    = msgview->handlebox;
		break;
	default:
		return;
	}

	gtk_container_remove(GTK_CONTAINER(handlebox), 
			     GTK_WIDGET(toolbar_data->toolbar));

	toolbar_init(toolbar_data);
 	toolbar_data = toolbar_create(type, handlebox, data);
	switch(type) {
	case TOOLBAR_MAIN:
		mainwin->toolbar = toolbar_data;
		break;
	case TOOLBAR_COMPOSE:
		compose->toolbar = toolbar_data;
		break;
	case TOOLBAR_MSGVIEW:
		msgview->toolbar = toolbar_data;
		break;
	}

	toolbar_style(type, prefs_common.toolbar_style, data);

	if (type == TOOLBAR_MAIN) {
		toolbar_main_set_sensitive((MainWindow*)data);
		account_set_menu_only_toolbar();
	}
}

#define GTK_BUTTON_SET_SENSITIVE(widget,sensitive) {		\
	gboolean in_btn = FALSE;				\
	if (GTK_IS_BUTTON(widget))				\
		in_btn = GTK_BUTTON(widget)->in_button;		\
	gtk_widget_set_sensitive(widget, sensitive);		\
	if (GTK_IS_BUTTON(widget))				\
		GTK_BUTTON(widget)->in_button = in_btn;		\
}

void toolbar_main_set_sensitive(gpointer data)
{
	SensitiveCond state;
	gboolean sensitive;
	MainWindow *mainwin = (MainWindow*)data;
	Toolbar *toolbar = mainwin->toolbar;
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

	
	if (toolbar->get_btn)
		SET_WIDGET_COND(toolbar->get_btn, 
			M_HAVE_ACCOUNT|M_UNLOCKED);

	if (toolbar->getall_btn) {
		SET_WIDGET_COND(toolbar->getall_btn, 
			M_HAVE_ACCOUNT|M_UNLOCKED);
		SET_WIDGET_COND(GTK_WIDGET_PTR(toolbar->getall_combo),
			M_HAVE_ACCOUNT|M_UNLOCKED);
	}
	if (toolbar->send_btn) {
		SET_WIDGET_COND(toolbar->send_btn,
			M_HAVE_QUEUED_MAILS);
	}
	if (toolbar->compose_mail_btn) {
		SET_WIDGET_COND(GTK_WIDGET_PTR(toolbar->compose_combo),
			M_HAVE_ACCOUNT);
		SET_WIDGET_COND(toolbar->compose_news_btn, 
			M_HAVE_ACCOUNT);
	}
	if (toolbar->reply_btn) {
		SET_WIDGET_COND(toolbar->reply_btn,
			M_HAVE_ACCOUNT|M_TARGET_EXIST);
		SET_WIDGET_COND(GTK_WIDGET_PTR(toolbar->reply_combo),
			M_HAVE_ACCOUNT|M_TARGET_EXIST);
	}
	if (toolbar->replyall_btn) {
		SET_WIDGET_COND(toolbar->replyall_btn,
			M_HAVE_ACCOUNT|M_TARGET_EXIST);
		SET_WIDGET_COND(GTK_WIDGET_PTR(toolbar->replyall_combo),
			M_HAVE_ACCOUNT|M_TARGET_EXIST);
	}
	if (toolbar->replylist_btn) {
		SET_WIDGET_COND(toolbar->replylist_btn,
			M_HAVE_ACCOUNT|M_TARGET_EXIST);
		SET_WIDGET_COND(GTK_WIDGET_PTR(toolbar->replylist_combo),
			M_HAVE_ACCOUNT|M_TARGET_EXIST);
	}
	if (toolbar->replysender_btn) {
		SET_WIDGET_COND(toolbar->replysender_btn,
			M_HAVE_ACCOUNT|M_TARGET_EXIST);
		SET_WIDGET_COND(GTK_WIDGET_PTR(toolbar->replysender_combo),
			M_HAVE_ACCOUNT|M_TARGET_EXIST);
	}
	if (toolbar->fwd_btn) {
		SET_WIDGET_COND(toolbar->fwd_btn, 
			M_HAVE_ACCOUNT|M_TARGET_EXIST);
		SET_WIDGET_COND(GTK_WIDGET_PTR(toolbar->fwd_combo),
			M_HAVE_ACCOUNT|M_TARGET_EXIST); 
	}
	if (toolbar->fwd_combo) {
		GtkWidget *submenu = gtk_item_factory_get_widget(toolbar->fwd_combo->factory, "/Redirect");
		SET_WIDGET_COND(submenu, 
			M_HAVE_ACCOUNT|M_TARGET_EXIST); 
	}

	if (prefs_common.next_unread_msg_dialog == NEXTUNREADMSGDIALOG_ASSUME_NO) {
		SET_WIDGET_COND(toolbar->next_btn, M_MSG_EXIST);
	} else {
		SET_WIDGET_COND(toolbar->next_btn, 0);
	}
	
	if (toolbar->trash_btn)
		SET_WIDGET_COND(toolbar->trash_btn,
			M_TARGET_EXIST|M_ALLOW_DELETE);

	if (toolbar->delete_btn)
		SET_WIDGET_COND(toolbar->delete_btn,
			M_TARGET_EXIST|M_ALLOW_DELETE);

	if (toolbar->exec_btn)
		SET_WIDGET_COND(toolbar->exec_btn, 
			M_DELAY_EXEC);
	
	if (toolbar->learn_ham_btn)
		SET_WIDGET_COND(toolbar->learn_ham_btn,
			M_TARGET_EXIST|M_CAN_LEARN_SPAM);

	if (toolbar->learn_spam_btn)
		SET_WIDGET_COND(toolbar->learn_spam_btn, 
			M_TARGET_EXIST|M_CAN_LEARN_SPAM);

	for (cur = toolbar->action_list; cur != NULL;  cur = cur->next) {
		ToolbarSylpheedActions *act = (ToolbarSylpheedActions*)cur->data;
		
		SET_WIDGET_COND(act->widget, M_TARGET_EXIST|M_UNLOCKED);
	}

#undef SET_WIDGET_COND

	state = main_window_get_current_state(mainwin);

	for (cur = entry_list; cur != NULL; cur = cur->next) {
		Entry *e = (Entry*) cur->data;

		if (e->widget != NULL) {
			sensitive = ((e->cond & state) == e->cond);
			GTK_BUTTON_SET_SENSITIVE(e->widget, sensitive);	
		}
	}
	
	while (entry_list != NULL) {
		Entry *e = (Entry*) entry_list->data;

		g_free(e);
		entry_list = g_slist_remove(entry_list, e);
	}

	g_slist_free(entry_list);

	activate_compose_button(toolbar, 
				prefs_common.toolbar_style,
				toolbar->compose_btn_type);
	
}

void toolbar_comp_set_sensitive(gpointer data, gboolean sensitive)
{
	Compose *compose = (Compose*)data;
	GSList *items = compose->toolbar->action_list;

	if (compose->toolbar->send_btn)
		GTK_BUTTON_SET_SENSITIVE(compose->toolbar->send_btn, sensitive);
	if (compose->toolbar->sendl_btn)
		GTK_BUTTON_SET_SENSITIVE(compose->toolbar->sendl_btn, sensitive);
	if (compose->toolbar->draft_btn )
		GTK_BUTTON_SET_SENSITIVE(compose->toolbar->draft_btn , sensitive);
	if (compose->toolbar->insert_btn )
		GTK_BUTTON_SET_SENSITIVE(compose->toolbar->insert_btn , sensitive);
	if (compose->toolbar->attach_btn)
		GTK_BUTTON_SET_SENSITIVE(compose->toolbar->attach_btn, sensitive);
	if (compose->toolbar->sig_btn)
		GTK_BUTTON_SET_SENSITIVE(compose->toolbar->sig_btn, sensitive);
	if (compose->toolbar->exteditor_btn)
		GTK_BUTTON_SET_SENSITIVE(compose->toolbar->exteditor_btn, sensitive);
	if (compose->toolbar->linewrap_current_btn)
		GTK_BUTTON_SET_SENSITIVE(compose->toolbar->linewrap_current_btn, sensitive);
	if (compose->toolbar->linewrap_all_btn)
		GTK_BUTTON_SET_SENSITIVE(compose->toolbar->linewrap_all_btn, sensitive);
	if (compose->toolbar->addrbook_btn)
		GTK_BUTTON_SET_SENSITIVE(compose->toolbar->addrbook_btn, sensitive);
#ifdef USE_ASPELL
	if (compose->toolbar->spellcheck_btn)
		GTK_BUTTON_SET_SENSITIVE(compose->toolbar->spellcheck_btn, sensitive);
#endif
	for (; items != NULL; items = g_slist_next(items)) {
		ToolbarSylpheedActions *item = (ToolbarSylpheedActions *)items->data;
		GTK_BUTTON_SET_SENSITIVE(item->widget, sensitive);
	}
}

/**
 * Initialize toolbar structure
 **/
void toolbar_init(Toolbar * toolbar) {

	toolbar->toolbar          	= NULL;
	toolbar->get_btn          	= NULL;
	toolbar->getall_btn       	= NULL;
	toolbar->getall_combo       	= NULL;
	toolbar->send_btn         	= NULL;
	toolbar->compose_mail_btn 	= NULL;
	toolbar->compose_news_btn 	= NULL;
	toolbar->compose_combo	 	= NULL;
	toolbar->reply_btn        	= NULL;
	toolbar->replysender_btn  	= NULL;
	toolbar->replyall_btn     	= NULL;
	toolbar->replylist_btn    	= NULL;
	toolbar->fwd_btn          	= NULL;
	toolbar->trash_btn       	= NULL;
	toolbar->delete_btn       	= NULL;
	toolbar->prev_btn         	= NULL;
	toolbar->next_btn         	= NULL;
	toolbar->exec_btn         	= NULL;

	/* compose buttons */ 
	toolbar->sendl_btn        	= NULL;
	toolbar->draft_btn        	= NULL;
	toolbar->insert_btn       	= NULL;
	toolbar->attach_btn       	= NULL;
	toolbar->sig_btn          	= NULL;	
	toolbar->exteditor_btn    	= NULL;	
	toolbar->linewrap_current_btn	= NULL;	
	toolbar->linewrap_all_btn     	= NULL;	
	toolbar->addrbook_btn     	= NULL;	
#ifdef USE_ASPELL
	toolbar->spellcheck_btn   	= NULL;
#endif

	toolbar_destroy(toolbar);
}

/*
 */
static void toolbar_reply(gpointer data, guint action)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;
	MessageView *msgview;
	GSList *msginfo_list = NULL;

	g_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)toolbar_item->parent;
		msginfo_list = summary_get_selection(mainwin->summaryview);
		msgview = (MessageView*)mainwin->messageview;
		break;
	case TOOLBAR_MSGVIEW:
		msgview = (MessageView*)toolbar_item->parent;
		g_return_if_fail(msgview != NULL);	
		msginfo_list = g_slist_append(msginfo_list, msgview->msginfo);
		break;
	default:
		return;
	}

	g_return_if_fail(msgview != NULL);
	g_return_if_fail(msginfo_list != NULL);
	compose_reply_from_messageview(msgview, msginfo_list, action);
	g_slist_free(msginfo_list);

	/* TODO: update reply state ion summaryview */
}


/* exported functions */

void inc_mail_cb(gpointer data, guint action, GtkWidget *widget)
{
	MainWindow *mainwin = (MainWindow*)data;

	inc_mail(mainwin, prefs_common.newmail_notify_manu);
}

void inc_all_account_mail_cb(gpointer data, guint action, GtkWidget *widget)
{
	MainWindow *mainwin = (MainWindow*)data;

	inc_all_account_mail(mainwin, FALSE, prefs_common.newmail_notify_manu);
}

void send_queue_cb(gpointer data, guint action, GtkWidget *widget)
{
	GList *list;
	gboolean found;
	gboolean got_error = FALSE;
	gchar *errstr = NULL;

	if (prefs_common.work_offline)
		if (alertpanel(_("Offline warning"), 
			       _("You're working offline. Override?"),
			       GTK_STOCK_NO, GTK_STOCK_YES,
			       NULL) != G_ALERTALTERNATE)
		return;

	/* ask for confirmation before sending queued messages only
	   in online mode and if there is at least one message queued
	   in any of the folder queue
	*/
	if (prefs_common.confirm_send_queued_messages) {
		found = FALSE;
		/* check if there's a queued message */
		for (list = folder_get_list(); !found && list != NULL; list = list->next) {
			Folder *folder = list->data;

			found = !procmsg_queue_is_empty(folder->queue);
		}
		/* if necessary, ask for confirmation before sending */
		if (found && !prefs_common.work_offline) {
			if (alertpanel(_("Send queued messages"), 
			    	   _("Send all queued messages?"),
			    	   GTK_STOCK_CANCEL, _("_Send"),
				   NULL) != G_ALERTALTERNATE)
				return;
		}
	}

	for (list = folder_get_list(); list != NULL; list = list->next) {
		Folder *folder = list->data;

		if (folder->queue) {
			if (procmsg_send_queue(folder->queue, 
					       prefs_common.savemsg,
					       &errstr) < 0)
				got_error = TRUE;
		}
	}
	if (got_error) {
		if (!errstr)
			alertpanel_error_log(_("Some errors occurred while "
					   "sending queued messages."));
		else {
			gchar *tmp = g_strdup_printf(_("Some errors occurred "
					"while sending queued messages:\n%s"), errstr);
			g_free(errstr);
			alertpanel_error_log(tmp);
			g_free(tmp);
		}
	}
}

void compose_mail_cb(gpointer data, guint action, GtkWidget *widget)
{
	MainWindow *mainwin = (MainWindow*)data;
	PrefsAccount *ac = NULL;
	FolderItem *item = mainwin->summaryview->folder_item;	
        GList * list;
        GList * cur;
	
	if (item) {
		ac = account_find_from_item(item);
		if (ac && ac->protocol != A_NNTP) {
			compose_new_with_folderitem(ac, item, NULL);		/* CLAWS */
			return;
		}
	}

	/*
	 * CLAWS - use current account
	 */
	if (cur_account && (cur_account->protocol != A_NNTP)) {
		compose_new_with_folderitem(cur_account, item, NULL);
		return;
	}

	/*
	 * CLAWS - just get the first one
	 */
	list = account_get_list();
	for (cur = list ; cur != NULL ; cur = g_list_next(cur)) {
		ac = (PrefsAccount *) cur->data;
		if (ac->protocol != A_NNTP) {
			compose_new_with_folderitem(ac, item, NULL);
			return;
		}
	}
}

void compose_news_cb(gpointer data, guint action, GtkWidget *widget)
{
	MainWindow *mainwin = (MainWindow*)data;
	PrefsAccount * ac = NULL;
	GList * list;
	GList * cur;

	if (mainwin->summaryview->folder_item) {
		ac = mainwin->summaryview->folder_item->folder->account;
		if (ac && ac->protocol == A_NNTP) {
			compose_new_with_folderitem(ac,
				    mainwin->summaryview->folder_item, NULL);
			return;
		}
	}

	list = account_get_list();
	for(cur = list ; cur != NULL ; cur = g_list_next(cur)) {
		ac = (PrefsAccount *) cur->data;
		if (ac->protocol == A_NNTP) {
			compose_new_with_folderitem(ac,
				    mainwin->summaryview->folder_item, NULL);
			return;
		}
	}
}
