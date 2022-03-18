/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2001-2018 Hiroyuki Yamamoto & The Claws Mail Team
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

/* (alfons) - based on a contribution by Satoshi Nagayasu; revised for colorful 
 * menu and more Sylpheed integration. The idea to put the code in a separate
 * file is just that it make it easier to allow "user changeable" label colors.
 */

#include "config.h"
#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "colorlabel.h"
#include "utils.h"
#include "gtkutils.h"
#include "prefs_common.h"

static gchar *labels[COLORLABELS] = {
	N_("Orange"),
	N_("Red") ,
	N_("Pink"),
	N_("Sky blue"),
	N_("Blue"),
	N_("Green"),
	N_("Brown"),
	N_("Grey"),
	N_("Light brown"),
	N_("Dark red"),
	N_("Dark pink"),
	N_("Steel blue"),
	N_("Gold"),
	N_("Bright green"),
	N_("Magenta")
};

#define CL(x) ((gdouble)x / 65535)
static GdkRGBA default_colors[COLORLABELS] = {
	{ CL(65535), CL(39168), CL(0),     1.0 },
	{ CL(65535), CL(0),     CL(0),     1.0 },
	{ CL(65535), CL(26112), CL(65535), 1.0 },
	{ CL(0),     CL(52224), CL(65535), 1.0 },
	{ CL(0),     CL(0),     CL(65535), 1.0 },
	{ CL(0),     CL(39168), CL(0),     1.0 },
	{ CL(26112), CL(13056), CL(13056), 1.0 },
	{ CL(43520), CL(43520), CL(43520), 1.0 },
	{ CL(49152), CL(29184), CL(21504), 1.0 },
	{ CL(49152), CL(0),     CL(0),     1.0 },
	{ CL(52224), CL(4096),  CL(29696), 1.0 },
	{ CL(20480), CL(37888), CL(52480), 1.0 },
	{ CL(65535), CL(54528), CL(0),     1.0 },
	{ CL(0),     CL(55296), CL(0),     1.0 },
	{ CL(49152), CL(24576), CL(49152), 1.0 }
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
	/* color here is initialized from default_colors[] at startup */
	GdkRGBA		color;

	/* XXX: note that the label member is supposed to be dynamically 
	 * allocated and freed */
	gchar			*label;
	GtkWidget		*widget;
} label_colors[NUM_MENUS][COLORLABELS] = {
    {
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL }},
    {
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL },
	{ LCCF_ALL, { 0 }, NULL, NULL }}
};

#define LABEL_COLOR_WIDTH	28
#define LABEL_COLOR_HEIGHT	16

#define LABEL_COLORS_ELEMS (sizeof label_colors[0] / sizeof label_colors[0][0])

#define G_RETURN_VAL_IF_INVALID_COLOR(color, val) \
	do if ((color) < 0 || (color) >= LABEL_COLORS_ELEMS) {	\
		return val;				    	\
	} while(0)

static void colorlabel_recreate        (gint);
static void colorlabel_recreate_label  (gint);

void colorlabel_update_colortable_from_prefs(void)
{
	gint i, c;

	for (i = 0; i < NUM_MENUS; i++) {
		for (c = 0; c < COLORLABELS; c++) {
			label_colors[i][c].color = prefs_common.custom_colorlabel[c].color;
			g_free(label_colors[i][c].label);
			label_colors[i][c].label =
					g_strdup(prefs_common.custom_colorlabel[c].label);
		}
	}
}


gint colorlabel_get_color_count(void)
{
	return LABEL_COLORS_ELEMS;
}

GdkRGBA colorlabel_get_color(gint color_index)
{
	GdkRGBA invalid = { 0 };

	G_RETURN_VAL_IF_INVALID_COLOR(color_index, invalid);

	return label_colors[0][color_index].color;
}

GdkRGBA colorlabel_get_default_color(gint color_index)
{
	GdkRGBA invalid = { 0 };

	G_RETURN_VAL_IF_INVALID_COLOR(color_index, invalid);

	return default_colors[color_index];
}
		
gchar *colorlabel_get_color_default_text(gint color_index)
{
	G_RETURN_VAL_IF_INVALID_COLOR(color_index, NULL);

	return labels[color_index];
}

static gboolean colorlabel_drawing_area_expose_event_cb
	(GtkWidget *widget, cairo_t *cr, gpointer data)
{
	GdkRGBA *color = (GdkRGBA *)data;
	GtkAllocation allocation;

	gtk_widget_get_allocation(widget, &allocation);

	cairo_set_source_rgb(cr, 0., 0., 0.);
	cairo_rectangle(cr, 0, 0,
	    allocation.width - 1,
	    allocation.height - 1);
	cairo_stroke(cr);

	gdk_cairo_set_source_rgba(cr, color);
	cairo_rectangle(cr, 1, 1,
	    allocation.width - 2,
	    allocation.height - 2);
	cairo_fill(cr);
	
	return FALSE;
}

