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
#ifdef WIN32
#include <w32lib.h>
#else
#include <dirent.h>
#endif
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
#include "messageview.h"
#include "prefs_common.h"
#include "menu.h"
#include "prefs_actions.h"
#include "manage_window.h"
#include "gtkutils.h"
#include "toolbar.h"
#include "prefs_toolbar.h"

/* elements */
#define TOOLBAR_TAG_INDEX        "toolbar"
#define TOOLBAR_TAG_ITEM         "item"
#define TOOLBAR_TAG_SEPARATOR    SEPARATOR

#define TOOLBAR_ICON_FILE   "file"    
#define TOOLBAR_ICON_TEXT   "text"     
#define TOOLBAR_ICON_ACTION "action"    

gboolean      toolbar_is_duplicate           (gint            	action,
					      ToolbarType	source);
static void   toolbar_parse_item             (XMLFile        	*file,
					      ToolbarType	source);

static gint   toolbar_ret_val_from_text      (const gchar	*text);
static gchar *toolbar_ret_text_from_val      (gint            	val);

static void   toolbar_set_default_main       (void);
static void   toolbar_set_default_compose    (void);
static void   toolbar_set_default_msgview    (void);

static ToolbarType detect_window(gpointer data);

struct ToolbarText 
{
	gchar *index_str;
	const gchar *descr;
};
/* 
 *  Text Database linked to enumarated values in toolbar.h 
 */
static struct ToolbarText toolbar_text [] = {
	
	{ "A_RECEIVE_ALL",   N_("Receive Mail on all Accounts")         },
	{ "A_RECEIVE_CUR",   N_("Receive Mail on current Account")      },
	{ "A_SEND_QUEUED",   N_("Send Queued Message(s)")               },
	{ "A_COMPOSE_EMAIL", N_("Compose Email")                        },
	{ "A_COMPOSE_NEWS",  N_("Compose News")                         },
	{ "A_REPLY_MESSAGE", N_("Reply to Message")                     },
	{ "A_REPLY_SENDER",  N_("Reply to Sender")                      },
	{ "A_REPLY_ALL",     N_("Reply to All")                         },
	{ "A_REPLY_ML",      N_("Reply to Mailing-list")                },
	{ "A_FORWARD",       N_("Forward Message")                      }, 
	{ "A_DELETE",        N_("Delete Message")                       },
	{ "A_EXECUTE",       N_("Execute")                              },
	{ "A_GOTO_NEXT",     N_("Goto Next Message")                    },

	{ "A_SEND",          N_("Send Message")                         },
	{ "A_SENDL",         N_("Put into queue folder and send later") },
	{ "A_DRAFT",         N_("Save to draft folder")                 },
	{ "A_INSERT",        N_("Insert file")                          },   
	{ "A_ATTACH",        N_("Attach file")                          },
	{ "A_SIG",           N_("Insert signature")                     },
	{ "A_EXTEDITOR",     N_("Edit with external editor")            },
	{ "A_LINEWRAP",      N_("Wrap all long lines")                  }, 
	{ "A_ADDRBOOK",      N_("Address book")                         },

	{ "A_SYL_ACTIONS",   N_("Sylpheed Actions Feature")             }, 
	{ "A_SEPARATOR",     ("")                                       }
};

/* struct holds configuration files and a list of
 * currently active toolbar items 
 * TOOLBAR_MAIN, TOOLBAR_COMPOSE and TOOLBAR_MSGVIEW
 * give us an index
 */
static ToolbarConfig toolbar_config[3] = {
	{ "toolbar_main.xml",    NULL},
	{ "toolbar_compose.xml", NULL}, 
  	{ "toolbar_msgview.xml", NULL}
};

