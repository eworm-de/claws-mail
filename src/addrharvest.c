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
 * Functions for an E-Mail address harvester.
 * Code still needs some work. Address parsing not strictly correct.
 */

#include <sys/stat.h>
#include <dirent.h>
#include <glib.h>
#include <string.h>

#include "utils.h"
#include "mgutils.h"
#include "addrharvest.h"
#include "addritem.h"

/* Mail header names of interest */
static gchar *_headerFrom_     = HEADER_FROM;
static gchar *_headerReplyTo_  = HEADER_REPLY_TO;
static gchar *_headerSender_   = HEADER_SENDER;
static gchar *_headerErrorsTo_ = HEADER_ERRORS_TO;
static gchar *_headerCC_       = HEADER_CC;
static gchar *_headerTo_       = HEADER_TO;

#define ADDR_BUFFSIZE    1024
#define MSG_BUFFSIZE     16384
#define DFL_FOLDER_SIZE  20

/* Noise strings included by some other E-Mail clients */
#define REM_NAME_STRING  "(E-mail)"
#define REM_NAME_STRING2 "(E-mail 2)"

/*
 * Header entry.
 */
typedef struct _HeaderEntry HeaderEntry;
struct _HeaderEntry {
	gchar      *header;
	gboolean   selected;
	ItemFolder *folder;
	gint       count;
};

/*
 * Build header table entry.
 * Enter: harvester Harvester object.
 *        name      Header name.
 */
static void *addrharvest_build_entry(
		AddressHarvester* harvester, gchar *name )
{
	HeaderEntry *entry;

	entry = g_new0( HeaderEntry, 1 );
	entry->header = name;
	entry->selected = FALSE;
	entry->folder = NULL;
	entry->count = 0;
	harvester->headerTable = g_list_append( harvester->headerTable, entry );
}

static void addrharvest_print_hdrentry( HeaderEntry *entry, FILE *stream ) {
	fprintf( stream, "Header Entry\n" );
	fprintf( stream, "    name : %s\n", entry->header );
	fprintf( stream, "selected : %s\n", entry->selected ? "yes" : "no" );
}

/*
 * Free key in table.
 */
static gint addrharvest_free_table_vis( gpointer key, gpointer value, gpointer data ) {
	g_free( key );
	key = NULL;
	value = NULL;
	return TRUE;
}

/*
 * Free lookup table.
 */
static void addrharvest_free_table( AddressHarvester* harvester ) {
	GList *node;
	HeaderEntry *entry;

	/* Free header list */
	node = harvester->headerTable;
	while( node ) {
		entry = ( HeaderEntry * ) node->data;
		entry->header = NULL;
		entry->selected = FALSE;
		entry->folder = NULL;
		entry->count = 0;
		g_free( entry );
		node = g_list_next( node );
	}
	g_list_free( harvester->headerTable );
	harvester->headerTable = NULL;

	/* Free duplicate table */
	g_hash_table_freeze( harvester->dupTable );
	g_hash_table_foreach_remove( harvester->dupTable, addrharvest_free_table_vis, NULL );
	g_hash_table_thaw( harvester->dupTable );
	g_hash_table_destroy( harvester->dupTable );
	harvester->dupTable = NULL;
}

/*
* Create new object.
* Return: Harvester.
*/
AddressHarvester *addrharvest_create( void ) {
	AddressHarvester *harvester;

	harvester = g_new0( AddressHarvester, 1 );
	harvester->path = NULL;
	harvester->dupTable = g_hash_table_new( g_str_hash, g_str_equal );
	harvester->folderSize = DFL_FOLDER_SIZE;
	harvester->retVal = MGU_SUCCESS;

	/* Build header table */
	harvester->headerTable = NULL;
	addrharvest_build_entry( harvester, _headerFrom_ );
	addrharvest_build_entry( harvester, _headerReplyTo_ );
	addrharvest_build_entry( harvester, _headerSender_ );
	addrharvest_build_entry( harvester, _headerErrorsTo_ );
	addrharvest_build_entry( harvester, _headerCC_ );
	addrharvest_build_entry( harvester, _headerTo_ );

	return harvester;
}

/*
* Properties...
*/
/*
 * Specify path to folder that will be harvested.
 * Entry: harvester Harvester object.
 *        value     Full directory path.
 */
void addrharvest_set_path( AddressHarvester* harvester, const gchar *value ) {
	g_return_if_fail( harvester != NULL );
	harvester->path = mgu_replace_string( harvester->path, value );
	g_strstrip( harvester->path );
}

/*
 * Specify maximum folder size.
 * Entry: harvester Harvester object.
 *        value     Folder size.
 */
