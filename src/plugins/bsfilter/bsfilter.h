/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2009 Colin Leroy <colin@colino.net> and 
 * the Claws Mail team
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

#ifndef BSFILTER_H
#define BSFILTER_H 1

#include <glib.h>
#include "folder.h"

typedef struct _BsfilterConfig BsfilterConfig;

typedef void (*MessageCallback) (gchar *, gint total, gint done, gboolean thread_safe);

struct _BsfilterConfig
{
	gboolean		 process_emails;
	gboolean 		 receive_spam;
	gchar 			*save_folder;
	guint 			 max_size;
	gchar			*bspath;
	gboolean		 whitelist_ab;
	gchar			*whitelist_ab_folder;
	gboolean		 learn_from_whitelist;
	gboolean		 mark_as_read;
};

BsfilterConfig *bsfilter_get_config	      (void);
void		    bsfilter_save_config	      (void);
void 	            bsfilter_set_message_callback (MessageCallback callback);
gint bsfilter_gtk_init(void);
void bsfilter_gtk_done(void);
int bsfilter_learn(MsgInfo *msginfo, GSList *msglist, gboolean spam);
void bsfilter_register_hook(void);
void bsfilter_unregister_hook(void);
FolderItem *bsfilter_get_spam_folder(MsgInfo *msginfo);
#endif
