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
 * Functions necessary to access LDIF files (LDAP Data Interchange Format files).
 */

#include <sys/stat.h>
#include <glib.h>

#include "mgutils.h"
#include "ldif.h"
#include "addritem.h"
#include "addrcache.h"

/*
* Create new cardfile object.
*/
LdifFile *ldif_create() {
	LdifFile *ldifFile;
	ldifFile = g_new0( LdifFile, 1 );
	ldifFile->path = NULL;
	ldifFile->file = NULL;
	ldifFile->bufptr = ldifFile->buffer;
	ldifFile->retVal = MGU_SUCCESS;
	return ldifFile;
}

/*
* Properties...
*/
void ldif_set_file( LdifFile* ldifFile, const gchar *value ) {
	g_return_if_fail( ldifFile != NULL );
	ldifFile->path = mgu_replace_string( ldifFile->path, value );
	g_strstrip( ldifFile->path );
}

/*
* Free up cardfile object by releasing internal memory.
*/
void ldif_free( LdifFile *ldifFile ) {
	g_return_if_fail( ldifFile != NULL );

	/* Close file */
	if( ldifFile->file ) fclose( ldifFile->file );

	/* Free internal stuff */
	g_free( ldifFile->path );

	/* Clear pointers */
	ldifFile->file = NULL;
	ldifFile->path = NULL;
	ldifFile->retVal = MGU_SUCCESS;

	/* Now release file object */
	g_free( ldifFile );

}

/*
* Display object to specified stream.
*/
void ldif_print_file( LdifFile *ldifFile, FILE *stream ) {
	g_return_if_fail( ldifFile != NULL );
	fprintf( stream, "LDIF File:\n" );
	fprintf( stream, "file spec: '%s'\n", ldifFile->path );
	fprintf( stream, "  ret val: %d\n",   ldifFile->retVal );
}

/*
* Open file for read.
* return: TRUE if file opened successfully.
*/
static gint ldif_open_file( LdifFile* ldifFile ) {
	/* printf( "Opening file\n" ); */
	if( ldifFile->path ) {
		ldifFile->file = fopen( ldifFile->path, "r" );
		if( ! ldifFile->file ) {
			/* printf( "can't open %s\n", ldifFile->path ); */
			ldifFile->retVal = MGU_OPEN_FILE;
			return ldifFile->retVal;
		}
	}
	else {
		/* printf( "file not specified\n" ); */
		ldifFile->retVal = MGU_NO_FILE;
		return ldifFile->retVal;
	}

	/* Setup a buffer area */
	ldifFile->buffer[0] = '\0';
	ldifFile->bufptr = ldifFile->buffer;
	ldifFile->retVal = MGU_SUCCESS;
	return ldifFile->retVal;
}

/*
* Close file.
*/
static void ldif_close_file( LdifFile *ldifFile ) {
	g_return_if_fail( ldifFile != NULL );
	if( ldifFile->file ) fclose( ldifFile->file );
	ldifFile->file = NULL;
}

/*
* Read line of text from file.
* Return: ptr to buffer where line starts.
*/
static gchar *ldif_get_line( LdifFile *ldifFile ) {
	gchar buf[ LDIFBUFSIZE ];
	gchar ch;
	gchar *ptr;

	if( feof( ldifFile->file ) ) return NULL;

	ptr = buf;
	while( TRUE ) {
		*ptr = '\0';
		ch = fgetc( ldifFile->file );
		if( ch == '\0' || ch == EOF ) {
			if( *buf == '\0' ) return NULL;
			break;
		}
		if( ch == '\n' ) break;
		*ptr = ch;
		ptr++;
	}

	/* Copy into private buffer */
	return g_strdup( buf );
}

/*
* Parse tag name from line buffer.
* Return: Buffer containing the tag name, or NULL if no delimiter char found.
*/
static gchar *ldif_get_tagname( char* line, gchar dlm ) {
	gint len = 0;
	gchar *tag = NULL;
	gchar *lptr = line;
	while( *lptr++ ) {
		if( *lptr == dlm ) {
			len = lptr - line;
			tag = g_strndup( line, len+1 );
			tag[ len ] = '\0';
			g_strdown( tag );
			return tag;
		}
	}
	return tag;
}

/*
* Parse tag value from line buffer.
* Return: Buffer containing the tag value. Empty string is returned if
* no delimiter char found.
*/
static gchar *ldif_get_tagvalue( gchar* line, gchar dlm ) {
	gchar *value = NULL;
	gchar *start = NULL;
	gchar *lptr;
	gint len = 0;

	for( lptr = line; *lptr; lptr++ ) {
		if( *lptr == dlm ) {
			if( ! start )
				start = lptr + 1;
		}
	}
	if( start ) {
		len = lptr - start;
		value = g_strndup( start, len+1 );
	}
	else {
		/* Ensure that we get an empty string */
		value = g_strndup( "", 1 );
	}
	value[ len ] = '\0';
	return value;
}

