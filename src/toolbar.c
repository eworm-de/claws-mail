/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2001-2025 the Claws Mail team and Hiroyuki Yamamoto
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>
#include <setjmp.h>

#include "main.h"
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
#include "imap.h"
#include "account.h"
#include "send_message.h"
#ifndef USE_ALT_ADDRBOOK
	#include "addressbook.h"
#else
	#include "addressbook-dbus.h"
#endif

/* elements */
#define TOOLBAR_TAG_INDEX        "toolbar"
#define TOOLBAR_TAG_ITEM         "item"
#define TOOLBAR_TAG_SEPARATOR    "separator"

#define TOOLBAR_ICON_FILE   "file"    
#define TOOLBAR_ICON_TEXT   "text"     
#define TOOLBAR_ICON_ACTION "action"    

static void toolbar_init(Toolbar * toolbar);
static gboolean      toolbar_is_duplicate		(gint           action,
					      	 ToolbarType	source);
static void   toolbar_parse_item		(XMLFile        *file,
					      	 ToolbarType	source, gboolean *rewrite);

static gint   toolbar_ret_val_from_text		(const gchar	*text);
static gchar *toolbar_ret_text_from_val		(gint           val);

typedef struct _DefaultToolbar DefaultToolbar;
struct _DefaultToolbar {
	gint action;
};
static void toolbar_set_default_generic(ToolbarType toolbar_type,
						 DefaultToolbar *default_toolbar);

static void	toolbar_style			(ToolbarType 	 type, 
						 guint 		 action, 
						 gpointer 	 data);

static MainWindow *get_mainwin			(gpointer data);
static void activate_compose_button 		(Toolbar	*toolbar,
				     		 ToolbarStyle	 style,
				     		 ComposeButtonType type);

/* toolbar callbacks */
static void toolbar_reply			(gpointer 	 data, 
						 guint		 action);

static void toolbar_learn			(gpointer 	 data, 
						 guint		 action);

static void toolbar_delete_dup		(gpointer 	 data, 
						 guint		 action);

static void toolbar_trash_cb			(GtkWidget	*widget,
					 	 gpointer        data);

static void toolbar_delete_cb			(GtkWidget	*widget,
					 	 gpointer        data);

static void toolbar_delete_dup_cb   		(GtkWidget   	*widget, 

					 	 gpointer     	 data);

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

static void toolbar_watch_thread_cb	   	(GtkWidget	*widget,
					    	 gpointer 	 data);

static void toolbar_mark_cb	   	(GtkWidget	*widget,
					    	 gpointer 	 data);

static void toolbar_unmark_cb	   	(GtkWidget	*widget,
					    	 gpointer 	 data);

static void toolbar_lock_cb	   	(GtkWidget	*widget,
					    	 gpointer 	 data);

static void toolbar_unlock_cb	   	(GtkWidget	*widget,
					    	 gpointer 	 data);

static void toolbar_all_read_cb	   	(GtkWidget	*widget,
					    	 gpointer 	 data);

static void toolbar_all_unread_cb	   	(GtkWidget	*widget,
					    	 gpointer 	 data);

static void toolbar_read_cb	   	(GtkWidget	*widget,
					    	 gpointer 	 data);

static void toolbar_unread_cb	   	(GtkWidget	*widget,
					    	 gpointer 	 data);

static void toolbar_run_processing_cb	   	(GtkWidget	*widget,
					    	 gpointer 	 data);

static void toolbar_print_cb			(GtkWidget	*widget,
					    	 gpointer 	 data);

static void toolbar_actions_execute_cb	   	(GtkWidget     	*widget,
				  	    	 gpointer      	 data);
static void toolbar_plugins_execute_cb      (GtkWidget      *widget,
                             gpointer        data);


static void toolbar_send_cb			(GtkWidget	*widget,
					 	 gpointer	 data);
static void toolbar_send_later_cb		(GtkWidget	*widget,
					 	 gpointer	 data);
static void toolbar_draft_cb			(GtkWidget	*widget,
					 	 gpointer	 data);
static void toolbar_close_cb			(GtkWidget	*widget,
					 	 gpointer	 data);
static void toolbar_preferences_cb		(GtkWidget	*widget,
					 	 gpointer	 data);
static void toolbar_open_mail_cb		(GtkWidget	*widget,
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
#ifdef USE_ENCHANT
static void toolbar_check_spelling_cb  		(GtkWidget   	*widget, 
					 	 gpointer     	 data);
#endif
static void toolbar_cancel_inc_cb		(GtkWidget	*widget,
						 gpointer	 data);
static void toolbar_cancel_send_cb		(GtkWidget	*widget,
						 gpointer	 data);
static void toolbar_cancel_all_cb		(GtkWidget	*widget,
						 gpointer	 data);

struct {
	gchar *index_str;
	const gchar *descr;
} toolbar_text [] = {
	{ "A_RECEIVE_ALL",   	N_("Receive Mail from all Accounts")       },
	{ "A_RECEIVE_CUR",   	N_("Receive Mail from current Account")    },
	{ "A_SEND_QUEUED",   	N_("Send Queued Messages")                 },
	{ "A_COMPOSE_EMAIL", 	N_("Write Email")                          },
	{ "A_COMPOSE_NEWS",  	N_("Write News")                           },
	{ "A_REPLY_MESSAGE", 	N_("Reply to Message")                     },
	{ "A_REPLY_SENDER",  	N_("Reply to Sender")                      },
	{ "A_REPLY_ALL",     	N_("Reply to All")                         },
	{ "A_REPLY_ML",      	N_("Reply to Mailing-list")                },
	{ "A_OPEN_MAIL",        N_("Open email")                           },
	{ "A_FORWARD",       	N_("Forward Message")                      }, 
	{ "A_TRASH",        	N_("Trash Message")   	                   },
	{ "A_DELETE_REAL",    	N_("Delete Message")                       },
	{ "A_DELETE_DUP",       N_("Delete duplicate messages") },
	{ "A_EXECUTE",       	N_("Execute")                              },
	{ "A_GOTO_PREV",     	N_("Go to Previous Unread Message")        },
	{ "A_GOTO_NEXT",     	N_("Go to Next Unread Message")            },

	{ "A_IGNORE_THREAD", 	N_("Ignore thread")                        },
	{ "A_WATCH_THREAD", 	N_("Watch thread")                         },
	{ "A_MARK", 			N_("Mark Message")                         },
	{ "A_UNMARK", 			N_("Unmark Message")                       },
	{ "A_LOCK", 			N_("Lock Message")                         },
	{ "A_UNLOCK", 			N_("Unlock Message")                       },
	{ "A_ALL_READ",			N_("Mark all Messages as read")            },
	{ "A_ALL_UNREAD",		N_("Mark all Messages as unread")          },
	{ "A_READ", 			N_("Mark Message as read")                 },
	{ "A_UNREAD", 			N_("Mark Message as unread")               },
	{ "A_RUN_PROCESSING",	N_("Run folder processing rules")          },

	{ "A_PRINT",	     	N_("Print")                                },
	{ "A_LEARN_SPAM",       N_("Learn Spam or Ham")                    },
	{ "A_GO_FOLDERS",   	N_("Open folder/Go to folder list")        },
	{ "A_PREFERENCES",      N_("Preferences")                          },

	{ "A_SEND",          	N_("Send Message")                         },
	{ "A_SEND_LATER",     	N_("Put into queue folder and send later") },
	{ "A_DRAFT",         	N_("Save to draft folder")                 },
	{ "A_INSERT",        	N_("Insert file")                          },   
	{ "A_ATTACH",        	N_("Attach file")                          },
	{ "A_SIG",           	N_("Insert signature")                     },
	{ "A_REP_SIG",          N_("Replace signature")                    },
	{ "A_EXTEDITOR",     	N_("Edit with external editor")            },
	{ "A_LINEWRAP_CURRENT",	N_("Wrap long lines of current paragraph") }, 
	{ "A_LINEWRAP_ALL",     N_("Wrap all long lines")                  }, 
	{ "A_ADDRBOOK",      	N_("Address book")                         },
#ifdef USE_ENCHANT
	{ "A_CHECK_SPELLING",	N_("Check spelling")                       },
#endif
	{ "A_PRIVACY_SIGN",     N_("Sign")                                 },
	{ "A_PRIVACY_ENCRYPT",  N_("Encrypt")                              },
	{ "A_CLAWS_ACTIONS",   	N_("Claws Mail Actions Feature")           }, 
	{ "A_CANCEL_INC",       N_("Cancel receiving")                     },
	{ "A_CANCEL_SEND",      N_("Cancel sending")                       },
	{ "A_CANCEL_ALL",       N_("Cancel receiving/sending")             },
	{ "A_CLOSE",            N_("Close window")                         },
	{ "A_SEPARATOR",     	N_("Separator")                            },
	{ "A_CLAWS_PLUGINS",    N_("Claws Mail Plugins")                   }
};

/* migration table: support reading toolbar configuration files with
   old action names and converting them to current action names,
   see toolbar_parse_item(), which makes uses of this alias table.
*/
struct {
	const gchar *old_name;
	const gchar *current_name;
} toolbar_migration [] = {
	{ "A_SYL_ACTIONS",   "A_CLAWS_ACTIONS" },
	{ "A_SENDL",         "A_SEND_LATER" },
	{ NULL,              NULL }
};
	
/* struct holds configuration files and a list of
 * currently active toolbar items 
 * TOOLBAR_MAIN, TOOLBAR_COMPOSE and TOOLBAR_MSGVIEW
 * give us an index
 */
struct {
	const gchar  *conf_file;
	GSList       *item_list;
} toolbar_config[NUM_TOOLBARS] = {
	{ "toolbar_main.xml",    NULL},
	{ "toolbar_compose.xml", NULL}, 
  	{ "toolbar_msgview.xml", NULL}
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
	cm_return_val_if_fail(val >=0 && val < N_ACTION_VAL, NULL);

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
	cm_return_val_if_fail(val >=0 && val < N_ACTION_VAL, NULL);

	return toolbar_text[val].index_str;
}

static gboolean toolbar_is_duplicate(gint action, ToolbarType source)
{
	GSList *cur;

	if ((action == A_SEPARATOR) || (action == A_CLAWS_ACTIONS) || (action == A_CLAWS_PLUGINS))
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
		gint main_items[] = {
					A_RECEIVE_ALL,   A_RECEIVE_CUR,   A_SEND_QUEUED,
					A_COMPOSE_EMAIL, A_REPLY_MESSAGE, A_REPLY_SENDER,
					A_REPLY_ALL,     A_REPLY_ML,      A_OPEN_MAIL,     A_FORWARD,
					A_TRASH,         A_DELETE_REAL,   A_DELETE_DUP,    A_EXECUTE,
					A_GOTO_PREV,     A_GOTO_NEXT,     A_IGNORE_THREAD, A_WATCH_THREAD,
					A_MARK,          A_UNMARK,        A_LOCK,          A_UNLOCK,
					A_ALL_READ,      A_ALL_UNREAD,    A_READ,          A_UNREAD,
					A_RUN_PROCESSING,
					A_PRINT,         A_ADDRBOOK,      A_LEARN_SPAM,    A_GO_FOLDERS,
					A_CANCEL_INC,    A_CANCEL_SEND,   A_CANCEL_ALL,    A_PREFERENCES };

		for (i = 0; i < sizeof main_items / sizeof main_items[0]; i++)  {
			items = g_list_append(items, gettext(toolbar_text[main_items[i]].descr));
		}	
	}
	else if (source == TOOLBAR_COMPOSE) {
		gint comp_items[] =   {
					A_SEND,          A_SEND_LATER,   A_DRAFT,
					A_INSERT,        A_ATTACH,       A_SIG,
					A_REP_SIG,       A_EXTEDITOR,    A_LINEWRAP_CURRENT,
					A_LINEWRAP_ALL,  A_ADDRBOOK,
#ifdef USE_ENCHANT
					A_CHECK_SPELLING,
#endif
					A_PRIVACY_SIGN, A_PRIVACY_ENCRYPT,
					A_CLOSE };

		for (i = 0; i < sizeof comp_items / sizeof comp_items[0]; i++) 
			items = g_list_append(items, gettext(toolbar_text[comp_items[i]].descr));
	}
	else if (source == TOOLBAR_MSGVIEW) {
		gint msgv_items[] =   {
					A_COMPOSE_EMAIL, A_REPLY_MESSAGE, A_REPLY_SENDER,
					A_REPLY_ALL,     A_REPLY_ML,      A_FORWARD,
					A_TRASH,         A_DELETE_REAL,   A_GOTO_PREV,      A_GOTO_NEXT,
					A_ADDRBOOK,      A_LEARN_SPAM,    A_CLOSE };

		for (i = 0; i < sizeof msgv_items / sizeof msgv_items[0]; i++) 
			items = g_list_append(items, gettext(toolbar_text[msgv_items[i]].descr));
	}

	return items;
}

