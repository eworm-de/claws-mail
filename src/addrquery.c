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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
static GList *_requestList_ = NULL;

/**
 * Mutex to protect list from multiple threads.
 */
static pthread_mutex_t _requestListMutex_ = PTHREAD_MUTEX_INITIALIZER;

/**
 * Current query ID. This is incremented for each query request created.
 */
static gint _currentQueryID_ = 0;

/**
 * Create new address query.
 * \return Initialized address query object.
 */
QueryRequest *reqreq_create( void ) {
	QueryRequest *req;

	req = g_new0( QueryRequest, 1 );
	req->queryID = 0;
	req->searchType = ADDRSEARCH_NONE;
	req->searchTerm = NULL;
	req->callBackEnd = NULL;
	req->callBackEntry = NULL;
	req->queryList = NULL;
	return req;
}

/**
 * Clear the query.
 * \param req Request query object.
 */
void qryreq_clear( QueryRequest *req ) {
	GList *node;

	g_return_if_fail( req != NULL );
	g_free( req->searchTerm );
	req->queryID = 0;
	req->searchType = ADDRSEARCH_NONE;
	req->searchTerm = NULL;
	req->callBackEnd = NULL;
	req->callBackEntry = NULL;

	/* Empty the list */
	node = req->queryList;
	while( node ) {
		node->data = NULL;
		node = g_list_next( node );
	}
	g_list_free( req->queryList );
	req->queryList = NULL;
}

/**
 * Free query.
 * \param req Request query object.
 */
void qryreq_free( QueryRequest *req ) {
	g_return_if_fail( req != NULL );
	qryreq_clear( req );
	g_free( req );
}

/**
 * Specify search type.
 * \param req   Request query object.
 * \param value Type.
 */
void qryreq_set_search_type( QueryRequest *req, const AddrSearchType value ) {
	g_return_if_fail( req != NULL );
	req->searchType = value;
}

/**
 * Specify search term to be used.
 * \param req   Request query object.
 * \param value Search term.
 */
void qryreq_set_search_term( QueryRequest *req, const gchar *value ) {
	req->searchTerm = mgu_replace_string( req->searchTerm, value );
	g_return_if_fail( req != NULL );
	g_strstrip( req->searchTerm );
}

/**
 * Add address query object to request.
 * \param req  Request query object.
 * \param aqo  Address query object that performs the search.
 */
void qryreq_add_query( QueryRequest *req, AddrQueryObject *aqo ) {
	g_return_if_fail( req != NULL );
	g_return_if_fail( aqo != NULL );
	req->queryList = g_list_append( req->queryList, aqo );
}

/**
 * Display object to specified stream.
 * \param req    Request query object.
 * \param stream Output stream.
 */
void qryreq_print( const QueryRequest *req, FILE *stream ) {
	GList *node;
	g_return_if_fail( req != NULL );

	fprintf( stream, "QueryRequest:\n" );
	fprintf( stream, "     queryID: %d\n",   req->queryID );
	fprintf( stream, "  searchType: %d\n",   req->searchType );
	fprintf( stream, "  searchTerm: '%s'\n", req->searchTerm );
	node = req->queryList;
	while( node ) {
		AddrQueryObject *aqo = node->data;
		fprintf( stream, "    --- type: %d\n", aqo->queryType );
		node = g_list_next( node );
	}
}

/**
 * Add query to list.
 *
 * \param searchTerm    Search term. A private copy will be made.
 * \param callBackEnd   Callback function that will be called when query
 * 			terminates.
 * \param callBackEntry Callback function that will be called after each
 * 			address entry has been read.
 * \return Initialize query request object. 			
 */
QueryRequest *qrymgr_add_request(
	const gchar *searchTerm, void *callBackEnd, void *callBackEntry )
{
	QueryRequest *req;

	req = g_new0( QueryRequest, 1 );
	req->searchTerm = g_strdup( searchTerm );
	req->callBackEnd = callBackEnd;
	req->callBackEntry = callBackEntry;
	req->timeStart = time( NULL );
	req->queryList = NULL;

	/* Insert in head of list */
	pthread_mutex_lock( & _requestListMutex_ );
	req->queryID = ++_currentQueryID_;
	_requestList_ = g_list_prepend( _requestList_, req );
	pthread_mutex_unlock( & _requestListMutex_ );

	return req;
}

/**
 * Find query in list.
 * \param  queryID ID of query to find.
 * \return Query object, or <i>NULL</i> if not found.
 */
QueryRequest *qrymgr_find_request( const gint queryID ) {
	QueryRequest *req;
	QueryRequest *q;
	GList *node;

	pthread_mutex_lock( & _requestListMutex_ );
	req = NULL;
	node = _requestList_;
	while( node ) {
		q = node->data;
		if( q->queryID == queryID ) {
			req = q;
			break;
		}
		node = g_list_next( node );
	}
	pthread_mutex_unlock( & _requestListMutex_ );

	return req;
}

/**
 * Delete specified query.
 * \param  queryID ID of query to retire.
 */
void qrymgr_delete_request( const gint queryID ) {
	QueryRequest *req;
	GList *node, *nf;

	pthread_mutex_lock( & _requestListMutex_ );

	/* Find node */
	nf = NULL;
	node = _requestList_;
	while( node ) {
		req = node->data;
		if( req->queryID == queryID ) {
			nf = node;
			qryreq_free( req );
			break;
		}
		node = g_list_next( node );
	}

	/* Free link element and associated query */
	if( nf ) {
		_requestList_ = g_list_remove_link( _requestList_, nf );
		g_list_free_1( nf );
	}

	pthread_mutex_unlock( & _requestListMutex_ );
}

/**
 * Initialize query manager.
 */
void qrymgr_initialize( void ) {
	_requestList_ = NULL;
}

/**
 * Free all queries.
 */
static void qrymgr_free_all_request( void ) {
	QueryRequest *req;
	GList *node;

	pthread_mutex_lock( & _requestListMutex_ );
	node = _requestList_;
	while( node ) {
		req = node->data;
		qryreq_free( req );
		node->data = NULL;
		node = g_list_next( node );
	}
	g_list_free( _requestList_ );
	_requestList_ = NULL;
	pthread_mutex_unlock( & _requestListMutex_ );
}

/**
 * Teardown query manager.
 */
void qrymgr_teardown( void ) {
	qrymgr_free_all_request();
}

/**
 * Display all queries to specified stream.
 * \param stream Output stream.
 */
void qrymgr_print( FILE *stream ) {
	QueryRequest *req;
	GList *node;

	pthread_mutex_lock( & _requestListMutex_ );
	fprintf( stream, "=== Query Manager ===\n" );
	node = _requestList_;
	while( node ) {
		req = node->data;
		qryreq_print( req, stream );
		fprintf( stream, "---\n" );
		node = g_list_next( node );
	}
	pthread_mutex_unlock( & _requestListMutex_ );
}

/*
* End of Source.
*/


