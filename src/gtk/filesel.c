/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto
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

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkfilesel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkeditable.h>

#include "sylpheed.h"
#include "filesel.h"
#include "manage_window.h"
#include "gtkutils.h"
#include "utils.h"

static GtkWidget *filesel;
static gboolean filesel_ack;
static gboolean filesel_fin;
static gchar *filesel_oldfilename;

static void filesel_create(const gchar *title, gboolean multiple_files);
static void filesel_ok_cb(GtkWidget *widget, gpointer data);
static void filesel_cancel_cb(GtkWidget *widget, gpointer data);
static gint delete_event(GtkWidget *widget, GdkEventAny *event, gpointer data);
static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data);

static void filesel_file_list_select_row_multi(GtkCList *clist, gint row, gint col,
					       GdkEventButton *event, gpointer userdata);
static void filesel_file_list_select_row_single(GtkCList *clist, gint row, gint col,
					        GdkEventButton *event, gpointer userdata);

static void filesel_dir_list_select_row_multi(GtkCList *clist, gint row, gint col,
					      GdkEventButton *event, gpointer userdata);
static void filesel_dir_list_select_row_single(GtkCList *clist, gint row, gint col,
					       GdkEventButton *event, gpointer userdata);

static GList *filesel_get_multiple_filenames(void);

gchar *filesel_select_file(const gchar *title, const gchar *file)
{
	static gchar *filename = NULL;
	static gchar *cwd = NULL;

	filesel_create(title, FALSE);

	manage_window_set_transient(GTK_WINDOW(filesel));

	if (filename) {
		g_free(filename);
		filename = NULL;
	}

	if (!cwd)
		cwd = g_strconcat(sylpheed_get_startup_dir(), G_DIR_SEPARATOR_S, NULL);

	gtk_file_selection_set_filename(GTK_FILE_SELECTION(filesel), cwd);

	if (file) {
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(filesel),
						file);
		filesel_oldfilename = g_strdup(file);
	} else {
		filesel_oldfilename = NULL;
	}

	gtk_widget_show(filesel);

	filesel_ack = filesel_fin = FALSE;

	while (filesel_fin == FALSE)
		gtk_main_iteration();

	if (filesel_ack) {
		const gchar *str;

		str = gtk_file_selection_get_filename
			(GTK_FILE_SELECTION(filesel));
		if (str && str[0] != '\0') {
			gchar *dir;

			filename = g_strdup(str);
			dir = g_dirname(str);
			g_free(cwd);
			cwd = g_strconcat(dir, G_DIR_SEPARATOR_S, NULL);
			g_free(dir);
		}
	}

	if (filesel_oldfilename) 
		g_free(filesel_oldfilename);

	manage_window_focus_out(filesel, NULL, NULL);
	gtk_widget_destroy(filesel);
	GTK_EVENTS_FLUSH();

	return filename;
}

GList *filesel_select_multiple_files(const gchar *title, const gchar *file)
{
	/* ALF - sorry for the exuberant code duping... need to 
	 * be cleaned up. */
	static gchar *filename = NULL;
	static gchar *cwd = NULL;
	GList        *list = NULL;

	filesel_create(title, TRUE);

	manage_window_set_transient(GTK_WINDOW(filesel));

	if (filename) {
		g_free(filename);
		filename = NULL;
	}

	if (!cwd)
		cwd = g_strconcat(sylpheed_get_startup_dir(), G_DIR_SEPARATOR_S, NULL);

	gtk_file_selection_set_filename(GTK_FILE_SELECTION(filesel), cwd);

	if (file)
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(filesel),
						file);
	gtk_widget_show(filesel);

	filesel_ack = filesel_fin = FALSE;

	while (filesel_fin == FALSE)
		gtk_main_iteration();

	if (filesel_ack) {
		const gchar *fname = NULL;

		list = filesel_get_multiple_filenames();

		if (!list) {
			fname = gtk_file_selection_get_filename(GTK_FILE_SELECTION(filesel));
			list = g_list_append(list, g_strdup(fname));
		}
		/* store dir of first entry */
		if (NULL != (fname = list->data) && *fname) {
			gchar *dir;
			if (NULL != (dir = g_dirname(fname))) {
				g_free(cwd);
				cwd = g_strconcat(dir, G_DIR_SEPARATOR_S, NULL);
				g_free(dir); 
			}
		}
	}

	manage_window_focus_out(filesel, NULL, NULL);
	gtk_widget_destroy(filesel);
	GTK_EVENTS_FLUSH();

	return list;
}

