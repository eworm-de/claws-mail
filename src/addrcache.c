/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2001-2006 Match Grun and the Claws Mail team
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
 * Functions to maintain address cache.
 */

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "mgutils.h"
#include "addrcache.h"
#include "utils.h"

#define ID_TIME_OFFSET             998000000
#define ADDRCACHE_MAX_SEARCH_COUNT 1000

static int _nextCacheID__ = 0;

/*
 * Generate next cache ID.
 */
static int addrcache_next_cache_id() {
	int retVal;

	if( _nextCacheID__ == 0 ) {
		_nextCacheID__ = 1;
	}
	retVal = _nextCacheID__;
	++_nextCacheID__;
	return retVal;
}

/*
* Create new address cache.
*/
AddressCache *addrcache_create() {
	AddressCache *cache;
	gint t;

	cache = g_new0( AddressCache, 1 );
	cache->itemHash = g_hash_table_new( g_str_hash, g_str_equal );
	cache->cacheID = g_strdup_printf( "%d", addrcache_next_cache_id() );

	cache->dataRead = FALSE;
	cache->modified = FALSE;
	cache->dirtyFlag = FALSE;
	cache->accessFlag = FALSE;
	cache->name = NULL;
	cache->modifyTime = 0;

	/* Generate the next ID using system time */
	cache->nextID = 1;
	t = time( NULL );
	if( t > 0 ) {
		cache->nextID = t - ID_TIME_OFFSET;
	}

	cache->tempList = NULL;
	cache->rootFolder = addritem_create_item_folder();
	cache->rootFolder->isRoot = TRUE;
	ADDRITEM_PARENT(cache->rootFolder) = NULL;
	return cache;
}

/*
* Properties.
*/
ItemFolder *addrcache_get_root_folder( AddressCache *cache ) {
	g_return_val_if_fail( cache != NULL, NULL );
	return cache->rootFolder;
}
GList *addrcache_get_list_folder( AddressCache *cache ) {
	g_return_val_if_fail( cache != NULL, NULL );
	return cache->rootFolder->listFolder;
}
GList *addrcache_get_list_person( AddressCache *cache ) {
	g_return_val_if_fail( cache != NULL, NULL );
	return cache->rootFolder->listPerson;
}
gboolean addrcache_get_dirty( AddressCache *cache ) {
	g_return_val_if_fail( cache != NULL, FALSE );
	return cache->dirtyFlag;
}
void addrcache_set_dirty( AddressCache *cache, const gboolean value ) {
	g_return_if_fail( cache != NULL );
	cache->dirtyFlag = value;
}
gboolean addrcache_get_modified( AddressCache *cache ) {
	g_return_val_if_fail( cache != NULL, FALSE );
	return cache->modified;
}
void addrcache_set_modified( AddressCache *cache, const gboolean value ) {
	g_return_if_fail( cache != NULL );
	cache->modified = value;
}
gboolean addrcache_get_read_flag( AddressCache *cache ) {
	g_return_val_if_fail( cache != NULL, FALSE );
	return cache->dataRead;
}
void addrcache_set_read_flag( AddressCache *cache, const gboolean value ) {
	g_return_if_fail( cache != NULL );
	cache->dataRead = value;
}
gboolean addrcache_get_accessed( AddressCache *cache ) {
	g_return_val_if_fail( cache != NULL, FALSE );
	return cache->accessFlag;
}
void addrcache_set_accessed( AddressCache *cache, const gboolean value ) {
	g_return_if_fail( cache != NULL );
	cache->accessFlag = value;
}
gchar *addrcache_get_name( AddressCache *cache ) {
	g_return_val_if_fail( cache != NULL, NULL );
	return cache->name;
}
void addrcache_set_name( AddressCache *cache, const gchar *value ) {
	g_return_if_fail( cache != NULL );
	cache->name = mgu_replace_string( cache->name, value );
	g_strstrip( cache->name );
	cache->dirtyFlag = TRUE;
}

/*
* Generate next ID.
*/
void addrcache_next_id( AddressCache *cache ) {
	g_return_if_fail( cache != NULL );
	cache->nextID++;
}

/*
* Refresh internal variables. This can be used force a reload.
*/
void addrcache_refresh( AddressCache *cache ) {
	cache->dataRead = FALSE;
	cache->modified = TRUE;
	cache->accessFlag = FALSE;
	cache->modifyTime = 0;
}

/*
* Free hash table visitor function.
*/
static gint addrcache_free_item_vis( gpointer key, gpointer value, gpointer data ) {
	AddrItemObject *obj = ( AddrItemObject * ) value;

	if( ADDRITEM_TYPE(obj) == ITEMTYPE_PERSON ) {
		addritem_free_item_person( ( ItemPerson * ) obj );
	}
	else if( ADDRITEM_TYPE(obj) == ITEMTYPE_EMAIL ) {
		addritem_free_item_email( ( ItemEMail * ) obj );
	}
	else if( ADDRITEM_TYPE(obj) == ITEMTYPE_GROUP ) {
		addritem_free_item_group( ( ItemGroup * ) obj );
	}
	else if( ADDRITEM_TYPE(obj) == ITEMTYPE_FOLDER ) {
		addritem_free_item_folder( ( ItemFolder * ) obj );
	}
	key = NULL;
	value = NULL;
	return TRUE;
}

/*
* Free hash table of address cache items.
*/
static void addrcache_free_item_hash( GHashTable *table ) {
	g_return_if_fail( table != NULL );
	g_hash_table_foreach_remove( table, addrcache_free_item_vis, NULL );
}

