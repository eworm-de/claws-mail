/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2024 the Claws Mail team and Hiroyuki Yamamoto
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
#include <gdk/gdkkeysyms.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include "gtk/gtksctree.h"
#include <stdlib.h>
#include <stdarg.h>
#include <sys/stat.h>

#include "combobox.h"

#if HAVE_LIBCOMPFACE
#  include <compface.h>
#endif

#if HAVE_LIBCOMPFACE
#define XPM_XFACE_HEIGHT	(HEIGHT + 3)  /* 3 = 1 header + 2 colors */
#endif

#if (HAVE_WCTYPE_H && HAVE_WCHAR_H)
#  include <wchar.h>
#  include <wctype.h>
#endif

#include "defs.h"
#include "gtkutils.h"
#include "utils.h"
#include "gtksctree.h"
#include "codeconv.h"
#include "stock_pixmap.h"
#include "menu.h"
#include "prefs_account.h"
#include "prefs_common.h"
#include "manage_window.h"
#include "manual.h"

gboolean gtkut_get_font_size(GtkWidget *widget,
			     gint *width, gint *height)
{
	PangoLayout *layout;
	const gchar *str = "Abcdef";

	cm_return_val_if_fail(GTK_IS_WIDGET(widget), FALSE);

	layout = gtk_widget_create_pango_layout(widget, str);
	cm_return_val_if_fail(layout, FALSE);
	pango_layout_get_pixel_size(layout, width, height);
	if (width)
		*width = *width / g_utf8_strlen(str, -1);
	g_object_unref(layout);

	return TRUE;
}

void gtkut_widget_set_small_font_size(GtkWidget *widget)
{
	PangoFontDescription *font_desc;
	gint size;

	cm_return_if_fail(widget != NULL);
	cm_return_if_fail(gtk_widget_get_style(widget) != NULL);

	if (prefs_common.derive_from_normal_font || !SMALL_FONT) {
		font_desc = pango_font_description_from_string(NORMAL_FONT);
		size = pango_font_description_get_size(font_desc);
		pango_font_description_set_size(font_desc, size * PANGO_SCALE_SMALL);
		gtk_widget_override_font(widget, font_desc);
		pango_font_description_free(font_desc);
	} else {
		font_desc = pango_font_description_from_string(SMALL_FONT);
		gtk_widget_override_font(widget, font_desc);
		pango_font_description_free(font_desc);
	}
}

void gtkut_stock_button_add_help(GtkWidget *bbox, GtkWidget **help_btn)
{
	cm_return_if_fail(bbox != NULL);

	*help_btn = gtkut_stock_button("help-browser", "Help");

	gtk_widget_set_can_default(*help_btn, TRUE);
	gtk_box_pack_end(GTK_BOX (bbox), *help_btn, TRUE, TRUE, 0);
	gtk_button_box_set_child_secondary(GTK_BUTTON_BOX (bbox),
			*help_btn, TRUE);
	gtk_widget_set_sensitive(*help_btn,
			manual_available(MANUAL_MANUAL_CLAWS));
	gtk_widget_show(*help_btn);
}

void gtkut_stock_button_set_create_with_help(GtkWidget **bbox,
		GtkWidget **help_button,
		GtkWidget **button1, const gchar *stock_icon1, const gchar *label1,
		GtkWidget **button2, const gchar *stock_icon2, const gchar *label2,
		GtkWidget **button3, const gchar *stock_icon3, const gchar *label3)
{
	cm_return_if_fail(bbox != NULL);
	cm_return_if_fail(button1 != NULL);

	gtkut_stock_button_set_create(bbox, button1, stock_icon1, label1,
			button2, stock_icon2, label2, button3, stock_icon3, label3);

	gtkut_stock_button_add_help(*bbox, help_button);
}

void gtkut_stock_button_set_create(GtkWidget **bbox,
				   GtkWidget **button1, const gchar *stock_icon1, const gchar *label1,
				   GtkWidget **button2, const gchar *stock_icon2, const gchar *label2,
				   GtkWidget **button3, const gchar *stock_icon3, const gchar *label3)
{
	cm_return_if_fail(bbox != NULL);
	cm_return_if_fail(button1 != NULL);

	*bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(*bbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(*bbox), 5);

	*button1 = gtk_button_new_with_mnemonic(label1);
	gtk_button_set_image(GTK_BUTTON(*button1),
		gtk_image_new_from_icon_name(stock_icon1, GTK_ICON_SIZE_BUTTON));
	gtk_widget_set_can_default(*button1, TRUE);
	gtk_box_pack_start(GTK_BOX(*bbox), *button1, TRUE, TRUE, 0);
	gtk_widget_show(*button1);

	if (button2) {
		*button2 = gtk_button_new_with_mnemonic(label2);
		gtk_button_set_image(GTK_BUTTON(*button2),
			gtk_image_new_from_icon_name(stock_icon2, GTK_ICON_SIZE_BUTTON));
		gtk_widget_set_can_default(*button2, TRUE);
		gtk_box_pack_start(GTK_BOX(*bbox), *button2, TRUE, TRUE, 0);
		gtk_widget_show(*button2);
	}

	if (button3) {
		*button3 = gtk_button_new_with_mnemonic(label3);
		gtk_button_set_image(GTK_BUTTON(*button3),
			gtk_image_new_from_icon_name(stock_icon3, GTK_ICON_SIZE_BUTTON));
		gtk_widget_set_can_default(*button3, TRUE);
		gtk_box_pack_start(GTK_BOX(*bbox), *button3, TRUE, TRUE, 0);
		gtk_widget_show(*button3);
	}
}

