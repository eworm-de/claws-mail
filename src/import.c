/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2022 the Claws Mail team and Hiroyuki Yamamoto
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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "claws.h"
#include "main.h"
#include "inc.h"
#include "mbox.h"
#include "folderview.h"
#include "filesel.h"
#include "foldersel.h"
#include "gtkutils.h"
#include "manage_window.h"
#include "folder.h"
#include "codeconv.h"
#include "alertpanel.h"

static GtkWidget *window;
static GtkWidget *file_entry;
static GtkWidget *dest_entry;
static GtkWidget *file_button;
static GtkWidget *dest_button;
static GtkWidget *ok_button;
static GtkWidget *cancel_button;
static gboolean import_ok; /* see import_mbox() return values */

static void import_create(void);
static void import_ok_cb(GtkWidget *widget, gpointer data);
static void import_cancel_cb(GtkWidget *widget, gpointer data);
static void import_filesel_cb(GtkWidget *widget, gpointer data);
static void import_destsel_cb(GtkWidget *widget, gpointer data);
static gint delete_event(GtkWidget *widget, GdkEventAny *event, gpointer data);
static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data);

gint import_mbox(FolderItem *default_dest)
/* return values: -2 skipped/cancelled, -1 error, 0 OK */
{
	gchar *dest_id = NULL;

	import_ok = -2;	// skipped or cancelled

	if (!window) {
		import_create();
	}
	else {
		gtk_widget_show(window);
	}

	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	change_dir(claws_get_startup_dir());

	if (default_dest && default_dest->path) {
		dest_id = folder_item_get_identifier(default_dest);
	}

	if (dest_id) {
		gtk_entry_set_text(GTK_ENTRY(dest_entry), dest_id);
		g_free(dest_id);
	} else {
		gtk_entry_set_text(GTK_ENTRY(dest_entry), "");
	}
	gtk_entry_set_text(GTK_ENTRY(file_entry), "");
	gtk_widget_grab_focus(file_entry);

	manage_window_set_transient(GTK_WINDOW(window));

	gtk_main();

	gtk_widget_hide(window);
	gtk_window_set_modal(GTK_WINDOW(window), FALSE);

	return import_ok;
}

static void import_create(void)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *desc_label;
	GtkWidget *table;
	GtkWidget *file_label;
	GtkWidget *dest_label;
	GtkWidget *confirm_area;

	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "import");
	gtk_window_set_title(GTK_WINDOW(window), _("Import mbox file"));
	gtk_container_set_border_width(GTK_CONTAINER(window), 5);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(delete_event), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);

	desc_label = gtk_label_new
		(_("Locate the mbox file and specify the destination folder."));
	gtk_label_set_line_wrap(GTK_LABEL(desc_label), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox), desc_label, FALSE, FALSE, 0);

	table = gtk_grid_new();
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(table), 8);
	gtk_grid_set_row_spacing(GTK_GRID(table), 8);
	gtk_grid_set_column_spacing(GTK_GRID(table), 8);
	gtk_widget_set_size_request(table, 420, -1);

	file_label = gtk_label_new(_("Mbox file:"));
	gtk_label_set_xalign(GTK_LABEL(file_label), 1.0);
	gtk_grid_attach(GTK_GRID(table), file_label, 0, 0, 1, 1);

	dest_label = gtk_label_new(_("Destination folder:"));
	gtk_label_set_xalign(GTK_LABEL(dest_label), 1.0);
	gtk_grid_attach(GTK_GRID(table), dest_label, 0, 1, 1, 1);

	file_entry = gtk_entry_new();
	gtk_grid_attach(GTK_GRID(table), file_entry, 1, 0, 1, 1);
	gtk_widget_set_hexpand(file_entry, TRUE);
	gtk_widget_set_halign(file_entry, GTK_ALIGN_FILL);

	dest_entry = gtk_entry_new();
	gtk_grid_attach(GTK_GRID(table), dest_entry, 1, 1, 1, 1);
	gtk_widget_set_hexpand(dest_entry, TRUE);
	gtk_widget_set_halign(dest_entry, GTK_ALIGN_FILL);

	file_button = gtkut_get_browse_file_btn(_("_Browse"));
	gtk_grid_attach(GTK_GRID(table), file_button, 2, 0, 1, 1);

	g_signal_connect(G_OBJECT(file_button), "clicked",
			 G_CALLBACK(import_filesel_cb), NULL);

	dest_button = gtkut_get_browse_directory_btn(_("B_rowse"));
	gtk_grid_attach(GTK_GRID(table), dest_button, 2, 1, 1, 1);

	g_signal_connect(G_OBJECT(dest_button), "clicked",
			 G_CALLBACK(import_destsel_cb), NULL);

	gtkut_stock_button_set_create(&confirm_area,
				      &cancel_button, NULL, _("_Cancel"),
				      &ok_button, NULL, _("_OK"),
				      NULL, NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_button);

	g_signal_connect(G_OBJECT(ok_button), "clicked",
			 G_CALLBACK(import_ok_cb), NULL);
	g_signal_connect(G_OBJECT(cancel_button), "clicked",
			 G_CALLBACK(import_cancel_cb), NULL);

	gtk_widget_show_all(window);
}

