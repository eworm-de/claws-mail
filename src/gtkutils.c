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
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkctree.h>
#include <gtk/gtkcombo.h>
#include <gtk/gtkthemes.h>
#include <gtk/gtkbindings.h>
#include <stdarg.h>

#include "intl.h"
#include "gtkutils.h"
#include "utils.h"
#include "gtksctree.h"
#include "codeconv.h"

gint gtkut_get_font_width(GdkFont *font)
{
	gchar *str;
	gint width;

	if (conv_get_current_charset() == C_UTF_8)
		str = "Abcdef";
	else
		str = _("Abcdef");

	width = gdk_string_width(font, str);
	width /= strlen(str);

	return width;
}

gint gtkut_get_font_height(GdkFont *font)
{
	gchar *str;
	gint height;

	if (conv_get_current_charset() == C_UTF_8)
		str = "Abcdef";
	else
		str = _("Abcdef");

	height = gdk_string_height(font, str);

	return height;
}

void gtkut_convert_int_to_gdk_color(gint rgbvalue, GdkColor *color)
{
	g_return_if_fail(color != NULL);

	color->pixel = 0L;
	color->red   = (int) (((gdouble)((rgbvalue & 0xff0000) >> 16) / 255.0) * 65535.0);
	color->green = (int) (((gdouble)((rgbvalue & 0x00ff00) >>  8) / 255.0) * 65535.0);
	color->blue  = (int) (((gdouble) (rgbvalue & 0x0000ff)        / 255.0) * 65535.0);
}

void gtkut_button_set_create(GtkWidget **bbox,
			     GtkWidget **button1, const gchar *label1,
			     GtkWidget **button2, const gchar *label2,
			     GtkWidget **button3, const gchar *label3)
{
	g_return_if_fail(bbox != NULL);
	g_return_if_fail(button1 != NULL);

	*bbox = gtk_hbutton_box_new();
	gtk_button_box_set_layout(GTK_BUTTON_BOX(*bbox), GTK_BUTTONBOX_END);
	gtk_button_box_set_spacing(GTK_BUTTON_BOX(*bbox), 5);

	*button1 = gtk_button_new_with_label(label1);
	GTK_WIDGET_SET_FLAGS(*button1, GTK_CAN_DEFAULT);
	gtk_box_pack_start(GTK_BOX(*bbox), *button1, TRUE, TRUE, 0);
	gtk_widget_show(*button1);

	if (button2) {
		*button2 = gtk_button_new_with_label(label2);
		GTK_WIDGET_SET_FLAGS(*button2, GTK_CAN_DEFAULT);
		gtk_box_pack_start(GTK_BOX(*bbox), *button2, TRUE, TRUE, 0);
		gtk_widget_show(*button2);
	}

	if (button3) {
		*button3 = gtk_button_new_with_label(label3);
		GTK_WIDGET_SET_FLAGS(*button3, GTK_CAN_DEFAULT);
		gtk_box_pack_start(GTK_BOX(*bbox), *button3, TRUE, TRUE, 0);
		gtk_widget_show(*button3);
	}
}

#define CELL_SPACING 1
#define ROW_TOP_YPIXEL(clist, row) (((clist)->row_height * (row)) + \
				    (((row) + 1) * CELL_SPACING) + \
				    (clist)->voffset)
#define ROW_FROM_YPIXEL(clist, y) (((y) - (clist)->voffset) / \
				   ((clist)->row_height + CELL_SPACING))

void gtkut_ctree_node_move_if_on_the_edge(GtkCTree *ctree, GtkCTreeNode *node)
{
	GtkCList *clist = GTK_CLIST(ctree);
	gint row;
	GtkVisibility row_visibility, prev_row_visibility, next_row_visibility;

	g_return_if_fail(ctree != NULL);
	g_return_if_fail(node != NULL);

	row = g_list_position(clist->row_list, (GList *)node);
	if (row < 0 || row >= clist->rows || clist->row_height == 0) return;
	row_visibility = gtk_clist_row_is_visible(clist, row);
	prev_row_visibility = gtk_clist_row_is_visible(clist, row - 1);
	next_row_visibility = gtk_clist_row_is_visible(clist, row + 1);

	if (row_visibility == GTK_VISIBILITY_NONE) {
		gtk_clist_moveto(clist, row, -1, 0.5, 0);
		return;
	}
	if (row_visibility == GTK_VISIBILITY_FULL &&
	    prev_row_visibility == GTK_VISIBILITY_FULL &&
	    next_row_visibility == GTK_VISIBILITY_FULL)
		return;
	if (prev_row_visibility != GTK_VISIBILITY_FULL &&
	    next_row_visibility != GTK_VISIBILITY_FULL)
		return;

	if (prev_row_visibility != GTK_VISIBILITY_FULL) {
		gtk_clist_moveto(clist, row, -1, 0.2, 0);
		return;
	}
	if (next_row_visibility != GTK_VISIBILITY_FULL) {
		gtk_clist_moveto(clist, row, -1, 0.8, 0);
		return;
	}
}