void gtkut_stock_with_text_button_set_create(GtkWidget **bbox,
				   GtkWidget **button1, const gchar *label1, const gchar *text1,
				   GtkWidget **button2, const gchar *label2, const gchar *text2,
				   GtkWidget **button3, const gchar *label3, const gchar *text3)
{
	cm_return_if_fail(bbox != NULL);
	cm_return_if_fail(button1 != NULL);

	*bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(*bbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(*bbox), 5);

	*button1 = gtk_button_new_with_mnemonic(text1);
	gtk_button_set_image(GTK_BUTTON(*button1),
		gtk_image_new_from_icon_name(label1, GTK_ICON_SIZE_BUTTON));
	gtk_widget_set_can_default(*button1, TRUE);
	gtk_box_pack_start(GTK_BOX(*bbox), *button1, TRUE, TRUE, 0);
	gtk_widget_show(*button1);

	if (button2) {
		*button2 = gtk_button_new_with_mnemonic(text2);
		gtk_button_set_image(GTK_BUTTON(*button2),
			gtk_image_new_from_icon_name(label2, GTK_ICON_SIZE_BUTTON));
		gtk_widget_set_can_default(*button2, TRUE);
		gtk_box_pack_start(GTK_BOX(*bbox), *button2, TRUE, TRUE, 0);
		gtk_widget_show(*button2);
	}

	if (button3) {
		*button3 = gtk_button_new_with_mnemonic(text3);
		gtk_button_set_image(GTK_BUTTON(*button3),
			gtk_image_new_from_icon_name(label3, GTK_ICON_SIZE_BUTTON));
		gtk_widget_set_can_default(*button3, TRUE);
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

void gtkut_ctree_node_move_if_on_the_edge(GtkCMCTree *ctree, GtkCMCTreeNode *node, gint _row)
{
	GtkCMCList *clist = GTK_CMCLIST(ctree);
	gint row;
	GtkVisibility row_visibility, prev_row_visibility, next_row_visibility;
	gfloat row_align;

	cm_return_if_fail(ctree != NULL);
	cm_return_if_fail(node != NULL);

	row = (_row != -1 ? _row : g_list_position(clist->row_list, (GList *)node));

	if (row < 0 || row >= clist->rows || clist->row_height == 0) return;
	row_visibility = gtk_cmclist_row_is_visible(clist, row);
	prev_row_visibility = gtk_cmclist_row_is_visible(clist, row - 1);
	next_row_visibility = gtk_cmclist_row_is_visible(clist, row + 1);

	if (row_visibility == GTK_VISIBILITY_NONE) {
		row_align = 0.5;
		if (gtk_cmclist_row_is_above_viewport(clist, row))
			row_align = 0.2;
		else if (gtk_cmclist_row_is_below_viewport(clist, row))
			row_align = 0.8;
		gtk_cmclist_moveto(clist, row, -1, row_align, 0);
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
		gtk_cmclist_moveto(clist, row, -1, 0.2, 0);
		return;
	}
	if (next_row_visibility != GTK_VISIBILITY_FULL) {
		gtk_cmclist_moveto(clist, row, -1, 0.8, 0);
		return;
	}
}

#undef CELL_SPACING
#undef ROW_TOP_YPIXEL
#undef ROW_FROM_YPIXEL

gint gtkut_ctree_get_nth_from_node(GtkCMCTree *ctree, GtkCMCTreeNode *node)
{
	cm_return_val_if_fail(ctree != NULL, -1);
	cm_return_val_if_fail(node != NULL, -1);

	return g_list_position(GTK_CMCLIST(ctree)->row_list, (GList *)node);
}

/* get the next node, including the invisible one */
GtkCMCTreeNode *gtkut_ctree_node_next(GtkCMCTree *ctree, GtkCMCTreeNode *node)
{
	GtkCMCTreeNode *parent;

	if (!node) return NULL;

	if (GTK_CMCTREE_ROW(node)->children)
		return GTK_CMCTREE_ROW(node)->children;

	if (GTK_CMCTREE_ROW(node)->sibling)
		return GTK_CMCTREE_ROW(node)->sibling;

	for (parent = GTK_CMCTREE_ROW(node)->parent; parent != NULL;
	     parent = GTK_CMCTREE_ROW(parent)->parent) {
		if (GTK_CMCTREE_ROW(parent)->sibling)
			return GTK_CMCTREE_ROW(parent)->sibling;
	}

	return NULL;
}

/* get the previous node, including the invisible one */
GtkCMCTreeNode *gtkut_ctree_node_prev(GtkCMCTree *ctree, GtkCMCTreeNode *node)
{
	GtkCMCTreeNode *prev;
	GtkCMCTreeNode *child;

	if (!node) return NULL;

	prev = GTK_CMCTREE_NODE_PREV(node);
	if (prev == GTK_CMCTREE_ROW(node)->parent)
		return prev;

	child = prev;
	while (GTK_CMCTREE_ROW(child)->children != NULL) {
		child = GTK_CMCTREE_ROW(child)->children;
		while (GTK_CMCTREE_ROW(child)->sibling != NULL)
			child = GTK_CMCTREE_ROW(child)->sibling;
	}

	return child;
}

gboolean gtkut_ctree_node_is_selected(GtkCMCTree *ctree, GtkCMCTreeNode *node)
{
	GtkCMCList *clist = GTK_CMCLIST(ctree);
	GList *cur;

	for (cur = clist->selection; cur != NULL; cur = cur->next) {
		if (node == GTK_CMCTREE_NODE(cur->data))
			return TRUE;
	}

	return FALSE;
}

GtkCMCTreeNode *gtkut_ctree_find_collapsed_parent(GtkCMCTree *ctree,
						GtkCMCTreeNode *node)
{
	if (!node) return NULL;

	while ((node = GTK_CMCTREE_ROW(node)->parent) != NULL) {
		if (!GTK_CMCTREE_ROW(node)->expanded)
			return node;
	}

	return NULL;
}

void gtkut_ctree_expand_parent_all(GtkCMCTree *ctree, GtkCMCTreeNode *node)
{
	gtk_cmclist_freeze(GTK_CMCLIST(ctree));

	while ((node = gtkut_ctree_find_collapsed_parent(ctree, node)) != NULL)
		gtk_cmctree_expand(ctree, node);

	gtk_cmclist_thaw(GTK_CMCLIST(ctree));
}

gboolean gtkut_ctree_node_is_parent(GtkCMCTreeNode *parent, GtkCMCTreeNode *node)
{
	GtkCMCTreeNode *tmp;
	cm_return_val_if_fail(node != NULL, FALSE);
	cm_return_val_if_fail(parent != NULL, FALSE);
	tmp = node;
	
	while (tmp) {
		if(GTK_CMCTREE_ROW(tmp)->parent && GTK_CMCTREE_ROW(tmp)->parent == parent)
			return TRUE;
		tmp = GTK_CMCTREE_ROW(tmp)->parent;
	}
	
	return FALSE;
}

void gtkut_ctree_set_focus_row(GtkCMCTree *ctree, GtkCMCTreeNode *node)
{
	if (node == NULL)
		return;
	gtkut_clist_set_focus_row(GTK_CMCLIST(ctree),
				  gtkut_ctree_get_nth_from_node(ctree, node));
}

void gtkut_clist_set_focus_row(GtkCMCList *clist, gint row)
{
	clist->focus_row = row;
	GTKUT_CTREE_REFRESH(clist);
}

static gboolean gtkut_text_buffer_match_string(GtkTextBuffer *textbuf,
					const GtkTextIter *iter,
					gunichar *wcs, gint len,
					gboolean case_sens)
{
	GtkTextIter start_iter, end_iter;
	gchar *utf8str, *p;
	gint match_count;

	start_iter = end_iter = *iter;
	gtk_text_iter_forward_chars(&end_iter, len);

	utf8str = gtk_text_buffer_get_text(textbuf, &start_iter, &end_iter,
					   FALSE);
	if (!utf8str) return FALSE;

	if ((gint)g_utf8_strlen(utf8str, -1) != len) {
		g_free(utf8str);
		return FALSE;
	}

	for (p = utf8str, match_count = 0;
	     *p != '\0' && match_count < len;
	     p = g_utf8_next_char(p), match_count++) {
		gunichar wc;

		wc = g_utf8_get_char(p);

		if (case_sens) {
			if (wc != wcs[match_count])
				break;
		} else {
			if (g_unichar_tolower(wc) !=
			    g_unichar_tolower(wcs[match_count]))
				break;
		}
	}

	g_free(utf8str);

	if (match_count == len)
		return TRUE;
	else
		return FALSE;
}

static gboolean gtkut_text_buffer_find(GtkTextBuffer *buffer, const GtkTextIter *iter,
				const gchar *str, gboolean case_sens,
				GtkTextIter *match_pos)
{
	gunichar *wcs;
	gint len;
	glong items_read = 0, items_written = 0;
	GError *error = NULL;
	GtkTextIter iter_;
	gboolean found = FALSE;

	wcs = g_utf8_to_ucs4(str, -1, &items_read, &items_written, &error);
	if (error != NULL) {
		g_warning("an error occurred while converting a string from UTF-8 to UCS-4: %s",
			  error->message);
		g_error_free(error);
	}
	if (!wcs || items_written <= 0) return FALSE;
	len = (gint)items_written;

	iter_ = *iter;
	do {
		found = gtkut_text_buffer_match_string
			(buffer, &iter_, wcs, len, case_sens);
		if (found) {
			*match_pos = iter_;
			break;
		}
	} while (gtk_text_iter_forward_char(&iter_));

	g_free(wcs);

	return found;
}

static gboolean gtkut_text_buffer_find_backward(GtkTextBuffer *buffer,
					 const GtkTextIter *iter,
					 const gchar *str, gboolean case_sens,
					 GtkTextIter *match_pos)
{
	gunichar *wcs;
	gint len;
	glong items_read = 0, items_written = 0;
	GError *error = NULL;
	GtkTextIter iter_;
	gboolean found = FALSE;

	wcs = g_utf8_to_ucs4(str, -1, &items_read, &items_written, &error);
	if (error != NULL) {
		g_warning("an error occurred while converting a string from UTF-8 to UCS-4: %s",
			  error->message);
		g_error_free(error);
	}
	if (!wcs || items_written <= 0) return FALSE;
	len = (gint)items_written;

	iter_ = *iter;
	while (gtk_text_iter_backward_char(&iter_)) {
		found = gtkut_text_buffer_match_string
			(buffer, &iter_, wcs, len, case_sens);
		if (found) {
			*match_pos = iter_;
			break;
		}
	}

	g_free(wcs);

	return found;
}

gchar *gtkut_text_view_get_selection(GtkTextView *textview)
{
	GtkTextBuffer *buffer;
	GtkTextIter start_iter, end_iter;
	gboolean found;

	cm_return_val_if_fail(GTK_IS_TEXT_VIEW(textview), NULL);

	buffer = gtk_text_view_get_buffer(textview);
	found = gtk_text_buffer_get_selection_bounds(buffer,
						     &start_iter,
						     &end_iter);
	if (found)
		return gtk_text_buffer_get_text(buffer, &start_iter, &end_iter,
						FALSE);
	else
		return NULL;
}


void gtkut_text_view_set_position(GtkTextView *text, gint pos)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	GtkTextMark *mark;

	cm_return_if_fail(text != NULL);

	buffer = gtk_text_view_get_buffer(text);

	gtk_text_buffer_get_iter_at_offset(buffer, &iter, pos);
	gtk_text_buffer_place_cursor(buffer, &iter);
	mark = gtk_text_buffer_create_mark(buffer, NULL, &iter, TRUE);
	gtk_text_view_scroll_to_mark(text, mark, 0.0, FALSE, 0.0, 0.0);
}

gboolean gtkut_text_view_search_string(GtkTextView *text, const gchar *str,
					gboolean case_sens)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter, match_pos;
	GtkTextMark *mark;
	gint len;

	cm_return_val_if_fail(text != NULL, FALSE);
	cm_return_val_if_fail(str != NULL, FALSE);

	buffer = gtk_text_view_get_buffer(text);

	len = g_utf8_strlen(str, -1);
	cm_return_val_if_fail(len >= 0, FALSE);

	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

	if (gtkut_text_buffer_find(buffer, &iter, str, case_sens,
				   &match_pos)) {
		GtkTextIter end = match_pos;

		gtk_text_iter_forward_chars(&end, len);
		/* place "insert" at the last character */
		gtk_text_buffer_select_range(buffer, &end, &match_pos);
		gtk_text_view_scroll_to_mark(text, mark, 0.0, TRUE, 0.0, 0.5);
		return TRUE;
	}

	return FALSE;
}

