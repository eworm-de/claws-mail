/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2002 Match Grun
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
 * Address list item selection objects.
 */

#include <stdio.h>
#include <glib.h>

#include "addritem.h"
#include "addrselect.h"
#include "addressitem.h"
#include "mgutils.h"

/*
* Create a selection record from an address cache item.
* Enter: aio Item object.
*/
AddrSelectItem *addrselect_create_item( AddrItemObject *aio ) {
	AddrSelectItem *item = NULL;

	if( aio ) {
		item = g_new0( AddrSelectItem, 1 );
		item->objectType = aio->type;
		item->addressItem = aio;
		item->uid = g_strdup( aio->uid );
		item->cacheID = NULL;
	}
	return item;
}

/*
* Create a selection record from an address object (in tree node).
* Enter: obj Address object.
*/
AddrSelectItem *addrselect_create_node( AddressObject *obj ) {
	AddrSelectItem *item = NULL;

	if( obj ) {
		item = g_new0( AddrSelectItem, 1 );
		item->objectType = addressbook_type2item( obj->type );
		item->addressItem = NULL;
		item->uid = NULL;
		item->cacheID = NULL;
	}
	return item;
}

/*
* Create a copy of a selection record.
* Enter: item Address entry to copy.
*/
AddrSelectItem *addrselect_item_copy( AddrSelectItem *item ) {
	AddrSelectItem *copy = NULL;

	if( item ) {
		copy = g_new0( AddrSelectItem, 1 );
		copy->objectType = item->objectType;
		copy->addressItem = item->addressItem;
		copy->uid = g_strdup( item->uid );
		copy->cacheID = g_strdup( item->cacheID );
	}
	return copy;
}

/*
* Free selection record.
*/
void addrselect_item_free( AddrSelectItem *item ) {
	if( item ) {
		g_free( item->uid );
		g_free( item->cacheID );
		item->objectType = ITEMTYPE_NONE;
		item->addressItem = NULL;
		item->uid = NULL;
		item->cacheID = NULL;
	}
	g_free( item );
}

/*
* Properties.
*/
void addrselect_set_cache_id( AddrSelectItem *item, const gchar *value ) {
	g_return_if_fail( item != NULL );
	item->cacheID = mgu_replace_string( item->cacheID, value );
}

void addrselect_item_print( AddrSelectItem *item, FILE *stream ) {
	fprintf( stream, "Select Record\n" );
	fprintf( stream, "obj type: %d\n", item->objectType );
	fprintf( stream, "     uid: %s\n", item->uid );
	fprintf( stream, "cache id: %s\n", item->cacheID );
	fprintf( stream, "---\n" );
}

/*
* Create a new selection.
* Return: Initialized object.
*/
AddrSelectList *addrselect_list_create() {
	AddrSelectList *asl;

	asl = g_new0( AddrSelectList, 1 );
	asl->listSelect = NULL;
	return asl;
}

/*
* Clear list of selection records.
* Enter: asl List to process.
*/
void addrselect_list_clear( AddrSelectList *asl ) {
	GList *node;

	g_return_if_fail( asl != NULL );
	node = asl->listSelect;
	while( node ) {
		AddrSelectItem *item;

		item = node->data;
		addrselect_item_free( item );
		node->data = NULL;
		node = g_list_next( node );
	}
	g_list_free( asl->listSelect );
	asl->listSelect = NULL;
}

/*
* Free selection list.
* Enter: asl List to free.
*/
void addrselect_list_free( AddrSelectList *asl ) {
	g_return_if_fail( asl != NULL );

	addrselect_list_clear( asl );
	g_list_free( asl->listSelect );
	asl->listSelect = NULL;
	g_free( asl );
}

/*
* Test whether selection is empty.
* Enter: asl List to test.
* Return: TRUE if list is empty.
*/
gboolean addrselect_test_empty( AddrSelectList *asl ) {
	g_return_val_if_fail( asl != NULL, TRUE );
	return ( asl->listSelect == NULL );
}

/*
* Return list of AddrSelectItem objects.
* Enter: asl List to process.
* Return: List of selection items. The list should should be g_list_free()
* when done. Items contained in the list should not be freed!!!
*/
GList *addrselect_get_list( AddrSelectList *asl ) {
	GList *node, *list;

	g_return_if_fail( asl != NULL );
	list = NULL;
	node = asl->listSelect;
	while( node ) {
		list = g_list_append( list, node->data );
		node = g_list_next( node );
	}
	return list;
}

static gchar *addrselect_format_address( AddrItemObject * aio ) {
	gchar *buf = NULL;
	gchar *name = NULL;
	gchar *address = NULL;

	if( aio->type == ADDR_ITEM_EMAIL ) {
		ItemPerson *person = NULL;
		ItemEMail *email = ( ItemEMail * ) aio;

		person = ( ItemPerson * ) ADDRITEM_PARENT(email);
		if( email->address ) {
			if( ADDRITEM_NAME(email) ) {
				name = ADDRITEM_NAME(email);
				if( *name == '\0' ) {
					name = ADDRITEM_NAME(person);
				}
			}
			else if( ADDRITEM_NAME(person) ) {
				name = ADDRITEM_NAME(person);
			}
			else {
				buf = g_strdup( email->address );
			}
			address = email->address;
		}
	}
	else if( aio->type == ADDR_ITEM_PERSON ) {
		ItemPerson *person = ( ItemPerson * ) aio;
		GList *node = person->listEMail;

		name = ADDRITEM_NAME(person);
		if( node ) {
			ItemEMail *email = ( ItemEMail * ) node->data;
			address = email->address;
		}
	}
	if( address ) {
		if( name ) {
			buf = g_strdup_printf( "%s <%s>", name, address );
		}
		else {
			buf = g_strdup( address );
		}
	}
	return buf;
}

