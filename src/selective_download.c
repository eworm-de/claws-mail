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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <gtk/gtkoptionmenu.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <stdlib.h>

#include "intl.h"
#include "main.h"
#include "prefs.h"
#include "prefs_matcher.h"
#include "prefs_filtering.h"
#include "prefs_account.h"
#include "account.h"
#include "mainwindow.h"
#include "manage_window.h"
#include "menu.h"
#include "stock_pixmap.h"
#include "inc.h"
#include "utils.h"
#include "gtkutils.h"
#include "alertpanel.h"
#include "folder.h"
#include "inc.h"
#include "filtering.h"
#include "matcher_parser.h"
#include "selective_download.h"
#include "procheader.h"

static struct _SDView {
	MainWindow *mainwin;

	GtkWidget *window;
	GtkWidget *clist;
	GtkWidget *preview_btn;
	GtkWidget *remove_btn;
	GtkWidget *download_btn;
	GtkWidget *ac_button;
	GtkWidget *ac_label;
	GtkWidget *ac_menu;
	GtkWidget *preview_popup;
	GtkWidget *msgs_label;
	GtkWidget *show_old_chkbtn;

}selective;

static GdkPixmap *checkboxonxpm;
static GdkBitmap *checkboxonxpmmask;
static GdkPixmap *checkboxoffxpm;
static GdkBitmap *checkboxoffxpmmask;
static GdkPixmap *markxpm;
static GdkBitmap *markxpmmask;
static GdkPixmap *deletedxpm;
static GdkBitmap *deletedxpmmask;
static GdkPixmap *continuexpm;
static GdkBitmap *continuexpmmask;


/* local functions */
static void sd_clear_msglist();
static void sd_remove_header_files();
static void sd_toggle_btn();
static SD_State sd_header_filter (MsgInfo *msginfo);
static MsgInfo *sd_get_msginfo_from_file (const gchar *filename);

static void sd_clist_set_pixmap(HeaderItems *items, gint row);
static void sd_clist_get_items();
static void sd_clist_set_items();
static void sd_update_msg_num(PrefsAccount *acc);

enum {
	PREVIEW_NEW,
	PREVIEW_ALL,
	REMOVE,
	DOWNLOAD,
	DONE,
	CHECKBTN,
};

/* callbacks */
static void sd_action_cb(GtkWidget *widget, guint action);

static void sd_select_row_cb (GtkCList *clist, gint row, gint column,
			      GdkEvent *event, gpointer user_data);
static void sd_key_pressed (GtkWidget *widget,
			    GdkEventKey *event,
			    gpointer data);
/* account menu */
static void sd_ac_label_pressed(GtkWidget *widget, 
				GdkEventButton *event,
				gpointer data);

static void sd_ac_menu_popup_closed(GtkMenuShell *menu_shell);
static void sd_ac_menu_cb(GtkMenuItem *menuitem, gpointer data);
static void sd_ac_menu_set();

/* preview popup */
static void sd_preview_popup_closed(GtkMenuShell *menu_shell);
static void sd_preview_popup_cb(GtkWidget *widget, GdkEventButton *event);
static GtkItemFactoryEntry preview_popup_entries[] =
{
	{N_("/Preview _new Messages"), NULL, sd_action_cb, PREVIEW_NEW, NULL},
	{N_("/Preview _all Messages"), NULL, sd_action_cb, PREVIEW_ALL, NULL}
};

/* create dialog */
static void sd_window_create (MainWindow *mainwin);

void selective_download(MainWindow *mainwin)
{
	summary_write_cache(mainwin->summaryview);

	sd_remove_header_files();
	
	stock_pixmap_gdk(mainwin->window, STOCK_PIXMAP_CHECKBOX_OFF,
			 &checkboxoffxpm, &checkboxoffxpmmask);
	stock_pixmap_gdk(mainwin->window, STOCK_PIXMAP_CHECKBOX_ON,
			 &checkboxonxpm, &checkboxonxpmmask);
	stock_pixmap_gdk(mainwin->window, STOCK_PIXMAP_DELETED,
			 &deletedxpm, &deletedxpmmask);
	stock_pixmap_gdk(mainwin->window, STOCK_PIXMAP_CONTINUE,
			 &continuexpm, &continuexpmmask);
	stock_pixmap_gdk(mainwin->window, STOCK_PIXMAP_MARK,
			 &markxpm, &markxpmmask);
	inc_lock();

	if (!selective.window)
	    sd_window_create(mainwin);

	manage_window_set_transient(GTK_WINDOW(selective.window));
	gtk_widget_show(selective.window);
	sd_clear_msglist();
	sd_ac_menu_set();
	gtk_clist_clear(GTK_CLIST(selective.clist));


}

