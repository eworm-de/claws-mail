/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto
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

#ifndef __GTKUTILS_H__
#define __GTKUTILS_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtkctree.h>
#include <gtk/gtkcombo.h>
#include "gtkstext.h"
#include <stdlib.h>
#if HAVE_WCHAR_H
#  include <wchar.h>
#endif

#define GTK_EVENTS_FLUSH() \
{ \
	while (gtk_events_pending()) \
		gtk_main_iteration(); \
}

#define PIXMAP_CREATE(widget, pixmap, mask, xpm_d) \
{ \
	if (!pixmap) { \
		GtkStyle *style = gtk_widget_get_style(widget); \
		pixmap = gdk_pixmap_create_from_xpm_d \
			(widget->window, &mask, \
			 &style->bg[GTK_STATE_NORMAL], xpm_d); \
	} \
}

#define PIXMAP_CREATE_FROM_FILE(widget, pixmap, mask, filename) \
{ \
	if (!pixmap) { \
		GtkStyle *style = gtk_widget_get_style(widget); \
		pixmap = gdk_pixmap_create_from_xpm \
			(widget->window, &mask, \
			 &style->bg[GTK_STATE_NORMAL], filename); \
	} \
}

#define GTK_WIDGET_PTR(wid)	(*(GtkWidget **)wid)

#define GTKUT_CTREE_NODE_SET_ROW_DATA(node, d) \
{ \
	GTK_CTREE_ROW(node)->row.data = d; \
}

#define GTKUT_CTREE_NODE_GET_ROW_DATA(node) \
	(GTK_CTREE_ROW(node)->row.data)

#define GTKUT_CTREE_REFRESH(clist) \
	GTK_CLIST_CLASS(GTK_OBJECT(clist)->klass)->refresh(clist)

gint gtkut_get_font_width		(GdkFont	*font);
gint gtkut_get_font_height		(GdkFont	*font);

GdkFont *gtkut_font_load		(const gchar	*fontset_name);
GdkFont *gtkut_font_load_from_fontset	(const gchar	*fontset_name);

void gtkut_convert_int_to_gdk_color	(gint		 rgbvalue,
					 GdkColor	*color);

void gtkut_button_set_create		(GtkWidget	**bbox,
					 GtkWidget	**button1,
					 const gchar	 *label1,
					 GtkWidget	**button2,
					 const gchar	 *label2,
					 GtkWidget	**button3,
					 const gchar	 *label3);

void gtkut_ctree_node_move_if_on_the_edge
					(GtkCTree	*ctree,
					 GtkCTreeNode	*node);
gint gtkut_ctree_get_nth_from_node	(GtkCTree	*ctree,
					 GtkCTreeNode	*node);
GtkCTreeNode *gtkut_ctree_node_next	(GtkCTree	*ctree,
					 GtkCTreeNode	*node);
GtkCTreeNode *gtkut_ctree_find_collapsed_parent
					(GtkCTree	*ctree,
					 GtkCTreeNode	*node);
void gtkut_ctree_expand_parent_all	(GtkCTree	*ctree,
					 GtkCTreeNode	*node);
void gtkut_ctree_set_focus_row		(GtkCTree	*ctree,
					 GtkCTreeNode	*node);

void gtkut_clist_set_focus_row		(GtkCList	*clist,
					 gint		 row);

void gtkut_combo_set_items		(GtkCombo	*combo,
					 const gchar	*str1, ...);

gchar *gtkut_editable_get_selection	(GtkEditable	*editable);

void gtkut_container_remove		(GtkContainer	*container,
					 GtkWidget	*widget);

gboolean gtkut_stext_match_string	(GtkSText	*text,
					 gint		 pos,
					 wchar_t	*wcs,
					 gint		 len,
					 gboolean	 case_sens);
guint gtkut_stext_str_compare_n		(GtkSText	*text,
					 guint		 pos1,
					 guint		 pos2,
					 guint		 len,
					 guint		 text_len);
guint gtkut_stext_str_compare		(GtkSText	*text,
					 guint		 start_pos,
					 guint		 text_len,
					 const gchar	*str);
gboolean gtkut_stext_is_uri_string	(GtkSText	*text,
					 guint		 start_pos,
					 guint		 text_len);
void gtk_stext_clear			(GtkSText	*text);

void gtkut_widget_disable_theme_engine	(GtkWidget	*widget);
void gtkut_widget_wait_for_draw		(GtkWidget	*widget);
void gtkut_widget_get_uposition		(GtkWidget	*widget,
					 gint		*px,
					 gint		*py);
void gtkut_widget_init			(void);

void gtkut_widget_set_app_icon		(GtkWidget	*widget);
void gtkut_widget_set_composer_icon	(GtkWidget	*widget);

GtkWidget *gtkut_account_menu_new	(GList			*ac_list,
				  	 GtkSignalFunc	 	 callback,
					 gpointer		 data);
#endif /* __GTKUTILS_H__ */