gboolean gtkut_text_view_search_string_backward(GtkTextView *text, const gchar *str,
					gboolean case_sens)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter, match_pos;
	GtkTextMark *mark;
	gint len;

	cm_return_val_if_fail(text != NULL, FALSE);
	cm_return_val_if_fail(str != NULL, FALSE);

	buffer = gtk_text_view_get_buffer(text);

	len = g_utf8_strlen(str, -1);
	cm_return_val_if_fail(len >= 0, FALSE);

	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

	if (gtkut_text_buffer_find_backward(buffer, &iter, str, case_sens,
					    &match_pos)) {
		GtkTextIter end = match_pos;

		gtk_text_iter_forward_chars(&end, len);
		gtk_text_buffer_select_range(buffer, &match_pos, &end);
		gtk_text_view_scroll_to_mark(text, mark, 0.0, TRUE, 0.0, 0.5);
		return TRUE;
	}

	return FALSE;
}

void gtkut_window_popup(GtkWidget *window)
{
	GdkWindow *gdkwin;
	gint x, y, sx, sy, new_x, new_y;
	GdkRectangle workarea = {0};

	gdkwin = gtk_widget_get_window(window);

	cm_return_if_fail(window != NULL);
	cm_return_if_fail(gdkwin != NULL);

	gdk_monitor_get_workarea(gdk_display_get_primary_monitor(gdk_display_get_default()),
				 &workarea);

	sx = MAX(1, workarea.width);
	sy = MAX(1, workarea.height);

	gdk_window_get_origin(gdkwin, &x, &y);
	new_x = x % sx; if (new_x < 0) new_x = 0;
	new_y = y % sy; if (new_y < 0) new_y = 0;
	if (new_x != x || new_y != y)
		gdk_window_move(gdkwin, new_x, new_y);

	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(window), FALSE);
	gtk_window_present(GTK_WINDOW(window));
}

void gtkut_widget_get_uposition(GtkWidget *widget, gint *px, gint *py)
{
	GdkWindow *gdkwin;
	gint x, y;
	gint sx, sy;
	GdkRectangle workarea = {0};

	gdkwin = gtk_widget_get_window(widget);

	cm_return_if_fail(widget != NULL);
	cm_return_if_fail(gdkwin != NULL);

	gdk_monitor_get_workarea(gdk_display_get_primary_monitor(gdk_display_get_default()),
				 &workarea);

	sx = MAX(1, workarea.width);
	sy = MAX(1, workarea.height);

	/* gdk_window_get_root_origin ever return *rootwindow*'s position */
	gdk_window_get_root_origin(gdkwin, &x, &y);

	x %= sx; if (x < 0) x = 0;
	y %= sy; if (y < 0) y = 0;
	*px = x;
	*py = y;
}

static void gtkut_clist_bindings_add(GtkWidget *clist)
{
	GtkBindingSet *binding_set;

	binding_set = gtk_binding_set_by_class
		(GTK_CMCLIST_GET_CLASS(clist));

	gtk_binding_entry_add_signal(binding_set, GDK_KEY_n, GDK_CONTROL_MASK,
				     "scroll_vertical", 2,
				     G_TYPE_ENUM, GTK_SCROLL_STEP_FORWARD,
				     G_TYPE_FLOAT, 0.0);
	gtk_binding_entry_add_signal(binding_set, GDK_KEY_p, GDK_CONTROL_MASK,
				     "scroll_vertical", 2,
				     G_TYPE_ENUM, GTK_SCROLL_STEP_BACKWARD,
				     G_TYPE_FLOAT, 0.0);
}

void gtkut_widget_init(void)
{
	GtkWidget *clist;

	clist = gtk_cmclist_new(1);
	g_object_ref(G_OBJECT(clist));
	g_object_ref_sink (G_OBJECT(clist));
	gtkut_clist_bindings_add(clist);
	g_object_unref(G_OBJECT(clist));

	clist = gtk_cmctree_new(1, 0);
	g_object_ref(G_OBJECT(clist));
	g_object_ref_sink (G_OBJECT(clist));
	gtkut_clist_bindings_add(clist);
	g_object_unref(G_OBJECT(clist));

	clist = gtk_sctree_new_with_titles(1, 0, NULL);
	g_object_ref(G_OBJECT(clist));
	g_object_ref_sink (G_OBJECT(clist));
	gtkut_clist_bindings_add(clist);
	g_object_unref(G_OBJECT(clist));
}

void gtkut_widget_set_app_icon(GtkWidget *widget)
{
	static GList *icon_list = NULL;

	cm_return_if_fail(widget != NULL);
	cm_return_if_fail(gtk_widget_get_window(widget) != NULL);
	if (!icon_list) {
		GdkPixbuf *icon = NULL, *big_icon = NULL;
		priv_pixbuf_gdk(PRIV_PIXMAP_CLAWS_MAIL_ICON, &icon);
		priv_pixbuf_gdk(PRIV_PIXMAP_CLAWS_MAIL_LOGO, &big_icon);
		if (icon)
			icon_list = g_list_append(icon_list, icon);
		if (big_icon)
			icon_list = g_list_append(icon_list, big_icon);
	}
	if (icon_list)
		gtk_window_set_icon_list(GTK_WINDOW(widget), icon_list);
}

void gtkut_widget_set_composer_icon(GtkWidget *widget)
{
	static GList *icon_list = NULL;

	cm_return_if_fail(widget != NULL);
	cm_return_if_fail(gtk_widget_get_window(widget) != NULL);
	if (!icon_list) {
		GdkPixbuf *icon = NULL, *big_icon = NULL;
		stock_pixbuf_gdk(STOCK_PIXMAP_MAIL_COMPOSE, &icon);
		stock_pixbuf_gdk(STOCK_PIXMAP_MAIL_COMPOSE_LOGO, &big_icon);
		if (icon)
			icon_list = g_list_append(icon_list, icon);
		if (big_icon)
			icon_list = g_list_append(icon_list, big_icon);
	}
	if (icon_list)
		gtk_window_set_icon_list(GTK_WINDOW(widget), icon_list);
}

