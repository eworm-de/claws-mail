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
 * Functions necessary to define and perform LDAP queries.
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

#include "ldapquery.h"
#include "ldapctrl.h"
#include "mgutils.h"

#include "addritem.h"
#include "addrcache.h"

/*
 * Key for thread specific data.
 */
static pthread_key_t _queryThreadKey_;
static gboolean _queryThreadInit_ = FALSE;

/**
 * Create new LDAP query object.
 * \return Initialized query object.
 */
LdapQuery *ldapqry_create( void ) {
	LdapQuery *qry;

	qry = g_new0( LdapQuery, 1 );
	qry->control = NULL;
	qry->retVal = LDAPRC_SUCCESS;
	qry->queryType = LDAPQUERY_NONE;
	qry->queryName = NULL;
	qry->searchValue = NULL;
	qry->queryID = 0;
	qry->entriesRead = 0;
	qry->elapsedTime = 0;
	qry->stopFlag = FALSE;
	qry->busyFlag = FALSE;
	qry->agedFlag = FALSE;
	qry->completed = FALSE;
	qry->thread = NULL;
	qry->callBackStart = NULL;
	qry->callBackEntry = NULL;
	qry->callBackEnd = NULL;
	qry->folder = NULL;
	qry->server = NULL;

	/* Mutex to protect stop and busy flags */
	qry->mutexStop = g_malloc0( sizeof( pthread_mutex_t ) );
	pthread_mutex_init( qry->mutexStop, NULL );
	qry->mutexBusy = g_malloc0( sizeof( pthread_mutex_t ) );
	pthread_mutex_init( qry->mutexBusy, NULL );

	/* Mutex to protect critical section */
	qry->mutexEntry = g_malloc0( sizeof( pthread_mutex_t ) );
	pthread_mutex_init( qry->mutexEntry, NULL );

	return qry;
}

/**
 * Specify the reference to control data that will be used for the query. The calling
 * module should be responsible for creating and destroying this control object.
 * \param qry Query object.
 * \param ctl Control object.
 */
void ldapqry_set_control( LdapQuery *qry, LdapControl *ctl ) {
	g_return_if_fail( qry != NULL );
	qry->control = ctl;
}

/**
 * Specify query name to be used.
 * \param qry   Query object.
 * \param value Name.
 */
void ldapqry_set_name( LdapQuery* qry, const gchar *value ) {
	qry->queryName = mgu_replace_string( qry->queryName, value );
	g_strstrip( qry->queryName );
}

/**
 * Specify search value to be used.
 * \param qry Query object.
 * \param value 
 */
void ldapqry_set_search_value( LdapQuery *qry, const gchar *value ) {
	qry->searchValue = mgu_replace_string( qry->searchValue, value );
	g_strstrip( qry->searchValue );
}

/**
 * Specify error/status.
 * \param qry   Query object.
 * \param value Status.
 */
void ldapqry_set_error_status( LdapQuery* qry, const gint value ) {
	qry->retVal = value;
}

/**
 * Specify query type.
 * \param qry Query object.
 * \param value Query type, either:
 * <ul>
 * <li><code>LDAPQUERY_NONE</code></li>
 * <li><code>LDAPQUERY_STATIC</code></li>
 * <li><code>LDAPQUERY_DYNAMIC</code></li>
 * </ul>
 */
void ldapqry_set_query_type( LdapQuery* qry, const gint value ) {
	qry->queryType = value;
}

/**
 * Specify query ID.
 * \param qry Query object.
 * \param value ID for the query.
 */
void ldapqry_set_query_id( LdapQuery* qry, const gint value ) {
	qry->queryID = value;
}

/**
 * Specify maximum number of LDAP entries to retrieve.
 * \param qry Query object.
 * \param value Entries to read.
 */
void ldapqry_set_entries_read( LdapQuery* qry, const gint value ) {
	if( value > 0 ) {
		qry->entriesRead = value;
	}
	else {
		qry->entriesRead = 0;
	}
}

