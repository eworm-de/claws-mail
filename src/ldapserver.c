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
 * Functions necessary to access LDAP servers.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_LDAP

#include <glib.h>
#include <sys/time.h>
#include <string.h>
#include <ldap.h>
#include <lber.h>

#include "mgutils.h"
#include "addritem.h"
#include "addrcache.h"
#include "ldapctrl.h"
#include "ldapquery.h"
#include "ldapserver.h"
#include "utils.h"
#include "adbookbase.h"

/**
 * Create new LDAP server interface object with no control object.
 * \return Initialized LDAP server object.
 */
LdapServer *ldapsvr_create_noctl( void ) {
	LdapServer *server;

	server = g_new0( LdapServer, 1 );
	server->type = ADBOOKTYPE_LDAP;
	server->addressCache = addrcache_create();
	server->retVal = MGU_SUCCESS;
	server->control = NULL;
	server->listQuery = NULL;
	server->searchFlag = FALSE;
	return server;
}

/**
 * Create new LDAP server interface object.
 * \return Initialized LDAP server object.
 */
LdapServer *ldapsvr_create( void ) {
	LdapServer *server;

	server = ldapsvr_create_noctl();
	server->control = ldapctl_create();
	return server;
}

/**
 * Return name of server.
 * \param  server Server object.
 * \return Name for server.
 */
gchar *ldapsvr_get_name( LdapServer *server ) {
	g_return_val_if_fail( server != NULL, NULL );
	return addrcache_get_name( server->addressCache );
}

/**
 * Specify name to be used.
 * \param server Server object.
 * \param value      Name for server.
 */
void ldapsvr_set_name( LdapServer* server, const gchar *value ) {
	g_return_if_fail( server != NULL );
	addrcache_set_name( server->addressCache, value );
}

/**
 * Refresh internal variables to force a file read.
 * \param server Server object.
 */
void ldapsvr_force_refresh( LdapServer *server ) {
	addrcache_refresh( server->addressCache );
}

/**
 * Return status/error code.
 * \param  server Server object.
 * \return Status/error code.
 */
gint ldapsvr_get_status( LdapServer *server ) {
	g_return_val_if_fail( server != NULL, -1 );
	return server->retVal;
}

/**
 * Return reference to root level folder.
 * \param  server Server object.
 * \return Root level folder.
 */
ItemFolder *ldapsvr_get_root_folder( LdapServer *server ) {
	g_return_val_if_fail( server != NULL, NULL );
	/*
	printf( "ldapsvr_get_root_folder/start\n" );
	ldapsvr_print_data( server, stdout );
	printf( "ldapsvr_get_root_folder/done\n" );
	*/
	return addrcache_get_root_folder( server->addressCache );
}

/**
 * Test whether server data has been accessed.
 * \param  server Server object.
 * \return <i>TRUE</i> if data was accessed.
 */
gboolean ldapsvr_get_accessed( LdapServer *server ) {
	g_return_val_if_fail( server != NULL, FALSE );
	return server->addressCache->accessFlag;
}

/**
 * Specify that server's data whas beed accessed.
 * \param server Server object.
 * \param value      Value for flag.
 */
void ldapsvr_set_accessed( LdapServer *server, const gboolean value ) {
	g_return_if_fail( server != NULL );
	server->addressCache->accessFlag = value;
}

/**
 * Test whether server data has been modified.
 * \param  server Server object.
 * \return <i>TRUE</i> if data was modified.
 */
gboolean ldapsvr_get_modified( LdapServer *server ) {
	g_return_val_if_fail( server != NULL, FALSE );
	return server->addressCache->modified;
}

/**
 * Specify modify flag.
 * \param server Server object.
 * \param value      Value for flag.
 */
void ldapsvr_set_modified( LdapServer *server, const gboolean value ) {
	g_return_if_fail( server != NULL );
	server->addressCache->modified = value;
}

/**
 * Test whether data was read from server.
 * \param server Server object.
 * \return <i>TRUE</i> if data was read.
 */
gboolean ldapsvr_get_read_flag( LdapServer *server ) {
	g_return_val_if_fail( server != NULL, FALSE );
	return server->addressCache->dataRead;
}

/**
 * Test whether server is to be used for dynamic searches.
 * \param server Server object.
 * \return <i>TRUE</i> if server is used for dynamic searches.
 */
gboolean ldapsvr_get_search_flag( LdapServer *server ) {
	g_return_val_if_fail( server != NULL, FALSE );
	return server->searchFlag;
}

/**
 * Specify that server is to be used for dynamic searches.
 * \param server Server object.
 * \param value      Name for server.
 */
