/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2001-2012 Hiroyuki Yamamoto & The Claws Mail Team
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

#if !defined(COLORLABEL_H__)
#define COLORLABEL_H__

#include <glib.h>
#include <gtk/gtk.h>

/* max value of color label index (0..max) - see also procmsg.h */
#define COLORLABELS 15

#define MAINWIN_COLORMENU 0
#define SUMMARY_COLORMENU 1
#define NUM_MENUS 2

/* Columns for model used in GtkComboBox color menu */
typedef enum {
	COLORMENU_COL_PIXBUF,
	COLORMENU_COL_TEXT,
	COLORMENU_COL_ID,
	NUM_COLORMENU_COLS
} ColorMenuColumn;

void colorlabel_update_colortable_from_prefs(void);
gint colorlabel_get_color_count			(void);
GdkRGBA colorlabel_get_color			(gint		 color_index);
GdkRGBA colorlabel_get_default_color	(gint		 color_index);
gchar *colorlabel_get_color_default_text	(gint		 color_index);
GtkImage *colorlabel_create_color_pixmap	(GdkColor	 color);
GtkWidget *colorlabel_create_check_color_menu_item
						(gint		 color_index,
						 gboolean	 force,
						 gint		 menu_index);
GtkWidget *colorlabel_create_color_menu		(void);
guint colorlabel_get_color_menu_active_item	(GtkWidget	*menu);

/* Creates a GtkComboBox with selection of configured colors */
GtkWidget *colorlabel_create_combobox_colormenu(void);

/* Resets contents of an existing combobox with matching
 * model. Can be useful after prefs, and therefore configured
 * colors, change. */
void colorlabel_refill_combobox_colormenu(GtkComboBox *combobox);

/* Returns index of selected color in the colormenu combobox.
 * 0 if "none" is selected, and 1-16 for colors 0-15. */
gint colorlabel_get_combobox_colormenu_active(GtkComboBox *combobox);

/* Select specified color entry in the colormenu combobox.
 * The color parameter corresponds to return value of
 * colorlabel_get_combobox_colormenu_active(). */
void colorlabel_set_combobox_colormenu_active(GtkComboBox *combobox,
		gint color);

#endif /* COLORLABEL_H__ */
