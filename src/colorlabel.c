/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2001 Hiroyuki Yamamoto & The Sylpheed Claws Team
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

/* (alfons) - based on a contribution by Satoshi Nagayasu; revised for colorful 
 * menu and more Sylpheed integration. The idea to put the code in a separate
 * file is just that it make it easier to allow "user changeable" label colors.
 */

#include "defs.h"

#include <glib.h>
#include <gdk/gdkx.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkpixmap.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkwindow.h>

#include "intl.h"
#include "colorlabel.h"
#include "gtkutils.h"
#include "utils.h"

static gchar *labels[] = {
	N_("Orange"),
	N_("Red") ,
	N_("Pink"),
	N_("Sky blue"),
	N_("Blue"),
	N_("Green"),
	N_("Brown")
};

typedef enum LabelColorChangeFlags_ {
	LCCF_COLOR = 1 << 0,
	LCCF_LABEL = 1 << 1,
	LCCF_ALL   = LCCF_COLOR | LCCF_LABEL
} LabelColorChangeFlags;

/* XXX: if you add colors, make sure you also check the procmsg.h.
 * color indices are stored as 3 bits; that explains the max. of 7 colors */
static struct 
{
	LabelColorChangeFlags	changed; 
	GdkColor		color;

	/* XXX: note that the label member is supposed to be dynamically 
	 * allocated and fffreed */
	gchar			*label;
	GtkPixmap		*pixmap;
} label_colors[] = {
	{ LCCF_ALL, { 0, 0xffff, (0x99 << 8), 0x0 },		NULL, NULL },
	{ LCCF_ALL, { 0, 0xffff, 0, 0 },			NULL, NULL },
	{ LCCF_ALL, { 0, 0xffff, (0x66 << 8), 0xffff },		NULL, NULL },
	{ LCCF_ALL, { 0, 0x0, (0xcc << 8), 0xffff },		NULL, NULL },
	{ LCCF_ALL, { 0, 0x0, 0x0, 0xffff },			NULL, NULL },
	{ LCCF_ALL, { 0, 0x0, 0x99 << 8, 0x0 },			NULL, NULL },
	{ LCCF_ALL, { 0, 0x66 << 8, 0x33 << 8, 0x33 << 8 },	NULL, NULL }
};

#define LABEL_COLORS_ELEMS (sizeof label_colors / sizeof label_colors[0])

#define G_RETURN_VAL_IF_INVALID_COLOR(color, val) \
	g_return_val_if_fail((color) >= 0 && (color) < LABEL_COLORS_ELEMS, (val))

static void colorlabel_recreate        (gint);
static void colorlabel_recreate_label  (gint);

gint colorlabel_get_color_count(void)
{
	return LABEL_COLORS_ELEMS;
}

GdkColor colorlabel_get_color(gint color_index)
{
	GdkColor invalid = { 0 };

	G_RETURN_VAL_IF_INVALID_COLOR(color_index, invalid);

	return label_colors[color_index].color;
}

gchar *colorlabel_get_color_text(gint color_index)
{
	G_RETURN_VAL_IF_INVALID_COLOR(color_index, NULL);

	colorlabel_recreate_label(color_index);
	return label_colors[color_index].label;
}

GtkPixmap *colorlabel_create_color_pixmap(GdkColor color)
{
	const gchar *FMT = "+      c #%2.2X%2.2X%2.2X";
	gchar buf[40];

	/* black frame of 1 pixel */
	gchar *dummy_xpm[] = {
		"16 16 3 1",
		"       c None",
		".      c #000000",
		"+      c #000000",
		"................",
		".++++++++++++++.",
		".++++++++++++++.",
		".++++++++++++++.",
		".++++++++++++++.",
		".++++++++++++++.",
		".++++++++++++++.",
		".++++++++++++++.",
		".++++++++++++++.",
		".++++++++++++++.",
		".++++++++++++++.",
		".++++++++++++++.",
		".++++++++++++++.",
		".++++++++++++++.",
		".++++++++++++++.",
		"................"
	};

	GdkBitmap *xpmmask;
	GdkPixmap *xpm;
	GtkPixmap *pixmap;

	/* put correct color in xpm data */
	sprintf(buf, FMT, color.red >> 8, color.green >> 8, color.blue >> 8);
	dummy_xpm[3] = buf;				

	/* XXX: passing NULL as GdkWindow* seems to be possible */
	xpm = gdk_pixmap_create_from_xpm_d
		(GDK_ROOT_PARENT(), &xpmmask, NULL, (gchar **) &dummy_xpm);
	if (xpm == NULL)
		debug_print("*** NO XPM\n");
	pixmap = GTK_PIXMAP(gtk_pixmap_new(xpm, xpmmask)); 

	g_return_val_if_fail(pixmap, NULL);

	gdk_pixmap_unref(xpm);
	gdk_bitmap_unref(xpmmask);

	return pixmap;
}

/* XXX: this function to check if menus with colors and labels should
 * be recreated */
gboolean colorlabel_changed(void)
{
	gint n;

	for (n = 0; n < LABEL_COLORS_ELEMS; n++) {
		if (label_colors[n].changed) 
			return TRUE;
	}

	return FALSE;
}