static void toolbar_parse_item(XMLFile *file, ToolbarType source, gboolean *rewrite)
{
	GList *attr;
	gchar *name, *value;
	ToolbarItem *item = NULL;

	g_return_if_fail(rewrite != NULL);

	attr = xml_get_current_tag_attr(file);
	item = g_new0(ToolbarItem, 1);
	while( attr ) {
		name = ((XMLAttr *)attr->data)->name;
		value = ((XMLAttr *)attr->data)->value;
		
		if (g_utf8_collate(name, TOOLBAR_ICON_FILE) == 0) 
			item->file = g_strdup (value);
		else if (g_utf8_collate(name, TOOLBAR_ICON_TEXT) == 0)
			item->text = g_strdup (*value ? gettext(value):"");
		else if (g_utf8_collate(name, TOOLBAR_ICON_ACTION) == 0)
			item->index = toolbar_ret_val_from_text(value);

		if ((item->index == -1) && !strcmp(value, "A_DELETE")) {
			/* switch button */
			item->index = A_TRASH;
			g_free(item->file);
			item->file = g_strdup("trash_btn");
			g_free(item->text);
			item->text = g_strdup(C_("Toolbar", "Trash"));
			*rewrite = TRUE;
		}
		if (!strcmp(item->file, "mail") && !strcmp(value, "A_DRAFT")) {
			/* switch icon file */
			g_free(item->file);
			item->file = g_strdup("mail_draft");
			*rewrite = TRUE;
		}
		if (item->index == -1) {
			/* item not found in table: try migrating old action names to current ones */
			gint i;

			/* replace action name */
			for (i = 0; toolbar_migration[i].old_name != NULL; i++) {
				if (g_utf8_collate(value, toolbar_migration[i].old_name) == 0) {
					item->index = toolbar_ret_val_from_text(toolbar_migration[i].current_name);
					if (item->index != -1) {
						*rewrite = TRUE;
						debug_print("toolbar_parse_item: migrating action label from '%s' to '%s'\n",
							value, toolbar_migration[i].current_name);
						break;
					}
				}
			}
		}
		if ((item->index == -1) && !rewrite)
			g_warning("toolbar_parse_item: unrecognized action name '%s'", value);

		attr = g_list_next(attr);
	}
	if (item->index != -1) {
		if (!toolbar_is_duplicate(item->index, source))  {
			toolbar_config[source].item_list = g_slist_append(toolbar_config[source].item_list,
									 item);
		} else {
			toolbar_item_destroy(item);
		}        
	} else {
		toolbar_item_destroy(item);
	}    
}

const gchar *toolbar_get_short_text(int action) {
	switch(action) {
	case A_RECEIVE_ALL: 	return _("Get Mail");
	case A_RECEIVE_CUR: 	return _("Get");
	case A_SEND_QUEUED: 	return _("Send");
	case A_COMPOSE_EMAIL: 	return C_("Toolbar", "Write");
	case A_COMPOSE_NEWS: 	return C_("Toolbar", "Write");
	case A_REPLY_MESSAGE: 	return _("Reply");
	case A_REPLY_SENDER: 	return C_("Toolbar", "Sender");
	case A_REPLY_ALL: 		return _("All");
	case A_REPLY_ML: 		return _("List");
	case A_OPEN_MAIL: 		return _("Open");
	case A_FORWARD: 		return _("Forward");
	case A_TRASH: 			return C_("Toolbar", "Trash");
	case A_DELETE_REAL:		return _("Delete");
	case A_DELETE_DUP: 		return _("Delete duplicates");
	case A_EXECUTE:			return _("Execute");
	case A_GOTO_PREV: 		return _("Prev");
	case A_GOTO_NEXT: 		return _("Next");

	case A_IGNORE_THREAD: 	return _("Ignore thread");
	case A_WATCH_THREAD: 	return _("Watch thread");
	case A_MARK: 			return _("Mark");
	case A_UNMARK: 			return _("Unmark");
	case A_LOCK: 			return _("Lock");
	case A_UNLOCK: 			return _("Unlock");
	case A_ALL_READ: 		return _("All read");
	case A_ALL_UNREAD: 		return _("All unread");
	case A_READ: 			return _("Read");
	case A_UNREAD: 			return _("Unread");
	case A_RUN_PROCESSING:	return _("Run proc. rules");

	case A_PRINT:	 		return _("Print");
	case A_LEARN_SPAM: 		return _("Spam");
	case A_GO_FOLDERS: 		return _("Folders");
	case A_PREFERENCES:		return _("Preferences");

	case A_SEND: 			return _("Send");
	case A_SEND_LATER:		return _("Send later");
	case A_DRAFT: 			return _("Draft");
	case A_INSERT: 			return _("Insert");
	case A_ATTACH: 			return _("Attach");
	case A_SIG: 			return _("Insert sig.");
	case A_REP_SIG: 		return _("Replace sig.");
	case A_EXTEDITOR:		return _("Edit");
	case A_LINEWRAP_CURRENT:return _("Wrap para.");
	case A_LINEWRAP_ALL:	return _("Wrap all");
	case A_ADDRBOOK: 		return _("Address");
	#ifdef USE_ENCHANT
	case A_CHECK_SPELLING:	return _("Check spelling");
	#endif
	case A_PRIVACY_SIGN:	return _("Sign");
	case A_PRIVACY_ENCRYPT:	return _("Encrypt");

	case A_CANCEL_INC:		return _("Stop");
	case A_CANCEL_SEND:		return _("Stop");
	case A_CANCEL_ALL:		return _("Stop all");
	case A_CLOSE: 			return _("Close");
	default:				return "";
	}
}

gint toolbar_get_icon(int action) {
	switch(action) {
	case A_RECEIVE_ALL: 	return STOCK_PIXMAP_MAIL_RECEIVE_ALL;
	case A_RECEIVE_CUR: 	return STOCK_PIXMAP_MAIL_RECEIVE;
	case A_SEND_QUEUED: 	return STOCK_PIXMAP_MAIL_SEND_QUEUE;
	case A_COMPOSE_EMAIL: 	return STOCK_PIXMAP_MAIL_COMPOSE;
	case A_COMPOSE_NEWS: 	return STOCK_PIXMAP_NEWS_COMPOSE;
	case A_REPLY_MESSAGE: 	return STOCK_PIXMAP_MAIL_REPLY;
	case A_REPLY_SENDER: 	return STOCK_PIXMAP_MAIL_REPLY_TO_AUTHOR;
	case A_REPLY_ALL: 		return STOCK_PIXMAP_MAIL_REPLY_TO_ALL;
	case A_REPLY_ML: 		return STOCK_PIXMAP_MAIL_REPLY_TO_LIST;
	case A_OPEN_MAIL: 		return STOCK_PIXMAP_OPEN_MAIL;
	case A_FORWARD: 		return STOCK_PIXMAP_MAIL_FORWARD;
	case A_TRASH: 			return STOCK_PIXMAP_TRASH;
	case A_DELETE_REAL:		return STOCK_PIXMAP_DELETE;
	case A_DELETE_DUP: 		return STOCK_PIXMAP_DELETE_DUP;
	case A_EXECUTE:			return STOCK_PIXMAP_EXEC;
	case A_GOTO_PREV: 		return STOCK_PIXMAP_UP_ARROW;
	case A_GOTO_NEXT: 		return STOCK_PIXMAP_DOWN_ARROW;

	case A_IGNORE_THREAD: 	return STOCK_PIXMAP_MARK_IGNORETHREAD;
	case A_WATCH_THREAD: 	return STOCK_PIXMAP_MARK_WATCHTHREAD;
	case A_MARK:   			return STOCK_PIXMAP_MARK_MARK;
	case A_UNMARK:   		return STOCK_PIXMAP_MARK_UNMARK;
	case A_LOCK:   			return STOCK_PIXMAP_MARK_LOCKED;
	case A_UNLOCK:   		return STOCK_PIXMAP_MARK_UNLOCKED;
	case A_ALL_READ:		return STOCK_PIXMAP_MARK_ALLREAD;
	case A_ALL_UNREAD:		return STOCK_PIXMAP_MARK_ALLUNREAD;
	case A_READ:   			return STOCK_PIXMAP_MARK_READ;
	case A_UNREAD:   		return STOCK_PIXMAP_MARK_UNREAD;
	case A_RUN_PROCESSING:	return STOCK_PIXMAP_DIR_OPEN;

	case A_PRINT:	 		return STOCK_PIXMAP_PRINTER_BTN;
	case A_LEARN_SPAM: 		return STOCK_PIXMAP_SPAM_BTN;
	case A_GO_FOLDERS: 		return STOCK_PIXMAP_GO_FOLDERS;
	case A_PREFERENCES:		return STOCK_PIXMAP_PREFERENCES;

	case A_SEND: 			return STOCK_PIXMAP_MAIL_SEND;
	case A_SEND_LATER:		return STOCK_PIXMAP_MAIL_SEND_QUEUE;
	case A_DRAFT: 			return STOCK_PIXMAP_MAIL_DRAFT;
	case A_INSERT: 			return STOCK_PIXMAP_INSERT_FILE;
	case A_ATTACH: 			return STOCK_PIXMAP_MAIL_ATTACH;
	case A_SIG: 			return STOCK_PIXMAP_MAIL_SIGN;
	case A_REP_SIG: 		return STOCK_PIXMAP_MAIL_SIGN;
	case A_EXTEDITOR:		return STOCK_PIXMAP_EDIT_EXTERN;
	case A_LINEWRAP_CURRENT:return STOCK_PIXMAP_LINEWRAP_CURRENT;
	case A_LINEWRAP_ALL:	return STOCK_PIXMAP_LINEWRAP_ALL;
	case A_ADDRBOOK: 		return STOCK_PIXMAP_ADDRESS_BOOK;
	#ifdef USE_ENCHANT
	case A_CHECK_SPELLING:	return STOCK_PIXMAP_CHECK_SPELLING;
	#endif
	case A_PRIVACY_SIGN:	return STOCK_PIXMAP_MAIL_PRIVACY_SIGNED;
	case A_PRIVACY_ENCRYPT:	return STOCK_PIXMAP_MAIL_PRIVACY_ENCRYPTED;

	case A_CANCEL_INC:		return STOCK_PIXMAP_CANCEL;
	case A_CANCEL_SEND:		return STOCK_PIXMAP_CANCEL;
	case A_CANCEL_ALL:		return STOCK_PIXMAP_CANCEL;
	case A_CLOSE: 			return STOCK_PIXMAP_CLOSE;
	default:				return -1;
	}
}