static gboolean move_bar = FALSE;
static guint move_bar_id;

static gboolean move_bar_cb(gpointer data)
{
	GtkWidget *w = (GtkWidget *)data;
	if (!move_bar)
		return FALSE;

	if (!GTK_IS_PROGRESS_BAR(w)) {
		return FALSE;
	}
	gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(w), 0.1);
	gtk_progress_bar_pulse(GTK_PROGRESS_BAR(w));
	GTK_EVENTS_FLUSH();
	return TRUE;
}

GtkWidget *label_window_create(const gchar *str)
{
	GtkWidget *window;
	GtkWidget *label, *vbox, *hbox;
	GtkWidget *wait_progress = gtk_progress_bar_new();

	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "gtkutils");
	gtk_widget_set_size_request(window, 380, 70);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_title(GTK_WINDOW(window), str);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	manage_window_set_transient(GTK_WINDOW(window));

	label = gtk_label_new(str);
	
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, FALSE, 0);
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start(GTK_BOX(hbox), wait_progress, TRUE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_widget_show_all(vbox);

	gtk_widget_show_now(window);
	
	if (move_bar_id == 0) {
		move_bar_id = g_timeout_add(200, move_bar_cb, wait_progress);
		move_bar = TRUE;
	}

	GTK_EVENTS_FLUSH();

	return window;
}

void label_window_destroy(GtkWidget *window)
{
	move_bar = FALSE;
	g_source_remove(move_bar_id);
	move_bar_id = 0;
	GTK_EVENTS_FLUSH();
	gtk_widget_destroy(window);	
}

GtkWidget *gtkut_account_menu_new(GList			*ac_list,
					GCallback		callback,
				  gpointer		data)
{
	GList *cur_ac;
	GtkWidget *optmenu;
	GtkListStore *menu;
	GtkTreeIter iter;
	PrefsAccount *account;
	gchar *name;
	
	cm_return_val_if_fail(ac_list != NULL, NULL);

	optmenu = gtkut_sc_combobox_create(NULL, FALSE);
	menu = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(optmenu)));

	for (cur_ac = ac_list; cur_ac != NULL; cur_ac = cur_ac->next) {
		account = (PrefsAccount *) cur_ac->data;
		if (account->name)
			name = g_strdup_printf("%s: %s <%s>",
					       account->account_name,
					       account->name,
					       account->address);
		else
			name = g_strdup_printf("%s: %s",
					       account->account_name,
					       account->address);
		COMBOBOX_ADD_ESCAPED(menu, name, account->account_id);
		g_free(name);
	}
	gtk_combo_box_set_active(GTK_COMBO_BOX(optmenu), 0);

	if( callback != NULL )
		g_signal_connect(G_OBJECT(optmenu), "changed", callback, data);

	return optmenu;
}

/*!
 *\brief	Tries to find a focused child using a lame strategy
 */
GtkWidget *gtkut_get_focused_child(GtkContainer *parent)
{
	GtkWidget *result = NULL;
	GList *child_list = NULL;
	GList *c;

	cm_return_val_if_fail(parent, NULL);

	/* Get children list and see which has the focus. */
	child_list = gtk_container_get_children(parent);
	if (!child_list)
		return NULL;

	for (c = child_list; c != NULL; c = g_list_next(c)) {
		if (c->data && GTK_IS_WIDGET(c->data)) {
			if (gtk_widget_has_focus(GTK_WIDGET(c->data))) {
				result = GTK_WIDGET(c->data);
				break;
			}
		}
	}
	
	/* See if the returned widget is a container itself; if it is,
	 * see if one of its children is focused. If the focused 
	 * container has no focused child, it is itself a focusable 
	 * child, and has focus. */
	if (result && GTK_IS_CONTAINER(result)) {
		GtkWidget *tmp =  gtkut_get_focused_child(GTK_CONTAINER(result)); 
		
		if (tmp) 
			result = tmp;
	} else {
		/* Try the same for each container in the chain */
		for (c = child_list; c != NULL && !result; c = g_list_next(c)) {
			if (c->data && GTK_IS_WIDGET(c->data) 
			&&  GTK_IS_CONTAINER(c->data)) {
				result = gtkut_get_focused_child
					(GTK_CONTAINER(c->data));
			}
		}
	
	}
	
	g_list_free(child_list);
		
	return result;
}

/*!
 *\brief	Create a Browse (file) button based on GTK stock
 */
GtkWidget *gtkut_get_browse_file_btn(const gchar *button_label)
{
	GtkWidget *button;

	button = gtk_button_new_with_mnemonic(button_label);
	gtk_button_set_image(GTK_BUTTON(button),
		gtk_image_new_from_icon_name("folder", GTK_ICON_SIZE_BUTTON));

	return button;
}

/*!
 *\brief	Create a Browse (directory) button based on GTK stock
 */
GtkWidget *gtkut_get_browse_directory_btn(const gchar *button_label)
{
	GtkWidget *button;

	button = gtk_button_new_with_mnemonic(button_label);
	gtk_button_set_image(GTK_BUTTON(button),
		gtk_image_new_from_icon_name("folder", GTK_ICON_SIZE_BUTTON));

	return button;
}

GtkWidget *gtkut_get_replace_btn(const gchar *button_label)
{
	GtkWidget *button;

	button = gtk_button_new_with_mnemonic(button_label);
	gtk_button_set_image(GTK_BUTTON(button),
		gtk_image_new_from_icon_name("view-refresh", GTK_ICON_SIZE_BUTTON));

	return button;
}

GtkWidget *gtkut_stock_button(const gchar *stock_image, const gchar *label)
{
	GtkWidget *button;
	
	cm_return_val_if_fail(stock_image != NULL, NULL);

	button = gtk_button_new_from_icon_name(stock_image, GTK_ICON_SIZE_BUTTON);
	if (label != NULL)
		gtk_button_set_label(GTK_BUTTON(button), _(label));
	gtk_button_set_use_underline(GTK_BUTTON(button), TRUE);
	gtk_button_set_always_show_image(GTK_BUTTON(button), TRUE);
	
	return button;
};

/**
 * merge some part of code into one function : it creates a frame and add
 *	these into gtk box widget passed in param.
 * \param box gtk box where adding new created frame.
 * \param pframe pointer with which to assign the frame. If NULL, no pointer
 *	is assigned but the frame is anyway created and added to @box.
 * \param frame_label frame label of new created frame.
 */
GtkWidget *gtkut_get_options_frame(GtkWidget *box, GtkWidget **pframe,
		const gchar *frame_label)
{
	GtkWidget *vbox;
	GtkWidget *frame;

	frame = gtk_frame_new(frame_label);
	gtk_widget_show(frame);
	gtk_box_pack_start(GTK_BOX(box), frame, FALSE, TRUE, 0);
	gtk_frame_set_label_align(GTK_FRAME(frame), 0.01, 0.5);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER (frame), vbox);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);

	if (pframe != NULL)
		*pframe = frame;

	return vbox;
}

