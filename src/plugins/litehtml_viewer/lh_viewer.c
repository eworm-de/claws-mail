/*
 * Claws Mail -- A GTK based, lightweight, and fast e-mail client
 * Copyright(C) 2019 the Claws Mail Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write tothe Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#include "claws-features.h"
#endif

#include <codeconv.h>
#include "common/utils.h"
#include "mainwindow.h"
#include "statusbar.h"
#include "lh_viewer.h"

static gchar *content_types[] = { "text/html", NULL };

MimeViewer *lh_viewer_create();

MimeViewerFactory lh_viewer_factory = {
	content_types,
	0,
	lh_viewer_create
};

static GtkWidget *lh_get_widget(MimeViewer *_viewer)
{
	debug_print("LH: get_widget\n");
	LHViewer *viewer = (LHViewer *)_viewer;
	return viewer->vbox;
}

static void lh_show_mimepart(MimeViewer *_viewer, const gchar *infile,
		MimeInfo *partinfo)
{
	debug_print("LH: show_mimepart\n");
	LHViewer *viewer = (LHViewer *)_viewer;
	gchar *string = procmime_get_part_as_string(partinfo, TRUE);
	gchar *utf8 = NULL;
	const gchar *charset;

	if (string == NULL) {
		g_warning("LH: couldn't get MIME part file");
		return;
	}

	charset = procmime_mimeinfo_get_parameter(partinfo, "charset");
	if (charset != NULL && g_ascii_strcasecmp("utf-8", charset) != 0) {
		gsize length;
		GError *error = NULL;
		debug_print("LH: converting mimepart to UTF-8 from %s\n", charset);
		utf8 = g_convert(string, -1, "utf-8", charset, NULL, &length, &error);
		if (error) {
			g_warning("LH: failed mimepart conversion to UTF-8: %s", error->message);
			g_free(string);
			g_error_free(error);
			return;
		}
		debug_print("LH: successfully converted %" G_GSIZE_FORMAT " bytes\n", length);
	} else {
		utf8 = string;
	}

	lh_widget_set_partinfo(viewer->widget, partinfo);
	lh_widget_open_html(viewer->widget, utf8);
	g_free(utf8);
}

static void lh_clear_viewer(MimeViewer *_viewer)
{
	debug_print("LH: clear_viewer\n");
	LHViewer *viewer = (LHViewer *)_viewer;
	lh_widget_clear(viewer->widget);
}

static void lh_destroy_viewer(MimeViewer *_viewer)
{
	LHViewer *viewer = (LHViewer *)_viewer;

	debug_print("LH: destroy_viewer\n");
	g_free(viewer);
}

/*
static void lh_print_viewer (MimeViewer *_viewer)
{
    debug_print("LH: print_viewer\n");
    
    LHViewer* viewer = (LHViewer *) _viewer;
    lh_widget_print(viewer->widget);    
}
*/


static gboolean lh_scroll_page(MimeViewer *_viewer, gboolean up)
{
	LHViewer *viewer = (LHViewer *)_viewer;
	GtkAdjustment *vadj = NULL;

	if (!viewer || (viewer->widget == NULL))
		return FALSE;

	vadj = gtk_scrolled_window_get_vadjustment(
				GTK_SCROLLED_WINDOW(lh_widget_get_widget(viewer->widget)));
	return gtkutils_scroll_page(lh_widget_get_widget(viewer->widget), vadj, up);
}

static void lh_scroll_one_line(MimeViewer *_viewer, gboolean up)
{
	LHViewer *viewer = (LHViewer *)_viewer;
	GtkAdjustment *vadj = NULL;

	if (!viewer || (viewer->widget == NULL))
		return;

	vadj = gtk_scrolled_window_get_vadjustment(
					GTK_SCROLLED_WINDOW(lh_widget_get_widget(viewer->widget)));
	gtkutils_scroll_one_line(lh_widget_get_widget(viewer->widget), vadj, up);
}

/***************************************************************/
MimeViewer *lh_viewer_create()
{
	debug_print("LH: viewer_create\n");

	LHViewer *viewer = g_new0(LHViewer, 1);
	viewer->mimeviewer.factory = &lh_viewer_factory;
	viewer->widget = lh_widget_new();

	viewer->mimeviewer.get_widget = lh_get_widget;
	viewer->mimeviewer.show_mimepart = lh_show_mimepart;

	viewer->mimeviewer.clear_viewer = lh_clear_viewer;
	viewer->mimeviewer.destroy_viewer = lh_destroy_viewer;

	viewer->mimeviewer.scroll_page = lh_scroll_page;
	viewer->mimeviewer.scroll_one_line = lh_scroll_one_line;

	viewer->vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_name(GTK_WIDGET(viewer->vbox), "litehtml_viewer");

	GtkWidget *w = lh_widget_get_widget(viewer->widget);
	gtk_box_pack_start(GTK_BOX(viewer->vbox), w,
			TRUE, TRUE, 1);

	gtk_widget_show_all(viewer->vbox);

	return (MimeViewer *)viewer;
}

void lh_widget_statusbar_push(const gchar* msg)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();
	STATUSBAR_PUSH(mainwin, msg);
}

void lh_widget_statusbar_pop()
{
        MainWindow *mainwin = mainwindow_get_mainwindow();
        STATUSBAR_POP(mainwin);
}
