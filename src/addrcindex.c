/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2002-2007 Match Grun and the Claws Mail team
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

/*
 * Functions to maintain address completion index.
 */

#include <stdio.h>
#include <string.h>

#include "mgutils.h"
#include "addritem.h"
#include "addrcindex.h"

/*
static gint _n_created = 0;
static gint _n_freed   = 0;
*/

typedef struct {
	gchar     *name;
	ItemEMail *address;
}
AddrIndexEntry;

static gchar *addrcindex_function( gpointer data ) {
	return ( ( AddrIndexEntry * ) data )->name;
}

/*
* Create new completion index.
*/
AddrCacheIndex *addrcindex_create( void ) {
	AddrCacheIndex *index;

	/*
	++_n_created;
	printf( "addrcindex_create/1/%d\n", _n_created );
	*/
	index = g_new0( AddrCacheIndex, 1 );
	index->completion = g_completion_new( addrcindex_function );
	index->addressList = NULL;
	index->invalid = TRUE;

	return index;
}

/*
* Clear the completion index.
*/
void addrcindex_clear( AddrCacheIndex *index ) {
	if( index ) {
		/* Clear completion index */
		g_completion_clear_items( index->completion );

		/* Clear address list */	
		g_list_free( index->addressList );
		index->addressList = NULL;
		index->invalid = TRUE;
	}
}

/*
* Free completion index.
*/
void addrcindex_free( AddrCacheIndex *index ) {
	if( index ) {
		/*
		++_n_freed;
		printf( "addrcindex_free/2/%d\n", _n_freed );
		*/
		/* Clear out */
		addrcindex_clear( index );

		/* Free up */
		g_completion_free( index->completion );
		index->completion = NULL;
		index->invalid = FALSE;

		g_free( index );
	}
}

/**
 * Mark index as invalid. Will need to be rebuilt.
 * \param index Address completion index.
 */
void addrcindex_invalidate( AddrCacheIndex *index ) {
	g_return_if_fail( index != NULL );
	index->invalid = TRUE;
}

/**
 * Mark index as valid.
 * \param index Address completion index.
 */
void addrcindex_validate( AddrCacheIndex *index ) {
	g_return_if_fail( index != NULL );
	index->invalid = FALSE;
}

/*
 * Add completion entry to index.
 * Enter: index Index.
 *        name  Name.
 *        email EMail entry to add.
 */
void addrcindex_add_entry(
	AddrCacheIndex *index, gchar *name, ItemEMail *email )
{
	AddrIndexEntry *entry;

	entry = g_new0( AddrIndexEntry, 1 );
	entry->name = g_strdup( name );
	entry->address = email;
	g_strdown( entry->name );
	index->addressList = g_list_append( index->addressList, entry );
}

/*
* Add an email entry into index. The index will also include all name fields
* for the person.
* 
* Add address into index.
* Enter: index Index.
*        email E-Mail to add.
*/
void addrcindex_add_email( AddrCacheIndex *index, ItemEMail *email ) {
	ItemPerson *person;
	gchar *name;
	GSList *uniqName;
	GSList *node;
	gboolean flag;

	g_return_if_fail( index != NULL );
	g_return_if_fail( email != NULL );

	flag = FALSE;
	uniqName = NULL;
	name = ADDRITEM_NAME( email );
	if( name != NULL ) {
		if( strlen( name ) > 0 ) {
			uniqName = g_slist_append( uniqName, name );
		}
	}
	name = email->address;
	if( mgu_slist_test_unq_nc( uniqName, name ) ) {
		uniqName = g_slist_append( uniqName, name );
	}
	if( name ) {
		if( strlen( name ) > 0 ) flag = TRUE;
	}

	/* Bail if no email address */
	if( ! flag ) {
		g_slist_free( uniqName );
		return;
	}

	person = ( ItemPerson * ) ADDRITEM_PARENT( email );
	if( person != NULL ) {
		name = ADDRITEM_NAME( person );
		if( mgu_slist_test_unq_nc( uniqName, name ) ) {
			uniqName = g_slist_append( uniqName, name );
		}

		name = person->nickName;
		if( mgu_slist_test_unq_nc( uniqName, name ) ) {
			uniqName = g_slist_append( uniqName, name );
		}

		name = person->firstName;
		if( mgu_slist_test_unq_nc( uniqName, name ) ) {
			uniqName = g_slist_append( uniqName, name );
		}

		name = person->lastName;
		if( mgu_slist_test_unq_nc( uniqName, name ) ) {
			uniqName = g_slist_append( uniqName, name );
		}
	}

	/* Create a completion entry for each unique name */
	node = uniqName;
	while( node ) {
		addrcindex_add_entry( index, node->data, email );
		node = g_slist_next( node );
	}
	g_slist_free( uniqName );

}

