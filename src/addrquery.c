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

#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <pthread.h>

#include "mgutils.h"
#include "addrquery.h"

/**
 * Query list for tracking current queries.
 */
static GList *_queryList_ = NULL;

/**
 * Mutex to protect list from multiple threads.
 */
static pthread_mutex_t _queryListMutex_ = PTHREAD_MUTEX_INITIALIZER;

/**
 * Create new address query.
 * \return Initialized address query object.
 */
AddrQuery *addrqry_create( void ) {
	AddrQuery *qry;

	qry = g_new0( AddrQuery, 1 );
	qry->queryType = ADDRQUERY_NONE;
	qry->queryID = 0;
	qry->idleID = 0;
	qry->searchTerm = NULL;
	qry->callBack = NULL;
	qry->target = NULL;
	qry->serverObject = NULL;
	qry->queryObject = NULL;
	return qry;
}

/**
 * Clear the query.
 * \param qry Address query object.
 */
void addrqry_clear( AddrQuery *qry ) {
	g_return_if_fail( qry != NULL );
	g_free( qry->searchTerm );
	qry->queryType = ADDRQUERY_NONE;
	qry->queryID = 0;
	qry->idleID = 0;
	qry->searchTerm = NULL;
	qry->callBack = NULL;
	qry->target = NULL;
	qry->serverObject = NULL;
	qry->queryObject = NULL;
}

/**
 * Free query.
 * \param qry Address query object.
 */
void addrqry_free( AddrQuery *qry ) {
	g_return_if_fail( qry != NULL );
	addrqry_clear( qry );
	g_free( qry );
}

/**
 * Specify query type.
 * \param qry   Address query object.
 * \param value Type.
 */
void addrqry_set_query_type( AddrQuery *qry, const AddrQueryType value ) {
	g_return_if_fail( qry != NULL );
	qry->queryType = value;
}

/**
 * Specify idle ID.
 * \param qry   Address query object.
 * \param value Idle ID.
 */
void addrqry_set_idle_id( AddrQuery *qry, const guint value ) {
	g_return_if_fail( qry != NULL );
	qry->idleID = value;
}

/**
 * Specify search term to be used.
 * \param qry   Address query object.
 * \param value Search term.
 */
void addrqry_set_search_term( AddrQuery* qry, const gchar *value ) {
	qry->searchTerm = mgu_replace_string( qry->searchTerm, value );
	g_return_if_fail( qry != NULL );
	g_strstrip( qry->searchTerm );
}

/**
 * Specify server object to be used.
 * \param qry   Address query object.
 * \param value Server object that performs the search.
 */
void addrqry_set_server( AddrQuery* qry, const gpointer value ) {
	g_return_if_fail( qry != NULL );
	qry->serverObject = value;
}

/**
 * Specify query object to be used.
 * \param qry   Address query object.
 * \param value Query object that performs the search.
 */
void addrqry_set_query( AddrQuery* qry, const gpointer value ) {
	g_return_if_fail( qry != NULL );
	qry->queryObject = value;
}

/**
 * Display object to specified stream.
 * \param qry    Address query object.
 * \param stream Output stream.
 */
void addrqry_print( const AddrQuery *qry, FILE *stream ) {
	g_return_if_fail( qry != NULL );

	fprintf( stream, "AddressQuery:\n" );
	fprintf( stream, "     queryID: %d\n",   qry->queryID );
	fprintf( stream, "      idleID: %d\n",   qry->idleID );
	fprintf( stream, "  searchTerm: '%s'\n", qry->searchTerm );
}

/**
 * Add query to list.
 * \param queryID    ID of query being executed.
 * \param searchTerm Search term. A private copy will be made.
 * \param callBack   Callback function.
 * \param target     Target object to receive data.
 */
AddrQuery *qrymgr_add_query(
	const gint queryID, const gchar *searchTerm, void *callBack,
	gpointer target )
{
	AddrQuery *qry;

	qry = g_new0( AddrQuery, 1 );
	qry->queryType = ADDRQUERY_NONE;
	qry->queryID = queryID;
	qry->idleID = 0;
	qry->searchTerm = g_strdup( searchTerm );
	qry->callBack = callBack;
	qry->target = NULL;
	qry->timeStart = time( NULL );
	qry->serverObject = NULL;
	qry->queryObject = NULL;

	/* Insert in head of list */
	pthread_mutex_lock( & _queryListMutex_ );
	_queryList_ = g_list_prepend( _queryList_, qry );
	pthread_mutex_unlock( & _queryListMutex_ );

	return qry;
}

/**
 * Find query in list.
 * \param  queryID ID of query to find.
 * \return Query object, or <i>NULL</i> if not found.
 */
AddrQuery *qrymgr_find_query( const gint queryID ) {
	AddrQuery *qry;
	AddrQuery *q;
	GList *node;

	pthread_mutex_lock( & _queryListMutex_ );
	qry = NULL;
	node = _queryList_;
	while( node ) {
		q = node->data;
		if( q->queryID == queryID ) {
			qry = q;
			break;
		}
		node = g_list_next( node );
	}
	pthread_mutex_unlock( & _queryListMutex_ );

	return qry;
}

/**
 * Delete specified query.
 * \param  queryID ID of query to retire.
 */
void qrymgr_delete_query( const gint queryID ) {
	AddrQuery *qry;
	GList *node, *nf;

	pthread_mutex_lock( & _queryListMutex_ );

	/* Find node */
	nf = NULL;
	node = _queryList_;
	while( node ) {
		qry = node->data;
		if( qry->queryID == queryID ) {
			nf = node;
			addrqry_free( qry );
			break;
		}
		node = g_list_next( node );
	}

	/* Free link element and associated query */
	if( nf ) {
		_queryList_ = g_list_remove_link( _queryList_, nf );
		g_list_free_1( nf );
	}

	pthread_mutex_unlock( & _queryListMutex_ );
}

/**
 * Initialize query manager.
 */
void qrymgr_initialize( void ) {
	_queryList_ = NULL;
}

/**
 * Free all queries.
 */
static void qrymgr_free_all_query( void ) {
	AddrQuery *qry;
	GList *node;

	pthread_mutex_lock( & _queryListMutex_ );
	node = _queryList_;
	while( node ) {
		qry = node->data;
		addrqry_free( qry );
		node->data = NULL;
		node = g_list_next( node );
	}
	g_list_free( _queryList_ );
	_queryList_ = NULL;
	pthread_mutex_unlock( & _queryListMutex_ );
}

/**
 * Teardown query manager.
 */
void qrymgr_teardown( void ) {
	qrymgr_free_all_query();
}

/**
 * Display all queries to specified stream.
 * \param stream Output stream.
 */
void qrymgr_print( FILE *stream ) {
	AddrQuery *qry;
	GList *node;

	pthread_mutex_lock( & _queryListMutex_ );
	fprintf( stream, "=== Query Manager ===\n" );
	node = _queryList_;
	while( node ) {
		qry = node->data;
		addrqry_print( qry, stream );
		fprintf( stream, "---\n" );
		node = g_list_next( node );
	}
	pthread_mutex_unlock( & _queryListMutex_ );
}

/*
* End of Source.
*/