static void toolbar_set_default_generic(ToolbarType toolbar_type, DefaultToolbar *default_toolbar)
{
	gint i;

	g_return_if_fail(default_toolbar != NULL);

	for (i = 0; default_toolbar[i].action != N_ACTION_VAL; i++) {
		
		ToolbarItem *toolbar_item = g_new0(ToolbarItem, 1);
		
		if (default_toolbar[i].action != A_SEPARATOR) {
			gchar *file = NULL;
			gint icon;

			icon = toolbar_get_icon(default_toolbar[i].action);
			if (icon > -1) {
				file = stock_pixmap_get_name((StockPixmap)icon);
			}
			toolbar_item->file  = g_strdup(file);
			toolbar_item->index = default_toolbar[i].action;
			toolbar_item->text  = g_strdup(toolbar_get_short_text(default_toolbar[i].action));
		} else {
			toolbar_item->file  = g_strdup(TOOLBAR_TAG_SEPARATOR);
			toolbar_item->index = A_SEPARATOR;
		}

		if (toolbar_item->index != -1) {
			if (!toolbar_is_duplicate(toolbar_item->index, toolbar_type)) {
				toolbar_config[toolbar_type].item_list = 
					g_slist_append(toolbar_config[toolbar_type].item_list, toolbar_item);
			} else {
				toolbar_item_destroy(toolbar_item);
			}
		} else {
			toolbar_item_destroy(toolbar_item);
		}
	}
}

void toolbar_set_default(ToolbarType source)
{
	DefaultToolbar default_toolbar_main[] = {
#ifdef GENERIC_UMPC
		{ A_GO_FOLDERS},
		{ A_OPEN_MAIL},		
		{ A_SEPARATOR}, 
#endif
		{ A_RECEIVE_ALL},
		{ A_SEPARATOR}, 
		{ A_SEND_QUEUED},
		{ A_COMPOSE_EMAIL},
		{ A_SEPARATOR},
		{ A_REPLY_MESSAGE}, 
#ifndef GENERIC_UMPC
		{ A_REPLY_ALL},
		{ A_REPLY_SENDER},
#endif
		{ A_FORWARD},
		{ A_SEPARATOR},
		{ A_TRASH},
#ifndef GENERIC_UMPC
		{ A_LEARN_SPAM},
#endif
		{ A_SEPARATOR},
		{ A_GOTO_NEXT},
		{ N_ACTION_VAL}
	};
	DefaultToolbar default_toolbar_compose[] = {
#ifdef GENERIC_UMPC
		{ A_CLOSE},
		{ A_SEPARATOR}, 
#endif
		{ A_SEND},
		{ A_SEND_LATER},
		{ A_DRAFT},
		{ A_SEPARATOR}, 
#ifndef GENERIC_UMPC
		{ A_INSERT},
#endif
		{ A_ATTACH},
		{ A_SEPARATOR},
		{ A_ADDRBOOK},
		{ N_ACTION_VAL}
	};

	DefaultToolbar default_toolbar_msgview[] = {
#ifdef GENERIC_UMPC
		{ A_CLOSE},
		{ A_SEPARATOR}, 
#endif
		{ A_REPLY_MESSAGE}, 
		{ A_REPLY_ALL},
		{ A_REPLY_SENDER},
		{ A_FORWARD},
		{ A_SEPARATOR},
		{ A_TRASH},
#ifndef GENERIC_UMPC
		{ A_LEARN_SPAM},
#endif
		{ A_GOTO_NEXT},
		{ N_ACTION_VAL}
	};
	
	if (source == TOOLBAR_MAIN)
		toolbar_set_default_generic(TOOLBAR_MAIN, default_toolbar_main);
	else if  (source == TOOLBAR_COMPOSE)
		toolbar_set_default_generic(TOOLBAR_COMPOSE, default_toolbar_compose);
	else if  (source == TOOLBAR_MSGVIEW)
		toolbar_set_default_generic(TOOLBAR_MSGVIEW, default_toolbar_msgview);
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
	if( pfile ) {
		fp = pfile->fp;
		if (fprintf(fp, "<?xml version=\"1.0\" encoding=\"%s\" ?>\n", CS_INTERNAL) < 0)
			goto fail;

		if (fprintf(fp, "<%s>\n", TOOLBAR_TAG_INDEX) < 0)
			goto fail;

		for (cur = toolbar_config[source].item_list; cur != NULL; cur = cur->next) {
			ToolbarItem *toolbar_item = (ToolbarItem*) cur->data;
			
			if (toolbar_item->index != A_SEPARATOR) {
				if (fprintf(fp, "\t<%s %s=\"%s\" %s=\"",
					TOOLBAR_TAG_ITEM, 
					TOOLBAR_ICON_FILE, toolbar_item->file,
					TOOLBAR_ICON_TEXT) < 0)
					goto fail;
				if (xml_file_put_escape_str(fp, toolbar_item->text) < 0)
					goto fail;
				if (fprintf(fp, "\" %s=\"%s\"/>\n",
					TOOLBAR_ICON_ACTION, 
					toolbar_ret_text_from_val(toolbar_item->index)) < 0)
					goto fail;
			} else {
				if (fprintf(fp, "\t<%s/>\n", TOOLBAR_TAG_SEPARATOR) < 0)
					goto fail;
			}
		}

		if (fprintf(fp, "</%s>\n", TOOLBAR_TAG_INDEX) < 0)
			goto fail;
	
		g_free( fileSpec );
		if (prefs_file_close (pfile) < 0 ) 
			g_warning("failed to write toolbar configuration to file");
		return;
		
fail:
		FILE_OP_ERROR(fileSpec, "fprintf");
		g_free(fileSpec);
		prefs_file_close_revert (pfile);
	} else {
		g_free(fileSpec);
		g_warning("failed to open toolbar configuration file for writing");
	}
}

void toolbar_read_config_file(ToolbarType source)
{
	XMLFile *file   = NULL;
	gchar *fileSpec = NULL;
	jmp_buf    jumper;
	gboolean rewrite = FALSE;

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

		for (;;) {
			if (!file->level) 
				break;
			/* Get item tag */
			if (xml_parse_next_tag(file)) 
				longjmp(jumper, 1);

			/* Get next tag (icon, icon_text or icon_action) */
			if (xml_compare_tag(file, TOOLBAR_TAG_ITEM)) {
				toolbar_parse_item(file, source, &rewrite);
			} else if (xml_compare_tag(file, TOOLBAR_TAG_SEPARATOR)) {
				ToolbarItem *item = g_new0(ToolbarItem, 1);
			
				item->file   = g_strdup(TOOLBAR_TAG_SEPARATOR);
				item->index  = A_SEPARATOR;
				toolbar_config[source].item_list = 
					g_slist_append(toolbar_config[source].item_list, item);
			}

		}
		xml_close_file(file);
		if (rewrite) {
			debug_print("toolbar_read_config_file: rewriting toolbar\n");
			toolbar_save_config_file(source);
		}
	}

	if ((!file) || (g_slist_length(toolbar_config[source].item_list) == 0)) {

		if (source == TOOLBAR_MAIN) 
			toolbar_set_default(TOOLBAR_MAIN);
		else if (source == TOOLBAR_COMPOSE) 
			toolbar_set_default(TOOLBAR_COMPOSE);
		else if (source == TOOLBAR_MSGVIEW) 
			toolbar_set_default(TOOLBAR_MSGVIEW);
		else {		
			g_warning("refusing to write unknown Toolbar Configuration number %d", source);
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

		toolbar_item_destroy(item);	
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

static void toolbar_action_execute(GtkWidget    *widget,
			    GSList       *action_list, 
			    gpointer     data,
			    gint         source) 
{
	GSList *cur;
	gint i = 0;

	for (cur = action_list; cur != NULL;  cur = cur->next) {
		ToolbarClawsActions *act = (ToolbarClawsActions*)cur->data;

		if (widget == act->widget) {
			i = prefs_actions_find_by_name(act->name);

			if (i != -1) 
				break;
		}
	}

	if (i != -1) 
		actions_execute(data, i, widget, source);
	else
		g_warning("error: did not find Action to execute");
}

gboolean toolbar_check_action_btns(ToolbarType type)
{
	GSList *temp, *curr, *list = toolbar_config[type].item_list;
	gboolean modified = FALSE;
	
	curr = list;
	while (curr != NULL) {
		ToolbarItem *toolbar_item = (ToolbarItem *) curr->data;
		temp = curr;
		curr = curr->next;
		
		if (toolbar_item->index != A_CLAWS_ACTIONS)
			continue;

		if (prefs_actions_find_by_name(toolbar_item->text) == -1) {
			list = g_slist_delete_link(list, temp);
			g_free(toolbar_item->file);
			g_free(toolbar_item->text);
			g_free(toolbar_item);
			modified = TRUE;
		}
	}
	
	return modified;
}

#define CLAWS_SET_TOOL_ITEM_TIP(widget,tip) { \
	gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(widget), tip);				\
}

