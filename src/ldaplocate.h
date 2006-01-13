/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2003-2006 Match Grun and the Sylpheed-Claws team
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
 * Functions to perform searches for LDAP entries.
 */

#ifndef __LDAPLOCATE_H__
#define __LDAPLOCATE_H__

#ifdef USE_LDAP

#include <glib.h>
#include "ldapserver.h"

/* Function prototypes */
gint ldaplocate_search_setup	( LdapServer *server,
				  const gchar *searchTerm,
				  void *callBackEntry,
				  void *callBackEnd );
gboolean ldaplocate_search_start( const gint queryID );
void ldaplocate_search_stop	( const gint queryID );

#endif	/* USE_LDAP */

#endif /* __LDAPLOCATE_H__ */