/*
* Free up folders and groups.
*/
static void addrcache_free_all_folders( ItemFolder *parent ) {
	GList *node;

	if( parent == NULL ) return;

	node = parent->listFolder;
	while( node ) {
		ItemFolder *folder = node->data;
		addrcache_free_all_folders( folder );
		node->data = NULL;
		node = g_list_next( node );
	}
	g_list_free( parent->listPerson );
	g_list_free( parent->listGroup );
	g_list_free( parent->listFolder );
	parent->listPerson = NULL;
	parent->listGroup = NULL;
	parent->listFolder = NULL;
}

/*
* Clear the address cache.
*/
void addrcache_clear( AddressCache *cache ) {
	g_return_if_fail( cache != NULL );

	/* printf( "...addrcache_clear :%s:\n", cache->name ); */
	/* Free up folders and hash table */
	addrcache_free_all_folders( cache->rootFolder );
	addrcache_free_item_hash( cache->itemHash );
	g_hash_table_destroy( cache->itemHash );
	cache->itemHash = NULL;
	ADDRITEM_PARENT(cache->rootFolder) = NULL;
	addritem_free_item_folder( cache->rootFolder );
	cache->rootFolder = NULL;
	if( cache->tempList ) g_list_free( cache->tempList );
	cache->tempList = NULL;

	/* Reset to initial state */
	cache->itemHash = g_hash_table_new( g_str_hash, g_str_equal );
	cache->rootFolder = addritem_create_item_folder();
	cache->rootFolder->isRoot = TRUE;
	ADDRITEM_PARENT(cache->rootFolder) = NULL;

	addrcache_refresh( cache );
}

/*
* Free address cache.
*/
void addrcache_free( AddressCache *cache ) {
	g_return_if_fail( cache != NULL );

	cache->dirtyFlag = FALSE;
	addrcache_free_all_folders( cache->rootFolder );
	addrcache_free_item_hash( cache->itemHash );
	g_hash_table_destroy( cache->itemHash );
	cache->itemHash = NULL;
	ADDRITEM_PARENT(cache->rootFolder) = NULL;
	addritem_free_item_folder( cache->rootFolder );
	cache->rootFolder = NULL;
	g_list_free( cache->tempList );
	cache->tempList = NULL;
	g_free( cache->cacheID );
	cache->cacheID = NULL;
	g_free( cache->name );
	cache->name = NULL;
	g_free( cache );
}

/*
* Check whether file has changed by comparing with cache.
* return: TRUE if file has changed.
*/
gboolean addrcache_check_file( AddressCache *cache, gchar *path ) {
	gboolean retVal;
	struct stat filestat;
	retVal = TRUE;
	if( path ) {
		if( 0 == g_stat( path, &filestat ) ) {
			if( filestat.st_mtime == cache->modifyTime ) retVal = FALSE;
		}
	}
	return retVal;
}

/*
* Save file time to cache.
* return: TRUE if time marked.
*/
gboolean addrcache_mark_file( AddressCache *cache, gchar *path ) {
	gboolean retVal = FALSE;
	struct stat filestat;
	if( path ) {
		if( 0 == g_stat( path, &filestat ) ) {
			cache->modifyTime = filestat.st_mtime;
			retVal = TRUE;
		}
	}
	return retVal;
}

/*
* Print list of items.
*/
void addrcache_print_item_list( GList *list, FILE *stream ) {
	GList *node = list;
	while( node ) {
		AddrItemObject *obj = node->data;
		if( ADDRITEM_TYPE(obj) == ITEMTYPE_PERSON ) {
			addritem_print_item_person( ( ItemPerson * ) obj, stream );
		}
		else if( ADDRITEM_TYPE(obj) == ITEMTYPE_GROUP ) {
			addritem_print_item_group( ( ItemGroup * ) obj, stream );
		}
		else if( ADDRITEM_TYPE(obj) == ITEMTYPE_FOLDER ) {
			addritem_print_item_folder( ( ItemFolder * ) obj, stream );
		}
		node = g_list_next( node );
	}
	fprintf( stream, "\t---\n" );
}

/*
* Print item hash table visitor function.
*/
static void addrcache_print_item_vis( gpointer key, gpointer value, gpointer data ) {
	AddrItemObject *obj = ( AddrItemObject * ) value;
	FILE *stream = ( FILE * ) data;
	if( ADDRITEM_TYPE(obj) == ITEMTYPE_PERSON ) {
		addritem_print_item_person( ( ItemPerson * ) obj, stream );
	}
	else if( ADDRITEM_TYPE(obj) == ITEMTYPE_EMAIL ) {
		printf( "addrcache: print email\n" );
	}
	else if( ADDRITEM_TYPE(obj) == ITEMTYPE_GROUP ) {
		addritem_print_item_group( ( ItemGroup * ) obj, stream );
	}
	else if( ADDRITEM_TYPE(obj) == ITEMTYPE_FOLDER ) {
		addritem_print_item_folder( ( ItemFolder * ) obj, stream );
	}
}

/*
* Dump entire address cache hash table contents.
*/
void addrcache_print( AddressCache *cache, FILE *stream ) {
	g_return_if_fail( cache != NULL );
	fprintf( stream, "AddressCache:\n" );
	fprintf( stream, "cache id : %s\n",  cache->cacheID );
	fprintf( stream, "next id  : %d\n",  cache->nextID );
	fprintf( stream, "name     : %s\n",  cache->name );
	fprintf( stream, "mod time : %ld\n", (long int)cache->modifyTime );
	fprintf( stream, "modified : %s\n",  cache->modified ? "yes" : "no" );
	fprintf( stream, "data read: %s\n",  cache->dataRead ? "yes" : "no" );
}

/*
* Dump entire address cache hash table contents.
*/
void addrcache_dump_hash( AddressCache *cache, FILE *stream ) {
	g_return_if_fail( cache != NULL );
	addrcache_print( cache, stream );
	g_hash_table_foreach( cache->itemHash, addrcache_print_item_vis, stream );
}

/*
 * Allocate ID for person.
 */
