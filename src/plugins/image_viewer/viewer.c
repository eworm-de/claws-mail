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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkpixmap.h>

#if HAVE_GDK_PIXBUF
#  include <gdk-pixbuf/gdk-pixbuf.h>
#else
#if HAVE_GDK_IMLIB
#  include <gdk_imlib.h>
#endif
#endif /* HAVE_GDK_PIXBUF */

#include "intl.h"
#include "procmime.h"
#include "utils.h"
#include "mimeview.h"

#include "viewerprefs.h"

typedef struct _ImageViewer ImageViewer;

MimeViewerFactory image_viewer_factory;
static void image_viewer_get_resized_size(gint w, gint h, gint aw, gint ah,
					  gint * sw, gint * sh);
static void image_viewer_clear_viewer(MimeViewer *imageviewer);

struct _ImageViewer
{
	MimeViewer mimeviewer;

	GtkWidget *scrolledwin;
	GtkWidget *image;
};

static GtkWidget *image_viewer_get_widget(MimeViewer *_mimeviewer)
{
	ImageViewer *imageviewer = (ImageViewer *) _mimeviewer;

	debug_print("image_viewer_get_widget\n");

	return imageviewer->scrolledwin;
}

#if HAVE_GDK_PIXBUF
static void image_viewer_show_mimepart(MimeViewer *_mimeviewer, const gchar *file, MimeInfo *mimeinfo)
{
	ImageViewer *imageviewer = (ImageViewer *) _mimeviewer;
	GdkPixbuf *pixbuf;
	GdkPixbuf *pixbuf_scaled;
	GdkPixmap *pixmap;
	GdkBitmap *mask;
	gint avail_width;
	gint avail_height;
	gint new_width;
	gint new_height;
	gchar *imgfile;

	debug_print("image_viewer_show_mimepart\n");

	g_return_if_fail(imageviewer != NULL);

	image_viewer_clear_viewer(_mimeviewer);

	imgfile = procmime_get_tmp_file_name(mimeinfo);
	if (procmime_get_part(imgfile, file, mimeinfo) < 0) {
		g_warning("Can't get mimepart file");	
		g_free(imgfile);
		return;
	}

	pixbuf = gdk_pixbuf_new_from_file(imgfile);
	unlink(imgfile);
	g_free(imgfile);
	if (!pixbuf) {
		g_warning("Can't load the image.");	
		return;
	}

	if (imageviewerprefs.resize_img) {
		avail_width = imageviewer->scrolledwin->parent->allocation.width;
		avail_height = imageviewer->scrolledwin->parent->allocation.height;
		if (avail_width > 8) avail_width -= 8;
		if (avail_height > 8) avail_height -= 8;

		image_viewer_get_resized_size(gdk_pixbuf_get_width(pixbuf),
				 gdk_pixbuf_get_height(pixbuf),
				 avail_width, avail_height,
				 &new_width, &new_height);

		pixbuf_scaled = gdk_pixbuf_scale_simple
			(pixbuf, new_width, new_height, GDK_INTERP_BILINEAR);
		gdk_pixbuf_unref(pixbuf);
		pixbuf = pixbuf_scaled;
	}

	gdk_pixbuf_render_pixmap_and_mask(pixbuf, &pixmap, &mask, 0);

	if (!imageviewer->image) {
		imageviewer->image = gtk_pixmap_new(pixmap, mask);

		gtk_scrolled_window_add_with_viewport
			(GTK_SCROLLED_WINDOW(imageviewer->scrolledwin),
			 imageviewer->image);
	} else
		gtk_pixmap_set(GTK_PIXMAP(imageviewer->image), pixmap, mask);

	gtk_widget_show(imageviewer->image);

	gdk_pixbuf_unref(pixbuf);
}
#else
#if HAVE_GDK_IMLIB
static void image_viewer_show_mimepart(MimeViewer *_mimeviewer, const gchar *file, MimeInfo *mimeinfo)
{
	ImageViewer *imageviewer = (ImageViewer *) _mimeviewer;
	GdkImlibImage *im;
	gint avail_width;
	gint avail_height;
	gint new_width;
	gint new_height;
	gchar *imgfile;

	debug_print("image_viewer_show_mimepart\n");

	g_return_if_fail(imageviewer != NULL);

	image_viewer_clear_viewer(imageviewer);

	imgfile = procmime_get_tmp_file_name(mimeinfo);
	if (procmime_get_part(imgfile, file, mimeinfo) < 0) {
		g_warning("Can't get mimepart file");	
		g_free(imgfile);
		return;
	}

	im = gdk_imlib_load_image(imgfile);
	unlink(imgfile);
	g_free(imgfile);
	if (!im) {
		g_warning("Can't load the image.");	
		return;
	}

	if (imageviewerprefs.resize_img) {
		avail_width = imageviewer->scrolledwin->parent->allocation.width;
		avail_height = imageviewer->scrolledwin->parent->allocation.height;
		if (avail_width > 8) avail_width -= 8;
		if (avail_height > 8) avail_height -= 8;

		image_viewer_get_resized_size(im->rgb_width, im->rgb_height,
				 avail_width, avail_height,
				 &new_width, &new_height);
	} else {
		new_width = im->rgb_width;
		new_height = im->rgb_height;
	}

	gdk_imlib_render(im, new_width, new_height);

	if (!imageviewer->image) {
		imageviewer->image = gtk_pixmap_new(gdk_imlib_move_image(im),
						    gdk_imlib_move_mask(im));

		gtk_scrolled_window_add_with_viewport
			(GTK_SCROLLED_WINDOW(imageviewer->scrolledwin),
			 imageviewer->image);
	} else
		gtk_pixmap_set(GTK_PIXMAP(imageviewer->image),
			       gdk_imlib_move_image(im),
			       gdk_imlib_move_mask(im));      

	gtk_widget_show(imageviewer->image);

	gdk_imlib_destroy_image(im);
}
#endif /* HAVE_GDK_IMLIB */
#endif /* HAVE_GDK_PIXBUF */

