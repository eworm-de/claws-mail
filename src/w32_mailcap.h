/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2002 Thorsten Maerz
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

#ifndef __W32_MAILCAP_H__
#define __W32_MAILCAP_H__

#include <glib.h>

#define W32_MC_ERR_READ_MAILCAP		-1
#define W32_MC_ERR_LIST_EXIST		-2

gint w32_mailcap_create(void);
void w32_mailcap_free(void);
gchar *w32_mailcap_lookup(const gchar*);

#endif	/* __W32_MAILCAP_H__ */
