/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2001 Match Grun
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
 * General functions for create common address book entries.
 */

#include <sys/stat.h>
#include <stdio.h>
#include <glib.h>

#include "addressitem.h"
#include "mgutils.h"

/*
* Create new address item.
*/
AddressItem *mgu_create_address_item( AddressObjectType type ) {
	AddressItem *item;
	item = g_new( AddressItem, 1 );
	ADDRESS_OBJECT(item)->type = type;
	item->name = NULL;
	item->address = NULL;
	item->remarks = NULL;
	item->externalID = NULL;
	item->categoryID = ADDRESS_ITEM_CAT_UNKNOWN;
	return item;
}

/*
* Create new address item.
*/
AddressItem *mgu_create_address( void ) {
	AddressItem *item;
	item = g_new( AddressItem, 1 );
	ADDRESS_OBJECT(item)->type = ADDR_ITEM;
	item->name = NULL;
	item->address = NULL;
	item->remarks = NULL;
	item->externalID = NULL;
	item->categoryID = ADDRESS_ITEM_CAT_UNKNOWN;
	return item;
}

/*
* Create copy of specified address item.
*/
AddressItem *mgu_copy_address_item( AddressItem *item ) {
	AddressItem *itemNew = NULL;
	if( item ) {
		itemNew = mgu_create_address_item( ADDRESS_OBJECT(item)->type );
		itemNew->name = g_strdup( item->name );
		itemNew->address = g_strdup( item->address );
		itemNew->remarks = g_strdup( item->remarks );
		itemNew->externalID = g_strdup( item->externalID );
		itemNew->categoryID = item->categoryID;
	}
	return itemNew;
}

/*
* Free address item.
*/
void mgu_free_address( AddressItem *item ) {
	g_return_if_fail( item != NULL );

	/* Free internal stuff */
	g_free( item->name );
	g_free( item->address );
	g_free( item->remarks );
	g_free( item->externalID );
	item->name = NULL;
	item->address = NULL;
	item->remarks = NULL;
	item->externalID = NULL;
	item->categoryID = ADDRESS_ITEM_CAT_UNKNOWN;

	/* Now release item */
	g_free( item );
}

/*
* Refresh internal variables to force a reload.
*/
void mgu_refresh_cache( AddressCache *cache ) {
	cache->dataRead = FALSE;
	cache->modified = TRUE;
	cache->modifyTime = 0;
}

/*
* Free up address list.
*/
void mgu_free_address_list( GList *addrList ) {
	AddressItem *item;
	GList *node;

	/* Free data in the list */
	node = addrList;
	while( node ) {
		item = node->data;
		mgu_free_address( item );
		node->data = NULL;
		node = g_list_next( node );
	}

	/* Now release linked list object */
	g_list_free( addrList );
}

/*
* Clear the cache.
*/
void mgu_clear_cache( AddressCache *cache ) {
	AddressItem *item;
	GList *node;
	g_return_if_fail( cache != NULL );

	/* Free data in the list */
	mgu_free_address_list( cache->addressList );
	cache->addressList = NULL;
	mgu_refresh_cache( cache );
}

/*
* Clear the cache by setting pointers to NULL and free list.
* Note that individual items are not free'd.
*/
void mgu_clear_cache_null( AddressCache *cache ) {
	GList *node;
	g_return_if_fail( cache != NULL );

	/* Free data in the list */
	node = cache->addressList;
	while( node ) {
		node->data = NULL;
		node = g_list_next( node );
	}

	/* Now release linked list object */
	g_list_free( cache->addressList );
	cache->addressList = NULL;
}

/*
* Create new cache.
*/
AddressCache *mgu_create_cache( void ) {
	AddressCache *cache;
	cache = g_new( AddressCache, 1 );
	cache->addressList = NULL;
	cache->dataRead = FALSE;
	cache->modified = FALSE;
	cache->modifyTime = 0;
	return cache;
}

/*
* Create new address item.
*/
void mgu_free_cache( AddressCache *cache ) {
	mgu_clear_cache( cache );
	cache->addressList = NULL;
}

/*
* Print address item.
*/
void mgu_print_address( AddressItem *item, FILE *stream ) {
	g_return_if_fail( item != NULL );
	fprintf( stream, "addr item:\n" );
	fprintf( stream, "\tname: '%s'\n", item->name );
	fprintf( stream, "\taddr: '%s'\n", item->address );
	fprintf( stream, "\trems: '%s'\n", item->remarks );
	fprintf( stream, "\tid  : '%s'\n", item->externalID );
	fprintf( stream, "\tcatg: '%d'\n", item->categoryID );
	fprintf( stream, "---\n" );
}

/*
* Print linked list containing address item(s).
*/
void mgu_print_address_list( GList *addrList, FILE *stream ) {
	GList *node;
	g_return_if_fail( addrList != NULL );

	/* Now process the list */
	node = addrList;
	while( node ) {
		gpointer *gptr = node->data;
		AddressItem *item = ( AddressItem * ) gptr;
		mgu_print_address( item, stream );		
		node = g_list_next( node );
	}
}