static void image_viewer_clear_viewer(MimeViewer *_mimeviewer)
{
	ImageViewer *imageviewer = (ImageViewer *) _mimeviewer;
	GtkAdjustment *hadj, *vadj;

	debug_print("image_viewer_clear_viewer\n");

	if (imageviewer->image)
		gtk_pixmap_set(GTK_PIXMAP(imageviewer->image), NULL, NULL);
	hadj = gtk_scrolled_window_get_hadjustment
		(GTK_SCROLLED_WINDOW(imageviewer->scrolledwin));
	gtk_adjustment_set_value(hadj, 0.0);
	vadj = gtk_scrolled_window_get_vadjustment
		(GTK_SCROLLED_WINDOW(imageviewer->scrolledwin));
	gtk_adjustment_set_value(vadj, 0.0);
}

static void image_viewer_destroy_viewer(MimeViewer *_mimeviewer)
{
	ImageViewer *imageviewer = (ImageViewer *) _mimeviewer;

	debug_print("image_viewer_destroy_viewer\n");

	image_viewer_clear_viewer(_mimeviewer);
	gtk_widget_unref(imageviewer->scrolledwin);
	g_free(imageviewer);
}

static void image_viewer_get_resized_size(gint w, gint h, gint aw, gint ah,
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

MimeViewer *image_viewer_create(void)
{
	ImageViewer *imageviewer;
	GtkWidget *scrolledwin;

	debug_print("Creating image view...\n");
	imageviewer = g_new0(ImageViewer, 1);
	imageviewer->mimeviewer.factory = &image_viewer_factory;

	imageviewer->mimeviewer.get_widget = image_viewer_get_widget;
	imageviewer->mimeviewer.show_mimepart = image_viewer_show_mimepart;
	imageviewer->mimeviewer.clear_viewer = image_viewer_clear_viewer;
	imageviewer->mimeviewer.destroy_viewer = image_viewer_destroy_viewer;

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_widget_ref(scrolledwin);

	gtk_widget_show_all(scrolledwin);

	imageviewer->scrolledwin  = scrolledwin;
	imageviewer->image        = NULL;

	return (MimeViewer *) imageviewer;
}

MimeViewerFactory image_viewer_factory =
{
	"image/*",
	0,
	
	image_viewer_create,
};

void viewer_init()
{
	mimeview_register_viewer_factory(&image_viewer_factory);
}

void viewer_done()
{
	mimeview_unregister_viewer_factory(&image_viewer_factory);
}
