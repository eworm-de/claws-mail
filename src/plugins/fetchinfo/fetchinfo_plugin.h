/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto and the Claws Mail Team
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef FETCHINFO_PLUGIN_H
#define FETCHINFO_PLUGIN_H 1

#include <glib.h>

typedef struct _FetchinfoConfig FetchinfoConfig;

struct _FetchinfoConfig
{
	gboolean	 fetchinfo_enable;
	gboolean	 fetchinfo_uidl;
	gboolean	 fetchinfo_account;
	gboolean	 fetchinfo_server;
	gboolean	 fetchinfo_userid;
	gboolean	 fetchinfo_time;
};

FetchinfoConfig *fetchinfo_get_config		(void);
void	      fetchinfo_save_config	(void);
gint fetchinfo_gtk_init(void);
void fetchinfo_gtk_done(void);

#endif
