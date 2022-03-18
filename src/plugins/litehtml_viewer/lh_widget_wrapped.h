/*
 * Claws Mail -- A GTK based, lightweight, and fast e-mail client
 * Copyright(C) 2019 the Claws Mail Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write tothe Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __LH_WIDGET_WRAPPED_H
#define __LH_WIDGET_WRAPPED_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lh_widget lh_widget_wrapped;

lh_widget_wrapped *lh_widget_new();
GtkWidget *lh_widget_get_widget(lh_widget_wrapped *w);
void lh_widget_open_html(lh_widget_wrapped *w, const gchar *path);
void lh_widget_clear(lh_widget_wrapped *w);
void lh_widget_destroy(lh_widget_wrapped *w);
void lh_widget_statusbar_push(const gchar* msg);
void lh_widget_statusbar_pop();
void lh_widget_print(lh_widget_wrapped *w);
void lh_widget_set_partinfo(lh_widget_wrapped *w, MimeInfo *partinfo);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* __LH_WIDGET_WRAPPED_H */