#if HAVE_LIBCOMPFACE
static gint create_xpm_from_xface(gchar *xpm[], const gchar *xface)
{
	static gchar *bit_pattern[] = {
		"....",
		"...#",
		"..#.",
		"..##",
		".#..",
		".#.#",
		".##.",
		".###",
		"#...",
		"#..#",
		"#.#.",
		"#.##",
		"##..",
		"##.#",
		"###.",
		"####"
	};

	static gchar *xface_header = "48 48 2 1";
	static gchar *xface_black  = "# c #000000";
	static gchar *xface_white  = ". c #ffffff";

	gint i, line = 0;
	const guchar *p;
	gchar buf[WIDTH * 4 + 1];  /* 4 = strlen("0x0000") */

	p = xface;

	strcpy(xpm[line++], xface_header);
	strcpy(xpm[line++], xface_black);
	strcpy(xpm[line++], xface_white);

	for (i = 0; i < HEIGHT; i++) {
		gint col;

		buf[0] = '\0';
     
		for (col = 0; col < 3; col++) {
			gint figure;

			p += 2;  /* skip '0x' */

			for (figure = 0; figure < 4; figure++) {
				gint n = 0;

				if ('0' <= *p && *p <= '9') {
					n = *p - '0';
				} else if ('a' <= *p && *p <= 'f') {
					n = *p - 'a' + 10;
				} else if ('A' <= *p && *p <= 'F') {
					n = *p - 'A' + 10;
				}

				strcat(buf, bit_pattern[n]);
				p++;  /* skip ',' */
			}

			p++;  /* skip '\n' */
		}

		strcpy(xpm[line++], buf);
		p++;
	}

	return 0;
}
#endif

gboolean get_tag_range(GtkTextIter *iter,
				       GtkTextTag *tag,
				       GtkTextIter *start_iter,
				       GtkTextIter *end_iter)
{
	GtkTextIter _start_iter, _end_iter;

	_end_iter = *iter;
	if (!gtk_text_iter_forward_to_tag_toggle(&_end_iter, tag)) {
		debug_print("Can't find end.\n");
		return FALSE;
	}

	_start_iter = _end_iter;
	if (!gtk_text_iter_backward_to_tag_toggle(&_start_iter, tag)) {
		debug_print("Can't find start.\n");
		return FALSE;
	}

	*start_iter = _start_iter;
	*end_iter = _end_iter;

	return TRUE;
}

#if HAVE_LIBCOMPFACE
GtkWidget *xface_get_from_header(const gchar *o_xface)
{
	static gchar *xpm_xface[XPM_XFACE_HEIGHT];
	static gboolean xpm_xface_init = TRUE;
	GdkPixbuf *pixbuf;
	GtkWidget *ret;
	gchar xface[2048];
	
	if (o_xface == NULL)
		return NULL;
	
	strncpy(xface, o_xface, sizeof(xface) - 1);
	xface[sizeof(xface) - 1] = '\0';

	if (uncompface(xface) < 0) {
		g_warning("uncompface failed");
		return NULL;
	}

	if (xpm_xface_init) {
		gint i;

		for (i = 0; i < XPM_XFACE_HEIGHT; i++) {
			xpm_xface[i] = g_malloc(WIDTH + 1);
			*xpm_xface[i] = '\0';
		}
		xpm_xface_init = FALSE;
	}

	create_xpm_from_xface(xpm_xface, xface);

	pixbuf = gdk_pixbuf_new_from_xpm_data((const char **)xpm_xface);
	ret = gtk_image_new_from_pixbuf(pixbuf);
	g_object_unref(pixbuf);

	return ret;
}
#endif

GtkWidget *face_get_from_header(const gchar *o_face)
{
	gchar face[2048];
	gchar *face_png;
	gsize pngsize;
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	GdkPixbufLoader *loader = gdk_pixbuf_loader_new ();
	GtkWidget *image;
	
	if (o_face == NULL || strlen(o_face) == 0)
		return NULL;

	strncpy2(face, o_face, sizeof(face));

	unfold_line(face); /* strip all whitespace and linebreaks */
	remove_space(face);

	face_png = g_base64_decode(face, &pngsize);
	debug_print("---------------------- loaded face png\n");

	if (!gdk_pixbuf_loader_write (loader, face_png, pngsize, &error) ||
	    !gdk_pixbuf_loader_close (loader, &error)) {
		g_warning("loading face failed");
		g_object_unref(loader);
		g_free(face_png);
		return NULL;
	}
	g_free(face_png);

	pixbuf = g_object_ref(gdk_pixbuf_loader_get_pixbuf(loader));

	g_object_unref(loader);

	if ((gdk_pixbuf_get_width(pixbuf) != 48) || (gdk_pixbuf_get_height(pixbuf) != 48)) {
		g_object_unref(pixbuf);
		g_warning("wrong_size");
		return NULL;
	}

	image = gtk_image_new_from_pixbuf(pixbuf);
	g_object_unref(pixbuf);
	return image;
}

static gboolean _combobox_separator_func(GtkTreeModel *model,
		GtkTreeIter *iter, gpointer data)
{
	gchar *txt = NULL;

	cm_return_val_if_fail(model != NULL, FALSE);

	gtk_tree_model_get(model, iter, COMBOBOX_TEXT, &txt, -1);

	if( txt == NULL )
		return TRUE;
	
	g_free(txt);
	return FALSE;
}

GtkWidget *gtkut_sc_combobox_create(GtkWidget *eventbox, gboolean focus_on_click)
{
	GtkWidget *combobox;
	GtkListStore *menu;
	GtkCellRenderer *rend;

	menu = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_INT, G_TYPE_BOOLEAN);

	combobox = gtk_combo_box_new_with_model(GTK_TREE_MODEL(menu));

	rend = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combobox), rend, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combobox), rend,
			"markup", COMBOBOX_TEXT,
			"sensitive", COMBOBOX_SENS,
			NULL);

	if( eventbox != NULL )
		gtk_container_add(GTK_CONTAINER(eventbox), combobox);
	gtk_widget_set_focus_on_click(GTK_WIDGET(combobox), focus_on_click);

	gtk_combo_box_set_row_separator_func(GTK_COMBO_BOX(combobox),
			(GtkTreeViewRowSeparatorFunc)_combobox_separator_func, NULL, NULL);

	return combobox;
}

static void gtkutils_smooth_scroll_do(GtkWidget *widget, GtkAdjustment *vadj,
				      gfloat old_value, gfloat last_value,
				      gint step)
{
	gint change_value;
	gboolean up;
	gint i;

	if (old_value < last_value) {
		change_value = last_value - old_value;
		up = FALSE;
	} else {
		change_value = old_value - last_value;
		up = TRUE;
	}

	for (i = step; i <= change_value; i += step) {
		gtk_adjustment_set_value(vadj, old_value + (up ? -i : i));
		g_signal_emit_by_name(G_OBJECT(vadj),
				      "value_changed", 0);
	}

	gtk_adjustment_set_value(vadj, last_value);
	g_signal_emit_by_name(G_OBJECT(vadj), "value_changed", 0);

	gtk_widget_queue_draw(widget);
}

static gboolean gtkutils_smooth_scroll_page(GtkWidget *widget, GtkAdjustment *vadj, gboolean up)
{
	gfloat upper;
	gfloat page_incr;
	gfloat old_value;
	gfloat last_value;

	page_incr = gtk_adjustment_get_page_increment(vadj);
	if (prefs_common.scroll_halfpage)
		page_incr /= 2;

	old_value = gtk_adjustment_get_value(vadj);
	if (!up) {
		upper = gtk_adjustment_get_upper(vadj) - gtk_adjustment_get_page_size(vadj);
		if (old_value < upper) {
			last_value = old_value + page_incr;
			last_value = MIN(last_value, upper);

			gtkutils_smooth_scroll_do(widget, vadj, old_value,
						  last_value,
						  prefs_common.scroll_step);
		} else
			return FALSE;
	} else {
		if (old_value > 0.0) {
			last_value = old_value - page_incr;
			last_value = MAX(last_value, 0.0);

			gtkutils_smooth_scroll_do(widget, vadj, old_value,
						  last_value,
						  prefs_common.scroll_step);
		} else
			return FALSE;
	}

	return TRUE;
}