#undef CELL_SPACING
#undef ROW_TOP_YPIXEL
#undef ROW_FROM_YPIXEL

gint gtkut_ctree_get_nth_from_node(GtkCTree *ctree, GtkCTreeNode *node)
{
	g_return_val_if_fail(ctree != NULL, -1);
	g_return_val_if_fail(node != NULL, -1);

	return g_list_position(GTK_CLIST(ctree)->row_list, (GList *)node);
}

void gtkut_ctree_set_focus_row(GtkCTree *ctree, GtkCTreeNode *node)
{
	gtkut_clist_set_focus_row(GTK_CLIST(ctree),
				  gtkut_ctree_get_nth_from_node(ctree, node));
}

void gtkut_clist_set_focus_row(GtkCList *clist, gint row)
{
	clist->focus_row = row;
	GTKUT_CTREE_REFRESH(clist);
}

void gtkut_combo_set_items(GtkCombo *combo, const gchar *str1, ...)
{
	va_list args;
	gchar *s;
	GList *combo_items = NULL;

	g_return_if_fail(str1 != NULL);

	combo_items = g_list_append(combo_items, (gpointer)str1);
	va_start(args, str1);
	s = va_arg(args, gchar*);
	while (s) {
		combo_items = g_list_append(combo_items, (gpointer)s);
		s = va_arg(args, gchar*);
	}
	va_end(args);

	gtk_combo_set_popdown_strings(combo, combo_items);

	g_list_free(combo_items);
}

void gtkut_widget_disable_theme_engine(GtkWidget *widget)
{
	GtkStyle *style, *new_style;

	style = gtk_widget_get_style(widget);

	if (style->engine) {
		GtkThemeEngine *engine;

		engine = style->engine;
		style->engine = NULL;
		new_style = gtk_style_copy(style);
		style->engine = engine;
		gtk_widget_set_style(widget, new_style);
	}
}

static void gtkut_widget_draw_cb(GtkWidget *widget, GdkRectangle *area,
				 gboolean *flag)
{
	*flag = TRUE;
	gtk_signal_disconnect_by_data(GTK_OBJECT(widget), flag);
}

void gtkut_widget_wait_for_draw(GtkWidget *widget)
{
	gboolean flag = FALSE;

	if (!GTK_WIDGET_VISIBLE(widget)) return;

	gtk_signal_connect(GTK_OBJECT(widget), "draw",
			   GTK_SIGNAL_FUNC(gtkut_widget_draw_cb), &flag);
	while (!flag)
		gtk_main_iteration();
}

void gtkut_widget_get_uposition(GtkWidget *widget, gint *px, gint *py)
{
	gint x, y;
	gint sx, sy;

	g_return_if_fail(widget != NULL);
	g_return_if_fail(widget->window != NULL);

	/* gdk_window_get_root_origin ever return *rootwindow*'s position*/
	gdk_window_get_root_origin(widget->window, &x, &y);

	sx = gdk_screen_width();
	sy = gdk_screen_height();
	x %= sx; if (x < 0) x += sx;
	y %= sy; if (y < 0) y += sy;
	*px = x;
	*py = y;
}

static void gtkut_clist_bindings_add(GtkWidget *clist)
{
	GtkBindingSet *binding_set;

	binding_set = gtk_binding_set_by_class
		(GTK_CLIST_CLASS(GTK_OBJECT(clist)->klass));

	gtk_binding_entry_add_signal(binding_set, GDK_n, GDK_CONTROL_MASK,
				     "scroll_vertical", 2,
				     GTK_TYPE_ENUM, GTK_SCROLL_STEP_FORWARD,
				     GTK_TYPE_FLOAT, 0.0);
	gtk_binding_entry_add_signal(binding_set, GDK_p, GDK_CONTROL_MASK,
				     "scroll_vertical", 2,
				     GTK_TYPE_ENUM, GTK_SCROLL_STEP_BACKWARD,
				     GTK_TYPE_FLOAT, 0.0);
}

void gtkut_widget_init(void)
{
	GtkWidget *clist;

	clist = gtk_clist_new(1);
	gtkut_clist_bindings_add(clist);
	gtk_widget_unref(clist);

	clist = gtk_ctree_new(1, 0);
	gtkut_clist_bindings_add(clist);
	gtk_widget_unref(clist);

	clist = gtk_sctree_new_with_titles(1, 0, NULL);
	gtkut_clist_bindings_add(clist);
	gtk_widget_unref(clist);
}
