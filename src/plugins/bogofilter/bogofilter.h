/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto and the Claws Mail Team
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

#ifndef BOGOFILTER_H
#define BOGOFILTER_H 1

#include <glib.h>

typedef struct _BogofilterConfig BogofilterConfig;

typedef void (*MessageCallback) (gchar *, gint total, gint done, gboolean thread_safe);

struct _BogofilterConfig
{
	gboolean		 process_emails;
	gboolean 		 receive_spam;
	gchar 			*save_folder;
	guint 			 max_size;
	gchar			*bogopath;
};

BogofilterConfig *bogofilter_get_config	      (void);
void		    bogofilter_save_config	      (void);
void 	            bogofilter_set_message_callback (MessageCallback callback);
gint bogofilter_gtk_init(void);
void bogofilter_gtk_done(void);
int bogofilter_learn(MsgInfo *msginfo, GSList *msglist, gboolean spam);
void bogofilter_register_hook(void);
void bogofilter_unregister_hook(void);
#endif
