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

#include <unistd.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gtkmathview/gtkmathview.h>

#include "common/sylpheed.h"
#include "common/version.h"
#include "plugin.h"
#include "utils.h"
#include "mimeview.h"

typedef struct _MathMLViewer MathMLViewer;

struct _MathMLViewer
{
	MimeViewer mimeviewer;
	
	GtkScrolledWindow	*scrollwin;;
	GtkMathView 		*mathview;
	gchar 			*filename;
};

static MimeViewerFactory mathml_viewer_factory;

static GtkWidget *mathml_get_widget(MimeViewer *_viewer)
{
	MathMLViewer *viewer = (MathMLViewer *) _viewer;

	debug_print("mathml_get_widget\n");

	return GTK_WIDGET(viewer->scrollwin);
}

static void mathml_show_mimepart(MimeViewer *_viewer, const gchar *infile, MimeInfo *partinfo)
{
	MathMLViewer *viewer = (MathMLViewer *) _viewer;

	debug_print("mathml_show_mimepart\n");

	if (viewer->filename != NULL) {
		g_unlink(viewer->filename);
		g_free(viewer->filename);
	}

	viewer->filename = procmime_get_tmp_file_name(partinfo);
	
	if (!(procmime_get_part(viewer->filename, partinfo) < 0)) {
		gchar *uri;
		
		uri = g_strconcat("file://", viewer->filename, NULL);
		gtk_math_view_load_uri(GTK_MATH_VIEW(viewer->mathview), uri);
		g_free(uri);
	}
}

static void mathml_clear_viewer(MimeViewer *_viewer)
{
	MathMLViewer *viewer = (MathMLViewer *) _viewer;

	debug_print("mathml_clear_viewer\n");

	gtk_math_view_unload(viewer->mathview);
}

static void mathml_destroy_viewer(MimeViewer *_viewer)
{
	MathMLViewer *viewer = (MathMLViewer *) _viewer;

	debug_print("mathml_destroy_viewer\n");

	gtk_widget_unref(GTK_WIDGET(viewer->scrollwin));
	g_unlink(viewer->filename);
	g_free(viewer->filename);
    	g_free(viewer);
}

static MimeViewer *mathml_viewer_create(void)
{
	MathMLViewer *viewer;

	debug_print("mathml_viewer_create\n");
	
	viewer = g_new0(MathMLViewer, 1);
	viewer->mimeviewer.factory = &mathml_viewer_factory;

	viewer->mimeviewer.get_widget = mathml_get_widget;
	viewer->mimeviewer.show_mimepart = mathml_show_mimepart;
	viewer->mimeviewer.clear_viewer = mathml_clear_viewer;
	viewer->mimeviewer.destroy_viewer = mathml_destroy_viewer;	

	viewer->scrollwin = GTK_SCROLLED_WINDOW(gtk_scrolled_window_new(NULL, NULL));
	gtk_widget_show(GTK_WIDGET(viewer->scrollwin));
	gtk_widget_ref(GTK_WIDGET(viewer->scrollwin));
	gtk_scrolled_window_set_policy(viewer->scrollwin, GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	viewer->mathview = GTK_MATH_VIEW(gtk_math_view_new(NULL, NULL));
	gtk_widget_show(GTK_WIDGET(viewer->mathview));
	viewer->filename = NULL;
	gtk_container_add(GTK_CONTAINER(viewer->scrollwin), GTK_WIDGET(viewer->mathview));

	return (MimeViewer *) viewer;
}

static gchar *content_types[] =
	{"text/mathml", NULL};

static MimeViewerFactory mathml_viewer_factory =
{
	content_types,
	0,
	
	mathml_viewer_create,
};

gint plugin_init(gchar **error)
{
	if ((sylpheed_get_version() > VERSION_NUMERIC)) {
		*error = g_strdup("Your sylpheed version is newer than the version the plugin was built with");
		return -1;
	}

	if ((sylpheed_get_version() < MAKE_NUMERIC_VERSION(0, 9, 3, 86))) {
		*error = g_strdup("Your sylpheed version is too old");
		return -1;
	}

	mimeview_register_viewer_factory(&mathml_viewer_factory);
	return 0;	
}

void plugin_done(void)
{
	mimeview_unregister_viewer_factory(&mathml_viewer_factory);
}

const gchar *plugin_name(void)
{
	return _("MathML Viewer");
}

const gchar *plugin_desc(void)
{
	return _("This plugin uses the GtkMathView widget to render "
	         "MathML attachments (Content-Type: text/mathml)");
}

const gchar *plugin_type(void)
{
	return "GTK2";
}