void addrcache_id_person( AddressCache *cache, ItemPerson *person ) {
	g_return_if_fail( cache != NULL );
	g_return_if_fail( person != NULL );
	if( ADDRITEM_ID(person) ) return;
	addrcache_next_id( cache );
	ADDRITEM_ID(person) = g_strdup_printf( "%d", cache->nextID );
}

/*
 * Allocate ID for group.
 */
void addrcache_id_group( AddressCache *cache, ItemGroup *group ) {
	g_return_if_fail( cache != NULL );
	g_return_if_fail( group != NULL );
	if( ADDRITEM_ID(group) ) return;
	addrcache_next_id( cache );
	ADDRITEM_ID(group) = g_strdup_printf( "%d", cache->nextID );
}

/*
 * Allocate ID for folder.
 */
void addrcache_id_folder( AddressCache *cache, ItemFolder *folder ) {
	g_return_if_fail( cache != NULL );
	g_return_if_fail( folder != NULL );
	if( ADDRITEM_ID(folder) ) return;
	addrcache_next_id( cache );
	ADDRITEM_ID(folder) = g_strdup_printf( "%d", cache->nextID );
}

/*
 * Allocate ID for email address.
 */
void addrcache_id_email( AddressCache *cache, ItemEMail *email ) {
	g_return_if_fail( cache != NULL );
	g_return_if_fail( email != NULL );
	if( ADDRITEM_ID(email) ) return;
	addrcache_next_id( cache );
	ADDRITEM_ID(email) = g_strdup_printf( "%d", cache->nextID );
}

/*
 * Allocate ID for user attribute.
 */
void addrcache_id_attribute( AddressCache *cache, UserAttribute *attrib ) {
	g_return_if_fail( cache != NULL );
	g_return_if_fail( attrib != NULL );
	if( attrib->uid ) return;
	addrcache_next_id( cache );
	attrib->uid = g_strdup_printf( "%d", cache->nextID );
}

/*
* Add person to hash table.
* return: TRUE if item added.
*/
gboolean addrcache_hash_add_person( AddressCache *cache, ItemPerson *person ) {
	if( g_hash_table_lookup( cache->itemHash, ADDRITEM_ID(person) ) ) {
		return FALSE;
	}
	g_hash_table_insert( cache->itemHash, ADDRITEM_ID(person), person );
	return TRUE;
}

/*
* Add email to hash table.
* return: TRUE if item added.
*/
gboolean addrcache_hash_add_email( AddressCache *cache, ItemEMail *email ) {
	if( g_hash_table_lookup( cache->itemHash, ADDRITEM_ID(email) ) ) {
		return FALSE;
	}
	g_hash_table_insert( cache->itemHash, ADDRITEM_ID(email), email );
	return TRUE;
}

/*
* Add group to hash table.
* return: TRUE if item added.
*/
gboolean addrcache_hash_add_group( AddressCache *cache, ItemGroup *group ) {
	g_return_val_if_fail( cache != NULL, FALSE );
	g_return_val_if_fail( group != NULL, FALSE );

	if( g_hash_table_lookup( cache->itemHash, ADDRITEM_ID(group) ) ) {
		return FALSE;
	}
	g_hash_table_insert( cache->itemHash, ADDRITEM_ID(group), group );
	return TRUE;
}

/*
* Add folder to hash table.
* return: TRUE if item added.
*/
gboolean addrcache_hash_add_folder( AddressCache *cache, ItemFolder *folder ) {
	g_return_val_if_fail( cache != NULL, FALSE );
	g_return_val_if_fail( folder != NULL, FALSE );

	if( g_hash_table_lookup( cache->itemHash, ADDRITEM_ID(folder) ) ) {
		return FALSE;
	}
	g_hash_table_insert( cache->itemHash, ADDRITEM_ID(folder), folder );
	return TRUE;
}

/*
* Add person to specified folder in cache.
*/
gboolean addrcache_folder_add_person( AddressCache *cache, ItemFolder *folder, ItemPerson *item ) {
	gboolean retVal = FALSE;

	g_return_val_if_fail( cache != NULL, FALSE );
	g_return_val_if_fail( folder != NULL, FALSE );
	g_return_val_if_fail( item != NULL, FALSE );

	retVal = addrcache_hash_add_person( cache, item );
	if( retVal ) {
		addritem_folder_add_person( folder, item );
		cache->dirtyFlag = TRUE;
	}
	return retVal;
}

/*
* Add folder to specified folder in cache.
*/
gboolean addrcache_folder_add_folder( AddressCache *cache, ItemFolder *folder, ItemFolder *item ) {
	gboolean retVal = FALSE;

	g_return_val_if_fail( cache != NULL, FALSE );
	g_return_val_if_fail( folder != NULL, FALSE );
	g_return_val_if_fail( item != NULL, FALSE );

	retVal = addrcache_hash_add_folder( cache, item );
	if( retVal ) {
		addritem_folder_add_folder( folder, item );
		cache->dirtyFlag = TRUE;
	}
	return TRUE;
}

/*
* Add folder to specified folder in cache.
*/
gboolean addrcache_folder_add_group( AddressCache *cache, ItemFolder *folder, ItemGroup *item ) {
	gboolean retVal = FALSE;

	g_return_val_if_fail( cache != NULL, FALSE );
	g_return_val_if_fail( folder != NULL, FALSE );
	g_return_val_if_fail( item != NULL, FALSE );

	retVal = addrcache_hash_add_group( cache, item );
	if( retVal ) {
		addritem_folder_add_group( folder, item );
		cache->dirtyFlag = TRUE;
	}
	return retVal;
}