/*
* Print formatted addresses list to specified stream.
* Enter: asl    List to process.
*        stream Stream.
*/
void addrselect_list_print( AddrSelectList *asl, FILE *stream ) {
	GList *node;

	g_return_if_fail( asl != NULL );
	fprintf( stream, "show selection...>>>\n" );
	node = asl->listSelect;
	while( node != NULL ) {
		AddrSelectItem *item;
		AddrItemObject *aio;
		gchar *addr;

		item = node->data;
		aio = ( AddrItemObject * ) item->addressItem;
		if( aio ) {
			fprintf( stream, "- %d : '%s'\n", aio->type, aio->name );
			if( aio->type == ADDR_ITEM_GROUP ) {
				ItemGroup *group = ( ItemGroup * ) aio;
				GList *node = group->listEMail;
				while( node ) {
					ItemEMail *email = node->data;
					addr = addrselect_format_address(
						( AddrItemObject * ) email );
					if( addr ) {
						fprintf( stream, "\tgrp >%s<\n", addr );
						g_free( addr );
					}
					node = g_list_next( node );
				}
			}
			else {
				addr = addrselect_format_address( aio );
				if( addr ) {
					fprintf( stream, "\t>%s<\n", addr );
					g_free( addr );
				}
			}
		}
		else {
			fprintf( stream, "- NULL" );
		}
		node = g_list_next( node );
	}
	fprintf( stream, "show selection...<<<\n" );
}

/*
* Print address items to specified stream.
* Enter: asl    List to process.
*        stream Stream.
*/
void addrselect_list_show( AddrSelectList *asl, FILE *stream ) {
	GList *node;

	g_return_if_fail( asl != NULL );
	fprintf( stream, "show selection...>>>\n" );
	node = asl->listSelect;
	while( node != NULL ) {
		AddrSelectItem *item;

		item = node->data;
		addrselect_item_print( item, stream );
		node = g_list_next( node );
	}
	fprintf( stream, "show selection...<<<\n" );
}

/*
* Test whether specified object is in list.
* Enter: list List to check.
*        aio  Object to test.
* Return item found, or NULL if not in list.
*/
static AddrSelectItem *addrselect_list_find( GList *list, AddrItemObject *aio ) {
	GList *node;

	node = list;
	while( node ) {
		AddrSelectItem *item;

		item = node->data;
		if( item->addressItem == aio ) return item;
		node = g_list_next( node );
	}
	return NULL;
}

/*
* Add a single object into the list.
* Enter: asl     Address selection object.
*        obj     Address object.
*        cacheID Cache ID. Should be g_free() after calling function.
* Return: Adjusted list.
*/
void addrselect_list_add_obj( AddrSelectList *asl, AddrItemObject *aio, gchar *cacheID ) {
	AddrSelectItem *item;

	g_return_if_fail( asl != NULL );
	if( aio == NULL ) return;

	/* Check whether object is in list */
	if( addrselect_list_find( asl->listSelect, aio ) ) return;

	if( aio->type == ADDR_ITEM_PERSON ||
	    aio->type == ADDR_ITEM_EMAIL ||
	    aio->type == ADDR_ITEM_GROUP ) {
		item = addrselect_create_item( aio );
		item->cacheID = g_strdup( cacheID );
		asl->listSelect = g_list_append( asl->listSelect, item );
	}
	/* addrselect_list_show( asl, stdout ); */
}

/*
* Add a single item into the list.
* Enter: asl     Address selection object.
*        item    Address select item.
*        cacheID Cache ID. Should be g_free() after calling function.
* Return: Adjusted list.
*/
void addrselect_list_add( AddrSelectList *asl, AddrSelectItem *item, gchar *cacheID ) {
	g_return_if_fail( asl != NULL );
	if( item == NULL ) return;

	/* Check whether object is in list */
	if( g_list_find( asl->listSelect, item ) ) return;

	item->cacheID = g_strdup( cacheID );
	asl->listSelect = g_list_append( asl->listSelect, item );
}

/*
* Remove specified object from list.
* Enter: asl  Address selection object.
*        aio  Object to remove.
*/
void addrselect_list_remove( AddrSelectList *asl, AddrItemObject *aio ) {
	GList *node;
	AddrSelectItem *item;

	g_return_if_fail( asl != NULL );
	if( aio == NULL ) return;
	node = asl->listSelect;
	while( node ) {
		item = node->data;
		if( item->addressItem == aio ) {
			addrselect_item_free( item );
			node->data = NULL;
			asl->listSelect = g_list_remove_link( asl->listSelect, node );
			break;
		}
		node = g_list_next( node );
	}
	/* addrselect_list_show( list, stdout ); */
}

/*
* End of Source.
*/