static GtkWidget *colorlabel_create_color_widget(GdkRGBA *color)
{
	GtkWidget *widget;

	widget = gtk_drawing_area_new();
	gtk_widget_set_size_request(widget, LABEL_COLOR_WIDTH - 2, 
				    LABEL_COLOR_HEIGHT - 4);

	g_signal_connect(G_OBJECT(widget), "draw", 
			G_CALLBACK(colorlabel_drawing_area_expose_event_cb),
			color);

	return widget;
}

static GdkPixbuf *colorlabel_create_colormenu_pixbuf(GdkRGBA *color)
{
	GdkPixbuf *pixbuf;
	guint32 pixel = 0;

	cm_return_val_if_fail(color != NULL, NULL);

	pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
			LABEL_COLOR_WIDTH - 2, LABEL_COLOR_HEIGHT - 4);

	/* "pixel" needs to be set to 0xrrggbb00 */
	pixel += (guint32)(color->red   * 255) << 24;
	pixel += (guint32)(color->green * 255) << 16;
	pixel += (guint32)(color->blue  * 255) <<  8;
	gdk_pixbuf_fill(pixbuf, pixel);

	return pixbuf;
}

/* XXX: colorlabel_recreate_XXX are there to make sure everything
 * is initialized ok, without having to call a global _xxx_init_
 * function */
static void colorlabel_recreate_color(gint color)
{
	GtkWidget *widget;
	int i;
	
	for (i = 0; i < NUM_MENUS; i++) {
		if (!(label_colors[i][color].changed & LCCF_COLOR))
			continue;

		widget = colorlabel_create_color_widget(&label_colors[i][color].color);
		cm_return_if_fail(widget);

		if (label_colors[i][color].widget) 
			gtk_widget_destroy(label_colors[i][color].widget);

		label_colors[i][color].widget = widget;		
		label_colors[i][color].changed &= ~LCCF_COLOR;
	}
}

static void colorlabel_recreate_label(gint color)
{
	int i;
	
	for (i = 0; i < NUM_MENUS; i++) {
		if (!(label_colors[i][color].changed & LCCF_LABEL))
			continue;

		if (label_colors[i][color].label == NULL) 
			label_colors[i][color].label = g_strdup(gettext(labels[color]));

		label_colors[i][color].changed &= ~LCCF_LABEL;
	}
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
GtkWidget *colorlabel_create_check_color_menu_item(gint color_index, gboolean force, gint menu_index)
{
	GtkWidget *label; 
	GtkWidget *hbox; 
	GtkWidget *vbox; 
	GtkWidget *item;
	gchar *accel;
	
	G_RETURN_VAL_IF_INVALID_COLOR(color_index, NULL);

	item = gtk_check_menu_item_new();

	if (force) {
		label_colors[menu_index][color_index].changed |= LCCF_COLOR;
		label_colors[menu_index][color_index].changed |= LCCF_LABEL;
	}
	colorlabel_recreate(color_index);

	/* XXX: gnome-core::panel::menu.c is a great example of
	 * how to create pixmap menus */
	label = gtk_label_new(label_colors[menu_index][color_index].label);

	gtk_widget_show(label);
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show(hbox);
	gtk_container_add(GTK_CONTAINER(item), hbox);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show(vbox);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 1);

	gtk_container_add(GTK_CONTAINER(vbox),
			  label_colors[menu_index][color_index].widget);
	gtk_widget_show(label_colors[menu_index][color_index].widget);

	gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);
	if (color_index < 9) {
		accel = gtk_accelerator_get_label(GDK_KEY_1+color_index, GDK_CONTROL_MASK);
		label = gtk_label_new(accel);
		gtk_widget_show(label);
		gtk_label_set_xalign(GTK_LABEL(label), 1.0);
		g_free(accel);
		gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);
		g_object_set_data(G_OBJECT(item), "accel_label", label);
	} else {
		label = gtk_label_new("");
		gtk_widget_show(label);
		gtk_label_set_xalign(GTK_LABEL(label), 1.0);
		gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 4);
		g_object_set_data(G_OBJECT(item), "accel_label", label);
	}
	return item;
}

/* colorlabel_create_color_menu() - creates a color menu without 
 * checkitems, probably for use in combo items */