gboolean gtkutils_scroll_page(GtkWidget *widget, GtkAdjustment *vadj, gboolean up)
{
	gfloat upper;
	gfloat page_incr;
	gfloat old_value;

	if (prefs_common.enable_smooth_scroll)
		return gtkutils_smooth_scroll_page(widget, vadj, up);

	page_incr = gtk_adjustment_get_page_increment(vadj);
	if (prefs_common.scroll_halfpage)
		page_incr /= 2;

	old_value = gtk_adjustment_get_value(vadj);
	if (!up) {
		upper = gtk_adjustment_get_upper(vadj) - gtk_adjustment_get_page_size(vadj);
		if (old_value < upper) {
			old_value += page_incr;
			old_value = MIN(old_value, upper);
			gtk_adjustment_set_value(vadj, old_value);
			g_signal_emit_by_name(G_OBJECT(vadj),
					      "value_changed", 0);
		} else
			return FALSE;
	} else {
		if (old_value > 0.0) {
			old_value -= page_incr;
			old_value = MAX(old_value, 0.0);
			gtk_adjustment_set_value(vadj, old_value);
			g_signal_emit_by_name(G_OBJECT(vadj),
					      "value_changed", 0);
		} else
			return FALSE;
	}
	return TRUE;
}

static void gtkutils_smooth_scroll_one_line(GtkWidget *widget, GtkAdjustment *vadj, gboolean up)
{
	gfloat upper;
	gfloat old_value;
	gfloat last_value;

	old_value = gtk_adjustment_get_value(vadj);
	if (!up) {
		upper = gtk_adjustment_get_upper(vadj) - gtk_adjustment_get_page_size(vadj);
		if (old_value < upper) {
			last_value = old_value + gtk_adjustment_get_step_increment(vadj);
			last_value = MIN(last_value, upper);

			gtkutils_smooth_scroll_do(widget, vadj, old_value,
						  last_value,
						  prefs_common.scroll_step);
		}
	} else {
		if (old_value > 0.0) {
			last_value = old_value - gtk_adjustment_get_step_increment(vadj);
			last_value = MAX(last_value, 0.0);

			gtkutils_smooth_scroll_do(widget, vadj, old_value,
						  last_value,
						  prefs_common.scroll_step);
		}
	}
}

void gtkutils_scroll_one_line(GtkWidget *widget, GtkAdjustment *vadj, gboolean up)
{
	gfloat upper;
	gfloat old_value;

	if (prefs_common.enable_smooth_scroll) {
		gtkutils_smooth_scroll_one_line(widget, vadj, up);
		return;
	}

	old_value = gtk_adjustment_get_value(vadj);
	if (!up) {
		upper = gtk_adjustment_get_upper(vadj) - gtk_adjustment_get_page_size(vadj);
		if (old_value < upper) {
			old_value += gtk_adjustment_get_step_increment(vadj);
			old_value = MIN(old_value, upper);
			gtk_adjustment_set_value(vadj, old_value);
			g_signal_emit_by_name(G_OBJECT(vadj),
					      "value_changed", 0);
		}
	} else {
		if (old_value > 0.0) {
			old_value -= gtk_adjustment_get_step_increment(vadj);
			old_value = MAX(old_value, 0.0);
			gtk_adjustment_set_value(vadj, old_value);
			g_signal_emit_by_name(G_OBJECT(vadj),
					      "value_changed", 0);
		}
	}
}

gboolean gtkut_tree_model_text_iter_prev(GtkTreeModel *model,
				 GtkTreeIter *iter,
				 const gchar* text)
/* do the same as gtk_tree_model_iter_next, but _prev instead.
   to use with widgets with one text column (gtk_combo_box_text_new()
   and with GtkComboBoxEntry's for instance),
*/
{
	GtkTreeIter cur_iter;
	gchar *cur_value;
	gboolean valid;
	gint count;

	cm_return_val_if_fail(model != NULL, FALSE);
	cm_return_val_if_fail(iter != NULL, FALSE);

	if (text == NULL || *text == '\0')
		return FALSE;

	valid = gtk_tree_model_get_iter_first(model, &cur_iter);
	count = 0;
	while (valid) {
		gtk_tree_model_get(model, &cur_iter, 0, &cur_value, -1);

		if (strcmp(text, cur_value) == 0) {
			g_free(cur_value);
			if (count <= 0)
				return FALSE;

			return gtk_tree_model_iter_nth_child(model, iter, NULL, count - 1);
		}

		g_free(cur_value);
		valid = gtk_tree_model_iter_next(model, &cur_iter);
		count++;
	}
	return FALSE;		
}

gboolean gtkut_tree_model_get_iter_last(GtkTreeModel *model,
				 GtkTreeIter *iter)
/* do the same as gtk_tree_model_get_iter_first, but _last instead.
*/
{
	gint count;

	cm_return_val_if_fail(model != NULL, FALSE);
	cm_return_val_if_fail(iter != NULL, FALSE);

	count = gtk_tree_model_iter_n_children(model, NULL);

	if (count <= 0)
		return FALSE;

	return gtk_tree_model_iter_nth_child(model, iter, NULL, count - 1);
}

GtkWidget *gtkut_window_new		(GtkWindowType	 type,
					 const gchar	*class)
{
	GtkWidget *window = gtk_window_new(type);
	gtk_window_set_role(GTK_WINDOW(window), class);
	gtk_widget_set_name(GTK_WIDGET(window), class);
	return window;
}

static gboolean gtkut_tree_iter_comp(GtkTreeModel *model, 
				     GtkTreeIter *iter1, 
				     GtkTreeIter *iter2)
{
	GtkTreePath *path1 = gtk_tree_model_get_path(model, iter1);
	GtkTreePath *path2 = gtk_tree_model_get_path(model, iter2);
	gboolean result;

	result = gtk_tree_path_compare(path1, path2) == 0;

	gtk_tree_path_free(path1);
	gtk_tree_path_free(path2);
	
	return result;
}

/*!
 *\brief	Get selected row number.
 */
gint gtkut_list_view_get_selected_row(GtkWidget *list_view)
{
	GtkTreeView *view = GTK_TREE_VIEW(list_view);
	GtkTreeModel *model = gtk_tree_view_get_model(view);
	int n_rows = gtk_tree_model_iter_n_children(model, NULL);
	GtkTreeSelection *selection;
	GtkTreeIter iter;
	int row;

	if (n_rows == 0) 
		return -1;
	
	selection = gtk_tree_view_get_selection(view);
	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		return -1;
	
	/* get all iterators and compare them... */
	for (row = 0; row < n_rows; row++) {
		GtkTreeIter itern;

		if (gtk_tree_model_iter_nth_child(model, &itern, NULL, row)
		 && gtkut_tree_iter_comp(model, &iter, &itern))
			return row;
	}
	
	return -1;
}

/*!
 *\brief	Select a row by its number.
 */
gboolean gtkut_list_view_select_row(GtkWidget *list, gint row)
{
	GtkTreeView *list_view = GTK_TREE_VIEW(list);
	GtkTreeSelection *selection = gtk_tree_view_get_selection(list_view);
	GtkTreeModel *model = gtk_tree_view_get_model(list_view);
	GtkTreeIter iter;
	GtkTreePath *path;

	if (!gtk_tree_model_iter_nth_child(model, &iter, NULL, row))
		return FALSE;
	
	gtk_tree_selection_select_iter(selection, &iter);

	path = gtk_tree_model_get_path(model, &iter);
	gtk_tree_view_set_cursor(list_view, path, NULL, FALSE);
	gtk_tree_path_free(path);
	
	return TRUE;
}

static GtkUIManager *gui_manager = NULL;

GtkUIManager *gtkut_create_ui_manager(void)
{
	cm_return_val_if_fail(gui_manager == NULL, gui_manager);
	return (gui_manager = gtk_ui_manager_new());
}

GtkUIManager *gtkut_ui_manager(void)
{
	return gui_manager;
}

typedef struct _ClawsIOClosure ClawsIOClosure;

struct _ClawsIOClosure
{
  ClawsIOFunc function;
  GIOCondition condition;
  GDestroyNotify notify;
  gpointer data;
};