/* XXX: colorlabel_recreate_XXX are there to make sure everything
 * is initialized ok, without having to call a global _xxx_init_
 * function */
static void colorlabel_recreate_color(gint color)
{
	GtkPixmap *pixmap;

	if (!(label_colors[color].changed & LCCF_COLOR))
		return;

	pixmap = GTK_PIXMAP(colorlabel_create_color_pixmap(label_colors[color].color));
	g_return_if_fail(pixmap);

	if (label_colors[color].pixmap)
		gtk_widget_destroy(GTK_WIDGET(label_colors[color].pixmap));

	label_colors[color].pixmap = pixmap;		
	label_colors[color].changed &= ~LCCF_COLOR;
}

static void colorlabel_recreate_label(gint color)
{
	if (!label_colors[color].changed & LCCF_LABEL)
		return;

	if (label_colors[color].label == NULL) 
		label_colors[color].label = g_strdup(gettext(labels[color]));

	label_colors[color].changed &= ~LCCF_LABEL;
}

/* XXX: call this function everytime when you're doing important
 * stuff with the label_colors[] array */
static void colorlabel_recreate(gint color)
{
	colorlabel_recreate_label(color);
	colorlabel_recreate_color(color);
}

static void colorlabel_recreate_all(void)
{
	gint n;

	for ( n = 0; n < LABEL_COLORS_ELEMS; n++) 
		colorlabel_recreate(n);
}

/* colorlabel_create_check_color_menu_item() - creates a color
 * menu item with a check box */
GtkWidget *colorlabel_create_check_color_menu_item(gint color_index)
{
	GtkWidget *label; 
	GtkWidget *hbox; 
	GtkWidget *align; 
	GtkWidget *item;

	G_RETURN_VAL_IF_INVALID_COLOR(color_index, NULL);

	item  = gtk_check_menu_item_new();

	colorlabel_recreate(color_index);

	/* XXX: gnome-core::panel::menu.c is a great example of
	 * how to create pixmap menus */
	label = gtk_label_new(label_colors[color_index].label);

	gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
	gtk_widget_show(label);
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox);
	gtk_container_add(GTK_CONTAINER(item), hbox);

	align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
	gtk_widget_show(align);
	gtk_container_set_border_width(GTK_CONTAINER(align), 1);

	gtk_container_add(GTK_CONTAINER(align),
			  GTK_WIDGET(label_colors[color_index].pixmap));
	gtk_widget_show(GTK_WIDGET(label_colors[color_index].pixmap));
	gtk_widget_set_usize(align, 16, 16);

	gtk_box_pack_start(GTK_BOX(hbox), align, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);

	return item;
}

/* colorlabel_create_color_menu() - creates a color menu without 
 * checkitems, probably for use in combo items */
GtkWidget *colorlabel_create_color_menu(void)
{
	GtkWidget *label; 
	GtkWidget *hbox; 
	GtkWidget *align; 
	GtkWidget *item;
	GtkWidget *menu;
	gint i;

	colorlabel_recreate_all();

	/* create the menu items. each item has its color code attached */
	menu = gtk_menu_new();
	gtk_object_set_data(GTK_OBJECT(menu), "label_color_menu", menu);

	item = gtk_menu_item_new_with_label(_("None"));
	gtk_menu_append(GTK_MENU(menu), item);
	gtk_object_set_data(GTK_OBJECT(item), "color", GUINT_TO_POINTER(0));
	gtk_widget_show(item);
	
	/* and the color items */
	for (i = 0; i < LABEL_COLORS_ELEMS; i++) {
		GtkPixmap *pixmap = colorlabel_create_color_pixmap(label_colors[i].color);

		item  = gtk_menu_item_new();
		gtk_object_set_data(GTK_OBJECT(item), "color", GUINT_TO_POINTER(i + 1));

		label = gtk_label_new(label_colors[i].label);
		
		gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);
		gtk_widget_show(label);
		hbox = gtk_hbox_new(FALSE, 0);
		gtk_widget_show(hbox);
		gtk_container_add(GTK_CONTAINER(item), hbox);

		align = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
		gtk_widget_show(align);
		gtk_container_set_border_width(GTK_CONTAINER(align), 1);

		gtk_container_add(GTK_CONTAINER(align), GTK_WIDGET(pixmap));
		gtk_widget_show(GTK_WIDGET(pixmap));
		gtk_widget_set_usize(align, 16, 16);

		gtk_box_pack_start(GTK_BOX(hbox), align, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);
		
		gtk_menu_append(GTK_MENU(menu), item);
		gtk_widget_show(item);
	}

	gtk_widget_show(menu);

	return menu;
}

guint colorlabel_get_color_menu_active_item(GtkWidget *menu)
{
	GtkWidget *menuitem;
	guint      color;

	g_return_val_if_fail
		(gtk_object_get_data(GTK_OBJECT(menu), "label_color_menu"), 0);
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	color = GPOINTER_TO_UINT
		(gtk_object_get_data(GTK_OBJECT(menuitem), "color"));
	return color;
}


