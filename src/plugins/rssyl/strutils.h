/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2005-2023 the Claws Mail Team and Andrej Kacian <andrej@kacian.sk>
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __STRUTILS_H
#define __STRUTILS_H

#include <glib.h>

gchar *rssyl_strreplace(gchar *source, gchar *pattern,
		gchar *replacement);

gchar *rssyl_replace_html_stuff(gchar *text,
		gboolean symbols, gboolean tags);

gchar *rssyl_format_string(gchar *str, gboolean replace_html,
		gboolean strip_nl);

gchar **strsplit_no_copy(gchar *str, char delimiter);

void strip_html(gchar *str);

gchar *my_normalize_url(const gchar *url);

#endif /* __STRUTILS_H */