void addrharvest_set_folder_size(
	AddressHarvester* harvester, const gint value )
{
	g_return_if_fail( harvester != NULL );
	if( value > 0 ) {
		harvester->folderSize = value;
	}
}

/*
 * Search (case insensitive) for header entry with specified name.
 * Enter: harvester Harvester.
 *        name      Header name.
 * Return: Header, or NULL if not found.
 */
static HeaderEntry *addrharvest_find( 
	AddressHarvester* harvester, const gchar *name ) {
	HeaderEntry *retVal;
	GList *node;

	retVal = NULL;
	node = harvester->headerTable;
	while( node ) {
		HeaderEntry *entry;

		entry = node->data;
		if( g_strcasecmp( entry->header, name ) == 0 ) {
			retVal = entry;
			break;
		}
		node = g_list_next( node );
	}
	return retVal;
}

/*
 * Set selection for specified heaader.
 * Enter: harvester Harvester.
 *        name      Header name.
 *        value     Value to set.
 */
void addrharvest_set_header(
	AddressHarvester* harvester, const gchar *name, const gboolean value )
{
	HeaderEntry *entry;

	g_return_if_fail( harvester != NULL );
	entry = addrharvest_find( harvester, name );
	if( entry != NULL ) {
		entry->selected = value;
	}
}

/*
 * Get address count
 * Enter: harvester Harvester.
 *        name      Header name.
 * Return: Address count, or -1 if header not found.
 */
gint addrharvest_get_count( AddressHarvester* harvester, const gchar *name ) {
	HeaderEntry *entry;
	gint count;

	count = -1;
	g_return_val_if_fail( harvester != NULL, count );
	entry = addrharvest_find( harvester, name );
	if( entry != NULL ) {
		count = entry->count;
	}
	return count;
}

/*
* Free up object by releasing internal memory.
* Enter: harvester Harvester.
*/
void addrharvest_free( AddressHarvester *harvester ) {
	g_return_if_fail( harvester != NULL );

	/* Free internal stuff */
	addrharvest_free_table( harvester );
	g_free( harvester->path );

	/* Clear pointers */
	harvester->path = NULL;
	harvester->retVal = MGU_SUCCESS;
	harvester->headerTable = NULL;

	harvester->folderSize = 0;

	/* Now release object */
	g_free( harvester );
}

/*
* Display object to specified stream.
* Enter: harvester Harvester.
*        stream    Output stream.
*/
void addrharvest_print( AddressHarvester *harvester, FILE *stream ) {
	GList *node;
	HeaderEntry *entry;

	g_return_if_fail( harvester != NULL );
	fprintf( stream, "Address Harvester:\n" );
	fprintf( stream, " file path: '%s'\n", harvester->path );
	fprintf( stream, "max folder: %d'\n", harvester->folderSize );

	node = harvester->headerTable;
	while( node ) {
		entry = node->data;
		fprintf( stream, "   header: %s", entry->header );
		fprintf( stream, "\t: %s", entry->selected ? "yes" : "no" );
		fprintf( stream, "\t: %d\n", entry->count );
		node = g_list_next( node );
	}
	fprintf( stream, "  ret val: %d\n", harvester->retVal );
}

/*
 * Insert address into cache.
 * Enter: harvester Harvester object.
 *        entry     Header object.
 *        cache     Address cache to load.
 *        name      Name.
 *        address   eMail address.
 */
static void addrharvest_insert_cache(
		AddressHarvester *harvester, HeaderEntry *entry,
		AddressCache *cache, const gchar *name,
		const gchar *address )
{
	ItemPerson *person;
	ItemFolder *folder;
	gchar *folderName;
	gboolean newFolder;
	gint cnt;
	gchar *key, *value;

	newFolder = FALSE;
	folder = entry->folder;
	if( folder == NULL ) {
		newFolder = TRUE;	/* No folder yet */
	}
	if( entry->count % harvester->folderSize == 0 ) {
		newFolder = TRUE;	/* Folder is full */
	}

	if( newFolder ) {
		cnt = 1 + ( entry->count / harvester->folderSize );
		folderName = g_strdup_printf( "%s (%d)", entry->header, cnt );
		folder = addritem_create_item_folder();
		addritem_folder_set_name( folder, folderName );
		addritem_folder_set_remarks( folder, "" );
		addrcache_id_folder( cache, folder );
		addrcache_add_folder( cache, folder );
		entry->folder = folder;
		g_free( folderName );
	}

	/* Insert address */
	key = g_strdup( address );
	g_strdown( key );
	person = g_hash_table_lookup( harvester->dupTable, key );
	if( person ) {
		/* Use longest name */
		value = ADDRITEM_NAME(person);
		if( strlen( name ) > strlen( value ) ) {
			addritem_person_set_common_name( person, name );
		}
		g_free( key );
	}
	else {
		/* Insert entry */
		person = addrcache_add_contact(
				cache, folder, name, address, "" );
		g_hash_table_insert( harvester->dupTable, key, person );
		entry->count++;
	}
}