static void import_ok_cb(GtkWidget *widget, gpointer data)
{
	GtkWidget *window;
	const gchar *utf8mbox, *destdir;
	FolderItem *dest;
	gchar *mbox;

	utf8mbox = gtk_entry_get_text(GTK_ENTRY(file_entry));
	destdir = gtk_entry_get_text(GTK_ENTRY(dest_entry));

	cm_return_if_fail(utf8mbox);
	cm_return_if_fail(destdir);

	if (!*utf8mbox) {
		alertpanel_error(_("Source mbox filename can't be left empty."));
		gtk_widget_grab_focus(file_entry);
		return;
	}
	if (!*destdir) {
		if (alertpanel(_("Import mbox file"),
						_("Destination folder is not set.\nImport mbox file to the Inbox folder?"),
						NULL, _("_OK"), NULL, _("_Cancel"), NULL, NULL, ALERTFOCUS_FIRST)
			== G_ALERTALTERNATE) {
			gtk_widget_grab_focus(dest_entry);
			return;
		}
	}

	mbox = g_filename_from_utf8(utf8mbox, -1, NULL, NULL, NULL);
	if (!mbox) {
		g_warning("import_ok_cb(): failed to convert character set");
		mbox = g_strdup(utf8mbox);
	}

	if (!*destdir) {
		dest = folder_find_item_from_path(INBOX_DIR);
	} else {
		dest = folder_find_item_from_identifier(destdir);
	}

	if (!dest) {
		alertpanel_error(_("Can't find the destination folder."));
		gtk_widget_grab_focus(dest_entry);
		g_free(mbox);
		return;
	} else {
		window = label_window_create(_("Importing mbox file..."));
		import_ok = proc_mbox(dest, mbox, FALSE, NULL);
		label_window_destroy(window);
	}

	g_free(mbox);

	if (gtk_main_level() > 1)
		gtk_main_quit();
}

static void import_cancel_cb(GtkWidget *widget, gpointer data)
{
	if (gtk_main_level() > 1)
		gtk_main_quit();
}

static void import_filesel_cb(GtkWidget *widget, gpointer data)
{
	gchar *filename;
	gchar *utf8_filename;

	filename = filesel_select_file_open(_("Select importing file"), NULL);
	if (!filename) return;

	utf8_filename = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
	if (!utf8_filename) {
		g_warning("import_filesel_cb(): failed to convert character set");
		utf8_filename = g_strdup(filename);
	}
	gtk_entry_set_text(GTK_ENTRY(file_entry), utf8_filename);
	g_free(utf8_filename);
}

static void import_destsel_cb(GtkWidget *widget, gpointer data)
{
	FolderItem *dest;
	gchar *path;

	dest = foldersel_folder_sel(NULL, FOLDER_SEL_COPY, NULL, FALSE,
			_("Select folder to import to"));
	if (!dest)
		 return;
	path = folder_item_get_identifier(dest);
	gtk_entry_set_text(GTK_ENTRY(dest_entry), path);
	g_free(path);
}

static gint delete_event(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	import_cancel_cb(NULL, NULL);
	return TRUE;
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_KEY_Escape)
		import_cancel_cb(NULL, NULL);
	return FALSE;
}