static void sd_clear_msglist()
{
	PrefsAccount *acc = cur_account;
	while (acc->msg_list != NULL) {
		HeaderItems *item = (HeaderItems*)acc->msg_list->data;

		acc->msg_list = g_slist_remove(acc->msg_list, item);
		g_free(item->from);
		g_free(item->subject);
		g_free(item->date);
		g_free(item);
	}	
	g_slist_free(acc->msg_list);
	sd_update_msg_num(acc);
}

/* sd_remove_header_files()
 * 
 * - removes any stale header files in HEADER_CACHE_DIR 
 *
 */
static void sd_remove_header_files()
{
	gchar *path = g_strconcat(get_header_cache_dir(), G_DIR_SEPARATOR_S, NULL);
	
	remove_all_files(path);
	g_free(path);
}

/* sd_toggle_btn()
 *
 * - checks whether at least on email is selected 
 *   if so, untoggle remove / download button
 */
static void sd_toggle_btn()
{
	PrefsAccount *acc = cur_account;
	GSList *cur;
	
	for (cur = acc->msg_list; cur != NULL; cur = cur->next) {
		HeaderItems *items = (HeaderItems*)cur->data;
		
		if (items->state == SD_CHECKED) {
			gtk_widget_set_sensitive (selective.remove_btn, TRUE);
			gtk_widget_set_sensitive (selective.download_btn, TRUE);
			return;
		}
	}

	gtk_widget_set_sensitive (selective.remove_btn, FALSE);
}

/* sd_header_filter(MsgInfo *msginfo)
 *
 * - parse header line and look for any applying filtering rules
 * - if message matches other MATCHACTION --> return
 *
 */
SD_State sd_header_filter(MsgInfo *msginfo)
{
	GSList *rules;

	/* parse header line for line */
	for (rules = global_processing; rules != NULL; rules = rules->next) { 

		FilteringProp *prop = (FilteringProp*) rules->data; 

		if ( matcherlist_match(prop->matchers, msginfo) ) {
			if (prop->action->type == MATCHACTION_DELETE_ON_SERVER) {
				debug_print(_("action matched\n"));
				return SD_CHECKED;
			}
			else {
				debug_print(_("action not matched\n"));
				return SD_UNCHECKED;
			}
		}
	}
	return SD_UNCHECKED;
}

/* sd_get_msginfo_from_file(const gchar *filename)
 *
 * - retrieve msginfo from saved header files
 */
static MsgInfo *sd_get_msginfo_from_file(const gchar *filename)
{
	MsgInfo  *msginfo;
	MsgFlags  msgflags = { 0, 0 };
	gchar	  date[80];

	msginfo = procheader_parse_file(filename, msgflags, TRUE, FALSE);

	if (msginfo && msginfo->date_t) {
		procheader_date_get_localtime(date, sizeof date, msginfo->date_t);
		if (msginfo->date) g_free(msginfo->date);
		msginfo->date = g_strdup(date);
	} 

	if (!msginfo)
		msginfo = g_new0(MsgInfo, 1);

	if (!msginfo->date) 
		msginfo->date = g_strdup(_("(No Date)"));
	if (!msginfo->from)
		msginfo->from = g_strdup(_("(No Sender)"));
	if (!msginfo->subject)
		msginfo->subject = g_strdup(_("(No Subject)"));
		
	return msginfo;
}