GtkWidget *colorlabel_create_color_menu(void)
{
	GtkWidget *label; 
	GtkWidget *item;
	GtkWidget *menu;
	gint i;

	colorlabel_recreate_all();

	/* create the menu items. each item has its color code attached */
	menu = gtk_menu_new();
	g_object_set_data(G_OBJECT(menu), "label_color_menu", menu);

	item = gtk_menu_item_new_with_label(_("None"));
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	g_object_set_data(G_OBJECT(item), "color", GUINT_TO_POINTER(0));
	gtk_widget_show(item);

	item = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
	gtk_widget_show(item);

	/* and the color items */
	for (i = 0; i < LABEL_COLORS_ELEMS; i++) {
		GtkWidget *hbox; 
		GtkWidget *vbox;
		GtkWidget *widget;

		item  = gtk_menu_item_new();
		g_object_set_data(G_OBJECT(item), "color",
				  GUINT_TO_POINTER(i + 1));

		label = gtk_label_new(label_colors[0][i].label);
		
		gtk_widget_show(label);
		hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_widget_show(hbox);
		gtk_container_add(GTK_CONTAINER(item), hbox);

		vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
		gtk_widget_show(vbox);
		gtk_container_set_border_width(GTK_CONTAINER(vbox), 1);

		widget = colorlabel_create_color_widget(&label_colors[0][i].color);
		gtk_widget_show(widget);
		gtk_box_pack_start(GTK_BOX(vbox), widget, FALSE, FALSE, 0);

		gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 4);
		
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		gtk_widget_show(item);
	}
	
	gtk_widget_show(menu);

	return menu;
}

guint colorlabel_get_color_menu_active_item(GtkWidget *menu)
{
	GtkWidget *menuitem;
	guint color;

	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	color = GPOINTER_TO_UINT
		(g_object_get_data(G_OBJECT(menuitem), "color"));
	return color;
}

static gboolean colormenu_separator_func(GtkTreeModel *model,
		GtkTreeIter *iter, gpointer data)
{
	gchar *txt;

	gtk_tree_model_get(model, iter, COLORMENU_COL_TEXT, &txt, -1);

	if (txt == NULL)
		return TRUE;

	return FALSE;
}

GtkWidget *colorlabel_create_combobox_colormenu()
{
	GtkWidget *combobox;
	GtkListStore *store;
	GtkCellRenderer *renderer;

	store = gtk_list_store_new(3,
			GDK_TYPE_PIXBUF,
			G_TYPE_STRING,
			G_TYPE_INT);
	combobox = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));

	gtk_combo_box_set_row_separator_func(GTK_COMBO_BOX(combobox),
			(GtkTreeViewRowSeparatorFunc)colormenu_separator_func,
			NULL, NULL);

	renderer = gtk_cell_renderer_pixbuf_new();
	gtk_cell_renderer_set_padding(renderer, 2, 0);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combobox),
			renderer, FALSE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combobox),
			renderer,
			"pixbuf", COLORMENU_COL_PIXBUF,
			NULL);

	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combobox),
			renderer, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combobox),
			renderer,
			"text", COLORMENU_COL_TEXT,
			NULL);

	colorlabel_refill_combobox_colormenu(GTK_COMBO_BOX(combobox));

	return combobox;
}

void colorlabel_refill_combobox_colormenu(GtkComboBox *combobox)
{
	GtkListStore *store;
	GtkTreeIter iter;
	gint i;

	cm_return_if_fail(combobox != NULL);

	store = GTK_LIST_STORE(gtk_combo_box_get_model(combobox));

	cm_return_if_fail(store != NULL);

	gtk_list_store_clear(store);

	/* "None" */
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter,
			COLORMENU_COL_PIXBUF, NULL,
			COLORMENU_COL_TEXT, _("None"),
			COLORMENU_COL_ID, -1,
			-1);
	/* Separator */
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter,
			COLORMENU_COL_PIXBUF, NULL,
			COLORMENU_COL_TEXT, NULL,
			COLORMENU_COL_ID, -1,
			-1);

	/* Menu items for individual colors */
	for (i = 0; i < LABEL_COLORS_ELEMS; i++) {
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter,
				COLORMENU_COL_PIXBUF, colorlabel_create_colormenu_pixbuf(&label_colors[0][i].color),
				COLORMENU_COL_TEXT, label_colors[0][i].label,
				COLORMENU_COL_ID, i,
				-1);
	}
}

gint colorlabel_get_combobox_colormenu_active(GtkComboBox *combobox)
{
	gint value;
	GtkTreeIter iter;
	GtkTreeModel *model;

	cm_return_val_if_fail(combobox != NULL, 0);

	if (!gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combobox), &iter))
		return 0;

	model = gtk_combo_box_get_model(GTK_COMBO_BOX(combobox));
	gtk_tree_model_get(model, &iter,
			COLORMENU_COL_ID, &value,
			-1);

	return value + 1;
}

void colorlabel_set_combobox_colormenu_active(GtkComboBox *combobox,
		gint color)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gint id;

	cm_return_if_fail(combobox != NULL);

	model = gtk_combo_box_get_model(combobox);
	cm_return_if_fail(model != NULL);

	if (!gtk_tree_model_get_iter_first(model, &iter))
		return;

	do {
		gtk_tree_model_get(model, &iter,
				COLORMENU_COL_ID, &id,
				-1);

		if (id == color - 1)
			break;
	} while (gtk_tree_model_iter_next(model, &iter));

	gtk_combo_box_set_active_iter(combobox, &iter);
}