/*
* Dump linked lists of character strings (for debug).
*/
static void ldif_dump_lists( GSList *listName, GSList *listAddr, GSList *listRem, GSList *listID, FILE *stream ) {
	fprintf( stream, "dump name\n" );
	fprintf( stream, "------------\n" );
	mgu_print_list( listName, stdout );
	fprintf( stream, "dump address\n" );
	fprintf( stream, "------------\n" );
	mgu_print_list( listAddr, stdout );
	fprintf( stream, "dump remarks\n" );
	fprintf( stdout, "------------\n" );
	mgu_print_list( listRem, stdout );
	fprintf( stream, "dump id\n" );
	fprintf( stdout, "------------\n" );
	mgu_print_list( listID, stdout );
}

typedef struct _Ldif_ParsedRec_ Ldif_ParsedRec;
struct _Ldif_ParsedRec_ {
	GSList *listCName;
	GSList *listFName;
	GSList *listLName;
	GSList *listNName;
	GSList *listAddress;
	GSList *listID;
};

/*
* Build an address list entry and append to list of address items. Name is formatted
* as "<first-name> <last-name>".
*/
static void ldif_build_items( LdifFile *ldifFile, Ldif_ParsedRec *rec, AddressCache *cache ) {
	GSList *nodeFirst;
	GSList *nodeAddress;
	gchar *firstName = NULL, *lastName = NULL, *fullName = NULL, *nickName = NULL;
	gint iLen = 0, iLenT = 0;
	ItemPerson *person;
	ItemEMail *email;

	nodeAddress = rec->listAddress;
	if( nodeAddress == NULL ) return;

	/* Find longest first name in list */
	nodeFirst = rec->listFName;
	while( nodeFirst ) {
		if( firstName == NULL ) {
			firstName = nodeFirst->data;
			iLen = strlen( firstName );
		}
		else {
			if( ( iLenT = strlen( nodeFirst->data ) ) > iLen ) {
				firstName = nodeFirst->data;
				iLen = iLenT;
			}
		}
		nodeFirst = g_slist_next( nodeFirst );
	}

	/* Format name */
	if( rec->listLName ) {
		lastName = rec->listLName->data;
	}

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
	}

	if( rec->listNName ) {
		nickName = rec->listNName->data;
	}

	person = addritem_create_item_person();
	addritem_person_set_common_name( person, fullName );
	addritem_person_set_first_name( person, firstName );
	addritem_person_set_last_name( person, lastName );
	addritem_person_set_nick_name( person, nickName );
	addrcache_id_person( cache, person );
	addrcache_add_person( cache, person );

	/* Add address item */
	while( nodeAddress ) {
		email = addritem_create_item_email();
		addritem_email_set_address( email, nodeAddress->data );
		addrcache_id_email( cache, email );
		addrcache_person_add_email( cache, person, email );
		nodeAddress = g_slist_next( nodeAddress );
	}
	g_free( fullName );
	fullName = firstName = lastName = NULL;

}

static void ldif_add_value( Ldif_ParsedRec *rec, gchar *tagName, gchar *tagValue ) {
	gchar *nm, *val;

	nm = g_strdup( tagName );
	g_strdown( nm );
	if( tagValue ) {
		val = g_strdup( tagValue );
	}
	else {
		val = g_strdup( "" );
	}
	g_strstrip( val );
	if( strcmp( nm, LDIF_TAG_COMMONNAME ) == 0 ) {
		rec->listCName = g_slist_append( rec->listCName, val );
	}
	else if( strcmp( nm, LDIF_TAG_FIRSTNAME ) == 0 ) {
		rec->listFName = g_slist_append( rec->listFName, val );
	}
	else if( strcmp( nm, LDIF_TAG_LASTNAME ) == 0 ) {
		rec->listLName = g_slist_append( rec->listLName, val );
	}
	else if( strcmp( nm, LDIF_TAG_NICKNAME ) == 0 ) {
		rec->listNName = g_slist_append( rec->listNName, val );
	}
	else if( strcmp( nm, LDIF_TAG_EMAIL ) == 0 ) {
		rec->listAddress = g_slist_append( rec->listAddress, val );
	}
	g_free( nm );
}

static void ldif_clear_rec( Ldif_ParsedRec *rec ) {
	g_slist_free( rec->listCName );
	g_slist_free( rec->listFName );
	g_slist_free( rec->listLName );
	g_slist_free( rec->listNName );
	g_slist_free( rec->listAddress );
	g_slist_free( rec->listID );
	rec->listCName = NULL;
	rec->listFName = NULL;
	rec->listLName = NULL;
	rec->listNName = NULL;
	rec->listAddress = NULL;
	rec->listID = NULL;
}