static void sd_clist_set_pixmap(HeaderItems *items, gint row)
{
	
	switch (items->state) {
	case SD_REMOVED:
		gtk_clist_set_pixmap (GTK_CLIST (selective.clist), 
				      row, 0,
				     deletedxpm, deletedxpmmask);
		break;
	case SD_CHECKED:
		gtk_clist_set_pixmap (GTK_CLIST (selective.clist), 
				      row, 0,
				      checkboxonxpm, checkboxonxpmmask);
		break;
	case SD_DOWNLOADED:
		gtk_clist_set_pixmap (GTK_CLIST (selective.clist), 
				      row, 0,
				      markxpm, markxpmmask);
		break;		
	default:
		gtk_clist_set_pixmap (GTK_CLIST (selective.clist), 
				      row, 0,
				      checkboxoffxpm, checkboxoffxpmmask);
		break;
	}
}

static void sd_clist_set_items()
{
	PrefsAccount *acc = cur_account;
	GSList *cur;
	gboolean show_old = 
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(selective.show_old_chkbtn));
	
	gtk_clist_clear(GTK_CLIST(selective.clist));
	gtk_clist_freeze(GTK_CLIST(selective.clist));

	for (cur = acc->msg_list; cur != NULL; cur = cur->next) {
		HeaderItems *items = (HeaderItems*)cur->data;
		gchar *row[5];
		gint row_num;
		
		row[0] = ("");
		row[1] = items->from;
		row[2] = items->subject;
		row[3] = items->date;
		row[4] = g_strdup_printf("%i KB", items->size/1024);
		
		switch (items->state) {
		case SD_REMOVED:
		case SD_DOWNLOADED:
			items->del_by_old_session = TRUE;
		default:
			break;
		}
		if (show_old) {
			if (items->received) {
				row_num = gtk_clist_append(GTK_CLIST(selective.clist), row);
				sd_clist_set_pixmap(items, row_num);
			}
		}
		else {
			row_num = gtk_clist_append(GTK_CLIST(selective.clist), row);
			sd_clist_set_pixmap(items, row_num);
		}
		g_free(row[4]);
	}

	gtk_clist_thaw(GTK_CLIST(selective.clist));
	sd_toggle_btn();
}

/* sd_update_msg_num(PrefsAccount *acc)
 * - keep track of msgs still on server
 * - count UNCHECKED items as well as downloaded but not removed
 */
static void sd_update_msg_num(PrefsAccount *acc)
{
	GSList *cur;
	gint msg_num = g_slist_length(acc->msg_list);
	gchar *text;

	for (cur = acc->msg_list; cur != NULL; cur = cur->next) {
		HeaderItems *items = (HeaderItems*) cur->data;

		if (items->state != SD_UNCHECKED)
			msg_num--;
		if (items->state == SD_DOWNLOADED)
			if (!acc->sd_rmmail_on_download)
				msg_num++;
	}

	text = g_strdup_printf("%i Messages", msg_num);
	gtk_label_set_text(GTK_LABEL(selective.msgs_label), text);

	g_free(text);
}

/* sd_clist_get_items()
 *
 * - get items for clist from Files
 */
static void sd_clist_get_items(void)
{
	GSList *cur;
	PrefsAccount *acc = cur_account;
	gchar *path = g_strconcat(get_header_cache_dir(), G_DIR_SEPARATOR_S, NULL);
	
	for (cur = acc->msg_list; cur != NULL; cur = cur->next) {

		HeaderItems *items = (HeaderItems*) cur->data;
		gchar *filename    = g_strdup_printf("%s%i", path, items->index);
		MsgInfo *msginfo   = sd_get_msginfo_from_file(filename);
		
		items->from        = g_strdup(msginfo->from);
		items->subject     = g_strdup(msginfo->subject);
		items->date	   = g_strdup(msginfo->date);
		
		msginfo->folder = folder_get_default_processing();

		/* move msg file to drop folder */
		if ((msginfo->msgnum = folder_item_add_msg(msginfo->folder, 
							   filename, TRUE)) < 0) {
			unlink(filename);
			return;
		}
		
		if (acc->sd_filter_on_recv)
			items->state = sd_header_filter(msginfo);

		folder_item_remove_msg(msginfo->folder, msginfo->msgnum);
		g_free(filename);
		procmsg_msginfo_free(msginfo);
	}

	g_free(path);
}