/**
 * Register a callback function that will be executed when the search
 * starts. When called, the function will be passed this query object
 * as an argument.
 * \param qry Query object.
 * \param func Function.
 */
void ldapqry_set_callback_start( LdapQuery *qry, void *func ) {
	qry->callBackStart = func;
}

/**
 * Register a callback function that will be executed when each entry
 * has been read and processed. When called, the function will be passed
 * this query object and a GList of ItemEMail objects as arguments. An
 * example of typical usage is shown below.
 *
 * <pre>
 * ------------------------------------------------------------
 * void myCallbackEntry( LdapQuery *qry, GList *listEMail ) {
 *   GList *node;
 *
 *   node = listEMail;
 *   while( node ) {
 *     ItemEMail *email = node->data;
 *     ... process email object ...
 *     node = g_list_next( node );
 *   }
 *   g_list_free( listEMail );
 * }
 * ...
 * ...
 * ldapqry_set_callback_entry( qry, myCallbackEntry );
 * ------------------------------------------------------------
 * </pre>
 *
 * \param qry Query object.
 * \param func Function.
 */
void ldapqry_set_callback_entry( LdapQuery *qry, void *func ) {
	pthread_mutex_lock( qry->mutexEntry );
	qry->callBackEntry = func;
	pthread_mutex_unlock( qry->mutexEntry );
}

/**
 * Register a callback function that will be executed when the search
 * is complete. When called, the function will be passed this query
 * object as an argument.
 * \param qry Query object.
 * \param func Function.
 */
void ldapqry_set_callback_end( LdapQuery *qry, void *func ) {
	qry->callBackEnd = func;
}

/**
 * Notify query to start/stop executing. This method should be called with a
 * value if <i>TRUE</i> to terminate an existing running query.
 *
 * \param qry Query object.
 * \param value Value of stop flag.
 */
void ldapqry_set_stop_flag( LdapQuery *qry, const gboolean value ) {
	g_return_if_fail( qry != NULL );

	pthread_mutex_lock( qry->mutexStop );
	qry->stopFlag = value;
	pthread_mutex_unlock( qry->mutexStop );
}

/**
 * Test value of stop flag. This method should be used to determine whether a
 * query has stopped running.
 * \param qry Query object.
 * \return Value of stop flag.
 */
gboolean ldapqry_get_stop_flag( LdapQuery *qry ) {
	gboolean value;
	g_return_if_fail( qry != NULL );

	pthread_mutex_lock( qry->mutexStop );
	value = qry->stopFlag;
	pthread_mutex_unlock( qry->mutexStop );
	return value;
}

/**
 * Set busy flag.
 * \param qry Query object.
 * \param value Value of busy flag.
 */
void ldapqry_set_busy_flag( LdapQuery *qry, const gboolean value ) {
	g_return_if_fail( qry != NULL );

	pthread_mutex_lock( qry->mutexBusy );
	qry->busyFlag = value;
	pthread_mutex_unlock( qry->mutexBusy );
}

/**
 * Test value of busy flag. This method will return a value of <i>FALSE</i>
 * when a query has completed running.
 * \param qry Query object.
 * \return Value of busy flag.
 */
gboolean ldapqry_get_busy_flag( LdapQuery *qry ) {
	gboolean value;
	g_return_if_fail( qry != NULL );

	pthread_mutex_lock( qry->mutexBusy );
	value = qry->busyFlag;
	pthread_mutex_unlock( qry->mutexBusy );
	return value;
}

/**
 * Set query aged flag.
 * \param qry Query object.
 * \param value Value of aged flag.
 */
void ldapqry_set_aged_flag( LdapQuery *qry, const gboolean value ) {
	g_return_if_fail( qry != NULL );
	qry->agedFlag = value;
}

/**
 * Test value of aged flag.
 * \param qry Query object.
 * \return <i>TRUE</i> if query has been marked as aged (and can be retired).
 */
gboolean ldapqry_get_aged_flag( LdapQuery *qry ) {
	g_return_if_fail( qry != NULL );
	return qry->agedFlag;
}