gint toolbar_ret_val_from_descr(const gchar *descr)
{
	gint i;

	for (i = 0; i < N_ACTION_VAL; i++) {
		if (g_strcasecmp(gettext(toolbar_text[i].descr), descr) == 0)
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
		if (g_strcasecmp(toolbar_text[i].index_str, text) == 0)
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

GList *toolbar_get_action_items(ToolbarType source)
{
	GList *items = NULL;
	gint i = 0;
	
	if (source == TOOLBAR_MAIN) {
		gint main_items[13] = { A_RECEIVE_ALL,   A_RECEIVE_CUR,   A_SEND_QUEUED,
					A_COMPOSE_EMAIL, A_REPLY_MESSAGE, A_REPLY_SENDER,  
					A_REPLY_ALL,     A_REPLY_ML,      A_FORWARD,       
					A_DELETE,        A_EXECUTE,       A_GOTO_NEXT,      
					A_SYL_ACTIONS };

		for (i = 0; i < sizeof(main_items)/sizeof(main_items[0]); i++) 
			items = g_list_append(items, gettext(toolbar_text[main_items[i]].descr));
	}
	else if (source == TOOLBAR_COMPOSE) {
		gint comp_items[10] = {	A_SEND,          A_SENDL,        A_DRAFT,
					A_INSERT,        A_ATTACH,       A_SIG,
					A_EXTEDITOR,     A_LINEWRAP,     A_ADDRBOOK,
					A_SYL_ACTIONS };	

		for (i = 0; i < sizeof(comp_items)/sizeof(comp_items[0]); i++) 
			items = g_list_append(items, gettext(toolbar_text[comp_items[i]].descr));
	}
	else if (source == TOOLBAR_MSGVIEW) {
		gint msgv_items[8/*9*/] = { A_COMPOSE_EMAIL, A_REPLY_MESSAGE, A_REPLY_SENDER,
					    A_REPLY_ALL,     A_REPLY_ML,      A_FORWARD,
					    A_DELETE,	     A_GOTO_NEXT/*   A_SYL_ACTIONS*/ };	

		for (i = 0; i < sizeof(msgv_items)/sizeof(msgv_items[0]); i++) 
			items = g_list_append(items, gettext(toolbar_text[msgv_items[i]].descr));
	}

	return items;
}

static void toolbar_parse_item(XMLFile *file, ToolbarType source)
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
#ifdef WIN32
			item->text = g_locale_to_utf8(value, -1, NULL, NULL, NULL);
#else
			item->text = g_strdup (value);
#endif
		else if (g_strcasecmp(name, TOOLBAR_ICON_ACTION) == 0)
			item->index = toolbar_ret_val_from_text(value);

		attr = g_list_next(attr);
	}
	if (item->index != -1) {
		
		if (!toolbar_is_duplicate(item->index, source)) 
			toolbar_config[source].item_list = g_slist_append(toolbar_config[source].item_list,
									 item);
	}
}