static gboolean  
claws_io_invoke (GIOChannel   *source,
	         GIOCondition  condition,
	         gpointer      data)
{
  ClawsIOClosure *closure = data;
  int fd;
#ifndef G_OS_WIN32
  fd = g_io_channel_unix_get_fd (source);
#else
  fd = g_io_channel_win32_get_fd (source);
#endif
  if (closure->condition & condition)
    closure->function (closure->data, fd, condition);

  return TRUE;
}

static void
claws_io_destroy (gpointer data)
{
  ClawsIOClosure *closure = data;

  if (closure->notify)
    closure->notify (closure->data);

  g_free (closure);
}

gint
claws_input_add    (gint	      source,
		    GIOCondition      condition,
		    ClawsIOFunc       function,
		    gpointer	      data,
		    gboolean	      is_sock)
{
  guint result;
  ClawsIOClosure *closure = g_new (ClawsIOClosure, 1);
  GIOChannel *channel;

  closure->function = function;
  closure->condition = condition;
  closure->notify = NULL;
  closure->data = data;

#ifndef G_OS_WIN32
  channel = g_io_channel_unix_new (source);
#else
  if (is_sock)
    channel = g_io_channel_win32_new_socket(source);
  else
    channel = g_io_channel_win32_new_fd(source);
#endif
  result = g_io_add_watch_full (channel, G_PRIORITY_DEFAULT, condition, 
				claws_io_invoke,
				closure, claws_io_destroy);
  g_io_channel_unref (channel);

  return result;
}

/**
 * Load a pixbuf fitting inside the specified size. EXIF orientation is
 * respected if available.
 *
 * @param[in] filename		the file to load
 * @param[in] box_width		the max width (-1 for no resize)
 * @param[in] box_height	the max height (-1 for no resize)
 * @param[out] error		the possible load error
 *
 * @return a GdkPixbuf
 */
GdkPixbuf *claws_load_pixbuf_fitting(GdkPixbuf *src_pixbuf, gboolean inline_img,
				     gboolean fit_img_height, int box_width, int box_height)
{
	gint w, h, orientation, angle;
	gint avail_width, avail_height;
	gboolean flip_horiz, flip_vert;
	const gchar *orient_str;
	GdkPixbuf *pixbuf, *t_pixbuf;

	pixbuf = src_pixbuf;

	if (pixbuf == NULL)
		return NULL;

	angle = 0;
	flip_horiz = flip_vert = FALSE;

	/* EXIF orientation */
	orient_str = gdk_pixbuf_get_option(pixbuf, "orientation");
	if (orient_str != NULL && *orient_str != '\0') {
		orientation = atoi(orient_str);
		switch(orientation) {
			/* See EXIF standard for different values */
			case 1:	break;
			case 2:	flip_horiz = 1;
				break;
			case 3:	angle = 180;
				break;
			case 4:	flip_vert = 1;
				break;
			case 5:	angle = 90;
				flip_horiz = 1;
				break;
			case 6:	angle = 270;
				break;
			case 7:	angle = 90;
				flip_vert = 1;
				break;
			case 8:	angle = 90;
				break;
		}
	}


	/* Rotate if needed */
	if (angle != 0) {
		t_pixbuf = gdk_pixbuf_rotate_simple(pixbuf, angle);
		g_object_unref(pixbuf);
		pixbuf = t_pixbuf;
	}

	/* Flip horizontally if needed */
	if (flip_horiz) {
		t_pixbuf = gdk_pixbuf_flip(pixbuf, TRUE);
		g_object_unref(pixbuf);
		pixbuf = t_pixbuf;
	}

	/* Flip vertically if needed */
	if (flip_vert) {
		t_pixbuf = gdk_pixbuf_flip(pixbuf, FALSE);
		g_object_unref(pixbuf);
		pixbuf = t_pixbuf;
	}

	w = gdk_pixbuf_get_width(pixbuf);
	h = gdk_pixbuf_get_height(pixbuf);

	avail_width = box_width-32;
	avail_height = box_height;
		
	if (box_width != -1 && box_height != -1 && avail_width - 100 > 0) {
		if (inline_img || fit_img_height) {
			if (w > avail_width) {
				h = (avail_width * h) / w;
				w = avail_width;
			}
			if (h > avail_height) {
				w = (avail_height * w) / h;
				h = avail_height;
			}
		} else {
			if (w > avail_width || h > avail_height) {
				h = (avail_width * h) / w;
				w = avail_width;
			}
		}
		t_pixbuf = gdk_pixbuf_scale_simple(pixbuf, 
			w, h, GDK_INTERP_BILINEAR);
		g_object_unref(pixbuf);
		pixbuf = t_pixbuf;
	}

	return pixbuf;
}

#if defined USE_GNUTLS
static void auto_configure_done(const gchar *hostname, gint port, gboolean ssl, AutoConfigureData *data)
{
	gboolean smtp = strcmp(data->tls_service, "submission") == 0 ? TRUE : FALSE;

	if (hostname != NULL) {
		if (data->hostname_entry)
			gtk_entry_set_text(data->hostname_entry, hostname);
		if (data->set_port)
			gtk_toggle_button_set_active(data->set_port,
				(ssl && port != data->default_ssl_port) || (!ssl && port != data->default_port));
		if (data->port)
			gtk_spin_button_set_value(data->port, port);
		else if (data->hostname_entry) {
			if ((ssl && port != data->default_ssl_port) || (!ssl && port != data->default_port)) {
				gchar *tmp = g_strdup_printf("%s:%d", hostname, port);
				gtk_entry_set_text(data->hostname_entry, tmp);
				g_free(tmp);
			} else
				gtk_entry_set_text(data->hostname_entry, hostname);
		}

		if (ssl && data->ssl_checkbtn) {
			gtk_toggle_button_set_active(data->ssl_checkbtn, TRUE);
			gtk_toggle_button_set_active(data->tls_checkbtn, FALSE);
		} else if (data->tls_checkbtn) {
			if (!GTK_IS_RADIO_BUTTON(data->ssl_checkbtn)) {
				/* Wizard where TLS is [x]SSL + [x]TLS */
				gtk_toggle_button_set_active(data->ssl_checkbtn, TRUE);
			}

			/* Even though technically this is against the RFCs,
			 * if a "_submission._tcp" SRV record uses port 465,
			 * it is safe to assume TLS-only service, instead of
			 * plaintext + STARTTLS one. */
			if (smtp && port == 465)
				gtk_toggle_button_set_active(data->ssl_checkbtn, TRUE);
			else
				gtk_toggle_button_set_active(data->tls_checkbtn, TRUE);
		}

		/* Check authentication by default. This is probably required if
		 * auto-configuration worked.
		 */
		if (data->auth_checkbtn)
			gtk_toggle_button_set_active(data->auth_checkbtn, TRUE);

		/* Set user ID to full email address, which is used by the
		 * majority of providers where auto-configuration works.
		 */
		if (data->uid_entry)
			gtk_entry_set_text(data->uid_entry, data->address);

		gtk_label_set_text(data->info_label, _("Done."));
	} else {
		gchar *msg;
		switch (data->resolver_error) {
		case G_RESOLVER_ERROR_NOT_FOUND:
			msg = g_strdup(_("Failed: no service record found."));
			break;
		case G_RESOLVER_ERROR_TEMPORARY_FAILURE:
			msg = g_strdup(_("Failed: network error."));
			break;
		default:
			msg = g_strdup_printf(_("Failed: unknown error (%d)."), data->resolver_error);
		}
		gtk_label_set_text(data->info_label, msg);
		g_free(msg);
	}
	gtk_widget_show(GTK_WIDGET(data->configure_button));
	gtk_widget_hide(GTK_WIDGET(data->cancel_button));
	g_free(data->address);
	g_free(data);
}