/**
 * Release the LDAP control data associated with the query.
 * \param qry Query object to process.
 */
void ldapqry_release_control( LdapQuery *qry ) {
	g_return_if_fail( qry != NULL );
	if( qry->control != NULL ) {
		ldapctl_free( qry->control );
	}
	qry->control = NULL;
}

/**
 * Clear LDAP query member variables.
 * \param qry Query object.
 */
void ldapqry_clear( LdapQuery *qry ) {
	g_return_if_fail( qry != NULL );

	/* Free internal stuff */
	g_free( qry->queryName );
	g_free( qry->searchValue );

	/* Clear pointers and value */
	qry->queryName = NULL;
	qry->searchValue = NULL;
	qry->retVal = LDAPRC_SUCCESS;
	qry->queryType = LDAPQUERY_NONE;
	qry->queryID = 0;
	qry->entriesRead = 0;
	qry->elapsedTime = 0;
	qry->stopFlag = FALSE;
	qry->busyFlag = FALSE;
	qry->agedFlag = FALSE;
	qry->completed = FALSE;
	qry->callBackStart = NULL;
	qry->callBackEntry = NULL;
	qry->callBackEnd = NULL;
}

/**
 * Free up LDAP query object by releasing internal memory. Note that
 * the thread object will be freed by the OS.
 * \param qry Query object to process.
 */
void ldapqry_free( LdapQuery *qry ) {
	g_return_if_fail( qry != NULL );

	/* Clear out internal members */
	ldapqry_clear( qry );

	/* Free the mutex */
	pthread_mutex_destroy( qry->mutexStop );
	pthread_mutex_destroy( qry->mutexBusy );
	pthread_mutex_destroy( qry->mutexEntry );
	g_free( qry->mutexStop );
	g_free( qry->mutexBusy );
	g_free( qry->mutexEntry );
	qry->mutexEntry = NULL;
	qry->mutexBusy = NULL;
	qry->mutexStop = NULL;

	/* Do not free folder - parent server object should free */	
	qry->folder = NULL;

	/* Do not free thread - thread should be terminated before freeing */
	qry->thread = NULL;

	/* Do not free LDAP control - should be destroyed before freeing */
	qry->control = NULL;

	/* Now release object */
	g_free( qry );
}

/**
 * Display object to specified stream.
 * \param qry    Query object to process.
 * \param stream Output stream.
 */
void ldapqry_print( const LdapQuery *qry, FILE *stream ) {
	g_return_if_fail( qry != NULL );

	fprintf( stream, "LdapQuery:\n" );
	fprintf( stream, "  control?: %s\n",   qry->control ? "yes" : "no" );
	fprintf( stream, "err/status: %d\n",   qry->retVal );
	fprintf( stream, "query type: %d\n",   qry->queryType );
	fprintf( stream, "query name: '%s'\n", qry->queryName );
	fprintf( stream, "search val: '%s'\n", qry->searchValue );
	fprintf( stream, "   queryID: %d\n",   qry->queryID );
	fprintf( stream, "   entries: %d\n",   qry->entriesRead );
	fprintf( stream, "   elapsed: %d\n",   qry->elapsedTime );
	fprintf( stream, " stop flag: %s\n",   qry->stopFlag  ? "yes" : "no" );
	fprintf( stream, " busy flag: %s\n",   qry->busyFlag  ? "yes" : "no" );
	fprintf( stream, " aged flag: %s\n",   qry->agedFlag  ? "yes" : "no" );
	fprintf( stream, " completed: %s\n",   qry->completed ? "yes" : "no" );
}

/**
 * Free linked lists of character strings.
 * \param listName  List of common names.
 * \param listAddr  List of addresses.
 * \param listFirst List of first names.
 * \param listLast  List of last names.
 */
static void ldapqry_free_lists(
		GSList *listName, GSList *listAddr, GSList *listFirst,
		GSList *listLast )
{
	mgu_free_list( listName );
	mgu_free_list( listAddr );
	mgu_free_list( listFirst );
	mgu_free_list( listLast );
}