static void toolbar_set_default_main(void) 
{
	struct {
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

			toolbar_item->file   = g_strdup(SEPARATOR);
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
		{ A_SEND,      STOCK_PIXMAP_MAIL_SEND,         _("Send")       },
		{ A_SENDL,     STOCK_PIXMAP_MAIL_SEND_QUEUE,   _("Send later") },
		{ A_DRAFT,     STOCK_PIXMAP_MAIL,              _("Draft")      },
		{ A_SEPARATOR, 0,                               ("")           }, 
		{ A_INSERT,    STOCK_PIXMAP_INSERT_FILE,       _("Insert")     },
		{ A_ATTACH,    STOCK_PIXMAP_MAIL_ATTACH,       _("Attach")     },
		{ A_SIG,       STOCK_PIXMAP_MAIL_SIGN,         _("Signature")  },
		{ A_SEPARATOR, 0,                               ("")           },
		{ A_EXTEDITOR, STOCK_PIXMAP_EDIT_EXTERN,       _("Editor")     },
		{ A_LINEWRAP,  STOCK_PIXMAP_LINEWRAP,          _("Linewrap")   },
		{ A_SEPARATOR, 0,                               ("")           },
		{ A_ADDRBOOK,  STOCK_PIXMAP_ADDRESS_BOOK,      _("Address")    }
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

			toolbar_item->file   = g_strdup(SEPARATOR);
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
		/*{ A_COMPOSE_EMAIL, STOCK_PIXMAP_MAIL_COMPOSE,         _("Email")   },
		  { A_SEPARATOR,     0,                                 ("")         },*/
		{ A_REPLY_MESSAGE, STOCK_PIXMAP_MAIL_REPLY,           _("Reply")   }, 
		{ A_REPLY_ALL,     STOCK_PIXMAP_MAIL_REPLY_TO_ALL,    _("All")     },
		{ A_REPLY_SENDER,  STOCK_PIXMAP_MAIL_REPLY_TO_AUTHOR, _("Sender")  },
		{ A_FORWARD,       STOCK_PIXMAP_MAIL_FORWARD,         _("Forward") },
		{ A_SEPARATOR,     0,                                 ("")         },
		{ A_DELETE,        STOCK_PIXMAP_CLOSE,                _("Delete")  },
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

			toolbar_item->file   = g_strdup(SEPARATOR);
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
		fprintf(fp, "<?xml version=\"1.0\" encoding=\"%s\" ?>\n",
			conv_get_current_charset_str());

		fprintf(fp, "<%s>\n", TOOLBAR_TAG_INDEX);

		for (cur = toolbar_config[source].item_list; cur != NULL; cur = cur->next) {
			ToolbarItem *toolbar_item = (ToolbarItem*) cur->data;
			
			if (g_strcasecmp(toolbar_item->file, SEPARATOR) != 0)
#ifdef WIN32
			{
			  	gchar *p_text=g_locale_from_utf8(toolbar_item->text,
					-1,NULL,NULL,NULL);
#endif
				fprintf(fp, "\t<%s %s=\"%s\" %s=\"%s\" %s=\"%s\"/>\n",
					TOOLBAR_TAG_ITEM, 
					TOOLBAR_ICON_FILE, toolbar_item->file,
#ifdef WIN32
					TOOLBAR_ICON_TEXT, p_text,
#else
					TOOLBAR_ICON_TEXT, toolbar_item->text,
#endif
					TOOLBAR_ICON_ACTION, 
					toolbar_ret_text_from_val(toolbar_item->index));
#ifdef WIN32
				g_free(p_text);
			}
#endif
			else 
				fprintf(fp, "\t<%s/>\n", TOOLBAR_TAG_SEPARATOR); 
		}

		fprintf(fp, "</%s>\n", TOOLBAR_TAG_INDEX);	
	
		if (prefs_write_close (pfile) < 0 ) 
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
			
				item->file   = g_strdup(SEPARATOR);
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

		if (item->file)
			g_free(item->file);
		if (item->text)
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
		actions_execute(data, i, widget, source);
	else
		g_warning ("Error: did not find Sylpheed Action to execute");
}

/*
 * Change the style of toolbar
 */
void common_toolbar_set_style(gpointer data, ToolbarType type)
{
	GtkWidget  *handlebox_wid;
	GtkWidget  *toolbar_wid;
	MainWindow *mainwin;
	Compose    *compose;
	MessageView *msgview;
	
	g_return_if_fail(data != NULL);
	
	switch (type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)data;
		handlebox_wid = mainwin->handlebox;
		toolbar_wid = mainwin->toolbar->toolbar;
		break;
	case TOOLBAR_COMPOSE:
		compose = (Compose*)data;
		handlebox_wid = compose->handlebox;
		toolbar_wid = compose->toolbar->toolbar;
		break;
	case TOOLBAR_MSGVIEW: 
		msgview = (MessageView*)data;
		handlebox_wid = msgview->handlebox;
		toolbar_wid = msgview->toolbar->toolbar;
		break;
	default:
		debug_print("toolbar_set_style: not supported for this type of window\n");
		return;
	}
	
	switch (prefs_common.toolbar_style) {
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
	}
	
	if (prefs_common.toolbar_style != TOOLBAR_NONE) {
		gtk_widget_show(handlebox_wid);
		gtk_widget_queue_resize(handlebox_wid);
	}
}

/*
 * Delete current/selected(s) message(s)
 */
