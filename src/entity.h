/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2017 Ricardo Mones and the Claws Mail team
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
#ifndef __ENTITY_H__
#define __ENTITY_H__

#include <glib.h>

/*
 * Try to decode the HTML entity pointed by str, whose first element
 * must be the '&' character.
 *
 * Returns a newly-allocated string with the decoded entity or NULL
 * on failure to decode (like an unknown or invalid entity).
 * Returned strings must be freed with g_free().
 */
gchar *entity_decode(gchar *str);

#endif /* __ENTITY_H__ */
