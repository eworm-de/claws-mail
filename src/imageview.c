/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999,2000 Hiroyuki Yamamoto
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
#include "mainwindow.h"
#include "prefs_common.h"
#include "procmime.h"
#include "imageview.h"
#include "utils.h"

ImageView *imageview_create(void)
{
	ImageView *imageview;
	GtkWidget *scrolledwin;

	debug_print(_("Creating image view...\n"));
	imageview = g_new0(ImageView, 1);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_widget_set_usize(scrolledwin, prefs_common.mainview_width, -1);

	gtk_widget_show_all(scrolledwin);

	imageview->scrolledwin  = scrolledwin;
	imageview->image        = NULL;

	return imageview;
}

void imageview_init(ImageView *imageview)
{
}

#if HAVE_GDK_PIXBUF
void imageview_show_image(ImageView *imageview, MimeInfo *mimeinfo,
			  const gchar *file)
{
	GdkPixbuf *pixbuf;
	GdkPixmap *pixmap;
	GdkBitmap *mask;

	pixbuf = gdk_pixbuf_new_from_file(file);

	if (!pixbuf) {
		g_warning(_("Can't load the image."));	
		if (imageview->image)
			gtk_widget_hide(imageview->image);
		return;
	}

	if (imageview->messageview->mainwin)
		main_window_cursor_wait(imageview->messageview->mainwin);

	gdk_pixbuf_render_pixmap_and_mask(pixbuf, &pixmap, &mask, 0);

	if (!imageview->image) {
		imageview->image = gtk_pixmap_new(pixmap, mask);

		gtk_scrolled_window_add_with_viewport
			(GTK_SCROLLED_WINDOW(imageview->scrolledwin),
			 imageview->image);
	} else
		gtk_pixmap_set(GTK_PIXMAP(imageview->image), pixmap, mask);

	gtk_widget_show(imageview->image);

	gdk_pixbuf_unref(pixbuf);

	if (imageview->messageview->mainwin)
		main_window_cursor_normal(imageview->messageview->mainwin);
}
#else
#if HAVE_GDK_IMLIB
void imageview_show_image(ImageView *imageview, MimeInfo *mimeinfo,
			  const gchar *file)
{
	GdkImlibImage *im;

	im = gdk_imlib_load_image((gchar *)file);

	if (!im) {
		g_warning(_("Can't load the image."));	
		if (imageview->image)
			gtk_widget_hide(imageview->image);
		return;
	}

	if (imageview->messageview->mainwin)
		main_window_cursor_wait(imageview->messageview->mainwin);

	gdk_imlib_render(im, im->rgb_width, im->rgb_height);

	if (!imageview->image) {
		imageview->image = gtk_pixmap_new(gdk_imlib_move_image(im),
						  gdk_imlib_move_mask(im));

		gtk_scrolled_window_add_with_viewport
			(GTK_SCROLLED_WINDOW(imageview->scrolledwin),
			 imageview->image);
	} else
		gtk_pixmap_set(GTK_PIXMAP(imageview->image),
			       gdk_imlib_move_image(im),
			       gdk_imlib_move_mask(im));      

	gtk_widget_show(imageview->image);

	gdk_imlib_destroy_image(im);

	if (imageview->messageview->mainwin)
		main_window_cursor_normal(imageview->messageview->mainwin);
}
#else
void imageview_show_image(ImageView *imageview, MimeInfo *mimeinfo,
			  const gchar *file)
{
}
#endif /* HAVE_GDK_IMLIB */
#endif /* HAVE_GDK_PIXBUF */

void imageview_clear(ImageView *imageview)
{
	GtkAdjustment *hadj, *vadj;

	if (imageview->image)
		gtk_pixmap_set(GTK_PIXMAP(imageview->image), NULL, NULL);
	hadj = gtk_scrolled_window_get_hadjustment
		(GTK_SCROLLED_WINDOW(imageview->scrolledwin));
	gtk_adjustment_set_value(hadj, 0.0);
	vadj = gtk_scrolled_window_get_vadjustment
		(GTK_SCROLLED_WINDOW(imageview->scrolledwin));
	gtk_adjustment_set_value(vadj, 0.0);
}

void imageview_destroy(ImageView *imageview)
{
	g_free(imageview);
}
