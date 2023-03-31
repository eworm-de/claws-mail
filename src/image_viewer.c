/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2023 the Claws Mail team and Hiroyuki Yamamoto
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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "procmime.h"
#include "file-utils.h"
#include "utils.h"
#include "mimeview.h"

#include "prefs_common.h"

typedef struct _ImageViewer ImageViewer;

MimeViewerFactory image_viewer_factory;
void image_viewer_get_resized_size(gint w, gint h, gint aw, gint ah,
					  gint * sw, gint * sh);
static void image_viewer_clear_viewer(MimeViewer *imageviewer);
static void scrolledwin_resize_cb(GtkWidget *scrolledwin, GtkAllocation *alloc,
				  ImageViewer *imageviewer);
struct _ImageViewer
{
	MimeViewer mimeviewer;

	gchar	  *file;
	MimeInfo  *mimeinfo;
	gboolean   resize_img;
	gboolean   fit_img_height;

	GtkWidget *scrolledwin;
	GtkWidget *image;
	GtkWidget *notebook;
	GtkWidget *filename;
	GtkWidget *filesize;
	GtkWidget *error_lbl;
	GtkWidget *error_msg;
	GtkWidget *content_type;
	GtkWidget *load_button;
};

static GtkWidget *image_viewer_get_widget(MimeViewer *_mimeviewer)
{
	ImageViewer *imageviewer = (ImageViewer *) _mimeviewer;

	debug_print("image_viewer_get_widget\n");

	return imageviewer->notebook;
}

