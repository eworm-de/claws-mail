/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2003 Match Grun
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

/*
 * Functions to define an address query (a request).
 */

#ifndef __ADDRQUERY_H__
#define __ADDRQUERY_H__

#include <glib.h>
#include <stdio.h>
#ifndef WIN32
#include <sys/time.h>
#endif

/* Query types */
typedef enum {
	ADDRQUERY_NONE,
	ADDRQUERY_LDAP
} AddrQueryType;

/* Address search call back function */
typedef gint ( AddrSearchCallbackFunc ) ( gint cacheID,
					  GList *listEMail,
					  gpointer target );

typedef void ( AddrSearchStaticFunc ) ( gint qid, AddrQueryType qty, gint status );

/* Data structures */
typedef struct {
	AddrQueryType queryType;
	gint     queryID;
	gint     idleID;
	gchar    *searchTerm;
	time_t   timeStart;
	AddrSearchStaticFunc *callBack;
	gpointer target;
	gpointer serverObject;
	gpointer queryObject;
}
AddrQuery;

/* Function prototypes */
AddrQuery *addrqry_create	( void );
void addrqry_clear		( AddrQuery *qry );
void addrqry_free		( AddrQuery *qry );
void addrqry_set_query_type	( AddrQuery *qry, const AddrQueryType value );
void addrqry_set_idle_id	( AddrQuery *qry, const guint value );
void addrqry_set_search_term	( AddrQuery* qry, const gchar *value );
void addrqry_set_server		( AddrQuery* qry, const gpointer server );
void addrqry_set_query		( AddrQuery* qry, const gpointer query );
void addrqry_print		( const AddrQuery *qry, FILE *stream );

void qrymgr_initialize		( void );
void qrymgr_teardown		( void );
AddrQuery *qrymgr_add_query(
		const gint queryID, const gchar *searchTerm,
		void *callBack, gpointer target );

AddrQuery *qrymgr_find_query	( const gint queryID );
void qrymgr_delete_query	( const gint queryID );
void qrymgr_print		( FILE *stream );

#endif /* __ADDRQUERY_H__ */

/*
* End of Source.
*/