/**
 * Add all LDAP attribute values to a list.
 * \param ld LDAP handle.
 * \param entry LDAP entry to process.
 * \param attr  LDAP attribute.
 * \return List of values.
 */
static GSList *ldapqry_add_list_values(
		LDAP *ld, LDAPMessage *entry, char *attr )
{
	GSList *list = NULL;
	gint i;
	gchar **vals;

	if( ( vals = ldap_get_values( ld, entry, attr ) ) != NULL ) {
		for( i = 0; vals[i] != NULL; i++ ) {
			/* printf( "lv\t%s: %s\n", attr, vals[i] ); */
			list = g_slist_append( list, g_strdup( vals[i] ) );
		}
	}
	ldap_value_free( vals );
	return list;
}

/**
 * Add a single attribute value to a list.
 * \param  ld    LDAP handle.
 * \param  entry LDAP entry to process.
 * \param  attr  LDAP attribute name to process.
 * \return List of values; only one value will be present.
 */
static GSList *ldapqry_add_single_value( LDAP *ld, LDAPMessage *entry, char *attr ) {
	GSList *list = NULL;
	gchar **vals;

	if( ( vals = ldap_get_values( ld, entry, attr ) ) != NULL ) {
		if( vals[0] != NULL ) {
			/* printf( "sv\t%s: %s\n", attr, vals[0] ); */
			list = g_slist_append( list, g_strdup( vals[0] ) );
		}
	}
	ldap_value_free( vals );
	return list;
}

/**
 * Build an address list entry and append to list of address items. Name is formatted
 * as "<first-name> <last-name>".
 *
 * \param  cache     Address cache to load.
 * \param  qry Query object to process.
 * \param  dn        DN for entry found on server.
 * \param  listName  List of common names for entry; see notes below.
 * \param  listAddr  List of EMail addresses for entry.
 * \param  listFirst List of first names for entry.
 * \param  listLast  List of last names for entry.
 *
 * \return List of ItemEMail objects.
 *
 * Notes:
 * 1) Each LDAP server entry may have multiple LDAP attributes with the same
 *    name. For example, a single entry for a person may have more than one
 *    common name, email address, etc.
*
 * 2) The DN for the entry is unique for the server.
 */
static GList *ldapqry_build_items_fl(
		AddressCache *cache, LdapQuery *qry, gchar *dn,
		GSList *listName, GSList *listAddr, GSList *listFirst,
		GSList *listLast )
{
	GSList *nodeAddress;
	gchar *firstName = NULL, *lastName = NULL, *fullName = NULL;
	gboolean allocated;
	ItemPerson *person;
	ItemEMail *email;
	ItemFolder *folder;
	GList *listReturn;

	listReturn = NULL;
	if( listAddr == NULL ) return listReturn;

	/* Find longest first name in list */
	firstName = mgu_slist_longest_entry( listFirst );

	/* Format last name */
	if( listLast ) {
		lastName = listLast->data;
	}

	/* Find longest common name */
	allocated = FALSE;
	fullName = mgu_slist_longest_entry( listName );
	if( fullName == NULL ) {
		/* Format a full name from first and last names */
		if( firstName ) {
			if( lastName ) {
				fullName = g_strdup_printf( "%s %s", firstName, lastName );
			}
			else {
				fullName = g_strdup_printf( "%s", firstName );
			}
		}
		else {
			if( lastName ) {
				fullName = g_strdup_printf( "%s", lastName );
			}
		}
		if( fullName ) {
			g_strchug( fullName ); g_strchomp( fullName );
			allocated = TRUE;
		}
	}

	/* Create new folder for results */
	if( qry->folder == NULL ) {
		folder = addritem_create_item_folder();
		addritem_folder_set_name( folder, qry->queryName );
		addritem_folder_set_remarks( folder, "" );
		addrcache_id_folder( cache, folder );
		addrcache_add_folder( cache, folder );
		qry->folder = folder;

		/* Specify folder type and back reference */			
		folder->folderType = ADDRFOLDER_LDAP_QUERY;
		folder->folderData = ( gpointer ) qry;
		folder->isHidden = TRUE;
	}

	/* Add person into folder */		
	person = addritem_create_item_person();
	addritem_person_set_common_name( person, fullName );
	addritem_person_set_first_name( person, firstName );
	addritem_person_set_last_name( person, lastName );
	addrcache_id_person( cache, person );
	addritem_person_set_external_id( person, dn );
	addrcache_folder_add_person( cache, qry->folder, person );

	qry->entriesRead++;

	/* Add each address item */
	nodeAddress = listAddr;
	while( nodeAddress ) {
		email = addritem_create_item_email();
		addritem_email_set_address( email, nodeAddress->data );
		addrcache_id_email( cache, email );
		addrcache_person_add_email( cache, person, email );
		addritem_person_add_email( person, email );
		listReturn = g_list_append( listReturn, email );
		nodeAddress = g_slist_next( nodeAddress );
	}

	/* Free any allocated memory */
	if( allocated ) {
		g_free( fullName );
	}
	fullName = firstName = lastName = NULL;

	return listReturn;
}