#define CLAWS_SET_ARROW_TIP(widget,tip) { \
	gtk_menu_tool_button_set_arrow_tooltip_text(GTK_MENU_TOOL_BUTTON(widget), tip);				\
}

static void activate_compose_button (Toolbar           *toolbar,
				     ToolbarStyle      style,
				     ComposeButtonType type)
{
	if ((!toolbar->compose_mail_btn))
		return;

	if (type == COMPOSEBUTTON_NEWS) {
		gtk_tool_button_set_icon_widget(
			GTK_TOOL_BUTTON(toolbar->compose_mail_btn),
			toolbar->compose_news_icon);
#ifndef GENERIC_UMPC
		CLAWS_SET_TOOL_ITEM_TIP(GTK_TOOL_ITEM(toolbar->compose_mail_btn), _("Write News message"));
#endif	
		gtk_widget_show(toolbar->compose_news_icon);
	} else {
		gtk_tool_button_set_icon_widget(
			GTK_TOOL_BUTTON(toolbar->compose_mail_btn),
			toolbar->compose_mail_icon);
#ifndef GENERIC_UMPC
		CLAWS_SET_TOOL_ITEM_TIP(GTK_TOOL_ITEM(toolbar->compose_mail_btn), _("Write Email"));
#endif	
		gtk_widget_show(toolbar->compose_mail_icon);
	}
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
	if ((!toolbar->learn_spam_btn))
		return;

	if (type == LEARN_SPAM) {
		gtk_tool_button_set_icon_widget(
			GTK_TOOL_BUTTON(toolbar->learn_spam_btn),
			toolbar->learn_spam_icon);
		gtk_tool_button_set_label(
			GTK_TOOL_BUTTON(toolbar->learn_spam_btn),
			_("Spam"));
#ifndef GENERIC_UMPC
		CLAWS_SET_TOOL_ITEM_TIP(GTK_TOOL_ITEM(toolbar->learn_spam_btn), _("Learn spam"));	
#endif
		gtk_widget_show(toolbar->learn_spam_icon);
	} else {
		gtk_tool_button_set_icon_widget(
			GTK_TOOL_BUTTON(toolbar->learn_spam_btn),
			toolbar->learn_ham_icon);
		gtk_tool_button_set_label(
			GTK_TOOL_BUTTON(toolbar->learn_spam_btn),
			_("Ham"));
#ifndef GENERIC_UMPC
		CLAWS_SET_TOOL_ITEM_TIP(GTK_TOOL_ITEM(toolbar->learn_spam_btn), _("Learn ham"));
#endif	
		gtk_widget_show(toolbar->learn_ham_icon);
	}
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
	const GList *list;
	const GList *cur;

	cm_return_if_fail(mainwin != NULL);

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
	
	cm_return_if_fail(data != NULL);
	
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

	cm_return_if_fail(toolbar_item != NULL);

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

	cm_return_if_fail(toolbar_item != NULL);

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

	cm_return_if_fail(toolbar_item != NULL);

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

	cm_return_if_fail(mainwin != NULL);
	summary_execute(mainwin->summaryview);
}

/*
 * Delete current/selected(s) message(s)
 */
static void toolbar_trash_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;

	cm_return_if_fail(toolbar_item != NULL);
	cm_return_if_fail(toolbar_item->parent);
	
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

	cm_return_if_fail(toolbar_item != NULL);
	cm_return_if_fail(toolbar_item->parent);
	
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

static void toolbar_delete_dup(gpointer data, guint all)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin = NULL;

	cm_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)toolbar_item->parent;
		if (all)
			mainwindow_delete_duplicated_all(mainwin);
		else
			mainwindow_delete_duplicated(mainwin);
		break;
	case TOOLBAR_COMPOSE:
	case TOOLBAR_MSGVIEW:
		break;
	default:
		debug_print("toolbar event not supported\n");
		return;
	}
}

static void toolbar_delete_dup_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin = NULL;

	cm_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)toolbar_item->parent;
		mainwindow_delete_duplicated(mainwin);
		break;
	case TOOLBAR_COMPOSE:
	case TOOLBAR_MSGVIEW:
		break;
	default:
		return;
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

	cm_return_if_fail(toolbar_item != NULL);

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

static void toolbar_learn(gpointer data, guint as_spam)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;
	MessageView *msgview;

	cm_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)toolbar_item->parent;
		if (as_spam) 
			mainwindow_learn(mainwin, TRUE);
		else
			mainwindow_learn(mainwin, FALSE);
		break;
	case TOOLBAR_MSGVIEW:
		msgview = (MessageView*)toolbar_item->parent;
		if (as_spam) 
			messageview_learn(msgview, TRUE);
		else
			messageview_learn(msgview, FALSE);
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

	cm_return_if_fail(toolbar_item != NULL);

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
	toolbar_reply(data, (prefs_common.reply_with_quote ? 
		      COMPOSE_REPLY_WITH_QUOTE : COMPOSE_REPLY_WITHOUT_QUOTE));
}


/*
 * Reply message to Sender and All recipients
 */
static void toolbar_reply_to_all_cb(GtkWidget *widget, gpointer data)
{
	toolbar_reply(data,
		      (prefs_common.reply_with_quote ? COMPOSE_REPLY_TO_ALL_WITH_QUOTE 
		      : COMPOSE_REPLY_TO_ALL_WITHOUT_QUOTE));
}


/*
 * Reply to Mailing List
 */
static void toolbar_reply_to_list_cb(GtkWidget *widget, gpointer data)
{
	toolbar_reply(data, 
		      (prefs_common.reply_with_quote ? COMPOSE_REPLY_TO_LIST_WITH_QUOTE 
		      : COMPOSE_REPLY_TO_LIST_WITHOUT_QUOTE));
}


/*
 * Reply to sender of message
 */ 
static void toolbar_reply_to_sender_cb(GtkWidget *widget, gpointer data)
{
	toolbar_reply(data, 
		      (prefs_common.reply_with_quote ? COMPOSE_REPLY_TO_SENDER_WITH_QUOTE 
		      : COMPOSE_REPLY_TO_SENDER_WITHOUT_QUOTE));
}

/*
 * Open addressbook
 */ 
static void toolbar_addrbook_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	Compose *compose;

	cm_return_if_fail(toolbar_item != NULL);

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
#ifndef USE_ALT_ADDRBOOK
	addressbook_open(compose);
#else
	GError* error = NULL;
	addressbook_connect_signals(compose);
	addressbook_dbus_open(TRUE, &error);
	if (error) {
		g_warning("%s", error->message);
		g_error_free(error);
	}
#endif
}


/*
 * Forward current/selected(s) message(s)
 */
static void toolbar_forward_cb(GtkWidget *widget, gpointer data)
{
	toolbar_reply(data, (COMPOSE_FORWARD));
}

/*
 * Goto Prev Unread Message
 */
static void toolbar_prev_unread_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;
	MessageView *msgview;

	cm_return_if_fail(toolbar_item != NULL);

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
#ifndef GENERIC_UMPC
			MsgInfo * msginfo = summary_get_selected_msg(msgview->mainwin->summaryview);
		       
			if (msginfo)
				messageview_show(msgview, msginfo, 
					 msgview->all_headers);
#endif
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

	cm_return_if_fail(toolbar_item != NULL);

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
#ifndef GENERIC_UMPC
			MsgInfo * msginfo = summary_get_selected_msg(msgview->mainwin->summaryview);
			
			if (msginfo)
				messageview_show(msgview, msginfo, 
					 msgview->all_headers);
#endif
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

	cm_return_if_fail(toolbar_item != NULL);

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

static void toolbar_watch_thread_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;

	cm_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow *) toolbar_item->parent;
		summary_toggle_watch_thread(mainwin->summaryview);
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

static void toolbar_mark_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;

	cm_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow *) toolbar_item->parent;
		summary_mark(mainwin->summaryview);
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

static void toolbar_unmark_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;

	cm_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow *) toolbar_item->parent;
		summary_unmark(mainwin->summaryview);
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

static void toolbar_lock_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;

	cm_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow *) toolbar_item->parent;
		summary_msgs_lock(mainwin->summaryview);
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

static void toolbar_unlock_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;

	cm_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow *) toolbar_item->parent;
		summary_msgs_unlock(mainwin->summaryview);
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

static void toolbar_all_read_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;

	cm_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow *) toolbar_item->parent;
		summary_mark_all_read(mainwin->summaryview, TRUE);
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

static void toolbar_all_unread_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;

	cm_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow *) toolbar_item->parent;
		summary_mark_all_unread(mainwin->summaryview, TRUE);
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

static void toolbar_read_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;

	cm_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow *) toolbar_item->parent;
		summary_mark_as_read(mainwin->summaryview);
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

static void toolbar_unread_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;

	cm_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow *) toolbar_item->parent;
		summary_mark_as_unread(mainwin->summaryview);
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

static void toolbar_run_processing_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;
	FolderItem *item;

	cm_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow *) toolbar_item->parent;
		item = mainwin->summaryview->folder_item;
		cm_return_if_fail(item != NULL);
		item->processing_pending = TRUE;
		folder_item_apply_processing(item);
		item->processing_pending = FALSE;
		break;
	default:
		debug_print("toolbar event not supported\n");
		break;
	}
}

static void toolbar_cancel_inc_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;

	cm_return_if_fail(toolbar_item != NULL);
	inc_cancel_all();
	imap_cancel_all();
}

static void toolbar_cancel_send_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;

	cm_return_if_fail(toolbar_item != NULL);
	send_cancel();
}

static void toolbar_cancel_all_cb(GtkWidget *widget, gpointer data)
{
	toolbar_cancel_inc_cb(widget, data);
	toolbar_cancel_send_cb(widget, data);
}