/*
 * Process email address entry, checking for unique alias and address. If the
 * address field is empty, no entries will be generated.
 * Enter: index Index.
 *        uniqName List of unique names to examine.
 *        email    EMail address item to process.
 * Return: List of entries from email object to add to index.
 */
static GSList *addrcindex_proc_mail(
	AddrCacheIndex *index, GSList *uniqName, ItemEMail *email )
{
	GSList *list;
	gchar *name;

	/* Test for address */
	list = NULL;
	name = email->address;
	if( name ) {
		if( strlen( name ) > 0 ) {
			/* Address was supplied */
			/* Append alias if unique */
			name = ADDRITEM_NAME( email );
			if( mgu_slist_test_unq_nc( uniqName, name ) ) {
				list = g_slist_append( list, name );
			}
			/* Then append the address if unique */
			/* Note is possible that the address has already */
			/* been entered into one of the name fields. */
			if( mgu_slist_test_unq_nc( uniqName, email->address ) ) {
				list = g_slist_append( list, email->address );
			}
		}
	}
	return list;
}

/*
* Add person's address entries into index. Each email address is processed.
* If the address field has been supplied, entries will be made. The index
* will include the address, alias and all name fields for the person.
* 
* Enter: index  Index.
*        person Person to add.
*/
void addrcindex_add_person( AddrCacheIndex *index, ItemPerson *person ) {
	gchar *name;
	GSList *uniqName;
	GSList *node;
	GSList *listMail;
	GList  *listEMail;
	ItemEMail *email;

	g_return_if_fail( index != NULL );
	g_return_if_fail( person != NULL );

	/* Build list of all unique names in person's name fields */
	uniqName = NULL;
	name = ADDRITEM_NAME( person );
	if( mgu_slist_test_unq_nc( uniqName, name ) ) {
		uniqName = g_slist_append( uniqName, name );
	}

	name = person->nickName;
	if( mgu_slist_test_unq_nc( uniqName, name ) ) {
		uniqName = g_slist_append( uniqName, name );
	}

	name = person->firstName;
	if( mgu_slist_test_unq_nc( uniqName, name ) ) {
		uniqName = g_slist_append( uniqName, name );
	}

	name = person->lastName;
	if( mgu_slist_test_unq_nc( uniqName, name ) ) {
		uniqName = g_slist_append( uniqName, name );
	}

	/* Process each email address entry */
	listEMail = person->listEMail;
	while( listEMail ) {
		email = listEMail->data;
		listMail = addrcindex_proc_mail( index, uniqName, email );
		if( listMail ) {
			/* Create a completion entry for the address item */
			node = listMail;
			while( node ) {
				/* printf( "\tname-m::%s::\n", node->data ); */
				addrcindex_add_entry( index, node->data, email );
				node = g_slist_next( node );
			}
			/* ... and all person's name entries */
			node = uniqName;
			while( node ) {
				/* printf( "\tname-p::%s::\n", node->data ); */
				addrcindex_add_entry( index, node->data, email );
				node = g_slist_next( node );
			}
			g_slist_free( listMail );
		}
		listEMail = g_list_next( listEMail );
	}

	/* Free up the list */
	g_slist_free( uniqName );
}

/*
* Print index to stream.
* Enter: index  Index.
*        stream Output stream.
*/
void addrcindex_print( AddrCacheIndex *index, FILE *stream ) {
	GList *node;
	AddrIndexEntry *entry;
	ItemEMail *email;

	g_return_if_fail( index != NULL );
	fprintf( stream, "AddressSearchIndex:\n" );
	node = index->addressList;
	while( node ) {
		entry = node->data;
		email = entry->address;
		fprintf( stream, "\tname: '%s'\t'%s'\n", entry->name, email->address );
		node = g_list_next( node );
	}
}

/*
* Perform search for specified search term.
* Enter: index  Completion index.
*        search Search string.
* Return: List of references to ItemEMail objects meeting search criteria. The
*         list should be g_list_free() when no longer required.
*/
GList *addrcindex_search( AddrCacheIndex *index, const gchar *search ) {
	AddrIndexEntry *entry;
	gchar *prefix;
	GList *list;
	GList *node;
	GList *listEMail;

	g_return_if_fail( index != NULL );
	g_return_if_fail( search != NULL );

	listEMail = NULL;
	if( index->addressList != NULL ) { 
		/* Add items to list */
		g_completion_add_items( index->completion, index->addressList );

		/* Perform the search */
		prefix = g_strdup( search );
		g_strdown( prefix );
		list = g_completion_complete( index->completion, prefix, NULL );
		g_free( prefix );

		/* Build list of unique EMail objects */
		node = list;
		while( node ) {
			entry = node->data;
			/* printf( "\tname ::%s::\n", entry->name ); */
			if( NULL == g_list_find( listEMail, entry->address ) ) {
				listEMail = g_list_append(
						listEMail, entry->address );
			}
			node = g_list_next( node );
		}
	}

	return listEMail;
}

/*
* End of Source.
*/