/**
 * Process a single search entry.
 * \param  cache Address cache to load.
 * \param  qry   Query object to process.
 * \param  ld    LDAP handle.
 * \param  e     LDAP message.
 * \return List of EMail objects found.
 */
static GList *ldapqry_process_single_entry(
		AddressCache *cache, LdapQuery *qry, LDAP *ld, LDAPMessage *e )
{
	char *dnEntry;
	char *attribute;
	LdapControl *ctl;
	BerElement *ber;
	GSList *listName = NULL, *listAddress = NULL;
	GSList *listFirst = NULL, *listLast = NULL;
	GList *listReturn;

	listReturn = NULL;
	ctl = qry->control;
	dnEntry = ldap_get_dn( ld, e );
	/* printf( "DN: %s\n", dnEntry ); */

	/* Process all attributes */
	for( attribute = ldap_first_attribute( ld, e, &ber ); attribute != NULL;
		attribute = ldap_next_attribute( ld, e, ber ) ) {

		if( strcasecmp( attribute, ctl->attribEMail ) == 0 ) {
			listAddress = ldapqry_add_list_values( ld, e, attribute );
		}
		else if( strcasecmp( attribute, ctl->attribCName ) == 0 ) {
			listName = ldapqry_add_list_values( ld, e, attribute );
		}
		else if( strcasecmp( attribute, ctl->attribFName ) == 0 ) {
			listFirst = ldapqry_add_list_values( ld, e, attribute );
		}
		else if( strcasecmp( attribute, ctl->attribLName ) == 0 ) {
			listLast = ldapqry_add_single_value( ld, e, attribute );
		}

		/* Free memory used to store attribute */
		ldap_memfree( attribute );
	}

	/* Format and add items to cache */
	listReturn = ldapqry_build_items_fl(
		cache, qry, dnEntry, listName, listAddress, listFirst, listLast );

	/* Free up */
	ldapqry_free_lists( listName, listAddress, listFirst, listLast );
	listName = listAddress = listFirst = listLast = NULL;

	if( ber != NULL ) {
		ber_free( ber, 0 );
	}
	g_free( dnEntry );

	return listReturn;
}

/**
 * Check parameters that are required for a search. This should
 * be called before performing a search.
 * \param  qry Query object to process.
 * \return <i>TRUE</i> if search criteria appear OK.
 */
gboolean ldapqry_check_search( LdapQuery *qry ) {
	LdapControl *ctl;
	qry->retVal = LDAPRC_CRITERIA;

	/* Test for control data */
	ctl = qry->control;
	if( ctl == NULL ) {
		return FALSE;
	}

	/* Test for search value */
	if( qry->searchValue == NULL ) {
		return FALSE;
	}
	if( strlen( qry->searchValue ) < 1 ) {
		return FALSE;
	}

	qry->retVal = LDAPRC_SUCCESS;
	return TRUE;
}

