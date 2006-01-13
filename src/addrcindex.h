/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2002-2006 Match Grun and the Sylpheed-Claws team
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

/*
 * Functions to maintain address completion index.
 */

#ifndef __ADDRCINDEX_H__
#define __ADDRCINDEX_H__

#include <glib.h>
#include <stdio.h>

#include "addritem.h"

/*
 * Constants.
 */

/* Data structures */
typedef struct {
	GCompletion *completion;
	GList       *addressList;
	gboolean    invalid;
}
AddrCacheIndex;

/* Function prototypes */
AddrCacheIndex *addrcindex_create	( void );
void addrcindex_clear		( AddrCacheIndex *index );
void addrcindex_free		( AddrCacheIndex *index );
void addrcindex_invalidate	( AddrCacheIndex *index );
void addrcindex_validate	( AddrCacheIndex *index );
void addrcindex_add_entry	( AddrCacheIndex *index,
				  gchar *name,
				  ItemEMail *email );
void addrcindex_add_email	( AddrCacheIndex *index, ItemEMail *email );
void addrcindex_add_person	( AddrCacheIndex *index, ItemPerson *person );
void addrcindex_print		( AddrCacheIndex *index, FILE *stream );
GList *addrcindex_search	( AddrCacheIndex *index, const gchar *search );

#endif /* __ADDRCINDEX_H__ */

/*
* End of Source.
*/