void ldapsvr_set_search_flag( LdapServer *server, const gboolean value ) {
	g_return_if_fail( server != NULL );
	server->searchFlag = value;
}

/**
 * Specify the reference to control data that will be used for the query. The calling
 * module should be responsible for creating and destroying this control object.
 * \param server Server object.
 * \param ctl    Control data.
 */
void ldapsvr_set_control( LdapServer *server, LdapControl *ctl ) {
	g_return_if_fail( server != NULL );
	addrcache_refresh( server->addressCache );
	server->control = ctl;
}

/**
 * Release LDAP control object.
 * \param server Server object.
 */
static void ldapsvr_release_control( LdapServer *server ) {
	g_return_if_fail( server != NULL );
	ldapctl_free( server->control );
	server->control = NULL;
}

/**
 * Free all queries.
 * \param server Server object.
 */
void ldapsvr_free_all_query( LdapServer *server ) {
	GList *node;	
	g_return_if_fail( server != NULL );

	node = server->listQuery;
	while( node ) {
		LdapQuery *qry = node->data;
		ldapqry_free( qry );
		node->data = NULL;
		node = g_list_next( node );
	}
	g_list_free( server->listQuery );
	server->listQuery = NULL;
}

/**
 * Add query to server.
 * \param server Server object.
 * \param qry    Query object.
 */
void ldapsvr_add_query( LdapServer *server, LdapQuery *qry ) {
	g_return_if_fail( server != NULL );
	g_return_if_fail( qry != NULL );

	server->listQuery = g_list_append( server->listQuery, qry );
	qry->server = server;
}

/**
 * Free up LDAP server interface object by releasing internal memory.
 * \param server Server object.
 */
void ldapsvr_free( LdapServer *server ) {
	g_return_if_fail( server != NULL );

	/* Stop and cancel any queries that may be active */
	ldapsvr_stop_all_query( server );
	ldapsvr_cancel_all_query( server );

	/* Clear cache */
	addrcache_clear( server->addressCache );
	addrcache_free( server->addressCache );

	/* Free LDAP control block */
	ldapctl_free( server->control );
	server->control = NULL;

	/* Free all queries */
	ldapsvr_free_all_query( server );

	/* Clear pointers */
	server->type = ADBOOKTYPE_NONE;
	server->addressCache = NULL;
	server->retVal = MGU_SUCCESS;
	server->listQuery = NULL;
	server->searchFlag = FALSE;

	/* Now release LDAP object */
	g_free( server );
}

/**
 * Display object to specified stream.
 * \param server Server object.
 * \param stream     Output stream.
 */
void ldapsvr_print_data( LdapServer *server, FILE *stream ) {
	GList *node;
	gint  i;

	g_return_if_fail( server != NULL );

	fprintf( stream, "LdapServer:\n" );
	fprintf( stream, "  ret val: %d\n", server->retVal );
	fprintf( stream, "srch flag: %s\n",
			server->searchFlag ? "yes" : "no" );
	if( server->control ) {
		ldapctl_print( server->control, stream );
	}
	else {
		fprintf( stream, "  control: NULL\n" );
	}
	addrcache_print( server->addressCache, stream );
	addritem_print_item_folder( server->addressCache->rootFolder, stream );

	/* Dump queries */
	i = 1;
	node = server->listQuery;
	while( node ) {
		LdapQuery *qry = node->data;
		fprintf( stream, "    query: %2d : %s\n", i, qry->queryName );
		i++;
		node = g_list_next( node );
	}
}

/**
 * Return link list of persons.
 * \param server Server object.
 * \return List of persons.
 */
GList *ldapsvr_get_list_person( LdapServer *server ) {
	g_return_val_if_fail( server != NULL, NULL );
	return addrcache_get_list_person( server->addressCache );
}

/**
 * Return link list of folders. There are no "real" folders that are returned
 * from the server.
 * \param  server Server object.
 * \return List of folders.
 */
GList *ldapsvr_get_list_folder( LdapServer *server ) {
	g_return_val_if_fail( server != NULL, NULL );
	/* return addrcache_get_list_folder( server->addressCache ); */
	return NULL;
}

/**
 * Execute specified query.
 * \param server LDAP server.
 * \param qry    LDAP query.
 */