/*
* Add person to address cache.
* return: TRUE if item added.
*/
gboolean addrcache_add_person( AddressCache *cache, ItemPerson *person ) {
	gboolean retVal = FALSE;

	g_return_val_if_fail( cache != NULL, FALSE );
	g_return_val_if_fail( person != NULL, FALSE );

	retVal = addrcache_hash_add_person( cache, person );
	if( retVal ) {
		addritem_folder_add_person( cache->rootFolder, person );
		cache->dirtyFlag = TRUE;
	}
	return retVal;
}

/*
* Add EMail address to person.
* return: TRUE if item added.
*/
gboolean addrcache_person_add_email( AddressCache *cache, ItemPerson *person, ItemEMail *email ) {
	gboolean retVal = FALSE;

	g_return_val_if_fail( cache != NULL, FALSE );
	g_return_val_if_fail( person != NULL, FALSE );
	g_return_val_if_fail( email != NULL, FALSE );

	retVal = addrcache_hash_add_email( cache, email );
	if( retVal ) {
		addritem_person_add_email( person, email );
		cache->dirtyFlag = TRUE;
	}
	return retVal;
}

/*
* Add group to address cache.
* return: TRUE if item added.
*/
gboolean addrcache_add_group( AddressCache *cache, ItemGroup *group ) {
	gboolean retVal = FALSE;

	g_return_val_if_fail( cache != NULL, FALSE );
	g_return_val_if_fail( group != NULL, FALSE );

	retVal = addrcache_hash_add_group( cache, group );
	if( retVal ) {
		addritem_folder_add_group( cache->rootFolder, group );
		cache->dirtyFlag = TRUE;
	}
	return retVal;
}

/*
* Add EMail address to person.
* return: TRUE if item added.
*/
gboolean addrcache_group_add_email( AddressCache *cache, ItemGroup *group, ItemEMail *email ) {
	g_return_val_if_fail( cache != NULL, FALSE );
	g_return_val_if_fail( group != NULL, FALSE );
	g_return_val_if_fail( email != NULL, FALSE );

	addritem_group_add_email( group, email );
	cache->dirtyFlag = TRUE;
	return TRUE;
}

/*
* Add folder to address cache.
* return: TRUE if item added.
*/
gboolean addrcache_add_folder( AddressCache *cache, ItemFolder *folder ) {
	gboolean retVal = FALSE;

	g_return_val_if_fail( cache != NULL, FALSE );
	g_return_val_if_fail( folder != NULL, FALSE );

	retVal = addrcache_hash_add_folder( cache, folder );
	if( retVal ) {
		addritem_folder_add_folder( cache->rootFolder, folder );
		cache->dirtyFlag = TRUE;
	}
	return retVal;
}

/*
* Move person to destination folder.
* Enter: cache  Cache.
*        person Person to move.
*        target Target folder.
*/
void addrcache_folder_move_person(
	AddressCache *cache, ItemPerson *person, ItemFolder *target )
{
	ItemFolder *parent;

	g_return_if_fail( cache != NULL );
	g_return_if_fail( person != NULL );

	parent = ( ItemFolder * ) ADDRITEM_PARENT(person);
	if( ! parent ) parent = cache->rootFolder;
	parent->listPerson = g_list_remove( parent->listPerson, person );
	target->listPerson = g_list_append( target->listPerson, person );
	ADDRITEM_PARENT(person) = ADDRITEM_OBJECT(target);
	cache->dirtyFlag = TRUE;
}

/*
* Move group to destination folder.
* Enter: cache  Cache.
*        group  Group to move.
*        target Target folder.
*/
void addrcache_folder_move_group(
	AddressCache *cache, ItemGroup *group, ItemFolder *target )
{
	ItemFolder *parent;

	g_return_if_fail( cache != NULL );
	g_return_if_fail( group != NULL );

	parent = ( ItemFolder * ) ADDRITEM_PARENT(group);
	if( ! parent ) parent = cache->rootFolder;
	parent->listGroup = g_list_remove( parent->listGroup, group );
	target->listGroup = g_list_append( target->listGroup, group );
	ADDRITEM_PARENT(group) = ADDRITEM_OBJECT(target);
	cache->dirtyFlag = TRUE;
}

/*
* Move folder to destination folder.
* Enter: cache  Cache.
*        folder Folder to move.
*        target Target folder.
*/
void addrcache_folder_move_folder(
	AddressCache *cache, ItemFolder *folder, ItemFolder *target )
{
	ItemFolder *parent;

	g_return_if_fail( cache != NULL );
	g_return_if_fail( folder != NULL );

	parent = ( ItemFolder * ) ADDRITEM_PARENT(folder);
	if( ! parent ) parent = cache->rootFolder;
	parent->listFolder = g_list_remove( parent->listFolder, folder );
	target->listFolder = g_list_append( target->listFolder, folder );
	ADDRITEM_PARENT(folder) = ADDRITEM_OBJECT(target);
	cache->dirtyFlag = TRUE;
}

/*
* Return pointer to object (either person or group) for specified ID.
* param: uid Object ID.
* return: Object, or NULL if not found.
*/
AddrItemObject *addrcache_get_object( AddressCache *cache, const gchar *uid ) {
	AddrItemObject *obj = NULL;
	gchar *uidH;

	g_return_val_if_fail( cache != NULL, NULL );

	if( uid == NULL || *uid == '\0' ) return NULL;
	obj = ( AddrItemObject * ) g_hash_table_lookup( cache->itemHash, uid );
	if( obj ) {
		/* Check for matching UID */
		uidH = ADDRITEM_ID(obj);
		if( uidH ) {
			if( strcmp( uidH, uid ) == 0 ) return obj;
		}
	}
	return NULL;
}

/*
* Return pointer for specified object ID.
* param: uid Object ID.
* return: Person object, or NULL if not found.
*/
ItemPerson *addrcache_get_person( AddressCache *cache, const gchar *uid ) {
	ItemPerson *person = NULL;
	AddrItemObject *obj = addrcache_get_object( cache, uid );

	if( obj ) {
		if( ADDRITEM_TYPE(obj) == ITEMTYPE_PERSON ) {
			person = ( ItemPerson * ) obj;
		}
	}
	return person;
}