static void resolve_done(GObject *source, GAsyncResult *result, gpointer user_data)
{
	AutoConfigureData *data = (AutoConfigureData *)user_data;
	GResolver *resolver = (GResolver *)source;
	GError *error = NULL;
	gchar *hostname = NULL;
	guint16 port;
	GList *answers, *cur;
	gboolean found = FALSE;
	gboolean abort = FALSE;

	answers = g_resolver_lookup_service_finish(resolver, result, &error);

	if (answers) {
		for (cur = g_srv_target_list_sort(answers); cur; cur = cur->next) {
			GSrvTarget *target = (GSrvTarget *)cur->data;
			const gchar *h = g_srv_target_get_hostname(target);
			port = g_srv_target_get_port(target);
			if (h && strcmp(h,"") && port > 0) {
				hostname = g_strdup(h);
				found = TRUE;
				break;
			}
		}
		g_resolver_free_targets(answers);
	} else if (error) {
		if (error->code == G_IO_ERROR_CANCELLED)
			abort = TRUE;
		else
			data->resolver_error = error->code;
		debug_print("error %s\n", error->message);
		g_error_free(error);
	}

	if (found) {
		auto_configure_done(hostname, port, data->ssl_service != NULL, data);
	} else if (data->ssl_service && !abort) {
		/* Fallback to TLS */
		data->ssl_service = NULL;
		auto_configure_service(data);
	} else {
		auto_configure_done(NULL, 0, FALSE, data);
	}
	g_free(hostname);
	g_object_unref(resolver);
}

void auto_configure_service(AutoConfigureData *data)
{
	GResolver *resolver;
	const gchar *cur_service = data->ssl_service != NULL ? data->ssl_service : data->tls_service;

	cm_return_if_fail(cur_service != NULL);
	cm_return_if_fail(data->address != NULL);

	resolver = g_resolver_get_default();
	if (resolver != NULL) {
		const gchar *domain = strchr(data->address, '@') + 1;

		gtk_label_set_text(data->info_label, _("Configuring..."));
		gtk_widget_hide(GTK_WIDGET(data->configure_button));
		gtk_widget_show(GTK_WIDGET(data->cancel_button));
		g_resolver_lookup_service_async(resolver, cur_service, "tcp", domain,
					data->cancel, resolve_done, data);
	}
}

gboolean auto_configure_service_sync(const gchar *service, const gchar *domain, gchar **srvhost, guint16 *srvport)
{
	GResolver *resolver;
	GList *answers, *cur;
	GError *error = NULL;
	gboolean result = FALSE;

	cm_return_val_if_fail(service != NULL, FALSE);
	cm_return_val_if_fail(domain != NULL, FALSE);

	resolver = g_resolver_get_default();
	if (resolver == NULL)
		return FALSE;

	answers = g_resolver_lookup_service(resolver, service, "tcp", domain, NULL, &error);

	*srvhost = NULL;
	*srvport = 0;

	if (answers) {
		for (cur = g_srv_target_list_sort(answers); cur; cur = cur->next) {
			GSrvTarget *target = (GSrvTarget *)cur->data;
			const gchar *hostname = g_srv_target_get_hostname(target);
			guint16 port = g_srv_target_get_port(target);
			if (hostname && strcmp(hostname,"") && port > 0) {
				result = TRUE;
				*srvhost = g_strdup(hostname);
				*srvport = port;
				break;
			}
		}
		g_resolver_free_targets(answers);
	} else if (error) {
		g_error_free(error);
	}

	g_object_unref(resolver);
	return result;
}
#endif

gboolean gtkut_pointer_is_grabbed(GtkWidget *widget)
{
	GdkDisplay *display;
	GdkDevice *pointerdev;

	cm_return_val_if_fail(widget != NULL, FALSE);

	display = gtk_widget_get_display(widget);
	pointerdev = gdk_seat_get_pointer(gdk_display_get_default_seat(display));

	return gdk_display_device_is_grabbed(display, pointerdev);
}

gpointer gtkut_tree_view_get_selected_pointer(GtkTreeView *view,
		gint column, GtkTreeModel **_model, GtkTreeSelection **_selection,
		GtkTreeIter *_iter)
{
	GtkTreeIter iter;
	GtkTreeModel *model;
	GtkTreeSelection *sel;
	gpointer ptr;
	GType type;

	cm_return_val_if_fail(view != NULL, NULL);
	cm_return_val_if_fail(column >= 0, NULL);

	model = gtk_tree_view_get_model(view);
	if (_model != NULL)
		*_model = model;

	sel = gtk_tree_view_get_selection(view);
	if (_selection != NULL)
		*_selection = sel;

	if (!gtk_tree_selection_get_selected(sel, NULL, &iter))
		return NULL; /* No row selected */

	if (_iter != NULL)
		*_iter = iter;

	if (gtk_tree_selection_count_selected_rows(sel) > 1)
		return NULL; /* Can't work with multiselect */

	cm_return_val_if_fail(
			gtk_tree_model_get_n_columns(model) > column,
			NULL);

	type = gtk_tree_model_get_column_type(model, column);
	cm_return_val_if_fail(
			type == G_TYPE_POINTER || type == G_TYPE_STRING,
			NULL);

	gtk_tree_model_get(model, &iter, column, &ptr, -1);

	return ptr;
}

static GList *get_predefined_times(void)
{
	int h,m;
	GList *times = NULL;
	for (h = 0; h < 24; h++) {
		for (m = 0; m < 60; m += 15) {
			gchar *tmp = g_strdup_printf("%02d:%02d", h, m);
			times = g_list_append(times, tmp);
		}
	}
	return times;
}

static int get_list_item_num(int h, int m)
{
	if (m % 15 != 0)
		return -1;

	return (h*4 + m/15);
}

GtkWidget *gtkut_time_select_combo_new()
{
	GtkWidget *combo = gtk_combo_box_text_new_with_entry();
	GList *times = get_predefined_times();

	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), -1);
	combobox_set_popdown_strings(GTK_COMBO_BOX_TEXT(combo), times);

	list_free_strings_full(times);

	return combo;
}


void gtkut_time_select_select_by_time(GtkComboBox *combo, int hour, int minute)
{
	gchar *time_text = g_strdup_printf("%02d:%02d", hour, minute);
	gint num = get_list_item_num(hour, minute);

	if (num > -1)
		combobox_select_by_text(combo, time_text);
	else
		gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(combo))), time_text);

	g_free(time_text);
}

static void get_time_from_combo(GtkComboBox *combo, int *h, int *m)
{
	gchar *tmp;
	gchar **parts;

	if (!h || !m) 
		return;

	tmp = gtk_editable_get_chars(GTK_EDITABLE(gtk_bin_get_child(GTK_BIN(combo))), 0, -1);
	parts = g_strsplit(tmp, ":", 2);
	if (parts[0] && parts[1] && *parts[0] && *parts[1]) {
		*h = atoi(parts[0]);
		*m = atoi(parts[1]);
	}
	g_strfreev(parts);
	g_free(tmp);
}

gboolean gtkut_time_select_get_time(GtkComboBox *combo, int *hour, int *minute)
{
	const gchar *value = gtk_entry_get_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(combo))));

	if (value == NULL || strlen(value) != 5)
		return FALSE;

	if (hour == NULL || minute == NULL)
		return FALSE;

	get_time_from_combo(combo, hour, minute);

	if (*hour < 0 || *hour > 23)
		return FALSE;
	if (*minute < 0 || *minute > 59)
		return FALSE;

	return TRUE;
}

void gtk_calendar_select_today(GtkCalendar *calendar)
{
	time_t t = time (NULL);
	struct tm buft;
 	struct tm *lt = localtime_r (&t, &buft);

	mktime(lt);
	gtk_calendar_select_day(calendar, lt->tm_mday);
	gtk_calendar_select_month(calendar, lt->tm_mon, lt->tm_year + 1900);
}


#define RGBA_ELEMENT_TO_BYTE(x) (int)((gdouble)x * 255)
gchar *gtkut_gdk_rgba_to_string(GdkRGBA *rgba)
{
	gchar *str = g_strdup_printf("#%02x%02x%02x",
			RGBA_ELEMENT_TO_BYTE(rgba->red),
			RGBA_ELEMENT_TO_BYTE(rgba->green),
			RGBA_ELEMENT_TO_BYTE(rgba->blue));

	return str;
}
#undef RGBA_ELEMENT_TO_BYTE