static void toolbar_print_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;

	cm_return_if_fail(toolbar_item != NULL);

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
	compose_toolbar_cb(A_SEND_LATER, data);
}

static void toolbar_draft_cb(GtkWidget *widget, gpointer data)
{
	compose_toolbar_cb(A_DRAFT, data);
}

static void toolbar_close_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;
	MessageView *messageview;
	Compose *compose;

	cm_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow *) toolbar_item->parent;
		app_will_exit(NULL, mainwin);
		break;
	case TOOLBAR_MSGVIEW:
		messageview = (MessageView *)toolbar_item->parent;
		messageview_destroy(messageview);
		break;
	case TOOLBAR_COMPOSE:
		compose = (Compose *)toolbar_item->parent;
		compose_close_toolbar(compose);
		break;
	}
}

static void toolbar_preferences_cb(GtkWidget *widget, gpointer data)
{
	prefs_gtk_open();
}

static void toolbar_open_mail_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin;

	cm_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow *) toolbar_item->parent;
		summary_open_row(NULL, mainwin->summaryview);
		break;
	case TOOLBAR_MSGVIEW:
		debug_print("toolbar event not supported\n");
		break;
	case TOOLBAR_COMPOSE:
		debug_print("toolbar event not supported\n");
		break;
	}
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

static void toolbar_replace_sig_cb(GtkWidget *widget, gpointer data)
{
	compose_toolbar_cb(A_REP_SIG, data);
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

#ifdef USE_ENCHANT
static void toolbar_check_spelling_cb(GtkWidget *widget, gpointer data)
{
	compose_toolbar_cb(A_CHECK_SPELLING, data);
}
#endif

static void toolbar_privacy_sign_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	Compose *compose = (Compose *)toolbar_item->parent;
	gboolean state = gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(widget));

	cm_return_if_fail(compose != NULL);
	compose_use_signing(compose, state);
}

/* Any time the toggle button gets toggled, we want to update its tooltip. */
static void toolbar_privacy_sign_toggled_cb(GtkWidget *widget, gpointer data)
{
	gboolean state = gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(widget));

	if (state)
		gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(widget), _("Message will be signed"));
	else
		gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(widget), _("Message will not be signed"));
}

static void toolbar_privacy_encrypt_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	Compose *compose = (Compose *)toolbar_item->parent;
	gboolean state = gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(widget));

	cm_return_if_fail(compose != NULL);
	compose_use_encryption(compose, state);
}

/* Any time the toggle button gets toggled, we want to update its tooltip. */
static void toolbar_privacy_encrypt_toggled_cb(GtkWidget *widget, gpointer data)
{
	gboolean state = gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(widget));

	if (state)
		gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(widget), _("Message will be encrypted"));
	else
		gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(widget), _("Message will not be encrypted"));
}

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

	cm_return_if_fail(toolbar_item != NULL);

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

static void toolbar_plugins_execute_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = data;
	prefs_toolbar_execute_plugin_item(toolbar_item->parent, toolbar_item->type, toolbar_item->text);
}

static MainWindow *get_mainwin(gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin = NULL;
	MessageView *msgview;

	cm_return_val_if_fail(toolbar_item != NULL, NULL);

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

static void toolbar_go_folders_cb(GtkWidget *widget, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	MainWindow *mainwin = NULL;
	switch(toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)toolbar_item->parent;
		break;
	default:
		g_warning("wrong toolbar type");
		return;
	}

	if (!mainwin->in_folder) {
		FolderItem *item = folderview_get_selected_item(mainwin->folderview);
		if (item) {
			folderview_select(mainwin->folderview, item);
		}
	} else {
		folderview_grab_focus(mainwin->folderview);
		mainwindow_exit_folder(mainwin);
	}
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
		{ A_RECEIVE_ALL,		toolbar_inc_all_cb			},
		{ A_RECEIVE_CUR,		toolbar_inc_cb				},
		{ A_SEND_QUEUED,		toolbar_send_queued_cb		},
		{ A_COMPOSE_EMAIL,		toolbar_compose_cb			},
		{ A_COMPOSE_NEWS,		toolbar_compose_cb			},
		{ A_REPLY_MESSAGE,		toolbar_reply_cb			},
		{ A_REPLY_SENDER,		toolbar_reply_to_sender_cb	},
		{ A_REPLY_ALL,			toolbar_reply_to_all_cb		},
		{ A_REPLY_ML,			toolbar_reply_to_list_cb	},
		{ A_FORWARD,			toolbar_forward_cb			},
		{ A_TRASH,       		toolbar_trash_cb			},
		{ A_DELETE_REAL,       	toolbar_delete_cb			},
		{ A_EXECUTE,        	toolbar_exec_cb				},
		{ A_GOTO_PREV,      	toolbar_prev_unread_cb		},
		{ A_GOTO_NEXT,      	toolbar_next_unread_cb		},
		{ A_IGNORE_THREAD,		toolbar_ignore_thread_cb	},
		{ A_WATCH_THREAD,		toolbar_watch_thread_cb		},
		{ A_MARK,				toolbar_mark_cb				},
		{ A_UNMARK,				toolbar_unmark_cb			},
		{ A_LOCK,				toolbar_lock_cb				},
		{ A_UNLOCK,				toolbar_unlock_cb			},
		{ A_ALL_READ,			toolbar_all_read_cb			},
		{ A_ALL_UNREAD,			toolbar_all_unread_cb		},
		{ A_READ,				toolbar_read_cb				},
		{ A_UNREAD,				toolbar_unread_cb			},
		{ A_RUN_PROCESSING,		toolbar_run_processing_cb	},
		{ A_PRINT,				toolbar_print_cb			},
		{ A_LEARN_SPAM,			toolbar_learn_cb			},
		{ A_DELETE_DUP,			toolbar_delete_dup_cb		},
		{ A_GO_FOLDERS,			toolbar_go_folders_cb		},

		{ A_SEND,				toolbar_send_cb	   			},
		{ A_SEND_LATER,			toolbar_send_later_cb 		},
		{ A_DRAFT,				toolbar_draft_cb	  		},
		{ A_OPEN_MAIL,			toolbar_open_mail_cb		},
		{ A_CLOSE,				toolbar_close_cb			},
		{ A_PREFERENCES,		toolbar_preferences_cb		},
		{ A_INSERT,				toolbar_insert_cb	 		},
		{ A_ATTACH,				toolbar_attach_cb	 		},
		{ A_SIG,				toolbar_sig_cb		  		},
		{ A_REP_SIG,			toolbar_replace_sig_cb		},
		{ A_EXTEDITOR,			toolbar_ext_editor_cb 		},
		{ A_LINEWRAP_CURRENT,	toolbar_linewrap_current_cb	},
		{ A_LINEWRAP_ALL,		toolbar_linewrap_all_cb   	},
		{ A_ADDRBOOK,			toolbar_addrbook_cb			},
#ifdef USE_ENCHANT
		{ A_CHECK_SPELLING,     toolbar_check_spelling_cb	},
#endif
		{ A_PRIVACY_SIGN,		toolbar_privacy_sign_cb		},
		{ A_PRIVACY_ENCRYPT,	toolbar_privacy_encrypt_cb	},
		{ A_CLAWS_ACTIONS,		toolbar_actions_execute_cb	},
		{ A_CANCEL_INC,			toolbar_cancel_inc_cb		},
		{ A_CANCEL_SEND,		toolbar_cancel_send_cb		},
		{ A_CANCEL_ALL,			toolbar_cancel_all_cb		},
		{ A_CLAWS_PLUGINS,  	toolbar_plugins_execute_cb	},
	};

	num_items = sizeof(callbacks)/sizeof(callbacks[0]);

	for (i = 0; i < num_items; i++) {
		if (callbacks[i].index == item->index) {
			callbacks[i].func(widget, item);
			return;
		}
	}
}
#ifndef GENERIC_UMPC
#define TOOLBAR_ITEM(item,icon,text,tooltip) {								\
	item = GTK_WIDGET(gtk_tool_button_new(icon, text));						\
	gtk_widget_set_can_focus(gtk_bin_get_child(GTK_BIN(item)), FALSE);				\
	gtk_tool_item_set_homogeneous(GTK_TOOL_ITEM(item), FALSE);					\
	gtk_tool_item_set_is_important(GTK_TOOL_ITEM(item), TRUE);					\
	g_signal_connect (G_OBJECT(item), "clicked", G_CALLBACK(toolbar_buttons_cb), toolbar_item);	\
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(item), -1);				\
	CLAWS_SET_TOOL_ITEM_TIP(GTK_TOOL_ITEM(item), 							\
			tooltip);									\
}

#define TOOLBAR_TOGGLE_ITEM(item,icon,text,tooltip) {							\
	item = GTK_WIDGET(gtk_toggle_tool_button_new());						\
	gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(item), icon);					\
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(item), text);						\
	gtk_widget_set_can_focus(gtk_bin_get_child(GTK_BIN(item)), FALSE);				\
	gtk_tool_item_set_homogeneous(GTK_TOOL_ITEM(item), FALSE);					\
	gtk_tool_item_set_is_important(GTK_TOOL_ITEM(item), TRUE);					\
	g_signal_connect (G_OBJECT(item), "clicked", G_CALLBACK(toolbar_buttons_cb), toolbar_item);	\
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(item), -1);				\
	CLAWS_SET_TOOL_ITEM_TIP(GTK_TOOL_ITEM(item), 							\
			tooltip);									\
}

#define TOOLBAR_MENUITEM(item,icon,text,tooltip,menutip) {						\
	GtkWidget *child = NULL, *btn = NULL, *arr = NULL;						\
	GList *gchild = NULL;										\
	item = GTK_WIDGET(gtk_menu_tool_button_new(icon, text));					\
	gtk_tool_item_set_homogeneous(GTK_TOOL_ITEM(item), FALSE);				\
	gtk_tool_item_set_is_important(GTK_TOOL_ITEM(item), TRUE);					\
	g_signal_connect (G_OBJECT(item), "clicked", G_CALLBACK(toolbar_buttons_cb), toolbar_item);	\
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(item), -1);				\
	CLAWS_SET_TOOL_ITEM_TIP(GTK_TOOL_ITEM(item), 							\
			tooltip);									\
	CLAWS_SET_ARROW_TIP(GTK_MENU_TOOL_BUTTON(item), menutip);					\
	child = gtk_bin_get_child(GTK_BIN(item)); 							\
	gchild = gtk_container_get_children(								\
			GTK_CONTAINER(child)); 								\
	btn = (GtkWidget *)gchild->data;								\
	gtk_widget_set_can_focus(btn, FALSE);								\
	arr = (GtkWidget *)(gchild->next?gchild->next->data:NULL);					\
	gtk_widget_set_can_focus(arr, FALSE);								\
	g_list_free(gchild);										\
	gchild = gtk_container_get_children(GTK_CONTAINER(arr));					\
	gtk_widget_set_size_request(GTK_WIDGET(gchild->data), 9, -1);					\
	g_list_free(gchild);										\
}