/*
* Return pointer for specified object ID.
* param: uid group ID.
* return: Group object, or NULL if not found.
*/
ItemGroup *addrcache_get_group( AddressCache *cache, const gchar *uid ) {
	ItemGroup *group = NULL;
	AddrItemObject *obj = addrcache_get_object( cache, uid );

	if( obj ) {
		if( ADDRITEM_TYPE(obj) == ITEMTYPE_GROUP ) {
			group = ( ItemGroup * ) obj;
		}
	}
	return group;
}

/*
* Find email address in address cache.
* param: eid	EMail ID.
* return: email object for specified object ID and email ID, or NULL if not found.
*/
ItemEMail *addrcache_get_email( AddressCache *cache, const gchar *eid ) {
	ItemEMail *email = NULL;
	AddrItemObject *obj = addrcache_get_object( cache, eid );

	if( obj ) {
		if( ADDRITEM_TYPE(obj) == ITEMTYPE_EMAIL ) {
			email = ( ItemEMail * ) obj;
		}
	}
	return email;
}

/*
* Remove attribute from person.
* param: uid	Object ID for person.
*        aid	Attribute ID.
* return: UserAttribute object, or NULL if not found. Note that object should still be freed.
*/
UserAttribute *addrcache_person_remove_attrib_id( AddressCache *cache, const gchar *uid, const gchar *aid ) {
	UserAttribute *attrib = NULL;
	ItemPerson *person;

	if( aid == NULL || *aid == '\0' ) return NULL;

	person = addrcache_get_person( cache, uid );
	if( person ) {
		attrib = addritem_person_remove_attrib_id( person, aid );
		cache->dirtyFlag = TRUE;
	}
	return attrib;
}

/*
* Remove attribute from person.
* param: person	Person.
*        attrib	Attribute to remove.
* return: UserAttribute object. Note that object should still be freed.
*/
UserAttribute *addrcache_person_remove_attribute( AddressCache *cache, ItemPerson *person, UserAttribute *attrib ) {
	UserAttribute *found = NULL;

	g_return_val_if_fail( cache != NULL, NULL );

	if( person && attrib ) {
		found = addritem_person_remove_attribute( person, attrib );
		cache->dirtyFlag = TRUE;
	}
	return found;
}

/*
* Remove group from address cache.
* param: group	Group to remove.
* return: Group, or NULL if not found. Note that object should still be freed.
*/
ItemGroup *addrcache_remove_group( AddressCache *cache, ItemGroup *group ) {
	AddrItemObject *obj = NULL;

	g_return_val_if_fail( cache != NULL, NULL );

	if( group ) {
		gchar *uid = ADDRITEM_ID(group);
		if( uid == NULL || *uid == '\0' ) return NULL;
		obj = ( AddrItemObject * ) g_hash_table_lookup( cache->itemHash, uid );
		if( obj ) {
			ItemFolder *parent = ( ItemFolder * ) ADDRITEM_PARENT(group);
			if( ! parent ) parent = cache->rootFolder;

			/* Remove group from parent's list and hash table */
			parent->listGroup = g_list_remove( parent->listGroup, obj );
			g_hash_table_remove( cache->itemHash, uid );
			cache->dirtyFlag = TRUE;
			return group;
		}
	}
	return NULL;
}

/*
* Remove specified email from address cache. Note that object is only
* removed from cache and not parent objects.
* param: email	EMail to remove.
* return: EMail, or NULL if not found. Note that object should still be freed.
*/
ItemEMail *addrcache_remove_email( AddressCache *cache, ItemEMail *email ) {
	AddrItemObject *obj = NULL;

	g_return_val_if_fail( cache != NULL, NULL );

	if( email ) {
		gchar *eid = ADDRITEM_ID(email);
		if( eid == NULL || *eid == '\0' ) return NULL;
		obj = ( AddrItemObject * ) g_hash_table_lookup( cache->itemHash, eid );
		if( obj ) {
			if( ADDRITEM_TYPE(obj) == ITEMTYPE_EMAIL ) {
				/* Remove email addresses from hash table. */
				g_hash_table_remove( cache->itemHash, eid );
				cache->dirtyFlag = TRUE;
				return email;
			}
		}
	}
	return NULL;
}

/*
* Hash table visitor function to remove email from group.
*/
static void addrcache_allgrp_rem_email_vis( gpointer key, gpointer value, gpointer data ) {
	AddrItemObject *obj = ( AddrItemObject * ) value;
	ItemEMail *email = ( ItemEMail * ) data;

	if( ! email ) return;
	if( ADDRITEM_TYPE(obj) == ITEMTYPE_GROUP ) {
		ItemGroup *group = ( ItemGroup * ) value;
		if( group ) {
			/* Remove each email address that belongs to the person from the list */
			group->listEMail = g_list_remove( group->listEMail, email );
		}
	}
}

