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
#include <string.h>
#include <errno.h>
#include <regex.h>

#include "intl.h"
#include "main.h"
#include "prefs.h"
#include "prefs_matcher.h"
#include "prefs_filtering.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "account.h"
#include "mainwindow.h"
#include "foldersel.h"
#include "manage_window.h"
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

struct _SDView {
	MainWindow *mainwin;

	GtkWidget *window;
	GtkWidget *btn_getmail;
	GtkWidget *btn_remove;
	GtkWidget *btn_done;
	GtkWidget *clist;
	GtkWidget *label_account_name;
	GtkWidget *label_mails;
}selective;

GSList *header_item_list = NULL;

static GdkPixmap *checkboxonxpm;
static GdkPixmap *checkboxonxpmmask;
static GdkPixmap *checkboxoffxpm;
static GdkPixmap *checkboxoffxpmmask;

static void sd_window_create(MainWindow *mainwin);
static void sd_clist_set();
static void sd_remove_header_files();


void selective_download(MainWindow *mainwin)
{
	PrefsAccount *account = cur_account;

	summary_write_cache(mainwin->summaryview);
	main_window_lock(mainwin);

	sd_remove_header_files();

	header_item_list = NULL;
	
	sd_window_create(mainwin);

	stock_pixmap_gdk(selective.clist, STOCK_PIXMAP_CHECKBOX_OFF,
			 &checkboxoffxpm, &checkboxoffxpmmask);
	stock_pixmap_gdk(selective.clist, STOCK_PIXMAP_CHECKBOX_ON,
			 &checkboxonxpm, &checkboxonxpmmask);
	
	gtk_label_set_text(GTK_LABEL(selective.label_account_name), account->account_name);
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


/* sd_toggle_btn_remove()
 *
 * - checks whether at least on email is selected for deletion
 *   if so, untoggle remove button
 */
static gboolean sd_toggle_btn_remove()
{
	GSList *cur;
	gint num = 0;
	gpointer row;


	for (cur = header_item_list; cur != NULL; cur = cur->next) {
		HeaderItems *item = (HeaderItems*)cur->data;
		
		if (item->state == CHECKED) {
			gtk_widget_set_sensitive (selective.btn_remove, TRUE);
			return CHECKED;
		}
		num++;
	}

	gtk_widget_set_sensitive (selective.btn_remove, FALSE);
	return UNCHECKED;
}


/* sd_header_filter(MsgInfo *msginfo)
 *
 * - parse header line and look for any applying filtering rules
 * - if message matches other MATCHACTION --> return
 *
 */
gboolean sd_header_filter(MsgInfo *msginfo)
{
	GSList *rules;

	/* parse header line for line */
	for (rules = global_processing; rules != NULL; rules = rules->next) { 

		FilteringProp *prop = (FilteringProp*) rules->data; 
		gchar line[POPBUFSIZE];

		if ( matcherlist_match(prop->matchers, msginfo) ) {
			if (prop->action->type == MATCHACTION_DELETE_ON_SERVER) {
				return CHECKED;
			}
			else 
				return UNCHECKED;
		}
		
	}
	return UNCHECKED;
}


/* sd_get_msginfo_from_file(const gchar *filename)
 *
 * - retrieve msginfo from saved header files
 */
MsgInfo *sd_get_msginfo_from_file(const gchar *filename)
{
	FILE *fp;
	gchar buf[BUFFSIZE];
	MsgInfo *msginfo  = g_new0(MsgInfo, 1);
	MsgFlags msgflags = { 0, 0 };

	msginfo  = (MsgInfo*) procheader_parse(filename, msgflags, TRUE, FALSE);
	if ( (fp = fopen(filename, "r")) != NULL ) {
		static HeaderEntry hentry[] = { { SIZE_HEADER, NULL, FALSE},
						{ NULL, NULL, FALSE} };
		
		procheader_get_one_field (buf, sizeof(buf), fp, hentry);
		fclose(fp);
	}

	if (buf)
		msginfo->size = (off_t)atoi(buf + SIZE_HEADER_LEN);
	else
		msginfo->size = 0;
	
	return msginfo;
}


/* sd_clist_set()
 *
 * - fill clist and set toggle button accordingly
 */
void sd_clist_set()
{
	gint index = 1;

	gchar *path     = g_strconcat(get_header_cache_dir(), G_DIR_SEPARATOR_S, NULL);
	gchar *filename = g_strdup_printf("%s%i", path, index);
	
	while ( is_file_exist(filename) ) {

		MsgInfo *msginfo  = sd_get_msginfo_from_file(filename);
		HeaderItems *line = g_new0 (HeaderItems, 1);
		gchar *row[4];
		gint msgid;	
		
		line->index = index;

		row[0] = "";
		row[1] = line->from    = msginfo->from;
		row[2] = line->subject = msginfo->subject;
		row[3] = line->size    = to_human_readable(msginfo->size);
		
		gtk_clist_append (GTK_CLIST(selective.clist), row);

		msginfo->folder = folder_get_default_processing();

		/* move msg file to drop folder */
		if ((msginfo->msgnum = folder_item_add_msg(msginfo->folder, 
							   filename, TRUE)) < 0) {
			unlink(filename);
			return;
		}
		
		if (sd_header_filter(msginfo)) {
			line->state = CHECKED;
			gtk_clist_set_pixmap (GTK_CLIST (selective.clist), index - 1, 0,
					      checkboxonxpm, checkboxonxpmmask);
		}
		else {
			line->state = UNCHECKED;
			gtk_clist_set_pixmap (GTK_CLIST (selective.clist), index - 1, 0,
					      checkboxoffxpm, checkboxoffxpmmask);
		}
		
		folder_item_remove_msg(msginfo->folder, msginfo->msgnum);

		header_item_list = g_slist_append(header_item_list, line);
		
		index++;
		filename = g_strdup_printf("%s%i", path, index);
	}
	
	sd_toggle_btn_remove();

	g_free(path);
	g_free(filename);
}


/* --- Callbacks -- */
static void sd_btn_remove_cb()
{
	GSList *cur;
	PrefsAccount *account = cur_account;
	gboolean enable_dele  = FALSE;

	account->to_delete = NULL;

	/* loop through list collecting mails marked for delete */
	for (cur = header_item_list; cur != NULL; cur = cur->next) {
		
		HeaderItems *items = (HeaderItems*)cur->data;
		
		if (items->state == CHECKED) {
			gchar *row[4];

			/* replace deleted Mail */
			row[0] = "D";
			row[1] = items->from;
			row[2] = items->subject;
			row[3] = items->size;

			account->to_delete = g_slist_append(account->to_delete, 
							    GINT_TO_POINTER(items->index));

			gtk_clist_remove(GTK_CLIST(selective.clist), items->index-1);
			gtk_clist_insert(GTK_CLIST(selective.clist), items->index-1, row);
			gtk_clist_set_selectable(GTK_CLIST(selective.clist), 
						 items->index - 1, FALSE);
			enable_dele = TRUE;
			debug_print(_("marked to delete %i\n"), 
				    items->index);
			
			items->state = UNCHECKED;
		}
	}


	if (enable_dele == TRUE) {
		inc_selective_download(selective.mainwin, DELE_HEADER);
	}

	sd_toggle_btn_remove();	

	g_slist_free(account->to_delete);
	account->to_delete = NULL;	
}

static void sd_btn_receive_cb()
{

	manage_window_focus_in(selective.window, NULL, NULL);

	inc_selective_download(selective.mainwin, RETR_HEADER);

	gtk_clist_clear(GTK_CLIST(selective.clist));
	sd_clist_set();
}

static void sd_btn_done_cb()
{
	sd_remove_header_files();
	
	if (header_item_list)
		slist_free_strings (header_item_list);

	gtk_widget_hide(selective.window);
	main_window_unlock(selective.mainwin);
}

static void sd_select_row_cb(GtkCList *clist, gint row, gint column,
			     GdkEvent *event, gpointer user_data)
{

	if ((row >= 0) && (column >= 0)) {
		HeaderItems *item = (HeaderItems*) g_slist_nth_data (header_item_list, row);

		if (!item) return;
		if (!gtk_clist_get_selectable(GTK_CLIST(selective.clist), row)) return;

		if (item->state == UNCHECKED) {
			item->state = CHECKED;
			gtk_clist_set_pixmap (GTK_CLIST (selective.clist), row, 0,
					      checkboxonxpm, checkboxonxpmmask);
		} else {
			item->state = UNCHECKED;
			gtk_clist_set_pixmap (GTK_CLIST (selective.clist), row, 0,
					      checkboxoffxpm, checkboxoffxpmmask);
		}
		sd_toggle_btn_remove();
	}
}

static void sd_window_create(MainWindow *mainwin)
{
	GtkWidget *window;
	GtkWidget *table;
	GtkWidget *label_mails;
	GtkWidget *hbox_bottom;
	GtkWidget *label_fixed;
	GtkWidget *label_account_name;
	GtkWidget *scrolledwindow;
	GtkWidget *clist;
	GtkWidget *label_state;
	GtkWidget *label_from;
	GtkWidget *label_subject;
	GtkWidget *label_size;
	GtkWidget *hbox_toolbar;
	GtkWidget *toolbar;
	GtkWidget *btn_receive;
	GtkWidget *tmp_toolbar_icon;
	GtkWidget *btn_remove;
	GtkWidget *btn_done;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_object_set_data (GTK_OBJECT (window), "window", window);
	gtk_window_set_title (GTK_WINDOW (window), _("Selective Download"));
	gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	gtk_window_set_default_size (GTK_WINDOW (window), 450, 250);
	
	table = gtk_table_new (2, 2, FALSE);
	gtk_widget_ref (table);
	gtk_object_set_data_full (GTK_OBJECT (window), "table", table,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (table);
	gtk_container_add (GTK_CONTAINER (window), table);
	
	label_mails = gtk_label_new (_("0 Mail(s)"));
	gtk_widget_ref (label_mails);
	gtk_object_set_data_full (GTK_OBJECT (window), "label_mails", label_mails,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label_mails);
	gtk_table_attach (GTK_TABLE (table), label_mails, 1, 2, 1, 2,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment (GTK_MISC (label_mails), 0, 0.5);
	
	hbox_bottom = gtk_hbox_new (FALSE, 0);
	gtk_widget_ref (hbox_bottom);
	gtk_object_set_data_full (GTK_OBJECT (window), "hbox_bottom", hbox_bottom,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (hbox_bottom);
	gtk_table_attach (GTK_TABLE (table), hbox_bottom, 0, 1, 1, 2,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (GTK_FILL), 0, 0);
	
	label_fixed = gtk_label_new (_("current Account:"));
	gtk_widget_ref (label_fixed);
	gtk_object_set_data_full (GTK_OBJECT (window), "label_fixed", label_fixed,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label_fixed);
	gtk_box_pack_start (GTK_BOX (hbox_bottom), label_fixed, FALSE, FALSE, 0);
	
	label_account_name = gtk_label_new (_("none"));
	gtk_widget_ref (label_account_name);
	gtk_object_set_data_full (GTK_OBJECT (window), "label_account_name", label_account_name,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label_account_name);
	gtk_box_pack_start (GTK_BOX (hbox_bottom), label_account_name, TRUE, FALSE, 0);
	
	scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_ref (scrolledwindow);
	gtk_object_set_data_full (GTK_OBJECT (window), "scrolledwindow", scrolledwindow,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (scrolledwindow);
	gtk_table_attach (GTK_TABLE (table), scrolledwindow, 0, 1, 0, 1,
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			  (GtkAttachOptions) (GTK_EXPAND | GTK_FILL), 0, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	
	clist = gtk_clist_new (4);
	gtk_widget_ref (clist);
	gtk_object_set_data_full (GTK_OBJECT (window), "clist", clist,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (clist);
	gtk_container_add (GTK_CONTAINER (scrolledwindow), clist);
	gtk_container_set_border_width (GTK_CONTAINER (clist), 5);
	gtk_clist_set_column_width (GTK_CLIST (clist), 0, 21);
	gtk_clist_set_column_width (GTK_CLIST (clist), 1, 143);
	gtk_clist_set_column_width (GTK_CLIST (clist), 2, 158);
	gtk_clist_set_column_width (GTK_CLIST (clist), 3, 15);
	gtk_clist_set_selection_mode (GTK_CLIST (clist), GTK_SELECTION_BROWSE);
	gtk_clist_column_titles_show (GTK_CLIST (clist));
	
	label_state = gtk_label_new (_("#"));
	gtk_widget_ref (label_state);
	gtk_object_set_data_full (GTK_OBJECT (window), "label_state", label_state,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label_state);
	gtk_clist_set_column_widget (GTK_CLIST (clist), 0, label_state);
	
	label_from = gtk_label_new (_("From"));
	gtk_widget_ref (label_from);
	gtk_object_set_data_full (GTK_OBJECT (window), "label_from", label_from,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label_from);
	gtk_clist_set_column_widget (GTK_CLIST (clist), 1, label_from);
	
	label_subject = gtk_label_new (_("Subject"));
	gtk_widget_ref (label_subject);
	gtk_object_set_data_full (GTK_OBJECT (window), "label_subject", label_subject,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label_subject);
	gtk_clist_set_column_widget (GTK_CLIST (clist), 2, label_subject);
	
	label_size = gtk_label_new (_("Size"));
	gtk_widget_ref (label_size);
	gtk_object_set_data_full (GTK_OBJECT (window), "label_size", label_size,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (label_size);
	gtk_clist_set_column_widget (GTK_CLIST (clist), 3, label_size);
	
	hbox_toolbar = gtk_hbox_new (FALSE, 0);
	gtk_widget_ref (hbox_toolbar);
	gtk_object_set_data_full (GTK_OBJECT (window), "hbox_toolbar", hbox_toolbar,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (hbox_toolbar);
	gtk_table_attach (GTK_TABLE (table), hbox_toolbar, 1, 2, 0, 1,
			  (GtkAttachOptions) (GTK_FILL),
			  (GtkAttachOptions) (GTK_FILL), 0, 0);
	
	toolbar = gtk_toolbar_new (GTK_ORIENTATION_VERTICAL, GTK_TOOLBAR_BOTH);
	gtk_widget_ref (toolbar);
	gtk_object_set_data_full (GTK_OBJECT (window), "toolbar", toolbar,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (toolbar);
	gtk_box_pack_start (GTK_BOX (hbox_toolbar), toolbar, FALSE, FALSE, 0);
	gtk_toolbar_set_space_size (GTK_TOOLBAR (toolbar), 20);
	gtk_toolbar_set_space_style (GTK_TOOLBAR (toolbar), GTK_TOOLBAR_SPACE_LINE);
	
	tmp_toolbar_icon = stock_pixmap_widget(hbox_toolbar, STOCK_PIXMAP_MAIL);
	btn_receive = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
						  GTK_TOOLBAR_CHILD_BUTTON,
						  NULL,
						  _("Receive"),
						  _("preview E-Mail"), NULL,
						  tmp_toolbar_icon, NULL, NULL);
	gtk_widget_ref (btn_receive);
	gtk_object_set_data_full (GTK_OBJECT (window), "btn_receive", btn_receive,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (btn_receive);
	
	gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));
	
	tmp_toolbar_icon = stock_pixmap_widget(hbox_toolbar, STOCK_PIXMAP_CONTINUE);
	btn_remove = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
						 GTK_TOOLBAR_CHILD_BUTTON,
						 NULL,
						 _("Remove"),
						 _("remove selected E-Mails"), NULL,
						 tmp_toolbar_icon, NULL, NULL);
	gtk_widget_ref (btn_remove);
	gtk_object_set_data_full (GTK_OBJECT (window), "btn_remove", btn_remove,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_set_sensitive (btn_remove, FALSE);
	
	gtk_toolbar_append_space (GTK_TOOLBAR (toolbar));
	
	tmp_toolbar_icon = stock_pixmap_widget (hbox_toolbar, STOCK_PIXMAP_CLOSE);
	btn_done = gtk_toolbar_append_element (GTK_TOOLBAR (toolbar),
					       GTK_TOOLBAR_CHILD_BUTTON,
					       NULL,
					       _("Done"),
					       _("Exit Dialog"), NULL,
					       tmp_toolbar_icon, NULL, NULL);
	gtk_widget_ref (btn_done);
	gtk_object_set_data_full (GTK_OBJECT (window), "btn_done", btn_done,
				  (GtkDestroyNotify) gtk_widget_unref);
	gtk_widget_show (btn_done);
	
	gtk_signal_connect (GTK_OBJECT (window), "destroy",
			    GTK_SIGNAL_FUNC (sd_btn_done_cb),
			    NULL);
	gtk_signal_connect (GTK_OBJECT (btn_receive), "clicked",
			    GTK_SIGNAL_FUNC (sd_btn_receive_cb),
			    NULL);
	gtk_signal_connect (GTK_OBJECT (btn_remove), "clicked",
			    GTK_SIGNAL_FUNC (sd_btn_remove_cb),
			    NULL);
	gtk_signal_connect (GTK_OBJECT (btn_done), "clicked",
			    GTK_SIGNAL_FUNC (sd_btn_done_cb),
			    NULL);
	gtk_signal_connect (GTK_OBJECT (clist), "select_row",
			    GTK_SIGNAL_FUNC (sd_select_row_cb),
			    NULL);



	selective.mainwin            = mainwin;
	selective.window             = window;
	selective.btn_remove         = btn_remove;
	selective.btn_done           = btn_done;
	selective.clist              = clist;
	selective.label_account_name = label_account_name;
	selective.label_mails        = label_mails;

	gtk_widget_show_all(window);
}