#else

#define TOOLBAR_ITEM(item,icon,text,tooltip) {								\
	item = GTK_WIDGET(gtk_tool_button_new(icon, text));						\
	gtk_widget_set_can_focus(gtk_bin_get_child(GTK_BIN(item)), FALSE);				\
	gtk_tool_item_set_homogeneous(GTK_TOOL_ITEM(item), FALSE);					\
	gtk_tool_item_set_is_important(GTK_TOOL_ITEM(item), TRUE);					\
	g_signal_connect (G_OBJECT(item), "clicked", G_CALLBACK(toolbar_buttons_cb), toolbar_item);	\
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(item), -1);				\
}

#define TOOLBAR_TOGGLE_ITEM(item,icon,text,tooltip) {							\
	item = GTK_WIDGET(gtk_toggle_tool_button_new());						\
	gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(item), icon);					\
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(item), text);						\
	gtk_widget_set_can_focus(gtk_bin_get_child(GTK_BIN(item)), FALSE);				\
	gtk_tool_item_set_homogeneous(GTK_TOOL_ITEM(item), FALSE);					\
	gtk_tool_item_set_is_important(GTK_TOOL_ITEM(item), TRUE);					\
	g_signal_connect (G_OBJECT(item), "clicked", G_CALLBACK(toolbar_buttons_cb), toolbar_item);	\
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(item), -1);				\
}

#define TOOLBAR_MENUITEM(item,icon,text,tooltip,menutip) {						\
	GtkWidget *child = NULL, *btn = NULL, *arr = NULL;						\
	GList *gchild = NULL;										\
	item = GTK_WIDGET(gtk_menu_tool_button_new(icon, text));					\
	gtk_tool_item_set_homogeneous(GTK_TOOL_ITEM(item), FALSE);				\
	gtk_tool_item_set_is_important(GTK_TOOL_ITEM(item), TRUE);					\
	g_signal_connect (G_OBJECT(item), "clicked", G_CALLBACK(toolbar_buttons_cb), toolbar_item);	\
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(item), -1);				\
	child = gtk_bin_get_child(GTK_BIN(item)); 							\
	gchild = gtk_container_get_children(								\
			GTK_CONTAINER(child)); 								\
	btn = (GtkWidget *)gchild->data;								\
	gtk_widget_set_can_focus(btn, FALSE);								\
	arr = (GtkWidget *)(gchild->next?gchild->next->data:NULL);					\
	gtk_widget_set_can_focus(arr, FALSE);								\
	g_list_free(gchild);										\
	gchild = gtk_container_get_children(GTK_CONTAINER(arr));					\
	gtk_widget_set_size_request(GTK_WIDGET(gchild->data), 9, -1);					\
	g_list_free(gchild);										\
}
#endif

#define ADD_MENU_ITEM(name,cb,data) {							\
	item = gtk_menu_item_new_with_mnemonic(name);					\
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);				\
	g_signal_connect(G_OBJECT(item), "activate",					\
			 G_CALLBACK(cb),						\
			   toolbar_item);						\
	g_object_set_data(G_OBJECT(item), "int-value", GINT_TO_POINTER(data));		\
	gtk_widget_show(item);								\
}

#ifndef GENERIC_UMPC
static void toolbar_reply_menu_cb(GtkWidget *widget, gpointer data)
{
	gpointer int_value = g_object_get_data(G_OBJECT(widget), "int-value");
	ToolbarItem *toolbar_item = (ToolbarItem *)data;
	
	toolbar_reply(toolbar_item, GPOINTER_TO_INT(int_value));
}

static void toolbar_delete_dup_menu_cb(GtkWidget *widget, gpointer data)
{
	gpointer int_value = g_object_get_data(G_OBJECT(widget), "int-value");
	ToolbarItem *toolbar_item = (ToolbarItem *)data;
	
	toolbar_delete_dup(toolbar_item, GPOINTER_TO_INT(int_value));
}
#endif

static void toolbar_learn_menu_cb(GtkWidget *widget, gpointer data)
{
	gpointer int_value = g_object_get_data(G_OBJECT(widget), "int-value");
	ToolbarItem *toolbar_item = (ToolbarItem *)data;

	toolbar_learn(toolbar_item, GPOINTER_TO_INT(int_value));
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
	ToolbarClawsActions *action_item;
	GSList *cur;
	GSList *toolbar_list;
	Toolbar *toolbar_data;
	GtkWidget *menu;
	toolbar_read_config_file(type);
	toolbar_list = toolbar_get_list(type);

	toolbar_data = g_new0(Toolbar, 1); 

	toolbar = gtk_toolbar_new();

	gtk_orientable_set_orientation(GTK_ORIENTABLE(toolbar), GTK_ORIENTATION_HORIZONTAL);

	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH);
	gtk_toolbar_set_show_arrow(GTK_TOOLBAR(toolbar), TRUE);
	gtk_widget_set_hexpand(toolbar, TRUE);
	
	for (cur = toolbar_list; cur != NULL; cur = cur->next) {

		if (g_ascii_strcasecmp(((ToolbarItem*)cur->data)->file, TOOLBAR_TAG_SEPARATOR) == 0) {
			gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);
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
		icon_wid = stock_pixmap_widget(stock_pixmap_get_icon(toolbar_item->file));
			
		switch (toolbar_item->index) {

		case A_GO_FOLDERS:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Go to folder list"));
			toolbar_data->folders_btn = item;
			break;
		case A_RECEIVE_ALL:
			TOOLBAR_MENUITEM(item,icon_wid,toolbar_item->text,
				_("Receive Mail from all Accounts"),
				_("Receive Mail from selected Account"));
			toolbar_data->getall_btn = item;
			break;
		case A_RECEIVE_CUR:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Receive Mail from current Account"));
			toolbar_data->get_btn = item;
			break;
		case A_SEND_QUEUED:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Send Queued Messages"));
			toolbar_data->send_btn = item; 
			break;
		case A_CLOSE:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Close window"));
			toolbar_data->close_window_btn = item; 
			break;
		case A_PREFERENCES:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Open preferences"));
			toolbar_data->preferences_btn = item; 
			break;
		case A_OPEN_MAIL:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Open email"));
			toolbar_data->open_mail_btn = item; 
			break;
		case A_COMPOSE_EMAIL:
#ifndef GENERIC_UMPC
			TOOLBAR_MENUITEM(item,icon_wid,toolbar_item->text,
				_("Write Email"),
				_("Write with selected Account"));
			toolbar_data->compose_mail_btn = item; 
			toolbar_data->compose_mail_icon = icon_wid; 
			g_object_ref_sink(toolbar_data->compose_mail_icon);

			icon_news = stock_pixmap_widget(STOCK_PIXMAP_NEWS_COMPOSE);
			toolbar_data->compose_news_icon = icon_news; 
			g_object_ref_sink(toolbar_data->compose_news_icon);
#else
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,
				_("Write Email"));
			toolbar_data->compose_mail_btn = item; 
			toolbar_data->compose_mail_icon = icon_wid; 

			icon_news = stock_pixmap_widget(STOCK_PIXMAP_NEWS_COMPOSE);
			toolbar_data->compose_news_icon = icon_news; 
#endif
			break;
		case A_LEARN_SPAM:
			TOOLBAR_MENUITEM(item,icon_wid,toolbar_item->text,
				_("Spam"),
				_("Learn as..."));
			toolbar_data->learn_spam_btn = item; 
			toolbar_data->learn_spam_icon = icon_wid; 
			g_object_ref_sink(toolbar_data->learn_spam_icon);

			icon_ham = stock_pixmap_widget(STOCK_PIXMAP_HAM_BTN);
			toolbar_data->learn_ham_icon = icon_ham; 
			g_object_ref_sink(toolbar_data->learn_ham_icon);

			menu = gtk_menu_new();
			ADD_MENU_ITEM(_("Learn as _Spam"), toolbar_learn_menu_cb, TRUE);
			ADD_MENU_ITEM(_("Learn as _Ham"), toolbar_learn_menu_cb, FALSE);
			gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(toolbar_data->learn_spam_btn), menu);
			break;
		case A_DELETE_DUP:
#ifndef GENERIC_UMPC
			TOOLBAR_MENUITEM(item,icon_wid,toolbar_item->text,
				_("Delete duplicates"),
				_("Delete duplicates options"));
			toolbar_data->delete_dup_btn = item;

			menu = gtk_menu_new();
			ADD_MENU_ITEM(_("Delete duplicates in selected folder"), toolbar_delete_dup_menu_cb, FALSE);
			ADD_MENU_ITEM(_("Delete duplicates in all folders"), toolbar_delete_dup_menu_cb, TRUE);
			gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(toolbar_data->delete_dup_btn), menu);
#else
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Delete duplicates"));
			toolbar_data->delete_dup_btn = item;
#endif
			break;
		case A_REPLY_MESSAGE:
#ifndef GENERIC_UMPC
			TOOLBAR_MENUITEM(item,icon_wid,toolbar_item->text,
				_("Reply to Message"),
				_("Reply to Message options"));
			toolbar_data->reply_btn = item;

			menu = gtk_menu_new();
			ADD_MENU_ITEM(_("_Reply with quote"), toolbar_reply_menu_cb, COMPOSE_REPLY_WITH_QUOTE);
			ADD_MENU_ITEM(_("Reply without _quote"), toolbar_reply_menu_cb, COMPOSE_REPLY_WITHOUT_QUOTE);
			gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(toolbar_data->reply_btn), menu);
#else
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,
				_("Reply to Message"));
			toolbar_data->reply_btn = item;
#endif
			break;
		case A_REPLY_SENDER:
#ifndef GENERIC_UMPC
			TOOLBAR_MENUITEM(item,icon_wid,toolbar_item->text,
				_("Reply to Sender"),
				_("Reply to Sender options"));
			toolbar_data->replysender_btn = item;

			menu = gtk_menu_new();
			ADD_MENU_ITEM(_("_Reply with quote"), toolbar_reply_menu_cb, COMPOSE_REPLY_TO_SENDER_WITH_QUOTE);
			ADD_MENU_ITEM(_("Reply without _quote"), toolbar_reply_menu_cb, COMPOSE_REPLY_TO_SENDER_WITHOUT_QUOTE);
			gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(toolbar_data->replysender_btn), menu);