/*
* Remove specified person from address cache.
* param: person	Person to remove.
* return: Person, or NULL if not found. Note that object should still be freed.
*/
ItemPerson *addrcache_remove_person( AddressCache *cache, ItemPerson *person ) {
	AddrItemObject *obj = NULL;
	gchar *uid;

	g_return_val_if_fail( cache != NULL, NULL );

	if( person ) {
		uid = ADDRITEM_ID(person);
		if( uid == NULL || *uid == '\0' ) return NULL;
		obj = ( AddrItemObject * ) g_hash_table_lookup( cache->itemHash, uid );
		if( obj ) {
			if( ADDRITEM_TYPE(obj) == ITEMTYPE_PERSON ) {
				ItemFolder *parent;
				GList *node;

				/* Remove all email addresses for person */
				/* from groups and from hash table */
				node = person->listEMail;
				while( node ) {
					ItemEMail *email;
					gchar *eid;

					email = node->data;
					g_hash_table_foreach( cache->itemHash,
						addrcache_allgrp_rem_email_vis, email );
					eid = ADDRITEM_ID( email );
					g_hash_table_remove( cache->itemHash, eid );
					node = g_list_next( node );
				}

				/* Remove person from owning folder */
				parent = ( ItemFolder * ) ADDRITEM_PARENT(person);
				if( ! parent ) parent = cache->rootFolder;
				parent->listPerson = g_list_remove( parent->listPerson, person );
				g_hash_table_remove( cache->itemHash, uid );
				cache->dirtyFlag = TRUE;
				return person;
			}
		}
	}
	return NULL;
}

/*
* Remove email address in address cache for specified person.
* param: person	Person.
*        email	EMail to remove.
* return: EMail object, or NULL if not found. Note that object should still be freed.
*/
ItemEMail *addrcache_person_remove_email( AddressCache *cache, ItemPerson *person, ItemEMail *email ) {
	ItemEMail *found = NULL;

	g_return_val_if_fail( cache != NULL, NULL );

	if( person && email ) {
		found = addritem_person_remove_email( person, email );
		if( found ) {
			/* Remove email from all groups. */
			g_hash_table_foreach( cache->itemHash, addrcache_allgrp_rem_email_vis, email );

			/* Remove email from person's address list */
			if( person->listEMail ) {
				person->listEMail = g_list_remove( person->listEMail, email );
			}
			/* Unlink reference to person. */
			ADDRITEM_PARENT(email) = NULL;
			cache->dirtyFlag = TRUE;
		}
	}
	return found;
}

/*
* Move email address in address cache to new person. If member of group, address
* remains in group.
* param: cache  Cache.
*        email  EMail to remove.
*        target Target person.
* return: EMail object, or NULL if not found. Note that object should still be freed.
*/
void addrcache_person_move_email(
	AddressCache *cache, ItemEMail *email, ItemPerson *target )
{
	ItemPerson *person;

	g_return_if_fail( cache != NULL );

	if( email == NULL ) return;
	if( target == NULL ) return;

	person = ( ItemPerson * ) ADDRITEM_PARENT(email);
	if( person ) {
		ItemEMail *found;
		found = addritem_person_remove_email( person, email );
		if( found ) {
			/*
			if( person->listEMail ) {
				person->listEMail = g_list_remove( person->listEMail, found );
				addritem_person_add_email( target, found );
				cache->dirtyFlag = TRUE;
			}
			*/
			addritem_person_add_email( target, found );
			cache->dirtyFlag = TRUE;
		}
	}
}

/*
* Return link list of address items for root level folder. Note that the list contains
* references to items and should be g_free() when done. Do *NOT* attempt to use the
* addrcache_free_xxx() functions... this will destroy the address cache data!
* Return: List of items, or NULL if none.
*/
GList *addrcache_folder_get_address_list( AddressCache *cache, ItemFolder *folder ) {
	GList *list = NULL;
	GList *node = NULL;
	ItemFolder *f = folder;

	g_return_val_if_fail( cache != NULL, NULL );

	if( ! f ) f = cache->rootFolder;
	node = f->listPerson;
	while( node ) {
		list = g_list_append( list, node->data );
		node = g_list_next( node );
	}
	node = f->listGroup;
	while( node ) {
		list = g_list_append( list, node->data );
		node = g_list_next( node );
	}
	return list;
}

/*
* Return link list of persons for specified folder. Note that the list contains
* references to items and should be g_free() when done. Do *NOT* attempt to use the
* addrcache_free_xxx() functions... this will destroy the address cache data!
* Return: List of items, or NULL if none.
*/
GList *addrcache_folder_get_person_list( AddressCache *cache, ItemFolder *folder ) {
	ItemFolder *f = folder;

	g_return_val_if_fail( cache != NULL, NULL );

	if( ! f ) f = cache->rootFolder;
	return addritem_folder_get_person_list( f );
}

/*
* Return link list of group items for specified folder. Note that the list contains
* references to items and should be g_free() when done. Do *NOT* attempt to use the
* addrcache_free_xxx() functions... this will destroy the address cache data!
* Return: List of items, or NULL if none.
*/
GList *addrcache_folder_get_group_list( AddressCache *cache, ItemFolder *folder ) {
	ItemFolder *f = folder;

	g_return_val_if_fail( cache != NULL, NULL );

	if( ! f ) f = cache->rootFolder;
	return addritem_folder_get_group_list( f );
}

/*
* Return link list of folder items for specified folder. Note that the list contains
* references to items and should be g_free() when done. Do *NOT* attempt to used the
* addrcache_free_xxx() functions... this will destroy the address cache data!
* Return: List of items, or NULL if none.
*/
GList *addrcache_folder_get_folder_list( AddressCache *cache, ItemFolder *folder ) {
	GList *node = NULL;
	GList *list = NULL;
	ItemFolder *f = folder;

	g_return_val_if_fail( cache != NULL, NULL );

	if( ! f ) f = cache->rootFolder;
	node = f->listFolder;
	while( node ) {
		list = g_list_append( list, node->data );
		node = g_list_next( node );
	}
	return list;
}

/*
* Return link list of address items for root level folder. Note that the list contains
* references to items and should be g_free() when done. Do *NOT* attempt to used the
* addrcache_free_xxx() functions... this will destroy the address cache data!
* Return: List of items, or NULL if none.
*/
GList *addrcache_get_address_list( AddressCache *cache ) {
	g_return_val_if_fail( cache != NULL, NULL );
	return addrcache_folder_get_address_list( cache, cache->rootFolder );
}