/*
 * Remove specified string from name.
 * Enter: name Name.
 *        em   String to remove.
 */
static void addrharvest_del_email( gchar *name, gchar *em ) {
	gchar *p;
	gint ilen;

	ilen = strlen( em );
	while( p = strcasestr( name, em )  ) {
		memmove( p, p + ilen, ilen + 1 );
	}
}

/*
 * Find position of at (@) character in buffer.
 * Enter:  buffer Start of buffer.
 * Return: Position of at character, or NULL if not found.
 * Note: This function searches for the last occurrence of an 'at' character
 * prior to a valid delimiter character for the end of address. This enables
 * an address to be found where it is also used as the name of the
 * recipient. For example:
 *     "axle.rose@netscape.com" <axle.rose@netscape.com>
 * The last occurrence of the at character is detected.
 */
static gchar *addrharvest_find_at( const gchar *buffer ) {
	gchar *atCh;
	gchar *p;

	atCh = strchr( buffer, '@' );
	if( atCh ) {
		/* Search forward for another one */
		p = atCh + 1;
		while( *p ) {
			if( *p == '>' ) {
				break;
			}
			if( *p == ',' ) {
				break;
			}
			if( *p == '\n' ) {
				break;
			}
			if( *p == '@' ) {
				atCh = p;
				break;
			}
			p++;
		}
	}
	return atCh;
}

/*
 * Find start and end of address string.
 * Enter: buf Start address of buffer to process (not modified).
 *        atp Pointer to email at (@) character.
 *        bp  Pointer to start of email address (returned).
 *        ep  Pointer to end of email address (returned).
 */
static void addrharvest_find_address(
		const gchar *buf, const gchar *atp, const gchar **bp,
		const gchar **ep )
{
	const gchar *p;

	/* Find first non-separator char */
	*bp = NULL;
	p = buf;
	while( TRUE ) {
		if( strchr( ",; \n\r", *p ) == NULL ) break;
		p++;
	}
	*bp = p;

	/* Search forward for end of address */
	*ep = NULL;
	p = atp + 1;
	while( TRUE ) {
		if( strchr( ",;", *p ) ) break;
		p++;
	}
	*ep = p;
}

/*
 * Extract E-Mail address from buffer. If found, address is removed from
 * buffer.
 * Enter:  buffer Address buffer.
 * Return: E-Mail address, or NULL if none found. Must g_free() when done.
 */
static gchar *addrharvest_extract_address( gchar *buffer ) {
	gchar *addr;
	gchar *atCh, *p, *bp, *ep;
	gint len;

	addr = NULL;
	atCh = addrharvest_find_at( buffer );
	if( atCh ) {
		/* Search back for start of address */
		bp = NULL;
		p = atCh;
		while( p >= buffer ) {
			bp = p;
			if( *p == '<' ) {
				*p = ' ';
				bp++;
				break;
			}
			p--;
		}

		/* Search fwd for end */
		ep = NULL;
		ep = p = atCh;
		while( *p ) {
			if( *p == '>' ) {
				*p = ' ';
				break;
			}
			else if( *p == ' ' ) {
				break;
			}
			ep = p;
			p++;
		}

		/* Extract email */
		if( bp != NULL ) {
			len = ( ep - bp );
			if( len > 0 ) {
				addr = g_strndup( bp, len + 1 );
				memmove( bp, ep, len );
				*bp = ' ';
			}
		}	
	}
	return addr;
}

/*
 * Parse address from header buffer creating address in cache.
 * Enter: harvester Harvester object.
 *        entry     Header object.
 *        cache     Address cache to load.
 *        hdrBuf    Pointer to header buffer.
 */