#else
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,
				_("Reply to Sender"));
			toolbar_data->replysender_btn = item;
#endif
			break;
		case A_REPLY_ALL:
#ifndef GENERIC_UMPC
			TOOLBAR_MENUITEM(item,icon_wid,toolbar_item->text,
				_("Reply to All"),
				_("Reply to All options"));
			toolbar_data->replyall_btn = item;

			menu = gtk_menu_new();
			ADD_MENU_ITEM(_("_Reply with quote"), toolbar_reply_menu_cb, COMPOSE_REPLY_TO_ALL_WITH_QUOTE);
			ADD_MENU_ITEM(_("Reply without _quote"), toolbar_reply_menu_cb, COMPOSE_REPLY_TO_ALL_WITHOUT_QUOTE);
			gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(toolbar_data->replyall_btn), menu);
#else
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,
				_("Reply to All"));
			toolbar_data->replyall_btn = item;
#endif
			break;
		case A_REPLY_ML:
#ifndef GENERIC_UMPC
			TOOLBAR_MENUITEM(item,icon_wid,toolbar_item->text,
				_("Reply to Mailing-list"),
				_("Reply to Mailing-list options"));
			toolbar_data->replylist_btn = item;

			menu = gtk_menu_new();
			ADD_MENU_ITEM(_("_Reply with quote"), toolbar_reply_menu_cb, COMPOSE_REPLY_TO_LIST_WITH_QUOTE);
			ADD_MENU_ITEM(_("Reply without _quote"), toolbar_reply_menu_cb, COMPOSE_REPLY_TO_LIST_WITHOUT_QUOTE);
			gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(toolbar_data->replylist_btn), menu);
#else
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,
				_("Reply to Mailing-list"));
			toolbar_data->replylist_btn = item;
#endif
			break;
		case A_FORWARD:
#ifndef GENERIC_UMPC
			TOOLBAR_MENUITEM(item,icon_wid,toolbar_item->text,
				_("Forward Message"),
				_("Forward Message options"));
			toolbar_data->fwd_btn = item;

			menu = gtk_menu_new();
			ADD_MENU_ITEM(_("_Forward"), toolbar_reply_menu_cb, COMPOSE_FORWARD_INLINE);
			ADD_MENU_ITEM(_("For_ward as attachment"), toolbar_reply_menu_cb, COMPOSE_FORWARD_AS_ATTACH);
			ADD_MENU_ITEM(_("Redirec_t"), toolbar_reply_menu_cb, COMPOSE_REDIRECT);
			gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(toolbar_data->fwd_btn), menu);
#else
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,
				_("Forward Message"));
			toolbar_data->fwd_btn = item;
#endif
			break;
		case A_TRASH:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Trash Message"));
			toolbar_data->trash_btn = item;
			break;
		case A_DELETE_REAL:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Delete Message"));
			toolbar_data->delete_btn = item;
			break;
		case A_EXECUTE:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Execute"));
			toolbar_data->exec_btn = item;
			break;
		case A_GOTO_PREV:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Go to Previous Unread Message"));
			toolbar_data->prev_btn = item;
			break;
		case A_GOTO_NEXT:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Go to Next Unread Message"));
			toolbar_data->next_btn = item;
			break;
		
		/* Compose Toolbar */
		case A_SEND:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Send Message"));
			toolbar_data->send_btn = item;
			break;
		case A_SEND_LATER:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Put into queue folder and send later"));
			toolbar_data->sendl_btn = item;
			break;
		case A_DRAFT:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Save to draft folder"));
			toolbar_data->draft_btn = item; 
			break;
		case A_INSERT:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Insert file"));
			toolbar_data->insert_btn = item; 
			break;
		case A_ATTACH:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Attach file"));
			toolbar_data->attach_btn = item;
			break;
		case A_SIG:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Insert signature"));
			toolbar_data->sig_btn = item;
			break;
		case A_REP_SIG:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Replace signature"));
			toolbar_data->repsig_btn = item;
			break;
		case A_EXTEDITOR:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Edit with external editor"));
			toolbar_data->exteditor_btn = item;
			break;
		case A_LINEWRAP_CURRENT:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Wrap long lines of current paragraph"));
			toolbar_data->linewrap_current_btn = item;
			break;
		case A_LINEWRAP_ALL:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Wrap all long lines"));
			toolbar_data->linewrap_all_btn = item;
			break;
		case A_ADDRBOOK:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Address book"));
			toolbar_data->addrbook_btn = item;
			break;
#ifdef USE_ENCHANT
		case A_CHECK_SPELLING:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Check spelling"));
			toolbar_data->spellcheck_btn = item;
			break;
#endif
		case A_PRIVACY_SIGN:
			TOOLBAR_TOGGLE_ITEM(item,icon_wid,toolbar_item->text,_("Sign"));
			g_signal_connect (G_OBJECT(item), "toggled",
					G_CALLBACK(toolbar_privacy_sign_toggled_cb), NULL);
			/* Call the "toggled" handler to set correct tooltip. */
			toolbar_privacy_sign_toggled_cb(item, NULL);
			toolbar_data->privacy_sign_btn = item;
			break;
		case A_PRIVACY_ENCRYPT:
			TOOLBAR_TOGGLE_ITEM(item,icon_wid,toolbar_item->text,_("Encrypt"));
			g_signal_connect (G_OBJECT(item), "toggled",
					G_CALLBACK(toolbar_privacy_encrypt_toggled_cb), NULL);
			/* Call the "toggled" handler to set correct tooltip. */
			toolbar_privacy_encrypt_toggled_cb(item, NULL);
			toolbar_data->privacy_encrypt_btn = item;
			break;

		case A_CLAWS_ACTIONS:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,toolbar_item->text);
			action_item = g_new0(ToolbarClawsActions, 1);
			action_item->widget = item;
			action_item->name   = g_strdup(toolbar_item->text);

			toolbar_data->action_list = 
				g_slist_append(toolbar_data->action_list,
					       action_item);
			break;
		case A_CANCEL_INC:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Cancel receiving"));
			toolbar_data->cancel_inc_btn = item;
			break;
		case A_CANCEL_SEND:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Cancel sending"));
			toolbar_data->cancel_send_btn = item;
			break;
		case A_CANCEL_ALL:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,_("Cancel receiving/sending"));
			toolbar_data->cancel_all_btn = item;
			break;
		case A_CLAWS_PLUGINS:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text, toolbar_item->text);
			break;
		default:
			TOOLBAR_ITEM(item,icon_wid,toolbar_item->text,
				toolbar_ret_descr_from_val(toolbar_item->index));
			/* find and set the tool tip text */
			break;
		}

	}
	toolbar_data->toolbar = toolbar;

	gtk_widget_show_all(toolbar);

	if (type == TOOLBAR_MAIN) {
#ifdef GENERIC_UMPC
		MainWindow *mainwin = mainwindow_get_mainwindow();
		GtkWidget *progressbar = gtk_progress_bar_new();
		item = GTK_WIDGET(gtk_tool_item_new());
		gtk_container_add (GTK_CONTAINER (item), progressbar);
		gtk_widget_show(item);
		gtk_widget_set_size_request(progressbar, 84, -1);
		gtk_toolbar_insert(GTK_TOOLBAR(toolbar), GTK_TOOL_ITEM(item), -1);
		mainwin->progressbar = progressbar;
#endif
		activate_compose_button(toolbar_data, 
					prefs_common.toolbar_style, 
					toolbar_data->compose_btn_type);
	}
	if (type != TOOLBAR_COMPOSE)
		activate_learn_button(toolbar_data, prefs_common.toolbar_style,
				LEARN_SPAM);
	
	gtk_container_add(GTK_CONTAINER(container), toolbar);
	gtk_container_set_border_width(GTK_CONTAINER(container), 0);

	return toolbar_data; 
}

/**
 * Free toolbar structures
 */

#define UNREF_ICON(icon) if (toolbar->icon != NULL) \
			g_object_unref(toolbar->icon)

void toolbar_destroy(Toolbar * toolbar)
{
	UNREF_ICON(compose_mail_icon);
	UNREF_ICON(compose_news_icon);
	UNREF_ICON(learn_spam_icon);
	UNREF_ICON(learn_ham_icon);
	TOOLBAR_DESTROY_ITEMS(toolbar->item_list);
	TOOLBAR_DESTROY_ACTIONS(toolbar->action_list);
}

#undef UNREF_ICON

void toolbar_item_destroy(ToolbarItem *item)
{
	cm_return_if_fail(item != NULL);

	if (item->file)
		g_free(item->file);
	if (item->text)
		g_free(item->text);
	g_free(item);
}

void toolbar_update(ToolbarType type, gpointer data)
{
	Toolbar *toolbar_data;
	GtkWidget *handlebox;
	MainWindow *mainwin = (MainWindow*)data;
	Compose    *compose = (Compose*)data;
	MessageView *msgview = (MessageView*)data;

#ifndef GENERIC_UMPC
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
#else
	switch(type) {
	case TOOLBAR_MAIN:
		toolbar_data = mainwin->toolbar;
		handlebox    = mainwin->window;
		break;
	case TOOLBAR_COMPOSE:
		toolbar_data = compose->toolbar;
		handlebox    = compose->window;
		break;
	case TOOLBAR_MSGVIEW:
		toolbar_data = msgview->toolbar;
		handlebox    = msgview->window;
		break;
	default:
		return;
	}
	toolbar_init(toolbar_data);
 	toolbar_data = toolbar_create(type, handlebox, data);
#endif

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
	gtk_widget_set_sensitive(widget, sensitive);		\
}