/**
 * Touch the query. This nudges the touch time with the current time.
 * \param qry Query object to process.
 */
void ldapqry_touch( LdapQuery *qry ) {
	qry->touchTime = time( NULL );
	qry->agedFlag = FALSE;
}

/**
 * Perform the LDAP search, reading LDAP entries into cache.
 * Note that one LDAP entry can have multiple values for many of its
 * attributes. If these attributes are E-Mail addresses; these are
 * broken out into separate address items. For any other attribute,
 * only the first occurrence is read.
 * 
 * \param  qry Query object to process.
 * \return Error/status code.
 */
static gint ldapqry_perform_search( LdapQuery *qry ) {
	LdapControl *ctl;
	LDAP *ld;
	LDAPMessage *result, *e;
	char **attribs;
	gchar *criteria;
	gboolean entriesFound;
	gboolean first;
	struct timeval timeout;
	gint rc;
	time_t tstart, tend;
	AddressCache *cache;
	GList *listEMail;

	/* Initialize some variables */
	ctl = qry->control;
	cache = qry->server->addressCache;
	timeout.tv_sec = ctl->timeOut;
	timeout.tv_usec = 0L;
	entriesFound = FALSE;
	qry->elapsedTime = -1;
	qry->retVal = LDAPRC_SUCCESS;

	/* Check search criteria */	
	if( ! ldapqry_check_search( qry ) ) {
		return qry->retVal;
	}

	/* Initialize connection */
	ldapqry_touch( qry );
	tstart = qry->touchTime;
	tend = tstart - 1;
	if( ( ld = ldap_init( ctl->hostName, ctl->port ) ) == NULL ) {
		qry->retVal = LDAPRC_INIT;
		return qry->retVal;
	}
	if( ldapqry_get_stop_flag( qry ) ) {
		qry->retVal = LDAPRC_SUCCESS;
		return qry->retVal;
	}
	ldapqry_touch( qry );

	/*
	printf( "connected to LDAP host %s on port %d\n", ctl->hostName, ctl->port );
	*/

	/* Bind to the server, if required */
	if( ctl->bindDN ) {
		if( * ctl->bindDN != '\0' ) {
			/* printf( "binding...\n" ); */
			rc = ldap_simple_bind_s( ld, ctl->bindDN, ctl->bindPass );
			/* printf( "rc=%d\n", rc ); */
			if( rc != LDAP_SUCCESS ) {
				/*
				printf( "LDAP Error: ldap_simple_bind_s: %s\n",
					ldap_err2string( rc ) );
				*/
				ldap_unbind( ld );
				qry->retVal = LDAPRC_BIND;
				return qry->retVal;
			}
		}
	}
	if( ldapqry_get_stop_flag( qry ) ) {
		ldap_unbind( ld );
		qry->retVal = LDAPRC_SUCCESS;
		return qry->retVal;
	}
	ldapqry_touch( qry );

	/* Define all attributes we are interested in. */
	attribs = ldapctl_attribute_array( ctl );

	/* Create LDAP search string */
	criteria = ldapctl_format_criteria( ctl, qry->searchValue );
	/* printf( "Search criteria ::%s::\n", criteria ); */

	/*
	 * Execute the search - this step may take some time to complete
	 * depending on network traffic and server response time.
	 */
	rc = ldap_search_ext_s( ld, ctl->baseDN, LDAP_SCOPE_SUBTREE, criteria,
		attribs, 0, NULL, NULL, &timeout, 0, &result );
	ldapctl_free_attribute_array( attribs );
	g_free( criteria );
	criteria = NULL;
	if( rc == LDAP_TIMEOUT ) {
		ldap_unbind( ld );
		qry->retVal = LDAPRC_TIMEOUT;
		return qry->retVal;
	}
	if( rc != LDAP_SUCCESS ) {
		/*
		printf( "LDAP Error: ldap_search_st: %s\n", ldap_err2string( rc ) );
		*/
		ldap_unbind( ld );
		qry->retVal = LDAPRC_SEARCH;
		return qry->retVal;
	}
	if( ldapqry_get_stop_flag( qry ) ) {
		qry->retVal = LDAPRC_SUCCESS;
		return qry->retVal;
	}
	ldapqry_touch( qry );

	/*
	printf( "Total results are: %d\n", ldap_count_entries( ld, result ) );
	*/

	if( ldapqry_get_stop_flag( qry ) ) {
		qry->retVal = LDAPRC_SUCCESS;
		return qry->retVal;
	}

	/* Process results */
	first = TRUE;
	while( TRUE ) {
		ldapqry_touch( qry );
		if( qry->entriesRead >= ctl->maxEntries ) break;		

		/* Test for stop */		
		if( ldapqry_get_stop_flag( qry ) ) {
			break;
		}

		/* Retrieve entry */		
		if( first ) {
			first = FALSE;
			e = ldap_first_entry( ld, result );
		}
		else {
			e = ldap_next_entry( ld, e );
		}
		if( e == NULL ) break;

		entriesFound = TRUE;

		/* Setup a critical section here */
		pthread_mutex_lock( qry->mutexEntry );

		/* Process entry */
		listEMail = ldapqry_process_single_entry( cache, qry, ld, e );

		/* Process callback */
		if( qry->callBackEntry ) {
			qry->callBackEntry( qry, listEMail );
		}
		else {
			g_list_free( listEMail );
		}

		pthread_mutex_unlock( qry->mutexEntry );
	}

	/* Free up and disconnect */
	ldap_msgfree( result );
	ldap_unbind( ld );
	ldapqry_touch( qry );
	tend = qry->touchTime;
	qry->elapsedTime = tend - tstart;

	if( entriesFound ) {
		qry->retVal = LDAPRC_SUCCESS;
	}
	else {
		qry->retVal = LDAPRC_NOENTRIES;
	}

	return qry->retVal;
}

