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

#include <unistd.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "common/plugin.h"
#include "common/utils.h"
#include "mimeview.h"

typedef struct _dilloViewer dilloViewer;

struct _dilloViewer
{
	MimeViewer	 mimeviewer;
	GtkWidget	*widget;	
	GtkWidget	*socket;
	gchar 		*filename;
};

static MimeViewerFactory dillo_viewer_factory;

static GtkWidget *dillo_get_widget(MimeViewer *_viewer)
{
	dilloViewer *viewer = (dilloViewer *) _viewer;

	debug_print("dillo_get_widget\n");

	return GTK_WIDGET(viewer->widget);
}

static gboolean socket_destroy_cb(GtkObject *object, gpointer data)
{
	dilloViewer *viewer = (dilloViewer *) data;
	debug_print("Destroyed dillo socket %p\n", viewer->socket);
	viewer->socket = NULL;
	return FALSE;
}

static void dillo_show_mimepart(MimeViewer *_viewer, const gchar *infile, MimeInfo *partinfo)
{
	dilloViewer *viewer = (dilloViewer *) _viewer;

	debug_print("dillo_show_mimepart\n");

	if (viewer->filename != NULL) {
		unlink(viewer->filename);
		g_free(viewer->filename);
	}

	viewer->filename = procmime_get_tmp_file_name(partinfo);
	
	if (!(procmime_get_part(viewer->filename, infile, partinfo) < 0)) {
		gchar *cmd;
		if (viewer->socket)
			gtk_widget_destroy(viewer->socket);
		viewer->socket = gtk_socket_new();
		debug_print("Adding dillo socket %p", viewer->socket);
		gtk_container_add(GTK_CONTAINER(viewer->widget),
				  viewer->socket);
		gtk_widget_realize(viewer->socket);
		gtk_widget_show(viewer->socket);
		gtk_signal_connect(GTK_OBJECT(viewer->socket), 
				   "destroy", 
				   GTK_SIGNAL_FUNC(socket_destroy_cb),
				   viewer);
		cmd = g_strdup_printf("dillo -f -l -x %d \"%s\"", 
				(gint) GDK_WINDOW_XWINDOW(viewer->socket->window),
				viewer->filename);
		execute_command_line(cmd, TRUE);
		g_free(cmd);
	}
}

static void dillo_clear_viewer(MimeViewer *_viewer)
{
	dilloViewer *viewer = (dilloViewer *) _viewer;
	debug_print("dillo_clear_viewer\n");
	debug_print("Removing dillo socket %p\n", viewer->socket);
	if (viewer->socket) {
		gtk_widget_destroy(viewer->socket);
	}
		
}

static void dillo_destroy_viewer(MimeViewer *_viewer)
{
	dilloViewer *viewer = (dilloViewer *) _viewer;

	debug_print("dillo_destroy_viewer\n");

	gtk_widget_unref(GTK_WIDGET(viewer->widget));
	unlink(viewer->filename);
	g_free(viewer->filename);
    	g_free(viewer);
}

static MimeViewer *dillo_viewer_create()
{
	dilloViewer *viewer;

	debug_print("dillo_viewer_create\n");
	
	viewer = g_new0(dilloViewer, 1);
	viewer->mimeviewer.factory = &dillo_viewer_factory;

	viewer->mimeviewer.get_widget = dillo_get_widget;
	viewer->mimeviewer.show_mimepart = dillo_show_mimepart;
	viewer->mimeviewer.clear_viewer = dillo_clear_viewer;
	viewer->mimeviewer.destroy_viewer = dillo_destroy_viewer;	

	viewer->widget = gtk_event_box_new();
	gtk_widget_show(viewer->widget);
	gtk_widget_ref(viewer->widget);

	viewer->filename = NULL;

	return (MimeViewer *) viewer;
}

static MimeViewerFactory dillo_viewer_factory =
{
	"text/html",
	0,
	
	dillo_viewer_create,
};

gint plugin_init(gchar **error)
{
	mimeview_register_viewer_factory(&dillo_viewer_factory);
	return 0;	
}

void plugin_done()
{
	mimeview_unregister_viewer_factory(&dillo_viewer_factory);
}

const gchar *plugin_name()
{
	return "Dillo HTML Viewer";
}

const gchar *plugin_desc()
{
	return "This plugin renders HTML mail using the Dillo "
		"web browser.";
}