void common_toolbar_delete_cb(GtkWidget	  *widget,
			      gpointer	   data)
{
	ToolbarParent *parent = (ToolbarParent*)data;
	MainWindow *mainwin;
	MessageView *msgview;

	g_return_if_fail(parent != NULL);
	
	switch (parent->type) {
	case TOOLBAR_MSGVIEW:
		msgview = (MessageView*)parent->data;
		
		/* make sure the selected msg in summaryview is 
		   the one we are asked to delete
		*/
		if (msgview->mainwin->summaryview->selected) {
			SummaryView *summaryview = msgview->mainwin->summaryview;
			GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
			MsgInfo *msginfo = gtk_ctree_node_get_row_data(ctree, 
								       summaryview->selected);	
			if (msginfo->msgnum != msgview->msginfo->msgnum) {
				alertpanel_error(_("Message already removed from folder."));
				return;
			}
		}
		
		summary_delete(msgview->mainwin->summaryview);	

		if (msgview->mainwin->summaryview->selected) {
			SummaryView *summaryview = msgview->mainwin->summaryview;
			GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
			MsgInfo *msginfo = gtk_ctree_node_get_row_data(ctree, 
								       summaryview->selected);
			messageview_show(msgview, msginfo, 
					 msgview->all_headers);
		} else {
			toolbar_clear_list(TOOLBAR_MSGVIEW);
			TOOLBAR_DESTROY_ITEMS(msgview->toolbar->item_list);	
			TOOLBAR_DESTROY_ACTIONS(msgview->toolbar->action_list);
			gtk_widget_destroy(msgview->window);
		}
        	break;
        case TOOLBAR_MAIN:
		mainwin = (MainWindow*)parent->data;
        	summary_delete(mainwin->summaryview);
        	break;
        default: 
        	debug_print("toolbar_delete: Not supported for this type of window\n");
        	break;
	}
}


/*
 * Compose new message
 */
void common_toolbar_compose_cb(GtkWidget  *widget,
			       gpointer	   data)
{
	ToolbarParent *parent = (ToolbarParent*)data;
	MessageView *msgview;

	g_return_if_fail(parent != NULL);

	switch (parent->type) {
	case TOOLBAR_MSGVIEW:
		msgview = (MessageView*)parent->data;
		compose_new_with_folderitem(NULL, 
					    msgview->msginfo->folder);
		break;	
	default:
		debug_print("toolbar_compose: Not supported for this type of window\n");
	}
}


/*
 * Reply Message
 */
void common_toolbar_reply_cb(GtkWidget   *widget, 
			     gpointer     data)
{
	ToolbarParent *parent = (ToolbarParent*)data;
	MessageView *msgview;

	g_return_if_fail(parent != NULL);

	switch (parent->type) {
	case TOOLBAR_MSGVIEW:
		msgview = (MessageView*)parent->data;
		compose_reply(msgview->msginfo,
			      prefs_common.reply_with_quote ? COMPOSE_REPLY_WITH_QUOTE 
			      : COMPOSE_REPLY_WITHOUT_QUOTE,
			      FALSE, FALSE, FALSE, NULL);
		break;
	default:
		debug_print("toolbar_reply: Not supported for this type of window\n");
	}
}


/*
 * Reply message to Sender and All recipients
 */
void common_toolbar_reply_to_all_cb(GtkWidget   *widget, 
				    gpointer     data)
{
	ToolbarParent *parent = (ToolbarParent*)data;
	MessageView *msgview;

	g_return_if_fail(parent != NULL);

	switch (parent->type) {
	case TOOLBAR_MSGVIEW:
		msgview = (MessageView*)parent->data;
		compose_reply(msgview->msginfo,
			      prefs_common.reply_with_quote ? COMPOSE_REPLY_TO_ALL_WITH_QUOTE 
			      : COMPOSE_REPLY_TO_ALL_WITHOUT_QUOTE,
			      TRUE, FALSE, FALSE, NULL);
		break;
	default:
		debug_print("toolbar_reply_to_all: Not supported for this type of window\n");
	}
}


/*
 * Reply to Mailing List
 */
void common_toolbar_reply_to_list_cb(GtkWidget   *widget, 
				     gpointer     data)
{
	ToolbarParent *parent = (ToolbarParent*)data;
	MessageView *msgview;

	g_return_if_fail(parent != NULL);

	switch (parent->type) {
	case TOOLBAR_MSGVIEW:
		msgview = (MessageView*)parent->data;
		compose_reply(msgview->msginfo,
			      prefs_common.reply_with_quote ? COMPOSE_REPLY_TO_LIST_WITH_QUOTE 
			      : COMPOSE_REPLY_TO_LIST_WITHOUT_QUOTE,
			      FALSE, TRUE, FALSE, NULL);
		break;
	default:
		debug_print("toolbar_reply_to_list: Not supported for this type of window\n");
	}
}