/*
* Return link list of persons for root level folder. Note that the list contains
* references to items and should be g_free() when done. Do *NOT* attempt to used the
* addrcache_free_xxx() functions... this will destroy the address cache data!
* Return: List of items, or NULL if none.
*/
GList *addrcache_get_person_list( AddressCache *cache ) {
	g_return_val_if_fail( cache != NULL, NULL );
	return addritem_folder_get_person_list( cache->rootFolder );
}

/*
* Return link list of group items in root level folder. Note that the list contains
* references to items and should be g_free() when done. Do *NOT* attempt to used the
* addrcache_free_xxx() functions... this will destroy the address cache data!
* Return: List of items, or NULL if none.
*/
GList *addrcache_get_group_list( AddressCache *cache ) {
	g_return_val_if_fail( cache != NULL, NULL );
	return cache->rootFolder->listGroup;
}

/*
* Return link list of folder items in root level folder. Note that the list contains
* references to items and should be g_free() when done. Do *NOT* attempt to used the
* addrcache_free_xxx() functions... this will destroy the address cache data!
* Return: List of items, or NULL if none.
*/
GList *addrcache_get_folder_list( AddressCache *cache ) {
	g_return_val_if_fail( cache != NULL, NULL );
	return cache->rootFolder->listFolder;
}

/*
* Group visitor function.
*/
static void addrcache_get_grp_person_vis( gpointer key, gpointer value, gpointer data ) {
	AddrItemObject *obj = ( AddrItemObject * ) value;

	if( ADDRITEM_TYPE(obj) == ITEMTYPE_GROUP ) {
		AddressCache *cache = data;
		ItemGroup *group = ( ItemGroup * ) obj;
		ItemPerson *person = ( ItemPerson * ) cache->tempList->data;
		GList *node = group->listEMail;
		while( node ) {
			ItemEMail *email = ( ItemEMail * ) node->data;
			if( ADDRITEM_PARENT(email) == ADDRITEM_OBJECT(person) ) {
				if( ! g_list_find( cache->tempList, group ) ) {
					cache->tempList = g_list_append( cache->tempList, group );
				}
			}
			node = g_list_next( node );
		}
	}
}

/*
* Return linked list of groups which contain a reference to specified person's email
* address.
*/
GList *addrcache_get_group_for_person( AddressCache *cache, ItemPerson *person ) {
	GList *list = NULL;

	g_return_val_if_fail( cache != NULL, NULL );

	cache->tempList = NULL;
	cache->tempList = g_list_append( cache->tempList, person );
	g_hash_table_foreach( cache->itemHash, addrcache_get_grp_person_vis, cache );
	cache->tempList = g_list_remove( cache->tempList, person );
	list = cache->tempList;
	cache->tempList = NULL;
	return list;
}

/*
* Find root folder for specified folder.
* Enter: folder Folder to search.
* Return: root folder, or NULL if not found.
*/
ItemFolder *addrcache_find_root_folder( ItemFolder *folder ) {
	ItemFolder *item = folder;
	gint count = 0;

	while( item ) {
		if( item->isRoot ) break;
		if( ++count > ADDRCACHE_MAX_SEARCH_COUNT ) {
			item = NULL;
			break;
		}
		item = ( ItemFolder * ) ADDRITEM_PARENT(folder);
	}
	return item;
}

/*
* Get all person visitor function.
*/
static void addrcache_get_all_persons_vis( gpointer key, gpointer value, gpointer data ) {
	AddrItemObject *obj = ( AddrItemObject * ) value;

	if( ADDRITEM_TYPE(obj) == ITEMTYPE_PERSON ) {
		AddressCache *cache = data;
		cache->tempList = g_list_append( cache->tempList, obj );
	}
}

/*
* Return link list of all persons in address cache.  Note that the list contains
* references to items. Do *NOT* attempt to use the addrcache_free_xxx() functions...
* this will destroy the address cache data!
* Return: List of items, or NULL if none.
*/
GList *addrcache_get_all_persons( AddressCache *cache ) {
	GList *list = NULL;

	g_return_val_if_fail( cache != NULL, NULL );

	cache->tempList = NULL;
	g_hash_table_foreach( cache->itemHash, addrcache_get_all_persons_vis, cache );
	list = cache->tempList;
	cache->tempList = NULL;
	return list;
}

/*
* Get all groups visitor function.
*/
static void addrcache_get_all_groups_vis( gpointer key, gpointer value, gpointer data ) {
	AddrItemObject *obj = ( AddrItemObject * ) value;

	if( ADDRITEM_TYPE(obj) == ITEMTYPE_GROUP ) {
		AddressCache *cache = data;
		cache->tempList = g_list_append( cache->tempList, obj );
	}
}

/*
* Return link list of all groups in address cache.  Note that the list contains
* references to items. Do *NOT* attempt to use the addrcache_free_xxx() functions...
* this will destroy the address cache data!
* Return: List of items, or NULL if none.
*/
GList *addrcache_get_all_groups( AddressCache *cache ) {
	GList *list = NULL;

	g_return_val_if_fail( cache != NULL, NULL );

	cache->tempList = NULL;
	g_hash_table_foreach( cache->itemHash, addrcache_get_all_groups_vis, cache );
	list = cache->tempList;
	cache->tempList = NULL;
	return list;
}