static void ldif_print_record( Ldif_ParsedRec *rec, FILE *stream ) {
	fprintf( stdout, "LDIF Parsed Record:\n" );
	fprintf( stdout, "common name:" );
	mgu_print_list( rec->listCName, stdout );
	if( ! rec->listCName ) fprintf( stdout, "\n" );
	fprintf( stdout, "first name:" );
	mgu_print_list( rec->listFName, stdout );
	if( ! rec->listFName ) fprintf( stdout, "\n" );
	fprintf( stdout, "last name:" );
	mgu_print_list( rec->listLName, stdout );
	if( ! rec->listLName ) fprintf( stdout, "\n" );
	fprintf( stdout, "nick name:" );
	mgu_print_list( rec->listNName, stdout );
	if( ! rec->listNName ) fprintf( stdout, "\n" );
	fprintf( stdout, "address:" );
	mgu_print_list( rec->listAddress, stdout );
	if( ! rec->listAddress ) fprintf( stdout, "\n" );
	fprintf( stdout, "id:" );
	mgu_print_list( rec->listID, stdout );
	if( ! rec->listID ) fprintf( stdout, "\n" );
}

/*
* Read file data into address cache.
* Note that one LDIF record identifies one entity uniquely with the
* distinguished name (dn) tag. Each person can have multiple E-Mail
* addresses. Also, each person can have many common name (cn) tags.
*/
static void ldif_read_file( LdifFile *ldifFile, AddressCache *cache ) {
	gchar *tagName = NULL, *tagValue = NULL;
	gchar *lastTag = NULL, *fullValue = NULL;
	GSList *listValue = NULL;
	gboolean flagEOF = FALSE, flagEOR = FALSE;
	Ldif_ParsedRec *rec;

	rec = g_new0( Ldif_ParsedRec, 1 );
	ldif_clear_rec( rec );

	while( ! flagEOF ) {
		gchar *line =  ldif_get_line( ldifFile );
		if( line == NULL ) {
			flagEOF = flagEOR = TRUE;
		}
		else if( *line == '\0' ) {
			flagEOR = TRUE;
		}

		if( flagEOR ) {
			/* EOR, Output address data */
			if( lastTag ) {
				/* Save record */
				fullValue = mgu_list_coalesce( listValue );
				ldif_add_value( rec, lastTag, fullValue );
				/* ldif_print_record( rec, stdout ); */
				ldif_build_items( ldifFile, rec, cache );
				ldif_clear_rec( rec );
				g_free( lastTag );
				mgu_free_list( listValue );
				lastTag = NULL;
				listValue = NULL;
			}
		}
		if( line ) {
			flagEOR = FALSE;
			if( *line == ' ' ) {
				/* Continuation line */
				listValue = g_slist_append( listValue, g_strdup( line+1 ) );
			}
			else if( *line == '=' ) {
				/* Base-64 binary encode field */
				listValue = g_slist_append( listValue, g_strdup( line ) );
			}
			else {
				/* Parse line */
				tagName = ldif_get_tagname( line, LDIF_SEP_TAG );
				if( tagName ) {
					tagValue = ldif_get_tagvalue( line, LDIF_SEP_TAG );
					if( tagValue ) {
						if( lastTag ) {
							/* Save data */
							fullValue = mgu_list_coalesce( listValue );
							ldif_add_value( rec, lastTag, fullValue );
							g_free( lastTag );
							mgu_free_list( listValue );
							lastTag = NULL;
							listValue = NULL;
						}

						lastTag = g_strdup( tagName );
						listValue = g_slist_append( listValue, g_strdup( tagValue ) );
						g_free( tagValue );
					}
					g_free( tagName );
				}
			}
		}

	}

	/* Release data */
	ldif_clear_rec( rec );
	g_free( rec );
	g_free( lastTag );
	mgu_free_list( listValue );

}

// ============================================================================================
/*
* Read file into list. Main entry point
* Enter:  ldifFile LDIF control data.
*         cache    Address cache to load.
* Return: Status code.
*/
// ============================================================================================
gint ldif_import_data( LdifFile *ldifFile, AddressCache *cache ) {
	g_return_val_if_fail( ldifFile != NULL, MGU_BAD_ARGS );
	ldifFile->retVal = MGU_SUCCESS;
	addrcache_clear( cache );
	cache->dataRead = FALSE;
	ldif_open_file( ldifFile );
	if( ldifFile->retVal == MGU_SUCCESS ) {
		/* Read data into the cache */
		ldif_read_file( ldifFile, cache );
		ldif_close_file( ldifFile );

		/* Mark cache */
		cache->modified = FALSE;
		cache->dataRead = TRUE;
	}
	return ldifFile->retVal;
}

/*
* End of Source.
*/