/**
 * Wrapper around search.
 * \param  qry Query object to process.
 * \return Error/status code.
 */
gint ldapqry_search( LdapQuery *qry ) {
	gint retVal;

	g_return_val_if_fail( qry != NULL, -1 );
	g_return_val_if_fail( qry->control != NULL, -1 );

	ldapqry_touch( qry );
	qry->completed = FALSE;

	/* Process callback */	
	if( qry->callBackStart ) {
		qry->callBackStart( qry );
	}

	/* Setup pointer to thread specific area */
	pthread_setspecific( _queryThreadKey_, qry );

	pthread_detach( pthread_self() );
	
	/* Now perform the search */
	qry->entriesRead = 0;
	qry->retVal = LDAPRC_SUCCESS;
	ldapqry_set_busy_flag( qry, TRUE );
	ldapqry_set_stop_flag( qry, FALSE );
	retVal = ldapqry_perform_search( qry );
	if( retVal == LDAPRC_SUCCESS ) {
		qry->server->addressCache->dataRead = TRUE;
		qry->server->addressCache->accessFlag = FALSE;
		if( ldapqry_get_stop_flag( qry ) ) {
			/*
			printf( "Search was terminated prematurely\n" );
			*/
		}
		else {
			ldapqry_touch( qry );
			qry->completed = TRUE;
			/*
			printf( "Search ran to completion\n" );
			*/
		}
	}
	ldapqry_set_stop_flag( qry, TRUE );
	ldapqry_set_busy_flag( qry, FALSE );

	/* Process callback */	
	if( qry->callBackEnd ) {
		qry->callBackEnd( qry );
	}

	return qry->retVal;
}

/**
 * Read data into list using a background thread. Callback function will be
 * notified when search is complete.
 * \param  qry Query object to process.
 * \return Error/status code.
 */