void toolbar_main_set_sensitive(gpointer data)
{
	SensitiveCondMask state;
	gboolean sensitive;
	MainWindow *mainwin = (MainWindow*)data;
	Toolbar *toolbar = mainwin->toolbar;
	GSList *cur;
	GSList *entry_list = NULL;
	
	typedef struct _Entry Entry;
	struct _Entry {
		GtkWidget *widget;
		SensitiveCondMask cond;
		gboolean empty;
	};

#define SET_WIDGET_COND(w, ...)     \
do { \
	Entry *e = g_new0(Entry, 1); \
	e->widget = w; \
	e->cond = main_window_get_mask(__VA_ARGS__, -1); \
	entry_list = g_slist_append(entry_list, e); \
} while (0)
	
	/* match all bit flags */

	if (toolbar->get_btn)
		SET_WIDGET_COND(toolbar->get_btn, 
			M_HAVE_ACCOUNT, M_UNLOCKED, M_HAVE_RETRIEVABLE_ACCOUNT);

	if (toolbar->getall_btn) {
		SET_WIDGET_COND(toolbar->getall_btn, 
			M_HAVE_ACCOUNT, M_UNLOCKED, M_HAVE_ANY_RETRIEVABLE_ACCOUNT);
	}
	if (toolbar->send_btn) {
		SET_WIDGET_COND(toolbar->send_btn,
			M_HAVE_QUEUED_MAILS);
	}
	if (toolbar->compose_mail_btn) {
		SET_WIDGET_COND(toolbar->compose_mail_btn, 
			M_HAVE_ACCOUNT);
	}
	if (toolbar->close_window_btn) {
		SET_WIDGET_COND(toolbar->close_window_btn, 
			M_UNLOCKED);
	}
	if (toolbar->open_mail_btn) {
		SET_WIDGET_COND(toolbar->open_mail_btn, 
			M_TARGET_EXIST, M_SUMMARY_ISLIST);
	}
	if (toolbar->reply_btn) {
		SET_WIDGET_COND(toolbar->reply_btn,
			M_HAVE_ACCOUNT, M_TARGET_EXIST, M_SUMMARY_ISLIST);
	}
	if (toolbar->replyall_btn) {
		SET_WIDGET_COND(toolbar->replyall_btn,
			M_HAVE_ACCOUNT, M_TARGET_EXIST, M_SUMMARY_ISLIST);
	}
	if (toolbar->replylist_btn) {
		SET_WIDGET_COND(toolbar->replylist_btn,
			M_HAVE_ACCOUNT, M_TARGET_EXIST, M_SUMMARY_ISLIST);
	}
	if (toolbar->replysender_btn) {
		SET_WIDGET_COND(toolbar->replysender_btn,
			M_HAVE_ACCOUNT, M_TARGET_EXIST, M_SUMMARY_ISLIST);
	}
	if (toolbar->fwd_btn) {
		SET_WIDGET_COND(toolbar->fwd_btn, 
			M_HAVE_ACCOUNT, M_TARGET_EXIST, M_SUMMARY_ISLIST);
	}

	if (prefs_common.next_unread_msg_dialog == NEXTUNREADMSGDIALOG_ASSUME_NO) {
		SET_WIDGET_COND(toolbar->next_btn, M_MSG_EXIST, M_SUMMARY_ISLIST);
	} else {
		SET_WIDGET_COND(toolbar->next_btn, -1);
	}

	if (toolbar->trash_btn)
		SET_WIDGET_COND(toolbar->trash_btn,
			M_TARGET_EXIST, M_ALLOW_DELETE, M_NOT_NEWS);

	if (toolbar->delete_btn)
		SET_WIDGET_COND(toolbar->delete_btn,
			M_TARGET_EXIST, M_ALLOW_DELETE);

	if (toolbar->delete_dup_btn)
		SET_WIDGET_COND(toolbar->delete_dup_btn,
			M_MSG_EXIST, M_ALLOW_DELETE, M_SUMMARY_ISLIST);

	if (toolbar->exec_btn)
		SET_WIDGET_COND(toolbar->exec_btn, 
			M_DELAY_EXEC);
	
	if (toolbar->learn_spam_btn)
		SET_WIDGET_COND(toolbar->learn_spam_btn, 
			M_TARGET_EXIST, M_CAN_LEARN_SPAM, M_SUMMARY_ISLIST);

	if (toolbar->cancel_inc_btn)
		SET_WIDGET_COND(toolbar->cancel_inc_btn,
				M_INC_ACTIVE);

	if (toolbar->cancel_send_btn)
		SET_WIDGET_COND(toolbar->cancel_send_btn,
				M_SEND_ACTIVE);

	for (cur = toolbar->action_list; cur != NULL;  cur = cur->next) {
		ToolbarClawsActions *act = (ToolbarClawsActions*)cur->data;

		if (prefs_common.mainwin_toolbar_always_enable_actions)
			SET_WIDGET_COND(act->widget, M_UNLOCKED);
		else
			SET_WIDGET_COND(act->widget, M_TARGET_EXIST, M_UNLOCKED);
	}

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

		entry_list = g_slist_remove(entry_list, e);
		g_free(e);
	}

	/* match any bit flags */

	if (toolbar->cancel_all_btn)
		SET_WIDGET_COND(toolbar->cancel_all_btn,
				M_INC_ACTIVE, M_SEND_ACTIVE);

	for (cur = entry_list; cur != NULL; cur = cur->next) {
		Entry *e = (Entry*) cur->data;

		if (e->widget != NULL) {
			sensitive = ((e->cond & state) != 0);
			GTK_BUTTON_SET_SENSITIVE(e->widget, sensitive);	
		}
	}

	while (entry_list != NULL) {
		Entry *e = (Entry*) entry_list->data;

		entry_list = g_slist_remove(entry_list, e);
		g_free(e);
	}

	g_slist_free(entry_list);

	activate_compose_button(toolbar, 
				prefs_common.toolbar_style,
				toolbar->compose_btn_type);
	
#undef SET_WIDGET_COND
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
	if (compose->toolbar->repsig_btn)
		GTK_BUTTON_SET_SENSITIVE(compose->toolbar->repsig_btn, sensitive);
	if (compose->toolbar->exteditor_btn)
		GTK_BUTTON_SET_SENSITIVE(compose->toolbar->exteditor_btn, sensitive);
	if (compose->toolbar->linewrap_current_btn)
		GTK_BUTTON_SET_SENSITIVE(compose->toolbar->linewrap_current_btn, sensitive);
	if (compose->toolbar->linewrap_all_btn)
		GTK_BUTTON_SET_SENSITIVE(compose->toolbar->linewrap_all_btn, sensitive);
	if (compose->toolbar->addrbook_btn)
		GTK_BUTTON_SET_SENSITIVE(compose->toolbar->addrbook_btn, sensitive);
#ifdef USE_ENCHANT
	if (compose->toolbar->spellcheck_btn)
		GTK_BUTTON_SET_SENSITIVE(compose->toolbar->spellcheck_btn, sensitive);
#endif
	for (; items != NULL; items = g_slist_next(items)) {
		ToolbarClawsActions *item = (ToolbarClawsActions *)items->data;
		GTK_BUTTON_SET_SENSITIVE(item->widget, sensitive);
	}
}

/**
 * Initialize toolbar structure
 **/
static void toolbar_init(Toolbar * toolbar)
{

	toolbar->toolbar           = NULL;
	toolbar->folders_btn       = NULL;
	toolbar->get_btn           = NULL;
	toolbar->getall_btn        = NULL;
	toolbar->send_btn          = NULL;
	toolbar->compose_mail_btn  = NULL;
	toolbar->compose_mail_icon = NULL;
	toolbar->compose_news_icon = NULL;
	toolbar->reply_btn         = NULL;
	toolbar->replysender_btn   = NULL;
	toolbar->replyall_btn      = NULL;
	toolbar->replylist_btn     = NULL;
	toolbar->fwd_btn           = NULL;
	toolbar->trash_btn         = NULL;
	toolbar->delete_btn        = NULL;
	toolbar->delete_dup_btn    = NULL;
	toolbar->prev_btn          = NULL;
	toolbar->next_btn          = NULL;
	toolbar->exec_btn          = NULL;
	toolbar->separator         = NULL;
	toolbar->learn_spam_btn    = NULL;
	toolbar->learn_spam_icon   = NULL;
	toolbar->learn_ham_icon    = NULL;
	toolbar->cancel_inc_btn    = NULL;
	toolbar->cancel_send_btn   = NULL;
	toolbar->cancel_all_btn    = NULL;

	/* compose buttons */ 
	toolbar->sendl_btn         = NULL;
	toolbar->draft_btn         = NULL;
	toolbar->insert_btn        = NULL;
	toolbar->attach_btn        = NULL;
	toolbar->sig_btn           = NULL; 
	toolbar->repsig_btn        = NULL; 
	toolbar->exteditor_btn     = NULL; 
	toolbar->linewrap_current_btn = NULL;	
	toolbar->linewrap_all_btn  = NULL;	
	toolbar->addrbook_btn      = NULL; 

	toolbar->open_mail_btn     = NULL;
	toolbar->close_window_btn  = NULL;
	toolbar->preferences_btn   = NULL;
	toolbar->action_list       = NULL;
	toolbar->item_list         = NULL;
#ifdef USE_ENCHANT
	toolbar->spellcheck_btn    = NULL;
#endif

	toolbar->privacy_sign_btn  = NULL;
	toolbar->privacy_encrypt_btn = NULL;

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
	gboolean msg_is_selected = FALSE;
	gboolean msg_is_opened = FALSE;

	cm_return_if_fail(toolbar_item != NULL);

	switch (toolbar_item->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)toolbar_item->parent;
		msginfo_list = summary_get_selection(mainwin->summaryview);
		msgview = (MessageView*)mainwin->messageview;
		msg_is_opened = summary_has_opened_message(mainwin->summaryview);
		msg_is_selected = summary_is_opened_message_selected(mainwin->summaryview);
		break;
	case TOOLBAR_MSGVIEW:
		msgview = (MessageView*)toolbar_item->parent;
		cm_return_if_fail(msgview != NULL);	
		msginfo_list = g_slist_append(msginfo_list, msgview->msginfo);
		msg_is_opened = TRUE;
		msg_is_selected = TRUE;
		break;
	default:
		return;
	}

	cm_return_if_fail(msgview != NULL);
	cm_return_if_fail(msginfo_list != NULL);
	if (!msg_is_opened) {
		compose_reply_from_messageview(NULL, msginfo_list, action);
	} else if (msg_is_selected) {
		compose_reply_from_messageview(msgview, msginfo_list, action);
	} else {
		compose_reply_from_messageview(msgview, NULL, action);
	}
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

	inc_all_account_mail(mainwin, FALSE, FALSE, prefs_common.newmail_notify_manu);
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
			       NULL, _("_No"), NULL, _("_Yes"),
			       NULL, NULL, ALERTFOCUS_FIRST) != G_ALERTALTERNATE)
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
			    	   NULL, _("_Cancel"), NULL, _("_Send"),
				   NULL, NULL, ALERTFOCUS_FIRST) != G_ALERTALTERNATE)
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
			alertpanel_error_log(_("Some errors occurred "
					"while sending queued messages:\n%s"), errstr);
			g_free(errstr);
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
		if (ac && ac->protocol != A_NNTP && ac->protocol != A_IMAP4) {
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
