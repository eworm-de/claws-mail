/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto and the Sylpheed-Claws Team
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

#include "intl.h"
#include "common/utils.h"
#include "prefs.h"
#include "prefs_gtk.h"
#include "prefswindow.h"

#include "viewerprefs.h"

#define PREFS_BLOCK_NAME "ImageViewer"

struct ImageViewerPage
{
	PrefsPage page;
	
	GtkWidget *autoload;
	GtkWidget *resize;
};

ImageViewerPrefs imageviewerprefs;

static PrefParam param[] = {
	{"display_img", "TRUE", &imageviewerprefs.display_img, P_BOOL,
	 NULL, NULL, NULL},
	{"resize_image", "TRUE", &imageviewerprefs.resize_img, P_BOOL,
	 NULL, NULL, NULL},

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

static void imageviewer_create_widget_func(PrefsPage * _page,
					   GtkWindow * window,
					   gpointer data)
{
	struct ImageViewerPage *page = (struct ImageViewerPage *) _page;

	/* ------------------ code made by glade -------------------- */
	GtkWidget *table2;
	GtkWidget *label14;
	GtkWidget *label15;
	GtkWidget *autoload;
	GtkWidget *resize;

	table2 = gtk_table_new(2, 2, FALSE);
	gtk_widget_show(table2);
	gtk_container_set_border_width(GTK_CONTAINER(table2), 8);
	gtk_table_set_row_spacings(GTK_TABLE(table2), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table2), 8);

	label14 =
	    gtk_label_new(_("Automatically display attached images"));
	gtk_widget_show(label14);
	gtk_table_attach(GTK_TABLE(table2), label14, 0, 1, 0, 1,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label14), 0, 0.5);

	label15 = gtk_label_new(_("Resize attached images by default\n(Clicking image toggles scaling)"));
	gtk_widget_show(label15);
	gtk_table_attach(GTK_TABLE(table2), label15, 0, 1, 1, 2,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label15), 0, 0.5);

	autoload = gtk_check_button_new_with_label("");
	gtk_widget_show(autoload);
	gtk_table_attach(GTK_TABLE(table2), autoload, 1, 2, 0, 1,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);

	resize = gtk_check_button_new_with_label("");
	gtk_widget_show(resize);
	gtk_table_attach(GTK_TABLE(table2), resize, 1, 2, 1, 2,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	/* --------------------------------------------------------- */

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(resize), imageviewerprefs.resize_img);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autoload), imageviewerprefs.display_img);

	page->autoload = autoload;
	page->resize = resize;

	page->page.widget = table2;
}

static void imageviewer_destroy_widget_func(PrefsPage *_page)
{
}

static void imageviewer_save_func(PrefsPage * _page)
{
	struct ImageViewerPage *page = (struct ImageViewerPage *) _page;
	PrefFile *pfile;
	gchar *rcpath;

	imageviewerprefs.display_img =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
					 (page->autoload));
	imageviewerprefs.resize_img =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->resize));

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	pfile = prefs_write_open(rcpath);
	g_free(rcpath);
	if (!pfile || (prefs_set_block_label(pfile, PREFS_BLOCK_NAME) < 0))
		return;

	if (prefs_write_param(param, pfile->fp) < 0) {
		g_warning("failed to write ImageViewer configuration to file\n");
		prefs_file_close_revert(pfile);
		return;
	}
	fprintf(pfile->fp, "\n");

	prefs_file_close(pfile);
}

static struct ImageViewerPage imageviewer_page;

void image_viewer_prefs_init(void)
{
	static gchar *path[3];

	path[0] = _("Message View");
	path[1] = _("Image Viewer");
	path[2] = NULL;

	prefs_set_default(param);
	prefs_read_config(param, PREFS_BLOCK_NAME, COMMON_RC);

	imageviewer_page.page.path = path;
	imageviewer_page.page.create_widget = imageviewer_create_widget_func;
	imageviewer_page.page.destroy_widget = imageviewer_destroy_widget_func;
	imageviewer_page.page.save_page = imageviewer_save_func;

	prefs_gtk_register_page((PrefsPage *) &imageviewer_page);
}

void image_viewer_prefs_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) &imageviewer_page);
}