/*
* Print address cache.
*/
void mgu_print_cache( AddressCache *cache, FILE *stream ) {
	GList *node;
	g_return_if_fail( cache != NULL );
	fprintf( stream, "AddressCache:\n" );
	fprintf( stream, "modified : %s\n", cache->modified ? "yes" : "no" );
	fprintf( stream, "data read: %s\n", cache->dataRead ? "yes" : "no" );

	/* Now process the list */
	node = cache->addressList;
	while( node ) {
		gpointer *gptr;
		AddressItem *item;
		gptr = node->data;
		item = ( AddressItem * ) gptr;
		mgu_print_address( item, stream );		
		node = g_list_next( node );
	}
}

/*
* Dump linked list of character strings (for debug).
*/
void mgu_print_list( GSList *list, FILE *stream ) {
	GSList *node = list;
	while( node ) {
		fprintf( stream, "\t- >%s<\n", node->data );
		node = g_slist_next( node );
	}
}

/*
* Dump linked list of character strings (for debug).
*/
void mgu_print_dlist( GList *list, FILE *stream ) {
	GList *node = list;
	while( node ) {
		fprintf( stream, "\t- >%s<\n", node->data );
		node = g_list_next( node );
	}
}

/*
* Check whether file has changed by comparing with cache.
* return:	TRUE if file has changed.
*/
gboolean mgu_check_file( AddressCache *cache, gchar *path ) {
	gboolean retVal;
	struct stat filestat;
	retVal = TRUE;
	if( path ) {
		if( 0 == lstat( path, &filestat ) ) {
			if( filestat.st_mtime == cache->modifyTime ) retVal = FALSE;
		}
	}
	return retVal;
}

/*
* Save file time to cache.
* return:	TRUE if time marked.
*/
gboolean mgu_mark_cache( AddressCache *cache, gchar *path ) {
	gboolean retVal = FALSE;
	struct stat filestat;
	if( path ) {
		if( 0 == lstat( path, &filestat ) ) {
			cache->modifyTime = filestat.st_mtime;
			retVal = TRUE;
		}
	}
	return retVal;
}

/*
* Free linked list of character strings.
*/
void mgu_free_list( GSList *list ) {
	GSList *node = list;
	while( node ) {
		g_free( node->data );
		node->data = NULL;
		node = g_slist_next( node );
	}
	g_slist_free( list );
}

/*
* Free linked list of character strings.
*/
void mgu_free_dlist( GList *list ) {
	GList *node = list;
	while( node ) {
		g_free( node->data );
		node->data = NULL;
		node = g_list_next( node );
	}
	g_list_free( list );
}

/*
* Coalesce linked list of characaters into one long string.
*/
gchar *mgu_list_coalesce( GSList *list ) {
	gchar *str = NULL;
	gchar *buf = NULL;
	gchar *start = NULL;
	GSList *node = NULL;
	gint len;

	if( ! list ) return NULL;

	// Calculate maximum length of text
	len = 0;
	node = list;
	while( node ) {
		str = node->data;
		len += 1 + strlen( str );
		node = g_slist_next( node );
	}

	// Create new buffer.
	buf = g_new( gchar, len+1 );
	start = buf;
	node = list;
	while( node ) {
		str = node->data;
		len = strlen( str );
		strcpy( start, str );
		start += len;
		node = g_slist_next( node );
	}
	return buf;
}

/*
* Add address item to cache.
*/
void mgu_add_cache( AddressCache *cache, AddressItem *addrItem ) {
	cache->addressList = g_list_append( cache->addressList, addrItem );
	cache->modified = TRUE;
}

struct mgu_error_entry {
	gint	e_code;
	gchar	*e_reason;
};

static const struct mgu_error_entry mgu_error_list[] = {
	{ MGU_SUCCESS,		"Success" },
	{ MGU_BAD_ARGS,		"Bad arguments" },
	{ MGU_NO_FILE,		"File not specified" },
	{ MGU_OPEN_FILE,	"Error opening file" },
	{ MGU_ERROR_READ,	"Error reading file" },
	{ MGU_EOF,		"End of file encountered" },
	{ MGU_OO_MEMORY,	"Error allocating memory" },
	{ MGU_BAD_FORMAT,	"Bad file format" },
	{ MGU_LDAP_CONNECT,	"Error connecting to LDAP server" },
	{ MGU_LDAP_INIT,	"Error initializing LDAP" },
	{ MGU_LDAP_BIND,	"Error binding to LDAP server" },
	{ MGU_LDAP_SEARCH,	"Error searching LDAP database" },
	{ MGU_LDAP_TIMEOUT,	"Timeout performing LDAP operation" },
	{ MGU_LDAP_CRITERIA,	"Error in LDAP search criteria" },
	{ MGU_LDAP_CRITERIA,	"Error in LDAP search criteria" },
	{ MGU_LDAP_NOENTRIES,	"No LDAP entries found for search criteria" },
	{ -999,			NULL }
};

static const struct mgu_error_entry *mgu_error_find( gint err ) {
	gint i;
	for ( i = 0; mgu_error_list[i].e_code != -999; i++ ) {
		if ( err == mgu_error_list[i].e_code )
			return & mgu_error_list[i];
	}
	return NULL;
}

/*
* Return error message for specified error code.
*/
gchar *mgu_error2string( gint err ) {
	const struct mgu_error_entry *e;
	e = mgu_error_find( err );
	return ( e != NULL ) ? e->e_reason : "Unknown error";
}

/*
* End of Source.
*/