gint ldapqry_read_data_th( LdapQuery *qry ) {
	g_return_val_if_fail( qry != NULL, -1 );
	g_return_val_if_fail( qry->control != NULL, -1 );

	ldapqry_set_stop_flag( qry, FALSE );
	ldapqry_touch( qry );
	if( ldapqry_check_search( qry ) ) {
		if( qry->retVal == LDAPRC_SUCCESS ) {
			/*
			printf( "Starting LDAP search thread\n");
			*/
			ldapqry_set_busy_flag( qry, TRUE );
			qry->thread = g_malloc0( sizeof( pthread_t ) );

			/* Setup thread */			
			pthread_create( qry->thread, NULL,
				(void *) ldapqry_search, (void *) qry );
		}
	}
	return qry->retVal;
}

/**
 * Join the thread associated with the query. This should probably be removed
 * to prevent joining threads.
 * \param qry Query object to process.
 */
void ldapqry_join_threadX( LdapQuery *qry ) {
	g_return_if_fail( qry != NULL );

	/* Wait for thread */
	/* printf( "ldapqry_join_thread::Joining thread...\n" ); */
	pthread_join( * qry->thread, NULL );
	/* printf( "ldapqry_join_thread::Thread terminated\n" ); */
}

/**
 * Cleanup LDAP thread data. This function will be called when each thread
 * exits. Note that the thread object will be freed by the kernel.
 * \param ptr Pointer to object being destroyed (a query object in this case).
 */
static void ldapqry_destroyer( void * ptr ) {
	LdapQuery *qry;

	qry = ( LdapQuery * ) ptr;
	/*
	printf( "ldapqry_destroyer::%d::%s\n", (int) pthread_self(), qry->queryName );
	*/

	/* Perform any destruction here */
	if( qry->control != NULL ) {
		ldapctl_free( qry->control );
	}
	qry->control = NULL;
	qry->thread = NULL;
	ldapqry_set_busy_flag( qry, FALSE );
	/*
	printf( "...destroy exiting\n" );
	*/
}

/**
 * Cancel thread associated with query.
 * \param qry Query object to process.
 */
void ldapqry_cancel( LdapQuery *qry ) {
	g_return_if_fail( qry != NULL );

	/*
	printf( "cancelling::%d::%s\n", (int) pthread_self(), qry->queryName );
	*/
	if( ldapqry_get_busy_flag( qry ) ) {
		if( qry->thread ) {
			pthread_cancel( * qry->thread );
		}
	}
}

/**
 * Initialize LDAP query. This function should be called once before executing
 * any LDAP queries to initialize thread specific data.
 */
void ldapqry_initialize( void ) {
	/* printf( "ldapqry_initialize...\n" ); */
	if( ! _queryThreadInit_ ) {
		/*
		printf( "ldapqry_initialize::creating thread specific area\n" );
		*/
		pthread_key_create( &_queryThreadKey_, ldapqry_destroyer );
		_queryThreadInit_ = TRUE;
	}
	/* printf( "ldapqry_initialize... done!\n" ); */
}

/**
 * Age the query based on LDAP control parameters.
 * \param qry    Query object to process.
 * \param maxAge Maximum age of query (in seconds).
 */
void ldapqry_age( LdapQuery *qry, gint maxAge ) {
	gint age;

	g_return_if_fail( qry != NULL );

	/* Limit the time that queries can hang around */	
	if( maxAge < 1 ) maxAge = LDAPCTL_MAX_QUERY_AGE;

	/* Check age of query */
	age = time( NULL ) - qry->touchTime;
	if( age > maxAge ) {
		qry->agedFlag = TRUE;
	}
}

/**
 * Delete folder associated with query results.
 * \param qry Query object to process.
 */
void ldapqry_delete_folder( LdapQuery *qry ) {
	AddressCache *cache;
	ItemFolder *folder;

	g_return_if_fail( qry != NULL );

	folder = qry->folder;
	if( folder ) {
		cache = qry->server->addressCache;
		folder = addrcache_remove_folder_delete( cache, folder );
		if( folder ) {
			addritem_free_item_folder( folder );
		}
		qry->folder = NULL;
	}
}

#endif	/* USE_LDAP */

/*
 * End of Source.
 */