static void addrharvest_parse_address(
		AddressHarvester *harvester, HeaderEntry *entry,
		AddressCache *cache, const gchar *hdrBuf )
{
	gchar buffer[ ADDR_BUFFSIZE + 2 ];
	const gchar *bp;
	const gchar *ep;
	gchar *atCh, *email, *p;
	gint bufLen;

	/* Search for an address */
	while( atCh = addrharvest_find_at( hdrBuf ) ) {
		/* Find addres string */
		addrharvest_find_address( hdrBuf, atCh, &bp, &ep );

		/* Copy into buffer */
		bufLen = ( size_t ) ( ep - bp );
		if( bufLen > ADDR_BUFFSIZE ) {
			bufLen = ADDR_BUFFSIZE;
		}
		strncpy( buffer, bp, bufLen );
		buffer[ bufLen ] = '\0';
		buffer[ bufLen + 1 ] = '\0';
		buffer[ bufLen + 2 ] = '\0';

		/* Make whitespace */
		p = buffer;
		while( *p ) {
			if( *p == '\r' || *p == '\n' || *p == '\t' ) *p = ' ';
			p++;
		}

		/* Extract address from buffer */
		email = addrharvest_extract_address( buffer );
		if( email ) {
			/* Unescape characters */
			mgu_str_unescape( buffer );

			/* Remove noise characaters */
			addrharvest_del_email( buffer, REM_NAME_STRING );
			addrharvest_del_email( buffer, REM_NAME_STRING2 );

			/* Remove leading trailing quotes and spaces */
			mgu_str_ltc2space( buffer, '\"', '\"' );
			mgu_str_ltc2space( buffer, '\'', '\'' );
			mgu_str_ltc2space( buffer, '\"', '\"' );
			mgu_str_ltc2space( buffer, '(', ')' );
			g_strstrip( buffer );

			/* Insert into address book */
			addrharvest_insert_cache(
				harvester, entry, cache, buffer, email );
			g_free( email );
		}
		hdrBuf = ep;
	}
}

/*
 * Read specified file into address book.
 * Enter:  harvester Harvester object.
 *         fileName  File to read.
 *         cache     Address cache to load.
 * Return: Status.
 */
static gint addrharvest_readfile(
		AddressHarvester *harvester, const gchar *fileName,
		AddressCache *cache )
{
	gint retVal;
	FILE *msgFile;
	gchar buf[ MSG_BUFFSIZE ], tmp[ MSG_BUFFSIZE ];
	HeaderEntry *entry;

	msgFile = fopen( fileName, "rb" );
	if( ! msgFile ) {
		/* Cannot open file */
		retVal = MGU_OPEN_FILE;
		return retVal;
	}

	for( ;; ) {
		gint val;
		gchar *p;

		val = procheader_get_one_field(
			buf, sizeof(buf), msgFile, NULL );
		if( val == -1 ) {
			break;
		}
		conv_unmime_header( tmp, sizeof(tmp), buf, NULL );
		if(( p = strchr( tmp, ':' ) ) != NULL ) {
			const gchar *hdr;

			*p = '\0';
			hdr = p + 1;
			entry = addrharvest_find( harvester, tmp );
			if( entry && entry->selected ) {
				addrharvest_parse_address(
					harvester, entry, cache, hdr );
			}
		}
	}

	fclose( msgFile );
	return MGU_SUCCESS;
}

/*
 * ============================================================================
 * Read all files in specified directory into address book.
 * Enter:  harvester Harvester object.
 *         cache     Address cache to load.
 * Return: Status.
 * ============================================================================
 */
gint addrharvest_harvest( AddressHarvester *harvester, AddressCache *cache ) {
	gint retVal;
	DIR *dp;
	struct dirent *d;
	struct stat s;
	gint num;

	retVal = MGU_BAD_ARGS;
	g_return_val_if_fail( harvester != NULL, retVal );
	g_return_val_if_fail( cache != NULL, retVal );
	g_return_val_if_fail( harvester->path != NULL, retVal );

	/* Clear cache */
	addrcache_clear( cache );
	cache->dataRead = FALSE;

	if( chdir( harvester->path ) < 0 ) {
		/* printf( "Error changing dir\n" ); */
		return retVal;
	}

	if( ( dp = opendir( harvester->path ) ) == NULL ) {
		/* printf( "Error opening dir\n" ); */
		return retVal;
	}

	while( ( d = readdir( dp ) ) != NULL ) {
		stat( d->d_name, &s );
		if( S_ISREG( s.st_mode ) ) {
			if( ( num = to_number( d->d_name ) ) >= 0 ) {
				addrharvest_readfile(
					harvester, d->d_name, cache );
			}
		}
	}

	closedir( dp );

	/* Mark cache */
	cache->modified = FALSE;
	cache->dataRead = TRUE;

	return retVal;
}

/*
 * ============================================================================
 * Test whether any headers have been selected for processing.
 * Enter:  harvester Harvester object.
 * Return: TRUE if a header was selected, FALSE if none were selected.
 * ============================================================================
 */
gboolean addrharvest_check_header( AddressHarvester *harvester ) {
	gboolean retVal;
	GList *node;

	retVal = FALSE;
	g_return_val_if_fail( harvester != NULL, retVal );

	node = harvester->headerTable;
	while( node ) {
		HeaderEntry *entry;

		entry = ( HeaderEntry * ) node->data;
		if( entry->selected ) return TRUE;
		node = g_list_next( node );
	}
	return retVal;
}

/*
 * ============================================================================
 * End of Source.
 * ============================================================================
 */


