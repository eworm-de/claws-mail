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

#ifndef __HEADERS_DISPLAY_H__
#define __HEADERS_DISPLAY_H__

struct _HeaderDisplayProp
{
	gchar *name;
	int hidden;
};

typedef struct _HeaderDisplayProp HeaderDisplayProp;

gchar * header_display_prop_get_str(HeaderDisplayProp *dp);
HeaderDisplayProp * header_display_prop_read_str(gchar * buf);
void header_display_prop_free(HeaderDisplayProp *dp);

#endif /* __DISPLAY_H__ */