static void filesel_create(const gchar *title, gboolean multiple_files)
{
	filesel = gtk_file_selection_new(title);
	gtk_window_set_position(GTK_WINDOW(filesel), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(filesel), TRUE);
	gtk_window_set_wmclass
		(GTK_WINDOW(filesel), "file_selection", "Sylpheed");

	g_signal_connect(G_OBJECT(GTK_FILE_SELECTION(filesel)->ok_button),
			 "clicked", G_CALLBACK(filesel_ok_cb),
			 NULL);
	g_signal_connect
		(G_OBJECT(GTK_FILE_SELECTION(filesel)->cancel_button),
		 "clicked", G_CALLBACK(filesel_cancel_cb),
		 NULL);
	g_signal_connect(G_OBJECT(filesel), "delete_event",
			 G_CALLBACK(delete_event), NULL);
	g_signal_connect(G_OBJECT(filesel), "key_press_event",
			 G_CALLBACK(key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(filesel);

	if (multiple_files) {
#ifndef _MSC_VER
#warning GTK2 dir_list/file_list now GtkTreeView. selection handlers not working.
#endif
#if 1
		gtk_file_selection_set_select_multiple
			(GTK_FILE_SELECTION(filesel), TRUE);
#else
		gtk_clist_set_selection_mode
			(GTK_CLIST(GTK_FILE_SELECTION(filesel)->file_list),
			 GTK_SELECTION_MULTIPLE);
#endif

		gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(filesel)->file_list),
				   "select_row", 
				   GTK_SIGNAL_FUNC(filesel_file_list_select_row_multi),
				   NULL);
		gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(filesel)->file_list),
				   "unselect_row",
				   GTK_SIGNAL_FUNC(filesel_file_list_select_row_multi),
				   NULL);
		gtk_signal_connect(GTK_OBJECT(GTK_FILE_SELECTION(filesel)->dir_list),
				   "select_row",
				   GTK_SIGNAL_FUNC(filesel_dir_list_select_row_multi),
				   NULL);
	} else {
		gtk_signal_connect_after(GTK_OBJECT(GTK_FILE_SELECTION(filesel)->file_list),
					 "select_row", 
					 GTK_SIGNAL_FUNC(filesel_file_list_select_row_single),
					 NULL);
		gtk_signal_connect_after(GTK_OBJECT(GTK_FILE_SELECTION(filesel)->dir_list),
					 "select_row",
					 GTK_SIGNAL_FUNC(filesel_dir_list_select_row_single),
					 NULL);
	}
}

static void filesel_ok_cb(GtkWidget *widget, gpointer data)
{
	filesel_ack = TRUE;
	filesel_fin = TRUE;
}

static void filesel_cancel_cb(GtkWidget *widget, gpointer data)
{
	filesel_ack = FALSE;
	filesel_fin = TRUE;
}

static gint delete_event(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	filesel_cancel_cb(NULL, NULL);
	if (filesel_oldfilename) {
		g_free(filesel_oldfilename);
		filesel_oldfilename = NULL;
	}
	return TRUE;
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		filesel_cancel_cb(NULL, NULL);
	return FALSE;
}

/* handle both "select_row" and "unselect_row". note that we're using the
 * entry box to put there the selected file names in. we're not using these
 * entry box to get the selected file names. instead we use the clist selection.
 * the entry box is used only to retrieve dir name. */
static void filesel_file_list_select_row_multi(GtkCList *clist, gint row, gint col,
					       GdkEventButton *event, gpointer userdata)
{
	/* simple implementation in which we clear the file entry and refill it */
	GList    *list  = clist->selection;
	GtkEntry *entry = GTK_ENTRY(GTK_FILE_SELECTION(filesel)->selection_entry);

	gtk_editable_delete_text(GTK_EDITABLE(entry), 0, -1);

#define INVALID_FILENAME_CHARS     " "
	for (; list; list = list->next) {
		gint row = GPOINTER_TO_INT(list->data);
		gchar *text = NULL, *tmp;

		if (!gtk_clist_get_text(clist, row, 0, &text))
			break;

		/* NOTE: quick glance in source code of GtkCList
		 * reveals we should not free the returned 'text' */
		
		tmp = g_strconcat(text, " ", NULL);
		text = tmp;
		gtk_entry_append_text(entry, text); 
		g_free(text);
	}
#undef INVALID_FILENAME_CHARS
}

