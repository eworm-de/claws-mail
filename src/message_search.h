/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2012 Hiroyuki Yamamoto and the Claws Mail team
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

#ifndef __MESSAGE_SEARCH_H__
#define __MESSAGE_SEARCH_H__

#include <glib.h>

#include "messageview.h"
#include "compose.h"

typedef void (*SetPositionFunc)(void *obj, gint pos);
typedef gboolean (*SearchStringFunc)(void *obj,
	const gchar *str, gboolean case_sens);

typedef struct {
	SetPositionFunc set_position;
	SearchStringFunc search_string;
	SearchStringFunc search_string_backward;
} SearchInterface;

void message_search	(MessageView	*messageview);
void message_search_compose	(Compose	*compose);
void message_search_other	(SearchInterface	*source, void *obj);
void message_search_close	(void *obj);

#endif /* __MESSAGE_SEARCH_H__ */