/*
 * Reply to sender of message
 */ 
void common_toolbar_reply_to_sender_cb(GtkWidget   *widget, 
				       gpointer     data)
{
	ToolbarParent *parent = (ToolbarParent*)data;
	MessageView *msgview;

	g_return_if_fail(parent != NULL);

	switch (parent->type) {
	case TOOLBAR_MSGVIEW:
		msgview = (MessageView*)parent->data;
		compose_reply(msgview->msginfo,
			      prefs_common.reply_with_quote ? COMPOSE_REPLY_TO_SENDER_WITH_QUOTE 
			      : COMPOSE_REPLY_TO_SENDER_WITHOUT_QUOTE,
			      FALSE, FALSE, FALSE, NULL);
		break;
	default:
		debug_print("toolbar_reply_to_sender: Not supported for this type of window\n");
	}
}


/*
 * Forward current/selected(s) message(s)
 */
void common_toolbar_forward_cb(GtkWidget	*widget,
			       gpointer	         data)
{
	ToolbarParent *parent = (ToolbarParent*)data;
	MessageView *msgview;

	g_return_if_fail(parent != NULL);

	switch (parent->type) {
/* 
  REFERENCE FROM mainwindow.c
  
	case TOOLBAR_MAIN:
		if (prefs_common.forward_as_attachment)
			reply_cb(mainwin, COMPOSE_FORWARD_AS_ATTACH, NULL);
		else
			reply_cb(mainwin, COMPOSE_FORWARD, NULL);
		break;
*/		
	default:
		debug_print("toolbar_forward: Not supported for this type of window\n");
	}
}


/*
 * Goto Next Unread Message
 */
void common_toolbar_next_unread_cb(GtkWidget	*widget,
				   gpointer	 data)
{
	ToolbarParent *parent = (ToolbarParent*)data;
	MainWindow *mainwin;
	MessageView *msgview;

	g_return_if_fail(parent != NULL);

	switch (parent->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)parent->data;
		summary_select_next_unread(mainwin->summaryview);
		break;
		
	case TOOLBAR_MSGVIEW:
		msgview = (MessageView*)parent->data;
/*
 * TODO: Check if summaryview stay in the same place when this message view was created 
 * if summary have other message select the next will be based on message selected in summaryview
 */
		summary_select_next_unread(msgview->mainwin->summaryview);
		
		/** Now we need to update the messageview window */
		if (msgview->mainwin->summaryview->selected) {
			GtkCTree *ctree = GTK_CTREE(msgview->mainwin->summaryview->ctree);
			
			MsgInfo * msginfo = gtk_ctree_node_get_row_data(ctree, 
									msgview->mainwin->summaryview->selected);
		       
			messageview_show(msgview, msginfo, 
					 msgview->all_headers);
		} else {
			gtk_widget_destroy(msgview->window);
		}
		break;
	default:
		debug_print("toolbar_next_unread: Not supported for this type of window\n");
	}
}


/*
 * Execute actions from toolbar
 */
void common_toolbar_actions_execute_cb(GtkWidget *widget,
				       gpointer   data)
{
	ToolbarParent *parent = (ToolbarParent*)data;
	GSList *action_list;
	MainWindow *mainwin;
	Compose *compose;

	g_return_if_fail(parent != NULL);

	switch (parent->type) {
	case TOOLBAR_MAIN:
		mainwin = (MainWindow*)parent->data;
		action_list = mainwin->toolbar->action_list;
		break;
	case TOOLBAR_COMPOSE:
		compose = (Compose*)parent->data;
		action_list = compose->toolbar->action_list;
		break;
		/* case TOOLBAR_MSGVIEW: not supported yet */
	default:
		debug_print("toolbar_actions: not supported for this window type\n");
		return;
	}

	toolbar_action_execute(widget, action_list, parent->data, parent->type);
}