static void filesel_dir_list_select_row_multi(GtkCList *clist, gint row, gint col,
					      GdkEventButton *event, gpointer userdata)
{
	GtkEntry *entry     = GTK_ENTRY(GTK_FILE_SELECTION(filesel)->selection_entry);
	GtkCList *file_list = GTK_CLIST(GTK_FILE_SELECTION(filesel)->file_list);

	/* if dir list is selected we clean everything */
	gtk_editable_delete_text(GTK_EDITABLE(entry), 0, -1);
	gtk_clist_unselect_all(file_list);
}

static void filesel_file_list_select_row_single(GtkCList *clist, gint row, gint col,
					 GdkEventButton *event, gpointer userdata)
{
	gchar *text;

	if(gtk_clist_get_text(clist, row, 0, &text)) {
		filesel_oldfilename = g_strdup(text);
		debug_print("%s\n", filesel_oldfilename);
	} else {
		filesel_oldfilename = NULL;
	}
}

static void filesel_dir_list_select_row_single(GtkCList *clist, gint row, gint col,
				        GdkEventButton *event, gpointer userdata)
{
	gchar *buf;
	GtkEntry *entry = GTK_ENTRY(GTK_FILE_SELECTION(filesel)->selection_entry);

	buf = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
	if(filesel_oldfilename && !(*buf)) {
		gtk_editable_delete_text(GTK_EDITABLE(entry), 0, -1);
		gtk_entry_append_text(entry, filesel_oldfilename);
	}
	g_free(buf);
}

static GList *filesel_get_multiple_filenames(void)
{
#if 0
	/* as noted before we are not using the entry text when selecting
	 * multiple files. to much hassle to parse out invalid chars (chars
	 * that need to be escaped). instead we use the file_list. the
	 * entry is only useful for extracting the current directory. */
	GtkCList *file_list  = GTK_CLIST(GTK_FILE_SELECTION(filesel)->file_list);
	GList    *list = NULL, *sel_list;
	gchar 	 *cwd;	 
	gboolean  separator;

	g_return_val_if_fail(file_list->selection != NULL, NULL);

	cwd = g_dirname(gtk_file_selection_get_filename(GTK_FILE_SELECTION(filesel)));
	g_return_val_if_fail(cwd != NULL, NULL);

	/* only quick way to check the end of a multi byte string for our
	 * separator... */
	g_strreverse(cwd);
	separator = 0 == g_strncasecmp(cwd, G_DIR_SEPARATOR_S, strlen(G_DIR_SEPARATOR_S))
		?  TRUE : FALSE;
	g_strreverse(cwd);

	/* fetch the selected file names */
	for (sel_list = file_list->selection; sel_list; sel_list = sel_list->next) {
		gint   sel = GPOINTER_TO_INT(sel_list->data);
		gchar *sel_text = NULL;
		gchar *fname = NULL;
		
		gtk_clist_get_text(file_list, sel, 0, &sel_text);
		if (!sel_text) continue;
		sel_text = g_strdup(sel_text);

		if (separator)
			fname = g_strconcat(cwd, sel_text, NULL);
		else
			fname = g_strconcat(cwd, G_DIR_SEPARATOR_S, sel_text, NULL);
		
		list = g_list_append(list, fname);
		g_free(sel_text);
	}
	
	g_free(cwd);
	
	return list;
#else
/* GTK2 : gtk_file_selection_get_selections() returns newly allocated gchar**
 * all entries have complete path / are non-utf8 / are free()d in caller
 */
	GList *list = NULL;
	gchar **file_list, **list_ptr;
	
	file_list = gtk_file_selection_get_selections (GTK_FILE_SELECTION(filesel));

	/* fetch the selected file names */
	for (list_ptr = file_list; list_ptr && *list_ptr; list_ptr++)
		list = g_list_append(list, *list_ptr);
	g_free(file_list);
	
	return list;
#endif
}

