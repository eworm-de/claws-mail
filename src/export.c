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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkfilesel.h>
#include <gtk/gtksignal.h>

#include "intl.h"
#include "main.h"
#include "inc.h"
#include "mbox.h"
#include "filesel.h"
#include "foldersel.h"
#include "gtkutils.h"
#include "manage_window.h"
#include "folder.h"
#include "utils.h"

static GtkWidget *window;
static GtkWidget *src_entry;
static GtkWidget *file_entry;
static GtkWidget *src_button;
static GtkWidget *file_button;
static GtkWidget *ok_button;
static GtkWidget *cancel_button;
static gboolean export_ack;

static void export_create(void);
static void export_ok_cb(GtkWidget *widget, gpointer data);
static void export_cancel_cb(GtkWidget *widget, gpointer data);
static void export_srcsel_cb(GtkWidget *widget, gpointer data);
static void export_filesel_cb(GtkWidget *widget, gpointer data);
static gint delete_event(GtkWidget *widget, GdkEventAny *event, gpointer data);
static void key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data);

gint export_mbox(FolderItem *default_src)
{
	gint ok = 0;

	if (!window)
		export_create();
	else
		gtk_widget_show(window);

	change_dir(startup_dir);

	if (default_src && default_src->path)
		gtk_entry_set_text(GTK_ENTRY(src_entry), default_src->path);
	else
		gtk_entry_set_text(GTK_ENTRY(src_entry), "");
	gtk_entry_set_text(GTK_ENTRY(file_entry), "");
	gtk_widget_grab_focus(file_entry);

	manage_window_set_transient(GTK_WINDOW(window));

	gtk_main();

	if (export_ack) {
		gchar *srcdir, *mbox;
		FolderItem *src;

		srcdir = gtk_entry_get_text(GTK_ENTRY(src_entry));
		mbox = gtk_entry_get_text(GTK_ENTRY(file_entry));

		if (mbox && *mbox) {
			src = folder_find_item_from_path(srcdir);
			if (!src)
				g_warning("Can't find the folder.\n");
			else
				ok = export_to_mbox(src, mbox);
		}
	}

	gtk_widget_hide(window);

	return ok;
}

static void export_create(void)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *desc_label;
	GtkWidget *table;
	GtkWidget *file_label;
	GtkWidget *src_label;
	GtkWidget *confirm_area;

	window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_title(GTK_WINDOW(window), _("Export"));
	gtk_container_set_border_width(GTK_CONTAINER(window), 5);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, FALSE);
	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
			   GTK_SIGNAL_FUNC(delete_event), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
			   GTK_SIGNAL_FUNC(key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);

	vbox = gtk_vbox_new(FALSE, 4);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 4);

	desc_label = gtk_label_new
		(_("Specify target folder and mbox file."));
	gtk_box_pack_start(GTK_BOX(hbox), desc_label, FALSE, FALSE, 0);

	table = gtk_table_new(2, 3, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(table), 8);
	gtk_table_set_row_spacings(GTK_TABLE(table), 8);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);
	gtk_widget_set_usize(table, 420, -1);

	src_label = gtk_label_new(_("Source dir:"));
	gtk_table_attach(GTK_TABLE(table), src_label, 0, 1, 0, 1,
			 GTK_FILL, GTK_EXPAND|GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(src_label), 1, 0.5);

	file_label = gtk_label_new(_("Exporting file:"));
	gtk_table_attach(GTK_TABLE(table), file_label, 0, 1, 1, 2,
			 GTK_FILL, GTK_EXPAND|GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(file_label), 1, 0.5);

	src_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), src_entry, 1, 2, 0, 1,
			 GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	file_entry = gtk_entry_new();
	gtk_table_attach(GTK_TABLE(table), file_entry, 1, 2, 1, 2,
			 GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	src_button = gtk_button_new_with_label(_(" Select... "));
	gtk_table_attach(GTK_TABLE(table), src_button, 2, 3, 0, 1,
			 0, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(src_button), "clicked",
			   GTK_SIGNAL_FUNC(export_srcsel_cb), NULL);

	file_button = gtk_button_new_with_label(_(" Select... "));
	gtk_table_attach(GTK_TABLE(table), file_button, 2, 3, 1, 2,
			 0, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(file_button), "clicked",
			   GTK_SIGNAL_FUNC(export_filesel_cb), NULL);

	gtkut_button_set_create(&confirm_area,
				&ok_button,	_("OK"),
				&cancel_button, _("Cancel"),
				NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_button);

	gtk_signal_connect(GTK_OBJECT(ok_button), "clicked",
			   GTK_SIGNAL_FUNC(export_ok_cb), NULL);
	gtk_signal_connect(GTK_OBJECT(cancel_button), "clicked",
			   GTK_SIGNAL_FUNC(export_cancel_cb), NULL);

	gtk_widget_show_all(window);
}

static void export_ok_cb(GtkWidget *widget, gpointer data)
{
	export_ack = TRUE;
	if (gtk_main_level() > 1)
		gtk_main_quit();
}

static void export_cancel_cb(GtkWidget *widget, gpointer data)
{
	export_ack = FALSE;
	if (gtk_main_level() > 1)
		gtk_main_quit();
}

static void export_filesel_cb(GtkWidget *widget, gpointer data)
{
	gchar *filename;

	filename = filesel_select_file(_("Select exporting file"), NULL);
	if (filename)
		gtk_entry_set_text(GTK_ENTRY(file_entry), filename);
}

static void export_srcsel_cb(GtkWidget *widget, gpointer data)
{
	FolderItem *src;

	src = foldersel_folder_sel(NULL, FOLDER_SEL_ALL, NULL);
	if (src && src->path)
		gtk_entry_set_text(GTK_ENTRY(src_entry), src->path);
}

static gint delete_event(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	export_cancel_cb(NULL, NULL);
	return TRUE;
}

static void key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		export_cancel_cb(NULL, NULL);
}