void ldapsvr_execute_query( LdapServer *server, LdapQuery *qry ) {
	LdapControl *ctlCopy;

	g_return_if_fail( server != NULL );
	g_return_if_fail( qry != NULL );

	/* Copy server's control data to the query */
	ctlCopy = ldapctl_create();
	ldapctl_copy( server->control, ctlCopy );
	ldapqry_set_control( qry, ctlCopy );
	ldapqry_initialize();

	/* Perform query */	
	/* printf( "ldapsvr_execute_query::reading with thread...\n" ); */
	if( ldapqry_check_search( qry ) ) {
		ldapqry_read_data_th( qry );
		/*
		if( qry->retVal == LDAPRC_SUCCESS ) {
			printf( "ldapsvr_execute_query::SUCCESS with thread...\n" );
		}
		*/
	}
	/* printf( "ldapsvr_execute_query... terminated\n" ); */
}

/**
 * Stop all queries for specified ID.
 * \param server Server object.
 * \param queryID    Query ID to stop.
 */
void ldapsvr_stop_query_id( LdapServer *server, const gint queryID ) {
	GList *node;	
	g_return_if_fail( server != NULL );

	node = server->listQuery;
	while( node ) {
		LdapQuery *qry = node->data;
		if( qry->queryID == queryID ) {
			/* Notify thread to stop */
			ldapqry_set_stop_flag( qry, TRUE );
		}
		node = g_list_next( node );
	}
}

/**
 * Stop all queries by notifying each thread to stop.
 * \param server Server object.
 */
void ldapsvr_stop_all_query( LdapServer *server ) {
	GList *node;	
	g_return_if_fail( server != NULL );

	node = server->listQuery;
	while( node ) {
		LdapQuery *qry = node->data;
		ldapqry_set_stop_flag( qry, TRUE );
		node = g_list_next( node );
	}
}

/**
 * Cancel all query threads for server.
 * \param server Server object.
 */
void ldapsvr_cancel_all_query( LdapServer *server ) {
	GList *node;	
	g_return_if_fail( server != NULL );

	node = server->listQuery;
	while( node ) {
		LdapQuery *qry = node->data;
		/* Notify thread to stop */
		ldapqry_set_stop_flag( qry, TRUE );
		/* Now cancel thread */
		ldapqry_cancel( qry );
		node = g_list_next( node );
	}
}

/**
 * Search most recent query for specified search term. The most recent
 * completed query is returned. If no completed query is found, the most recent
 * incomplete is returned.
 * \param server LdapServer.
 * \param searchTerm Search term to locate.
 * \return Query object, or <i>NULL</i> if none found.
 */
LdapQuery *ldapsvr_locate_query( LdapServer *server, const gchar *searchTerm )
{
	LdapQuery *incomplete = NULL;
	GList *node;	
	g_return_if_fail( server != NULL );

	node = server->listQuery;
	node = g_list_last( node );
	/* Search backwards for query */
	while( node ) {
		LdapQuery *qry = node->data;
		if( g_strcasecmp( qry->searchValue, searchTerm ) == 0 ) {
			if( qry->agedFlag ) continue;
			if( qry->completed ) {
				/* Found */
				return qry;
			}
			if( ! incomplete ) {
				incomplete = qry;
			}
		}
		node = g_list_previous( node );
	}
	return incomplete;
}

/**
 * Retire aged queries. Only dynamic queries are retired.
 * \param server LdapServer.
 */
void ldapsvr_retire_query( LdapServer *server ) {
	GList *node;
	GList *listDelete;
	GList *listQuery;
	gint maxAge;
	LdapControl *ctl;

	/* printf( "ldapsvr_retire_query\n" ); */
	g_return_if_fail( server != NULL );
	ctl = server->control;
	maxAge = ctl->maxQueryAge;

	/* Identify queries to age and move to deletion list */
	listDelete = NULL;
	node = server->listQuery;
	while( node ) {
		LdapQuery *qry = node->data;

		node = g_list_next( node );
		if( qry->queryType == LDAPQUERY_STATIC ) continue;

		/* Only dynamic queries are retired */
		ldapqry_age( qry, maxAge );
		if( qry->agedFlag ) {
			/* Delete folder associated with query */
			/*
			printf( "deleting folder... ::%s::\n", qry->queryName );
			*/
			ldapqry_delete_folder( qry );
			listDelete = g_list_append( listDelete, qry );
		}
	}

	/* Delete queries */
	listQuery = server->listQuery;
	node = listDelete;
	while( node ) {
		LdapQuery *qry = node->data;

		listQuery = g_list_remove( listQuery, qry );
		ldapqry_free( qry );
		node->data = NULL;
		node = g_list_next( node );
	}
	server->listQuery = listQuery;

	/* Free up deletion list */
	g_list_free( listDelete );
}

#endif	/* USE_LDAP */

/*
 * End of Source.
 */