static gint sd_deleted_cb(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	sd_remove_header_files();
	sd_clear_msglist();
	gtk_widget_destroy(selective.window);	
	selective.window = NULL;
	inc_unlock();
	return TRUE;
}

/* --- Callbacks -- */
static void sd_action_cb(GtkWidget *widget, guint action)
{
	PrefsAccount *acc = cur_account;
	
	switch(action) {
	case PREVIEW_NEW:
	case PREVIEW_ALL:
		if ( (acc->protocol != A_APOP) &&
		     (acc->protocol != A_POP3) ) {
			alertpanel_error(
				_("Selected Account \"%s\" is not a POP Mail Server.\nPlease select a different Account"), acc->account_name);
			return;
		}
		sd_clear_msglist();
		if (action == PREVIEW_NEW) {
			gtk_widget_set_sensitive(selective.show_old_chkbtn, FALSE);
			inc_selective_download(selective.mainwin, acc, STYPE_PREVIEW_NEW);
		}
		else {
			gtk_widget_set_sensitive(selective.show_old_chkbtn, TRUE);
			inc_selective_download(selective.mainwin, acc, STYPE_PREVIEW_ALL);
		}
		
		gtk_clist_clear(GTK_CLIST(selective.clist));
		sd_clist_get_items();	
		sd_clist_set_items();
		break;
	case REMOVE:
		inc_selective_download(selective.mainwin, acc, STYPE_DELETE);
		sd_clist_set_items();
		break;
	case DOWNLOAD:
		inc_selective_download(selective.mainwin, acc, STYPE_DOWNLOAD);
		sd_clist_set_items();
		break;
	case DONE:
		sd_remove_header_files();
		sd_clear_msglist();
		gtk_widget_hide(selective.window);
		inc_unlock();
		break;
	case CHECKBTN:
		sd_clist_set_items();
		break;
	default:
		break;
	}
	
	sd_update_msg_num(acc);
}

/* Events */
static void sd_select_row_cb(GtkCList *clist, gint row, gint column,
			     GdkEvent *event, gpointer user_data)
{
	if ((row >= 0) && (column >= 0)) {
		PrefsAccount *acc  = cur_account;
		HeaderItems *items = (HeaderItems*) g_slist_nth_data (acc->msg_list, row);

		if (!items) return;
		if (!gtk_clist_get_selectable(GTK_CLIST(selective.clist), row)) return;

		if (items->state == SD_UNCHECKED) {
			items->state = SD_CHECKED;
			gtk_clist_set_pixmap (GTK_CLIST (selective.clist), row, 0,
					      checkboxonxpm, checkboxonxpmmask);
		} else if (items->state == SD_CHECKED) {
			items->state = SD_UNCHECKED;
			gtk_clist_set_pixmap (GTK_CLIST (selective.clist), row, 0,
					      checkboxoffxpm, checkboxoffxpmmask);
		}
		sd_toggle_btn();
	}
}

static void sd_key_pressed(GtkWidget *widget,
			   GdkEventKey *event,
			   gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		sd_action_cb(widget, DONE);
}

/* account menu */
static void sd_ac_label_pressed(GtkWidget *widget, GdkEventButton *event,
				    gpointer data)
{
	if (!event) return;

	gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NORMAL);
	gtk_object_set_data(GTK_OBJECT(selective.ac_menu), "menu_button",
			    widget);

	gtk_menu_popup(GTK_MENU(selective.ac_menu), NULL, NULL,
		       menu_button_position, widget,
		       event->button, event->time);
}

static void sd_ac_menu_popup_closed(GtkMenuShell *menu_shell)
{
	GtkWidget *button;

	button = gtk_object_get_data(GTK_OBJECT(menu_shell), "menu_button");
	if (!button) return;
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	gtk_clist_clear(GTK_CLIST(selective.clist));
	sd_clear_msglist();
	sd_toggle_btn();
	gtk_object_remove_data(GTK_OBJECT(selective.ac_menu), "menu_button");
}

