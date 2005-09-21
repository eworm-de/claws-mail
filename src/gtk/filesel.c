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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif 

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkfilesel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtkfilechooser.h>
#include <gtk/gtkfilechooserdialog.h>

#include "sylpheed.h"
#include "filesel.h"
#include "manage_window.h"
#include "gtkutils.h"
#include "utils.h"
#include "codeconv.h"

static gchar *last_selected_dir = NULL;
static GList *filesel_create(const gchar *title, const gchar *path, 
			     gboolean multiple_files,
			     gboolean open, gboolean folder_mode,
			     const gchar *filter)
{
	GSList *slist = NULL, *slist_orig = NULL;
	GList *list = NULL;

	gint action = (open == TRUE) ? 
			(folder_mode == TRUE ? GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
					       GTK_FILE_CHOOSER_ACTION_OPEN):
			(folder_mode == TRUE ? GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
					       GTK_FILE_CHOOSER_ACTION_SAVE);
			
	gchar * action_btn = (open == TRUE) ? GTK_STOCK_OPEN:GTK_STOCK_SAVE;
	GtkWidget *chooser = gtk_file_chooser_dialog_new (title, NULL, action, 
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				action_btn, GTK_RESPONSE_ACCEPT, 
				NULL);
	if (filter != NULL) {
		GtkFileFilter *file_filter = gtk_file_filter_new();
		gtk_file_filter_add_pattern(file_filter, filter);
		gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(chooser),
					    file_filter);
	}

	manage_window_set_transient (GTK_WINDOW(chooser));
	gtk_window_set_modal(GTK_WINDOW(chooser), TRUE);

	gtk_file_chooser_set_select_multiple (GTK_FILE_CHOOSER(chooser), multiple_files);

	if (path && strlen(path) > 0) {
		char *filename = NULL;
		char *realpath = strdup(path);
		if (path[strlen(path)-1] == G_DIR_SEPARATOR) {
			filename = "";
		} else if ((filename = strrchr(path, G_DIR_SEPARATOR)) != NULL) {
			filename++;
			*(strrchr(realpath, G_DIR_SEPARATOR)+1) = '\0';
		} else {
			filename = (char *) path;
			free(realpath); 
			realpath = strdup(get_home_dir());
		}
		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), realpath);
		if (action == GTK_FILE_CHOOSER_ACTION_SAVE)
			gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(chooser), filename);
		free(realpath);
	} else {
		if (!last_selected_dir)
			last_selected_dir = g_strdup_printf("%s%c", get_home_dir(), G_DIR_SEPARATOR);

		gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(chooser), last_selected_dir);
	}

	if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT) 
		slist = gtk_file_chooser_get_filenames (GTK_FILE_CHOOSER (chooser));
	
	manage_window_focus_out(chooser, NULL, NULL);
	gtk_widget_destroy (chooser);

	slist_orig = slist;
	
	if (slist) {
		gchar *tmp = strdup(slist->data);
		if (last_selected_dir)
			g_free(last_selected_dir);
		
		if (strrchr(tmp, G_DIR_SEPARATOR))
			*(strrchr(tmp, G_DIR_SEPARATOR)+1) = '\0';
		last_selected_dir = g_strdup(tmp);
		g_free(tmp);
	}

	while (slist) {
		list = g_list_append(list, slist->data);
		slist = slist->next;
	}
	
	if (slist_orig)
		g_slist_free(slist_orig);
	
	return list;
}

/**
 * This function lets the user select multiple files.
 * This opens an Open type dialog.
 * @param title the title of the dialog
 */
GList *filesel_select_multiple_files_open(const gchar *title)
{
	return filesel_create(title, NULL, TRUE, TRUE, FALSE, NULL);
}

/**
 * This function lets the user select one file.
 * This opens an Open type dialog if "file" is NULL, 
 * Save dialog if "file" contains a path.
 * @param title the title of the dialog
 * @param path the optional path to save to
 */
static gchar *filesel_select_file(const gchar *title, const gchar *path,
				  gboolean open, gboolean folder_mode,
				  const gchar *filter)
{
	GList * list = filesel_create(title, path, FALSE, open, folder_mode, filter);
	gchar * result = NULL;
	if (list) {
		result = strdup(list->data);
	}
	g_list_free(list);
	return result;
}
gchar *filesel_select_file_open(const gchar *title, const gchar *path)
{
	return filesel_select_file (title, path, TRUE, FALSE, NULL);
}

gchar *filesel_select_file_open_with_filter(const gchar *title, const gchar *path,
					    const gchar *filter)
{
	return filesel_select_file (title, path, TRUE, FALSE, filter);
}

gchar *filesel_select_file_save(const gchar *title, const gchar *path)
{
	return filesel_select_file (title, path, FALSE, FALSE, NULL);
}

gchar *filesel_select_file_open_folder(const gchar *title, const gchar *path)
{
	return filesel_select_file (title, path, TRUE, TRUE, NULL);
}

gchar *filesel_select_file_save_folder(const gchar *title, const gchar *path)
{
	return filesel_select_file (title, path, FALSE, TRUE, NULL);
}

