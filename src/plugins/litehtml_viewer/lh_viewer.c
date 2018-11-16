/*
 * Claws Mail -- A GTK+ based, lightweight, and fast e-mail client
 * Copyright(C) 1999-2015 the Claws Mail Team
 * == Fancy Plugin ==
 * This file Copyright (C) 2009-2015 Salvatore De Paolis
 * <iwkse@claws-mail.org> and the Claws Mail Team
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

static gchar *get_utf8_string(const gchar *string) {
        gchar *utf8 = NULL;
        gsize length;
        GError *error = NULL;
        gchar *locale = NULL;

	if (!g_utf8_validate(string, -1, NULL)) {
		const gchar *cur_locale = conv_get_current_locale();
		gchar* split = g_strstr_len(cur_locale, -1, ".");
		if (split) {
		    locale = ++split;
		} else {
		    locale = (gchar *) cur_locale;
		}
		debug_print("Try converting to UTF-8 from %s\n", locale);
		if (g_ascii_strcasecmp("utf-8", locale) != 0) {
		    utf8 = g_convert(string, -1, "utf-8", locale, NULL, &length, &error);
		    if (error) {
			    debug_print("Failed convertion to current locale: %s\n", error->message);
			    g_clear_error(&error);
			}
	    }
	    if (!utf8) {
	        debug_print("Use iso-8859-1 as last resort\n");
			utf8 = g_convert(string, -1, "utf-8", "iso-8859-1", NULL, &length, &error);
			if (error) {
				debug_print("Charset detection failed. Use text as is\n");
				utf8 = g_strdup(string);
				g_clear_error(&error);
			}
		}
	} else {
		utf8 = g_strdup(string);
	}

	return utf8;
}

static void lh_show_mimepart(MimeViewer *_viewer, const gchar *infole,
		MimeInfo *partinfo)
{
	debug_print("LH: show_mimepart\n");
	LHViewer *viewer = (LHViewer *)_viewer;

	gchar *msgfile = procmime_get_tmp_file_name(partinfo);
	debug_print("LH: msgfile '%s'\n", msgfile);

	if (procmime_get_part(msgfile, partinfo) < 0) {
		debug_print("LH: couldn't get MIME part file\n");
		g_free(msgfile);
		return;
	}

	gchar *contents, *utf8;
	gsize length;
	GError *error = NULL;
	if (!g_file_get_contents(msgfile, &contents, &length, &error)) {
		g_warning("LiteHTML viewer: couldn't read contents of file '%s': %s",
				msgfile, error->message);
		g_clear_error(&error);
		return;
	} else {
		utf8 = get_utf8_string(contents);
		g_free(contents);
	}

	g_free(msgfile);

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
	debug_print("LH: destroy_viewer\n");

	/* Just in case. */
	lh_clear_viewer(_viewer);

//	LHViewer *viewer = (LHViewer *)_viewer;
//	lh_widget_destroy(viewer->widget);
}

static void lh_print_viewer (MimeViewer *_viewer)
{
    debug_print("LH: print_viewer\n");
    
    LHViewer* viewer = (LHViewer *) _viewer;
    lh_widget_print(viewer->widget);    
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
	
	viewer->mimeviewer.print = lh_print_viewer;

	viewer->vbox = gtk_vbox_new(FALSE, 0);

	GtkWidget *w = lh_widget_get_widget(viewer->widget);
	gtk_box_pack_start(GTK_BOX(viewer->vbox), w,
			TRUE, TRUE, 1);

	gtk_widget_show_all(viewer->vbox);

	return (MimeViewer *)viewer;
}

void lh_widget_statusbar_push(gchar* msg)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();
	STATUSBAR_PUSH(mainwin, msg);
        g_free(msg);
}

void lh_widget_statusbar_pop()
{
        MainWindow *mainwin = mainwindow_get_mainwindow();
        STATUSBAR_POP(mainwin);
}