static void sd_ac_menu_cb(GtkMenuItem *menuitem, gpointer data)
{
	cur_account = (PrefsAccount *)data;
	gtk_label_set_text(GTK_LABEL(selective.ac_label), cur_account->account_name);
	gtk_widget_queue_resize(selective.ac_button);
	main_window_reflect_prefs_all();
}

static void sd_ac_menu_set()
{
	GList *cur_ac, *cur_item;
	GtkWidget *menuitem;
	PrefsAccount *ac_prefs;
	GList *account_list = account_get_list();

	/* destroy all previous menu item */
	cur_item = GTK_MENU_SHELL(selective.ac_menu)->children;
	while (cur_item != NULL) {
		GList *next = cur_item->next;
		gtk_widget_destroy(GTK_WIDGET(cur_item->data));
		cur_item = next;
	}

	gtk_label_set_text(GTK_LABEL(selective.ac_label), cur_account->account_name);

	for (cur_ac = account_list; cur_ac != NULL; cur_ac = cur_ac->next) {
		ac_prefs = (PrefsAccount *)cur_ac->data;
		
		menuitem = gtk_menu_item_new_with_label
			(ac_prefs->account_name
			 ? ac_prefs->account_name : _("Untitled"));
		gtk_widget_show(menuitem);
		gtk_menu_append(GTK_MENU(selective.ac_menu), menuitem);
		gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
					   GTK_SIGNAL_FUNC(sd_ac_menu_cb),
					   ac_prefs);
	}
}

/* receive button popup */
static void sd_preview_popup_closed(GtkMenuShell *menu_shell)
{
	gtk_button_set_relief(GTK_BUTTON(selective.preview_btn), GTK_RELIEF_NORMAL);
	manage_window_focus_in(selective.window, NULL, NULL);
}

static void sd_preview_popup_cb(GtkWidget *widget, GdkEventButton *event)
{
	if (!event) return;
	
	if (event->button == 1) {
		gtk_button_set_relief(GTK_BUTTON(widget), GTK_RELIEF_NORMAL);
		gtk_menu_popup(GTK_MENU(selective.preview_popup), NULL, NULL,
		       menu_button_position, widget,
		       event->button, event->time);
	}
}

