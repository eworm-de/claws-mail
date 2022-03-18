/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2022 the Claws Mail team and Hiroyuki Yamamoto
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

#ifndef __GTKUTILS_H__
#define __GTKUTILS_H__

#ifdef HAVE_CONFIG_H
#include "claws-features.h"
#endif

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#if HAVE_WCHAR_H
#  include <wchar.h>
#endif

#include "gtkcmctree.h"

#define GTK_EVENTS_FLUSH() \
{ \
	while (gtk_events_pending()) \
		gtk_main_iteration(); \
}

#define GTK_WIDGET_PTR(wid)	(*(GtkWidget **)wid)

#define GTKUT_CTREE_NODE_SET_ROW_DATA(node, d) \
{ \
	GTK_CMCTREE_ROW(node)->row.data = d; \
}

#define GTKUT_CTREE_NODE_GET_ROW_DATA(node) \
	(GTK_CMCTREE_ROW(node)->row.data)

#define GTKUT_CTREE_REFRESH(clist) \
	GTK_CMCLIST_GET_CLASS(clist)->refresh(clist)

/* String used in color button labels.
 * Instead of hardcoding a size which doesn't look the same on different
 * resolutions, use a space;m-space;space label and let GTK to compute
 * the appropriate button size for current font.
 * This macro is only used in gtkut_set_button_color(). */
#define GTKUT_COLOR_BUTTON_LABEL "\x20\xE2\x80\x83\x20"

/* Set "color" to the same color as "rgba" */
#define GTKUT_GDKRGBA_TO_GDKCOLOR(rgba, color) { \
	color.pixel = 0; \
	color.red   = (guint16)(rgba.red * 65535); \
	color.green = (guint16)(rgba.green * 65535); \
	color.blue  = (guint16)(rgba.blue * 65535); \
}

/* Set "rgba" to the same color as "color" */
#define GTKUT_GDKCOLOR_TO_GDKRGBA(color, rgba) { \
	rgba.red   = (gdouble)color.red / 65535; \
	rgba.green = (gdouble)color.green / 65535; \
	rgba.blue  = (gdouble)color.blue / 65535; \
	rgba.alpha = 1.0; \
}

/* Since GDK's gdk_rgba_to_string() produces a string
 * representation unsuitable for us, we have to have
 * our own function to produce a "#rrggbb" string from
 * a GdkRGBA.
 * The returned string has to be freed by the caller. */
gchar *gtkut_gdk_rgba_to_string(GdkRGBA *rgba);

gboolean gtkut_get_font_size		(GtkWidget	*widget,
					 gint		*width,
					 gint		*height);

void gtkut_stock_button_add_help(GtkWidget *bbox, GtkWidget **help_btn);

void gtkut_stock_button_set_create_with_help(GtkWidget **bbox,
		GtkWidget **help_button,
		GtkWidget **button1, const gchar *stock_icon1, const gchar *label1,
		GtkWidget **button2, const gchar *stock_icon2, const gchar *label2,
		GtkWidget **button3, const gchar *stock_icon3, const gchar *label3);

void gtkut_stock_button_set_create	(GtkWidget	**bbox,
					 GtkWidget	**button1,
					 const gchar	 *stock_icon1,
					 const gchar	 *label1,
					 GtkWidget	**button2,
					 const gchar	 *stock_icon2,
					 const gchar	 *label2,
					 GtkWidget	**button3,
					 const gchar	 *stock_icon3,
					 const gchar	 *label3);

void gtkut_stock_with_text_button_set_create(GtkWidget **bbox,
				   GtkWidget **button1, const gchar *label1, const gchar *text1,
				   GtkWidget **button2, const gchar *label2, const gchar *text2,
				   GtkWidget **button3, const gchar *label3, const gchar *text3);

void gtkut_ctree_node_move_if_on_the_edge
					(GtkCMCTree	*ctree,
					 GtkCMCTreeNode	*node,
					 gint		 _row);
gint gtkut_ctree_get_nth_from_node	(GtkCMCTree	*ctree,
					 GtkCMCTreeNode	*node);
GtkCMCTreeNode *gtkut_ctree_node_next	(GtkCMCTree	*ctree,
					 GtkCMCTreeNode	*node);
GtkCMCTreeNode *gtkut_ctree_node_prev	(GtkCMCTree	*ctree,
					 GtkCMCTreeNode	*node);
gboolean gtkut_ctree_node_is_selected	(GtkCMCTree	*ctree,
					 GtkCMCTreeNode	*node);
GtkCMCTreeNode *gtkut_ctree_find_collapsed_parent
					(GtkCMCTree	*ctree,
					 GtkCMCTreeNode	*node);
void gtkut_ctree_expand_parent_all	(GtkCMCTree	*ctree,
					 GtkCMCTreeNode	*node);
gboolean gtkut_ctree_node_is_parent	(GtkCMCTreeNode 	*parent, 
					 GtkCMCTreeNode 	*node);
void gtkut_ctree_set_focus_row		(GtkCMCTree	*ctree,
					 GtkCMCTreeNode	*node);

void gtkut_clist_set_focus_row		(GtkCMCList	*clist,
					 gint		 row);

gchar *gtkut_text_view_get_selection	(GtkTextView	*textview);
void gtkut_text_view_set_position		(GtkTextView *text, gint pos);
gboolean gtkut_text_view_search_string	(GtkTextView *text, const gchar *str,
					gboolean case_sens);
