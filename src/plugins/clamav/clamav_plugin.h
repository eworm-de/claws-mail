/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2003-2007 the Claws Mail Team
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

#ifndef CLAMAV_PLUGIN_H
#define CLAMAV_PLUGIN_H 1

#include <glib.h>

typedef struct _ClamAvConfig ClamAvConfig;

typedef void (*MessageCallback) (gchar *);

struct _ClamAvConfig
{
	gboolean	 clamav_enable;
	gboolean 	 clamav_enable_arc;
	guint 		 clamav_max_size;
	gboolean 	 clamav_recv_infected;
	gchar 		*clamav_save_folder;
};

ClamAvConfig *clamav_get_config		  (void);
void	      clamav_save_config	  (void);
void 	      clamav_set_message_callback (MessageCallback callback);
gint	      clamav_gtk_init(void);
void 	      clamav_gtk_done(void);

#endif