/*
* Remove folder from cache. Children are re-parented to parent folder.
* param: folder Folder to remove.
* return: Folder, or NULL if not found. Note that object should still be freed.
*/
ItemFolder *addrcache_remove_folder( AddressCache *cache, ItemFolder *folder ) {
	AddrItemObject *obj = NULL;

	g_return_val_if_fail( cache != NULL, NULL );

	if( folder ) {
		gchar *uid = ADDRITEM_ID(folder);
		if( uid == NULL || *uid == '\0' ) return NULL;
		obj = ( AddrItemObject * ) g_hash_table_lookup( cache->itemHash, uid );
		if( obj ) {
			ItemFolder *parent = ( ItemFolder * ) ADDRITEM_PARENT(folder);
			GList *node;
			AddrItemObject *aio;
			if( ! parent ) parent = cache->rootFolder;

			/* Re-parent children in folder */
			node = folder->listFolder;
			while( node ) {
				aio = ( AddrItemObject * ) node->data;
				parent->listFolder = g_list_append( parent->listFolder, aio );
				aio->parent = ADDRITEM_OBJECT(parent);
				node = g_list_next( node );
			}
			node = folder->listPerson;
			while( node ) {
				aio = ( AddrItemObject * ) node->data;
				parent->listPerson = g_list_append( parent->listPerson, aio );
				aio->parent = ADDRITEM_OBJECT(parent);
				node = g_list_next( node );
			}
			node = folder->listGroup;
			while( node ) {
				aio = ( AddrItemObject * ) node->data;
				parent->listGroup = g_list_append( parent->listGroup, aio );
				aio->parent = ADDRITEM_OBJECT(parent);
				node = g_list_next( node );
			}

			/* Remove folder from parent's list and hash table */
			parent->listFolder = g_list_remove( parent->listFolder, folder );
			ADDRITEM_PARENT(folder) = NULL;
			g_hash_table_remove( cache->itemHash, uid );
			cache->dirtyFlag = TRUE;
			return folder;
		}
	}
	return NULL;
}

/*
* Remove folder from cache. Children are deleted.
* param: folder Folder to remove.
* return: Folder, or NULL if not found. Note that object should still be freed.
*/
ItemFolder *addrcache_remove_folder_delete( AddressCache *cache, ItemFolder *folder ) {
	AddrItemObject *obj = NULL;

	g_return_val_if_fail( cache != NULL, NULL );

	if( folder ) {
		gchar *uid = ADDRITEM_ID(folder);
		if( uid == NULL || *uid == '\0' ) return NULL;
		obj = ( AddrItemObject * ) g_hash_table_lookup( cache->itemHash, uid );
		if( obj ) {
			ItemFolder *parent = ( ItemFolder * ) ADDRITEM_PARENT(folder);
			if( ! parent ) parent = cache->rootFolder;

			/* Remove groups */
			while( folder->listGroup ) {
				ItemGroup *item = ( ItemGroup * ) folder->listGroup->data;
				item = addrcache_remove_group( cache, item );
				if( item ) {
					addritem_free_item_group( item );
					item = NULL;
				}
			}

			while( folder->listPerson ) {
				ItemPerson *item = ( ItemPerson * ) folder->listPerson->data;
				item = addrcache_remove_person( cache, item );
				if( item ) {
					addritem_free_item_person( item );
					item = NULL;
				}
			}

			/* Recursive deletion of folder */
			while( folder->listFolder ) {
				ItemFolder *item = ( ItemFolder * ) folder->listFolder->data;
				item = addrcache_remove_folder_delete( cache, item );
				if( item ) {
					addritem_free_item_folder( item );
					item = NULL;
				}
			}

			/* Remove folder from parent's list and hash table */
			parent->listFolder = g_list_remove( parent->listFolder, folder );
			ADDRITEM_PARENT(folder) = NULL;
			g_hash_table_remove( cache->itemHash, uid );
			cache->dirtyFlag = TRUE;
			return folder;
		}
	}
	return NULL;
}

/**
 * Add person and address data to cache.
 * \param cache     Cache.
 * \param folder    Folder where to add person, or NULL for root folder.
 * \param name      Common name.
 * \param address   EMail address.
 * \param remarks   Remarks.
 * \return Person added. Do not *NOT* to use the 
 *         <code>addrbook_free_xxx()</code> functions...; this will destroy
 *         the address book data.
 */
ItemPerson *addrcache_add_contact(
		AddressCache *cache, ItemFolder *folder, const gchar *name,
		const gchar *address, const gchar *remarks )
{
	ItemPerson *person = NULL;
	ItemEMail *email = NULL;
	ItemFolder *f = folder;

	g_return_val_if_fail( cache != NULL, NULL );

	if( ! f ) f = cache->rootFolder;

	/* Create person object */
	person = addritem_create_item_person();
	addritem_person_set_common_name( person, name );
	addrcache_id_person( cache, person );
	addrcache_folder_add_person( cache, f, person );

	/* Create email object */
	email = addritem_create_item_email();
	addritem_email_set_address( email, address );
	addritem_email_set_remarks( email, remarks );
	addrcache_id_email( cache, email );
	addritem_person_add_email( person, email );
	cache->dirtyFlag = TRUE;

	return person;
}

/**
 * Create a new folder and add to address cache.
 * \param  cache  Address cache.
 * \param  folder Parent folder where to add folder, or <i>NULL</i> for
 *                root folder.
 * \return Folder that was created. This should <b>*NOT*</b> be
 *         <code>g_free()</code> when done.
 */
ItemFolder *addrcache_add_new_folder( AddressCache *cache, ItemFolder *parent )
{
	ItemFolder *folder;
	ItemFolder *p = parent;

	g_return_val_if_fail( cache != NULL, NULL );

	if( !p ) p = cache->rootFolder;
	folder = addritem_create_item_folder();
	addrcache_id_folder( cache, folder );
	if( addrcache_hash_add_folder( cache, folder ) ) {
		p->listFolder = g_list_append( p->listFolder, folder );
		ADDRITEM_PARENT(folder) = ADDRITEM_OBJECT(p);
		addrcache_set_dirty( cache, TRUE );
	}
	else {
		addritem_free_item_folder( folder );
		folder = NULL;
	}
	return folder;
}

/*
* End of Source.
*/