gboolean gtkut_text_view_search_string_backward	(GtkTextView *text, const gchar *str,
					gboolean case_sens);

GtkWidget *label_window_create(const gchar *str);
void label_window_destroy(GtkWidget *widget);

void gtkut_window_popup			(GtkWidget	*window);
GtkWidget *gtkut_window_new		(GtkWindowType	 type,
					 const gchar	*class);


void gtkut_widget_get_uposition		(GtkWidget	*widget,
					 gint		*px,
					 gint		*py);
void gtkut_widget_init			(void);

void gtkut_widget_set_app_icon		(GtkWidget	*widget);
void gtkut_widget_set_composer_icon	(GtkWidget	*widget);

GtkWidget *gtkut_account_menu_new	(GList			*ac_list,
				  	 GCallback	 	 callback,
					 gpointer		 data);

void gtkut_widget_set_small_font_size(GtkWidget *widget);
GtkWidget *gtkut_get_focused_child	(GtkContainer 	*parent);

GtkWidget *gtkut_get_browse_file_btn(const gchar *label);
GtkWidget *gtkut_get_browse_directory_btn(const gchar *label);
GtkWidget *gtkut_get_replace_btn(const gchar *label);
GtkWidget *gtkut_stock_button(const gchar *stock_image, const gchar *label);
GtkWidget *gtkut_get_options_frame(GtkWidget *box, GtkWidget **frame, const gchar *frame_label);
#if HAVE_LIBCOMPFACE
GtkWidget *xface_get_from_header(const gchar *o_xface);
#endif
gboolean get_tag_range(GtkTextIter *iter,
				       GtkTextTag *tag,
				       GtkTextIter *start_iter,
				       GtkTextIter *end_iter);

GtkWidget *face_get_from_header(const gchar *o_face);

GtkWidget *gtkut_sc_combobox_create(GtkWidget *eventbox, gboolean focus_on_click);
void gtkutils_scroll_one_line	(GtkWidget *widget, 
				 GtkAdjustment *vadj, 
				 gboolean up);
gboolean gtkutils_scroll_page	(GtkWidget *widget, 
				 GtkAdjustment *vadj, 
				 gboolean up);

gboolean gtkut_tree_model_text_iter_prev(GtkTreeModel *model,
				 GtkTreeIter *iter,
				 const gchar* text);
gboolean gtkut_tree_model_get_iter_last(GtkTreeModel *model,
				 GtkTreeIter *iter);

gint gtkut_list_view_get_selected_row(GtkWidget *list_view);
gboolean gtkut_list_view_select_row(GtkWidget *list, gint row);

GtkUIManager *gtkut_create_ui_manager(void);
GtkUIManager *gtkut_ui_manager(void);

GdkPixbuf *claws_load_pixbuf_fitting(GdkPixbuf *pixbuf, gboolean inline_img,
				     gboolean fit_img_height,
				     int box_width, int box_height);

GtkWidget *gtkut_time_select_combo_new();
void gtkut_time_select_select_by_time(GtkComboBox *combo, int hour, int minute);
gboolean gtkut_time_select_get_time(GtkComboBox *combo, int *hour, int *minute);

void gtk_calendar_select_today(GtkCalendar *calendar);

typedef void (*ClawsIOFunc)(gpointer data, gint source, GIOCondition condition);
gint
claws_input_add    (gint	      source,
		    GIOCondition      condition,
		    ClawsIOFunc       function,
		    gpointer	      data,
		    gboolean          is_sock);

#define CLAWS_SET_TIP(widget,tip) { 						\
	if (widget != NULL) {							\
		if (tip != NULL)						\
			gtk_widget_set_tooltip_text(GTK_WIDGET(widget), tip); 	\
		else								\
			gtk_widget_set_has_tooltip(GTK_WIDGET(widget), FALSE);	\
	}									\
}

#if defined USE_GNUTLS
typedef struct _AutoConfigureData {
	const gchar *ssl_service;
	const gchar *tls_service;
	gchar *address;
	gint resolver_error;

	GtkEntry *hostname_entry;
	GtkToggleButton *set_port;
	GtkSpinButton *port;
	gint default_port;
	gint default_ssl_port;
	GtkToggleButton *tls_checkbtn;
	GtkToggleButton *ssl_checkbtn;
	GtkToggleButton *auth_checkbtn;
	GtkEntry *uid_entry;
	GtkLabel *info_label;
	GtkButton *configure_button;
	GtkButton *cancel_button;
	GCancellable *cancel;
	GMainLoop *main_loop;
} AutoConfigureData;

void auto_configure_service(AutoConfigureData *data);
gboolean auto_configure_service_sync(const gchar *service, const gchar *domain, gchar **srvhost, guint16 *srvport);
#endif

gboolean gtkut_pointer_is_grabbed(GtkWidget *widget);

/* Returns pointer stored in selected row of a tree view's model
 * in a given column. The column has to be of type G_TYPE_POINTER
 * or G_TYPE_STRING (in this case, the returned value has to be
 * freed by the caller.
 * _model, _selection and _iter parameters are optional, and if
 * not NULL, they will be set to point to corresponding GtkTreeModel,
 * GtkTreeSelection, and GtkTreeIter of the selected row. */
gpointer gtkut_tree_view_get_selected_pointer(GtkTreeView *view,
		gint column, GtkTreeModel **_model, GtkTreeSelection **_selection,
		GtkTreeIter *_iter);

#endif /* __GTKUTILS_H__ */