static void sd_window_create(MainWindow *mainwin)
{
	GtkWidget *window;
	GtkWidget *table;
	GtkWidget *msgs_label;
	GtkWidget *bottom_hbox;
	GtkWidget *fixed_label;
	GtkWidget *expand_label;
	GtkWidget *ac_button;
	GtkWidget *ac_label;
	GtkWidget *ac_menu;
	GtkWidget *show_old_chkbtn;
	GtkWidget *scrolledwindow;
	GtkWidget *clist;
	GtkWidget *state_label;
	GtkWidget *from_label;
	GtkWidget *subject_label;
	GtkWidget *size_label;
	GtkWidget *date_label;
	GtkWidget *toolbar_hbox;
	GtkWidget *toolbar;
	GtkWidget *tmp_toolbar_icon;
	GtkWidget *preview_btn;
	GtkWidget *preview_popup;
	GtkWidget *remove_btn;
	GtkWidget *download_btn;
	GtkWidget *done_btn;
	gint n_menu_entries;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_object_set_data (GTK_OBJECT (window), "window", window);
	gtk_window_set_title (GTK_WINDOW (window), _("Selective download"));
	gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal (GTK_WINDOW (window), TRUE);
	gtk_window_set_policy (GTK_WINDOW (window), TRUE, TRUE, TRUE);

	table = gtk_table_new (2, 2, FALSE);
	gtk_container_add (GTK_CONTAINER (window), table);

	msgs_label = gtk_label_new (_("0 Messages"));
	gtk_table_attach (GTK_TABLE (table), msgs_label, 1, 2, 1, 2,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (msgs_label), 0, 0.5);

	bottom_hbox = gtk_hbox_new (FALSE, 0);
	gtk_table_attach (GTK_TABLE (table), bottom_hbox, 0, 1, 1, 2,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (GTK_FILL), 0, 0);

	show_old_chkbtn = gtk_check_button_new_with_label("Show only old Messages");
	gtk_box_pack_start(GTK_BOX(bottom_hbox), show_old_chkbtn, FALSE, FALSE, 0);

	gtk_signal_connect(GTK_OBJECT(show_old_chkbtn), "toggled",
			   GTK_SIGNAL_FUNC(sd_action_cb), GUINT_TO_POINTER(CHECKBTN));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(show_old_chkbtn), FALSE);
	GTK_WIDGET_UNSET_FLAGS(show_old_chkbtn, GTK_CAN_FOCUS);

	expand_label = gtk_label_new (" ");
	gtk_box_pack_start (GTK_BOX (bottom_hbox), expand_label, TRUE, TRUE, 0);
	
	fixed_label = gtk_label_new (_(" contains "));
	gtk_box_pack_end (GTK_BOX (bottom_hbox), fixed_label, FALSE, FALSE, 0);

	ac_menu = gtk_menu_new();
	gtk_signal_connect(GTK_OBJECT(ac_menu), "selection_done",
			   GTK_SIGNAL_FUNC(sd_ac_menu_popup_closed), NULL);
	ac_button = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(ac_button), GTK_RELIEF_NONE);
	GTK_WIDGET_UNSET_FLAGS(ac_button, GTK_CAN_FOCUS);
	gtk_widget_set_usize(ac_button, -1, 1);
	gtk_box_pack_start(GTK_BOX(bottom_hbox), ac_button, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(ac_button), "button_press_event",
			   GTK_SIGNAL_FUNC(sd_ac_label_pressed), GTK_OBJECT(ac_menu));

	ac_label = gtk_label_new("");
	gtk_container_add(GTK_CONTAINER(ac_button), ac_label);

	scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_table_attach (GTK_TABLE (table), scrolledwindow, 0, 1, 0, 1,
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), 
					GTK_POLICY_NEVER, 
					GTK_POLICY_AUTOMATIC);
	
	clist = gtk_clist_new (5);
	gtk_container_add (GTK_CONTAINER (scrolledwindow), clist);
	gtk_container_set_border_width (GTK_CONTAINER (clist), 5);
	gtk_clist_set_column_width (GTK_CLIST (clist), 0, 20);
	gtk_clist_set_column_width (GTK_CLIST (clist), 1, 150);
	gtk_clist_set_column_width (GTK_CLIST (clist), 2, 150);
	gtk_clist_set_column_width (GTK_CLIST (clist), 3, 100);
	gtk_clist_set_column_width (GTK_CLIST (clist), 4, 30);
	gtk_clist_set_selection_mode (GTK_CLIST (clist), GTK_SELECTION_BROWSE);
	gtk_clist_column_titles_show (GTK_CLIST (clist));
	
	state_label = gtk_label_new (_("#"));
	gtk_clist_set_column_widget (GTK_CLIST (clist), 0, state_label);
	
	from_label = gtk_label_new (_("From"));
	gtk_clist_set_column_widget (GTK_CLIST (clist), 1, from_label);

	subject_label = gtk_label_new (_("Subject"));
	gtk_clist_set_column_widget (GTK_CLIST (clist), 2, subject_label);
	
	date_label = gtk_label_new (_("Date"));
	gtk_clist_set_column_widget (GTK_CLIST (clist), 3, date_label);

	size_label = gtk_label_new (_("Size"));
	gtk_widget_ref (size_label);
	gtk_clist_set_column_widget (GTK_CLIST (clist), 4, size_label);
	
	toolbar_hbox = gtk_hbox_new (FALSE, 0);

	gtk_table_attach (GTK_TABLE (table), toolbar_hbox, 1, 2, 0, 1,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (GTK_FILL), 0, 0);
	
	toolbar = gtk_toolbar_new (GTK_ORIENTATION_VERTICAL, GTK_TOOLBAR_BOTH);

	gtk_box_pack_end (GTK_BOX (toolbar_hbox), toolbar, FALSE, FALSE, 0);
	gtk_toolbar_set_space_size (GTK_TOOLBAR (toolbar), 30);
	gtk_toolbar_set_space_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_SPACE_LINE);
	gtk_container_set_border_width (GTK_CONTAINER (toolbar), 5);
	
	tmp_toolbar_icon = stock_pixmap_widget(toolbar_hbox, STOCK_PIXMAP_MAIL_RECEIVE);
	preview_btn = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
						  GTK_TOOLBAR_CHILD_BUTTON,
						  NULL,
						  _("Preview Mail"),
						  _("preview old/new E-Mail on account"), NULL,
						  tmp_toolbar_icon, NULL, NULL);

	n_menu_entries = sizeof(preview_popup_entries)/sizeof(preview_popup_entries[0]);
	preview_popup = popupmenu_create(preview_btn, preview_popup_entries, n_menu_entries,
				      "<SelectiveDownload>", window);

	gtk_signal_connect(GTK_OBJECT(preview_popup), "selection_done",
			   GTK_SIGNAL_FUNC(sd_preview_popup_closed), NULL);

	gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));
	
	tmp_toolbar_icon = stock_pixmap_widget(toolbar_hbox, STOCK_PIXMAP_CLOSE);
	remove_btn = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
						 GTK_TOOLBAR_CHILD_BUTTON,
						 NULL,
						 _("Remove"),
						 _("remove selected E-Mails"), NULL,
						 tmp_toolbar_icon, NULL, NULL);

	gtk_widget_set_sensitive (remove_btn, FALSE);

	tmp_toolbar_icon = stock_pixmap_widget(toolbar_hbox, STOCK_PIXMAP_DOWN_ARROW);
	download_btn = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
						 GTK_TOOLBAR_CHILD_BUTTON,
						 NULL,
						 _("Download"),
						 _("Download selected E-Mails"), NULL,
						 tmp_toolbar_icon, NULL, NULL);

	gtk_widget_set_sensitive (download_btn, FALSE);

	gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));

	tmp_toolbar_icon = stock_pixmap_widget (toolbar_hbox, STOCK_PIXMAP_COMPLETE);
	done_btn = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
					       GTK_TOOLBAR_CHILD_BUTTON,
					       NULL,
					       _("Done"),
					       _("Exit Dialog"), NULL,
					       tmp_toolbar_icon, NULL, NULL);
	
	gtk_signal_connect (GTK_OBJECT (window), "delete_event",
			    GTK_SIGNAL_FUNC (gtk_widget_hide_on_delete),
			    NULL);
	gtk_signal_connect (GTK_OBJECT(window), "key_press_event",
			    GTK_SIGNAL_FUNC(sd_key_pressed),
			    NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);
	gtk_signal_connect (GTK_OBJECT (preview_btn), "button_press_event",
			    GTK_SIGNAL_FUNC (sd_preview_popup_cb),
			    NULL);

	gtk_signal_connect (GTK_OBJECT (remove_btn), "clicked",
			    GTK_SIGNAL_FUNC (sd_action_cb),
			    GUINT_TO_POINTER(REMOVE));
	gtk_signal_connect (GTK_OBJECT (download_btn), "clicked",
			    GTK_SIGNAL_FUNC (sd_action_cb),
			    GUINT_TO_POINTER(DOWNLOAD));
	gtk_signal_connect (GTK_OBJECT (done_btn), "clicked",
			    GTK_SIGNAL_FUNC (sd_action_cb),
			    GUINT_TO_POINTER(DONE));
	gtk_signal_connect (GTK_OBJECT (clist), "select_row",
			    GTK_SIGNAL_FUNC (sd_select_row_cb),
			    NULL);


	selective.mainwin         = mainwin;
	selective.window          = window;
	selective.clist           = clist;
	selective.preview_btn     = preview_btn;
	selective.remove_btn      = remove_btn;
	selective.download_btn    = download_btn;
	selective.ac_label        = ac_label;
	selective.ac_button       = ac_button;
	selective.ac_menu         = ac_menu;
	selective.preview_popup   = preview_popup;
	selective.msgs_label      = msgs_label;
	selective.show_old_chkbtn = show_old_chkbtn;

	gtk_widget_show_all(window);
}

