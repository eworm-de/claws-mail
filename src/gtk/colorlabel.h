/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2001 Hiroyuki Yamamoto & The Claws Mail Team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#if !defined(COLORLABEL_H__)
#define COLORLABEL_H__

#include <glib.h>
#include <gtk/gtkimage.h>

#define COLORLABELS 7
#define MAINWIN_COLORMENU 0
#define SUMMARY_COLORMENU 1
#define NUM_MENUS 2

void colorlabel_update_colortable_from_prefs(void);
gint colorlabel_get_color_count			(void);
GdkColor colorlabel_get_color			(gint		 color_index);
GdkColor colorlabel_get_default_color	(gint		 color_index);
gchar *colorlabel_get_color_text		(gint		 color_index);
gchar *colorlabel_get_color_default_text	(gint		 color_index);
GtkImage *colorlabel_create_color_pixmap	(GdkColor	 color);
gboolean colorlabel_changed			(void);
GtkWidget *colorlabel_create_check_color_menu_item
						(gint		 color_index,
						 gboolean	 force,
						 gint		 menu_index);
GtkWidget *colorlabel_create_color_menu		(void);
guint colorlabel_get_color_menu_active_item	(GtkWidget	*menu);

#endif /* COLORLABEL_H__ */
