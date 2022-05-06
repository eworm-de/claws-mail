/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2020 the Claws Mail Team
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
 * 
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "common/utils.h"
#include "prefs.h"
#include "prefs_gtk.h"
#include "prefswindow.h"

#include "prefs_common.h"

typedef struct _ImageViewerPage
{
	PrefsPage page;
	
	GtkWidget *window;		/* do not modify */

	GtkWidget *autoload_img;
	GtkWidget *resize_img;
	GtkWidget *fit_img_height_radiobtn;
	GtkWidget *fit_img_width_radiobtn;
	GtkWidget *inline_img;
	GtkWidget *print_imgs;
}ImageViewerPage;

static void imageviewer_create_widget_func(PrefsPage * _page,
					   GtkWindow * window,
					   gpointer data)
{
	ImageViewerPage *prefs_imageviewer = (ImageViewerPage *) _page;

	GtkWidget *table;
	GtkWidget *autoload_img;
	GtkWidget *resize_img;
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *fit_img_label;
	GtkWidget *fit_img_height_radiobtn;
	GtkWidget *fit_img_width_radiobtn;
	GtkWidget *inline_img;
	GtkWidget *print_imgs;

	table = gtk_grid_new();
	gtk_grid_set_column_homogeneous(GTK_GRID(table), FALSE);
	gtk_widget_show(table);
	gtk_container_set_border_width(GTK_CONTAINER(table), VBOX_BORDER);
	gtk_grid_set_row_spacing(GTK_GRID(table), 4);
	gtk_grid_set_column_spacing(GTK_GRID(table), 8);

	autoload_img = gtk_check_button_new_with_label(_("Automatically display attached images"));
	gtk_widget_show(autoload_img);
	gtk_grid_attach(GTK_GRID(table), autoload_img, 0, 0, 1, 1);

	resize_img = gtk_check_button_new_with_label(_("Resize attached images by default"));
	gtk_widget_show(resize_img);
	CLAWS_SET_TIP(resize_img,
			     _("Clicking image toggles scaling"));
	gtk_grid_attach(GTK_GRID(table), resize_img, 0, 1, 1, 1);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING);
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	gtk_widget_show(vbox);
	gtk_widget_show(hbox);

	fit_img_label = gtk_label_new (_("Fit image"));
	gtk_widget_set_halign(fit_img_label, GTK_ALIGN_END);
	gtk_widget_show(fit_img_label);
	CLAWS_SET_TIP(fit_img_label,
			     _("Right-clicking image toggles fitting height/width"));
	gtk_box_pack_start(GTK_BOX(hbox), fit_img_label, FALSE, FALSE, 0);

	fit_img_height_radiobtn = gtk_radio_button_new_with_label(NULL, _("Height"));
	gtk_widget_set_halign(fit_img_height_radiobtn, GTK_ALIGN_START);
	gtk_widget_show(fit_img_height_radiobtn);
	gtk_box_pack_start(GTK_BOX(hbox), fit_img_height_radiobtn, FALSE, FALSE, 0);

	fit_img_width_radiobtn = gtk_radio_button_new_with_label_from_widget(
					   GTK_RADIO_BUTTON(fit_img_height_radiobtn), _("Width"));
	gtk_widget_show(fit_img_width_radiobtn);
	gtk_box_pack_start(GTK_BOX(hbox), fit_img_width_radiobtn, FALSE, FALSE, 0);
	gtk_grid_attach(GTK_GRID(table), vbox, 0, 2, 1, 1);

	inline_img = gtk_check_button_new_with_label(_("Display images inline"));
	gtk_widget_show(inline_img);
	gtk_grid_attach(GTK_GRID(table), inline_img, 0, 3, 1, 1);
	
	print_imgs = gtk_check_button_new_with_label(_("Print images"));
	gtk_widget_show(print_imgs);
	gtk_grid_attach(GTK_GRID(table), print_imgs, 0, 4, 1, 1);
	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(resize_img), prefs_common.resize_img);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(autoload_img), prefs_common.display_img);
	if (prefs_common.fit_img_height)
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fit_img_height_radiobtn), TRUE);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fit_img_width_radiobtn), TRUE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(inline_img), prefs_common.inline_img);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(print_imgs), prefs_common.print_imgs);
	
	SET_TOGGLE_SENSITIVITY(resize_img, vbox);

	prefs_imageviewer->window	= GTK_WIDGET(window);
	prefs_imageviewer->autoload_img = autoload_img;
	prefs_imageviewer->resize_img 	= resize_img;
	prefs_imageviewer->fit_img_height_radiobtn 	= fit_img_height_radiobtn;
	prefs_imageviewer->fit_img_width_radiobtn 	= fit_img_width_radiobtn;
	prefs_imageviewer->inline_img 	= inline_img;
	prefs_imageviewer->print_imgs 	= print_imgs;

	prefs_imageviewer->page.widget = table;
}

static void imageviewer_destroy_widget_func(PrefsPage *_page)
{
}

static void imageviewer_save_func(PrefsPage * _page)
{
	ImageViewerPage *imageviewer = (ImageViewerPage *) _page;
	
	prefs_common.display_img =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
					 (imageviewer->autoload_img));
	prefs_common.resize_img =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
	    				 (imageviewer->resize_img));
	prefs_common.fit_img_height =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
	    				(imageviewer->fit_img_height_radiobtn));
	prefs_common.inline_img =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
	    				 (imageviewer->inline_img));
	prefs_common.print_imgs =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
	    				 (imageviewer->print_imgs));
}

ImageViewerPage *prefs_imageviewer;

void prefs_image_viewer_init(void)
{
	ImageViewerPage *page;
	static gchar *path[3];

	path[0] = _("Message View");
	path[1] = _("Image Viewer");
	path[2] = NULL;

	page = g_new0(ImageViewerPage, 1);
	page->page.path = path;
	page->page.create_widget = imageviewer_create_widget_func;
	page->page.destroy_widget = imageviewer_destroy_widget_func;
	page->page.save_page = imageviewer_save_func;
	page->page.weight = 160.0;
	prefs_gtk_register_page((PrefsPage *) page);
	prefs_imageviewer = page;
}

void prefs_image_viewer_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) prefs_imageviewer);
	g_free(prefs_imageviewer);
}