static void image_viewer_load_image(ImageViewer *imageviewer)
{
	GtkAllocation allocation;
	GdkPixbufAnimation *animation = NULL;
	GdkPixbuf *pixbuf = NULL;
	GError *error = NULL;
	GInputStream *stream;

	cm_return_if_fail(imageviewer != NULL);

	if (imageviewer->mimeinfo == NULL)
		return;

	stream = procmime_get_part_as_inputstream(imageviewer->mimeinfo);
	if (stream == NULL) {
		g_warning("couldn't get image MIME part");
		return;
	}

#if GDK_PIXBUF_MINOR >= 28
	animation = gdk_pixbuf_animation_new_from_stream(stream, NULL, &error);
#else
	pixbuf = gdk_pixbuf_new_from_stream(stream, NULL, &error);
#endif
	g_object_unref(stream);

	if (error) {
		g_warning("couldn't load image: %s", error->message);
		gtk_label_set_text(GTK_LABEL(imageviewer->error_lbl), _("Error:"));
		gtk_label_set_text(GTK_LABEL(imageviewer->error_msg), error->message);
		gtk_notebook_set_current_page(GTK_NOTEBOOK(imageviewer->notebook), 0);
		gtk_widget_hide(imageviewer->load_button);
		g_error_free(error);
		return;
	}

#if GDK_PIXBUF_MINOR >= 28
	if ((animation && gdk_pixbuf_animation_is_static_image(animation)) ||
	    imageviewer->resize_img || imageviewer->fit_img_height) {
		pixbuf = gdk_pixbuf_animation_get_static_image(animation);
		g_object_ref(pixbuf);
		g_object_unref(animation);
		animation = NULL;
#else
	if (imageviewer->resize_img || imageviewer->fit_img_height) {
#endif
		if (imageviewer->resize_img) {
			gtk_widget_get_allocation(imageviewer->scrolledwin, &allocation);
			pixbuf = claws_load_pixbuf_fitting(pixbuf, FALSE,
							   imageviewer->fit_img_height,
				      			   allocation.width,
				      			   allocation.height);
		} else
			pixbuf = claws_load_pixbuf_fitting(pixbuf, FALSE,
							   imageviewer->fit_img_height,
							   -1, -1);
	}

	if (!pixbuf && !animation) {
		g_warning("couldn't load the image");	
		return;
	}

	g_signal_handlers_block_by_func(G_OBJECT(imageviewer->scrolledwin),
			 G_CALLBACK(scrolledwin_resize_cb), imageviewer);

	if (animation)
		gtk_image_set_from_animation(GTK_IMAGE(imageviewer->image), animation);
	else
		gtk_image_set_from_pixbuf(GTK_IMAGE(imageviewer->image), pixbuf);

	gtk_widget_get_allocation(imageviewer->scrolledwin, &allocation);
	gtk_widget_size_allocate(imageviewer->scrolledwin, &allocation);

	gtk_widget_show(imageviewer->image);

	g_signal_handlers_unblock_by_func(G_OBJECT(imageviewer->scrolledwin), 
			 G_CALLBACK(scrolledwin_resize_cb), imageviewer);

	if (pixbuf)
		g_object_unref(pixbuf);
	if (animation)
		g_object_unref(animation);
}

static void image_viewer_set_notebook_page(MimeViewer *_mimeviewer)
{
	ImageViewer *imageviewer = (ImageViewer *) _mimeviewer;

	if (!prefs_common.display_img)
		gtk_notebook_set_current_page(GTK_NOTEBOOK(imageviewer->notebook), 0);
	else
		gtk_notebook_set_current_page(GTK_NOTEBOOK(imageviewer->notebook), 1);
}

static void image_viewer_show_mimepart(MimeViewer *_mimeviewer, const gchar *file, MimeInfo *mimeinfo)
{
	ImageViewer *imageviewer = (ImageViewer *) _mimeviewer;

	debug_print("image_viewer_show_mimepart\n");

	image_viewer_clear_viewer(_mimeviewer);
	g_free(imageviewer->file);
	imageviewer->file = g_strdup(file);
	imageviewer->mimeinfo = mimeinfo;

	gtk_label_set_text(GTK_LABEL(imageviewer->filename),
			   (procmime_mimeinfo_get_parameter(mimeinfo, "name") != NULL)?
			   procmime_mimeinfo_get_parameter(mimeinfo, "name") :
			   procmime_mimeinfo_get_parameter(mimeinfo, "filename"));
	gtk_label_set_text(GTK_LABEL(imageviewer->filesize), to_human_readable((goffset)mimeinfo->length));
	gtk_label_set_text(GTK_LABEL(imageviewer->content_type), mimeinfo->subtype);
	gtk_label_set_text(GTK_LABEL(imageviewer->error_lbl), "");
	gtk_label_set_text(GTK_LABEL(imageviewer->error_msg), "");

	if (prefs_common.display_img)
		image_viewer_load_image(imageviewer);
}

static void image_viewer_clear_viewer(MimeViewer *_mimeviewer)
{
	ImageViewer *imageviewer = (ImageViewer *) _mimeviewer;
	GtkAdjustment *hadj, *vadj;

	debug_print("image_viewer_clear_viewer\n");

	image_viewer_set_notebook_page(_mimeviewer);

	if (imageviewer->scrolledwin) {
		hadj = gtk_scrolled_window_get_hadjustment
			(GTK_SCROLLED_WINDOW(imageviewer->scrolledwin));
		if (hadj) {
			gtk_adjustment_set_value(hadj, 0.0);
		}
		vadj = gtk_scrolled_window_get_vadjustment
			(GTK_SCROLLED_WINDOW(imageviewer->scrolledwin));
		if (vadj) {
			gtk_adjustment_set_value(vadj, 0.0);
		}
	}
	g_free(imageviewer->file);
	imageviewer->file = NULL;
	imageviewer->mimeinfo = NULL;
	imageviewer->resize_img = prefs_common.resize_img;
	imageviewer->fit_img_height = prefs_common.fit_img_height;
}

static void image_viewer_destroy_viewer(MimeViewer *_mimeviewer)
{
	ImageViewer *imageviewer = (ImageViewer *) _mimeviewer;

	debug_print("image_viewer_destroy_viewer\n");

	image_viewer_clear_viewer(_mimeviewer);
	g_object_unref(imageviewer->notebook);
	g_free(imageviewer);
}

void image_viewer_get_resized_size(gint w, gint h, gint aw, gint ah,
			     gint *sw, gint *sh)
{
	gfloat wratio = 1.0;
	gfloat hratio = 1.0;
	gfloat ratio  = 1.0;

	if (w > aw)
		wratio = (gfloat)aw / (gfloat)w;
	if (h > ah)
		hratio = (gfloat)ah / (gfloat)h;

	ratio = (wratio > hratio) ? hratio : wratio;

	*sw = (gint)(w * ratio);
	*sh = (gint)(h * ratio);

	/* be paranoid */
	if (*sw <= 0 || *sh <= 0) {
		*sw = w;
		*sh = h;
	}
}

static void load_cb(GtkButton *button, ImageViewer *imageviewer)
{
	gtk_notebook_set_current_page(GTK_NOTEBOOK(imageviewer->notebook), 1);
	image_viewer_load_image(imageviewer);
}

static gboolean image_button_cb(GtkWidget *scrolledwin, GdkEventButton *event,
				      ImageViewer *imageviewer)
{
	if (event->button == 1 && imageviewer->image) {
		imageviewer->resize_img = !imageviewer->resize_img;
		image_viewer_load_image(imageviewer);
		return TRUE;
	} else if (event->button == 3 && imageviewer->image) {
		imageviewer->fit_img_height = !imageviewer->fit_img_height;
		image_viewer_load_image(imageviewer);
		return TRUE;
	}
	return FALSE;
}

static void scrolledwin_resize_cb(GtkWidget *scrolledwin, GtkAllocation *alloc,
				  ImageViewer *imageviewer)
{
	if (imageviewer->resize_img)
		image_viewer_load_image(imageviewer);
}

static MimeViewer *image_viewer_create(void)
{
	ImageViewer *imageviewer;
	GtkWidget *notebook;
	GtkWidget *table1;
	GtkWidget *label3;
	GtkWidget *label4;
	GtkWidget *filename;
	GtkWidget *filesize;
	GtkWidget *load_button;
	GtkWidget *label5;
	GtkWidget *content_type;
	GtkWidget *scrolledwin;
	GtkWidget *eventbox;
	GtkWidget *image;
	GtkWidget *error_lbl;
	GtkWidget *error_msg;

	notebook = gtk_notebook_new();
	gtk_widget_set_name(GTK_WIDGET(notebook), "image_viewer");
	gtk_widget_show(notebook);
	gtk_widget_set_can_focus(notebook, FALSE);
	gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), FALSE);
	gtk_notebook_set_show_border(GTK_NOTEBOOK(notebook), FALSE);

	table1 = gtk_grid_new();
	gtk_widget_show(table1);
	gtk_container_add(GTK_CONTAINER(notebook), table1);
	gtk_container_set_border_width(GTK_CONTAINER(table1), 8);
	gtk_grid_set_row_spacing(GTK_GRID(table1), 4);
	gtk_grid_set_column_spacing(GTK_GRID(table1), 4);

	label3 = gtk_label_new(_("Filename:"));
	gtk_widget_show(label3);
	gtk_label_set_xalign(GTK_LABEL(label3), 0.0);
	gtk_grid_attach(GTK_GRID(table1), label3, 0, 0, 2, 1);

	filename = gtk_label_new("");
	gtk_widget_show(filename);
	gtk_label_set_xalign(GTK_LABEL(filename), 0.0);
	gtk_grid_attach(GTK_GRID(table1), filename, 1, 0, 2, 1);
	gtk_widget_set_hexpand(filename, TRUE);
	gtk_widget_set_halign(filename, GTK_ALIGN_FILL);

	label4 = gtk_label_new(_("Filesize:"));
	gtk_widget_show(label4);
	gtk_label_set_xalign(GTK_LABEL(label4), 0.0);
	gtk_grid_attach(GTK_GRID(table1), label4, 0, 1, 2, 1);

	filesize = gtk_label_new("");
	gtk_widget_show(filesize);
	gtk_label_set_xalign(GTK_LABEL(filesize), 0.0);
	gtk_grid_attach(GTK_GRID(table1), filesize, 1, 1, 2, 1);
	gtk_widget_set_hexpand(filesize, TRUE);
	gtk_widget_set_halign(filesize, GTK_ALIGN_FILL);

	label5 = gtk_label_new(_("Content-Type:"));
	gtk_widget_show(label5);
	gtk_label_set_xalign(GTK_LABEL(label5), 0.0);
	gtk_grid_attach(GTK_GRID(table1), label5, 0, 2, 2, 1);
	gtk_widget_set_hexpand(label5, TRUE);
	gtk_widget_set_halign(label5, GTK_ALIGN_FILL);

	content_type = gtk_label_new("");
	gtk_widget_show(content_type);
	gtk_label_set_xalign(GTK_LABEL(content_type), 0.0);
	gtk_grid_attach(GTK_GRID(table1), content_type, 1, 2, 2, 1);
	gtk_widget_set_hexpand(content_type, TRUE);
	gtk_widget_set_halign(content_type, GTK_ALIGN_FILL);

	error_lbl = gtk_label_new("");
	gtk_widget_show(error_lbl);
	gtk_label_set_xalign(GTK_LABEL(error_lbl), 0.0);
	gtk_grid_attach(GTK_GRID(table1), error_lbl, 0, 3, 2, 1);
	gtk_widget_set_hexpand(error_lbl, TRUE);
	gtk_widget_set_halign(error_lbl, GTK_ALIGN_FILL);

	error_msg = gtk_label_new("");
	gtk_widget_show(error_msg);
	gtk_label_set_xalign(GTK_LABEL(error_msg), 0.0);
	gtk_grid_attach(GTK_GRID(table1), error_msg, 1, 3, 2, 1);
	gtk_widget_set_hexpand(error_msg, TRUE);
	gtk_widget_set_halign(error_msg, GTK_ALIGN_FILL);

	load_button = gtk_button_new_with_label(_("Load Image"));
	gtk_widget_show(load_button);
	gtk_widget_set_size_request(GTK_WIDGET(load_button), 6, -1);
	gtk_grid_attach(GTK_GRID(table1), load_button, 0, 4, 1, 1);
	gtk_widget_set_hexpand(load_button, TRUE);
	gtk_widget_set_halign(load_button, GTK_ALIGN_START);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwin);
	gtk_container_add(GTK_CONTAINER(notebook), scrolledwin);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scrolledwin), TRUE);

	eventbox = gtk_event_box_new();
	gtk_widget_show(eventbox);
	gtk_container_add(GTK_CONTAINER(scrolledwin), eventbox);

	image = gtk_image_new();
	gtk_container_add( GTK_CONTAINER(eventbox), image);

	debug_print("Creating image view...\n");
	imageviewer = g_new0(ImageViewer, 1);
	imageviewer->mimeviewer.factory = &image_viewer_factory;

	imageviewer->mimeviewer.get_widget = image_viewer_get_widget;
	imageviewer->mimeviewer.show_mimepart = image_viewer_show_mimepart;
	imageviewer->mimeviewer.clear_viewer = image_viewer_clear_viewer;
	imageviewer->mimeviewer.destroy_viewer = image_viewer_destroy_viewer;
	imageviewer->mimeviewer.get_selection = NULL;

	imageviewer->resize_img   = prefs_common.resize_img;
	imageviewer->fit_img_height   = prefs_common.fit_img_height;

	imageviewer->scrolledwin  = scrolledwin;
	imageviewer->image        = image;
	imageviewer->notebook	  = notebook;
	imageviewer->filename	  = filename;
	imageviewer->filesize	  = filesize;
	imageviewer->content_type = content_type;
	imageviewer->error_msg    = error_msg;
	imageviewer->error_lbl    = error_lbl;
	imageviewer->load_button = load_button;

	g_object_ref(notebook);

	g_signal_connect(G_OBJECT(load_button), "clicked",
			 G_CALLBACK(load_cb), imageviewer);
	g_signal_connect(G_OBJECT(eventbox), "button-press-event",
			 G_CALLBACK(image_button_cb), imageviewer);
	g_signal_connect(G_OBJECT(scrolledwin), "size-allocate",
			 G_CALLBACK(scrolledwin_resize_cb), imageviewer);

	image_viewer_set_notebook_page((MimeViewer *)imageviewer);

	return (MimeViewer *) imageviewer;
}

static gchar *content_types[] =
	{"image/*", NULL};

MimeViewerFactory image_viewer_factory =
{
	content_types,
	0,
	
	image_viewer_create,
};

void image_viewer_init(void)
{
	mimeview_register_viewer_factory(&image_viewer_factory);
}

void image_viewer_done(void)
{
	mimeview_unregister_viewer_factory(&image_viewer_factory);
}
