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
 * Functions necessary to access VCard files. VCard files are used
 * by GnomeCard for addressbook, and Netscape for sending business
 * card information. Refer to RFC2426 for more information.
 */

#include <sys/stat.h>
#include <glib.h>

#include "mgutils.h"
#include "vcard.h"

#define GNOMECARD_DIR     ".gnome"
#define GNOMECARD_FILE    "GnomeCard"
#define GNOMECARD_SECTION "[file]"
#define GNOMECARD_PARAM   "open"

#define VCARD_TEST_LINES  200

/*
* Specify name to be used.
*/
void vcard_set_name( VCardFile* cardFile, const gchar *name ) {
	/* Copy file name */
	if( cardFile->name ) g_free( cardFile->name );
	if( name ) cardFile->name = g_strdup( name );
	g_strstrip( cardFile->name );
}

/*
* Specify file to be used.
*/
void vcard_set_file( VCardFile* cardFile, const gchar *path ) {
	mgu_refresh_cache( cardFile->addressCache );

	/* Copy file path */
	if( cardFile->path ) g_free( cardFile->path );
	if( path ) cardFile->path = g_strdup( path );
	g_strstrip( cardFile->path );
}

/*
* Create new cardfile object.
*/
VCardFile *vcard_create() {
	VCardFile *cardFile;
	cardFile = g_new( VCardFile, 1 );
	cardFile->name = NULL;
	cardFile->path = NULL;
	cardFile->file = NULL;
	cardFile->bufptr = cardFile->buffer;
	cardFile->addressCache = mgu_create_cache();
	cardFile->retVal = MGU_SUCCESS;
	return cardFile;
}

/*
* Refresh internal variables to force a file read.
*/
void vcard_force_refresh( VCardFile *cardFile ) {
	mgu_refresh_cache( cardFile->addressCache );
}

/*
* Create new cardfile object for specified file.
*/
VCardFile *vcard_create_path( const gchar *path ) {
	VCardFile *cardFile;
	cardFile = vcard_create();
	vcard_set_file(cardFile, path );
	return cardFile;
}

/*
* Free up cardfile object by releasing internal memory.
*/
void vcard_free( VCardFile *cardFile ) {
	g_return_if_fail( cardFile != NULL );

	// fprintf( stdout, "freeing... VCardFile\n" );

	/* Close file */
	if( cardFile->file ) fclose( cardFile->file );

	/* Free internal stuff */
	g_free( cardFile->name );
	g_free( cardFile->path );

	/* Clear cache */
	mgu_clear_cache( cardFile->addressCache );
	mgu_free_cache( cardFile->addressCache );

	// Clear pointers
	cardFile->file = NULL;
	cardFile->name = NULL;
	cardFile->path = NULL;
	cardFile->addressCache = NULL;
	cardFile->retVal = MGU_SUCCESS;

	/* Now release file object */
	g_free( cardFile );

	// fprintf( stdout, "freeing... VCardFile done\n" );

}

/*
* Display object to specified stream.
*/
void vcard_print_file( VCardFile *cardFile, FILE *stream ) {
	GSList *node;
	g_return_if_fail( cardFile != NULL );
	fprintf( stream, "VCardFile:\n" );
	fprintf( stream, "     name: '%s'\n", cardFile->name );
	fprintf( stream, "file spec: '%s'\n", cardFile->path );
	fprintf( stream, "  ret val: %d\n",   cardFile->retVal );
	mgu_print_cache( cardFile->addressCache, stream );
}

/*
* Open file for read.
* return: TRUE if file opened successfully.
*/
gint vcard_open_file( VCardFile* cardFile ) {
	g_return_if_fail( cardFile != NULL );

	// fprintf( stdout, "Opening file\n" );
	cardFile->addressCache->dataRead = FALSE;
	if( cardFile->path ) {
		cardFile->file = fopen( cardFile->path, "r" );
		if( ! cardFile->file ) {
			// fprintf( stderr, "can't open %s\n", cardFile->path );
			cardFile->retVal = MGU_OPEN_FILE;
			return cardFile->retVal;
		}
	}
	else {
		// fprintf( stderr, "file not specified\n" );
		cardFile->retVal = MGU_NO_FILE;
		return cardFile->retVal;
	}

	/* Setup a buffer area */
	cardFile->buffer[0] = '\0';
	cardFile->bufptr = cardFile->buffer;
	cardFile->retVal = MGU_SUCCESS;
	return cardFile->retVal;
}

/*
* Close file.
*/
void vcard_close_file( VCardFile *cardFile ) {
	g_return_if_fail( cardFile != NULL );
	if( cardFile->file ) fclose( cardFile->file );
	cardFile->file = NULL;
}

/*
* Read line of text from file.
* Return: ptr to buffer where line starts.
*/
gchar *vcard_read_line( VCardFile *cardFile ) {
	while( *cardFile->bufptr == '\n' || *cardFile->bufptr == '\0' ) {
		if( fgets( cardFile->buffer, VCARDBUFSIZE, cardFile->file ) == NULL )
			return NULL;
		g_strstrip( cardFile->buffer );
		cardFile->bufptr = cardFile->buffer;
	}
	return cardFile->bufptr;
}

/*
* Read line of text from file.
* Return: ptr to buffer where line starts.
*/
gchar *vcard_get_line( VCardFile *cardFile ) {
	gchar buf[ VCARDBUFSIZE ];
	gchar *start, *end;
	gint len;

	if (vcard_read_line( cardFile ) == NULL ) {
		buf[0] = '\0';
		return;
	}

	/* Copy into private buffer */
	start = cardFile->bufptr;
	len = strlen( start );
	end = start + len;
	strncpy( buf, start, len );
	buf[ len ] = '\0';
	g_strstrip(buf);
	cardFile->bufptr = end + 1;

	/* Return a copy of buffer */	
	return g_strdup( buf );
}

/*
* Free linked lists of character strings.
*/
void vcard_free_lists( GSList *listName, GSList *listAddr, GSList *listRem, GSList* listID ) {
	mgu_free_list( listName );
	mgu_free_list( listAddr );
	mgu_free_list( listRem );
	mgu_free_list( listID );
}

/*
* Read quoted-printable text, which may span several lines into one long string.
* Param: cardFile - object.
* Param: tagvalue - will be placed into the linked list.
*/
gchar *vcard_read_qp( VCardFile *cardFile, char *tagvalue ) {
	GSList *listQP = NULL;
	gint len = 0;
	gchar *line = tagvalue;
	while( line ) {
		listQP = g_slist_append( listQP, line );
		len = strlen( line ) - 1;
		if( len > 0 ) {
			if( line[ len ] != '=' ) break;
			line[ len ] = '\0';
		}
		line = vcard_get_line( cardFile );
	}

	// Coalesce linked list into one long buffer.
	line = mgu_list_coalesce( listQP );

	// Clean up
	mgu_free_list( listQP );
	listQP = NULL;
	return line;
}

/*
* Parse tag name from line buffer.
* Return: Buffer containing the tag name, or NULL if no delimiter char found.
*/
gchar *vcard_get_tagname( char* line, gchar dlm ) {
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
gchar *vcard_get_tagvalue( gchar* line, gchar dlm ) {
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
		// Ensure that we get an empty string
		value = g_strndup( "", 1 );
	}
	value[ len ] = '\0';
	return value;
}

/*
* Dump linked lists of character strings (for debug).
*/
void vcard_dump_lists( GSList *listName, GSList *listAddr, GSList *listRem, GSList *listID, FILE *stream ) {
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

/*
* Build an address list entry and append to list of address items.
*/
void vcard_build_items( VCardFile *cardFile, GSList *listName, GSList *listAddr, GSList *listRem, GSList *listID ) {
	AddressItem *addrItem = NULL;
	GSList *nodeName = listName;
	GSList *nodeID = listID;
	while( nodeName ) {
		GSList *nodeAddress = listAddr;
		GSList *nodeRemarks = listRem;
		while( nodeAddress ) {
			addrItem = mgu_create_address();
			addrItem->name = g_strdup( nodeName->data );
			addrItem->address = g_strdup( nodeAddress->data );
			if( nodeRemarks ) {
				if( nodeRemarks->data ) {
					if( g_strcasecmp( nodeRemarks->data, "internet" ) == 0 ) {
						// Trivially exclude this one (appears for most records)
						addrItem->remarks = g_strdup( "" );
					}
					else {
						addrItem->remarks = g_strdup( nodeRemarks->data );
					}
				}
				else {
					addrItem->remarks = g_strdup( "" );
				}
			}
			else {
					addrItem->remarks = g_strdup( "" );
			}
/*
			if( nodeID ) {
				if( nodeID->data ) {
					addrItem->externalID = g_strdup( nodeID->data );
				}
				else {
					addrItem->externalID = g_strdup( "" );
				}
			}
			else {
				addrItem->externalID = g_strdup( "" );
			}
*/
			mgu_add_cache( cardFile->addressCache, addrItem );

			nodeAddress = g_slist_next( nodeAddress );
			nodeRemarks = g_slist_next( nodeRemarks );
		}
		nodeName = g_slist_next( nodeName );
		nodeID = g_slist_next( nodeID );
	}
	addrItem = NULL;
}

// Unescape characters in quoted-printable string.
void vcard_unescape_qp( gchar *value ) {
	gchar *ptr, *src, *dest;
	int d, v;
	char ch;
	gboolean gotch;
	ptr = value;
	while( *ptr ) {
		gotch = FALSE;
		if( *ptr == '=' ) {
			v = 0;
			ch = *(ptr + 1);
			if( ch ) {
				if( ch > '0' && ch < '8' ) v = ch - '0';
			}
			d = -1;
			ch = *(ptr + 2);
			if( ch ) {
				if( ch > '\x60' ) ch -= '\x20';
				if( ch > '0' && ch < ' ' ) d = ch - '0';
				d = ch - '0';
				if( d > 9 ) d -= 7;
				if( d > -1 && d < 16 ) {
					v = ( 16 * v ) + d;
					gotch = TRUE;
				}
			}
		}
		if( gotch ) {
			// Replace = with char and move down in buffer
			*ptr = v;
			src = ptr + 3;
			dest = ptr + 1;
			while( *src ) {
				*dest++ = *src++;
			}
			*dest = '\0';
		}
		ptr++;
	}
}

/*
* Read file into cache.
* Note that one VCard can have multiple E-Mail addresses (MAIL tags);
* these are broken out into separate address items. An address item
* is generated for the person identified by FN tag and each EMAIL tag.
* If a sub-type is included in the EMAIL entry, this will be used as
* the Remarks member. Also note that it is possible for one VCard
* entry to have multiple FN tags; this might not make sense. However,
* it will generate duplicate address entries for each person listed.
*/
void vcard_read_cache( VCardFile *cardFile ) {
	gchar *tagtemp = NULL, *tagname = NULL, *tagvalue = NULL, *tagtype = NULL, *tagrest = NULL;
	GSList *listName = NULL, *listAddress = NULL, *listRemarks = NULL, *listID = NULL;
	GSList *listQP = NULL;

	for( ;; ) {
		gchar *line =  vcard_get_line( cardFile );
		if( line == NULL ) break;

		// fprintf( stdout, "%s\n", line );

		/* Parse line */
		tagtemp = vcard_get_tagname( line, VCARD_SEP_TAG );
		if( tagtemp ) {
			// fprintf( stdout, "\ttemp:  %s\n", tagtemp );
			tagvalue = vcard_get_tagvalue( line, VCARD_SEP_TAG );
			tagname = vcard_get_tagname( tagtemp, VCARD_SEP_TYPE );
			tagtype = vcard_get_tagvalue( tagtemp, VCARD_SEP_TYPE );
			if( tagname == NULL ) {
				tagname = tagtemp;
				tagtemp = NULL;
			}

			// fprintf( stdout, "\tname:  %s\n", tagname );
			// fprintf( stdout, "\ttype:  %s\n", tagtype );
			// fprintf( stdout, "\tvalue: %s\n", tagvalue );

			if( tagvalue ) {
				if( g_strcasecmp( tagtype, VCARD_TYPE_QP ) == 0 ) {
					// Quoted-Printable: could span multiple lines
					tagvalue = vcard_read_qp( cardFile, tagvalue );
					vcard_unescape_qp( tagvalue );
					// fprintf( stdout, "QUOTED-PRINTABLE !!! final\n>%s<\n", tagvalue );
				}

				if( g_strcasecmp( tagname, VCARD_TAG_START ) == 0 &&  g_strcasecmp( tagvalue, VCARD_NAME ) == 0 ) {
					// fprintf( stdout, "start card\n" );
					vcard_free_lists( listName, listAddress, listRemarks, listID );
					listName = listAddress = listRemarks = listID = NULL;
				}
				if( g_strcasecmp( tagname, VCARD_TAG_FULLNAME ) == 0 ) {
					// fprintf( stdout, "- full name: %s\n", tagvalue );
					listName = g_slist_append( listName, g_strdup( tagvalue ) );
				}
				if( g_strcasecmp( tagname, VCARD_TAG_EMAIL ) == 0 ) {
					// fprintf( stdout, "- address: %s\n", tagvalue );
					listAddress = g_slist_append( listAddress, g_strdup( tagvalue ) );
					listRemarks = g_slist_append( listRemarks, g_strdup( tagtype ) );
				}
				if( g_strcasecmp( tagname, VCARD_TAG_UID ) == 0 ) {
					// fprintf( stdout, "- id: %s\n", tagvalue );
					listID = g_slist_append( listID, g_strdup( tagvalue ) );
				}
				if( g_strcasecmp( tagname, VCARD_TAG_END ) == 0 && g_strcasecmp( tagvalue, VCARD_NAME ) == 0 ) {
					// VCard is complete
					// fprintf( stdout, "end card\n--\n" );
					// vcard_dump_lists( listName, listAddress, listRemarks, listID, stdout );
					vcard_build_items( cardFile, listName, listAddress, listRemarks, listID );
					vcard_free_lists( listName, listAddress, listRemarks, listID );
					listName = listAddress = listRemarks = listID = NULL;
				}
				g_free( tagvalue );
			}
			g_free( tagname );
			g_free( tagtype );
		}
	}

	// Free lists
	vcard_free_lists( listName, listAddress, listRemarks, listID );
	listName = listAddress = listRemarks = listID = NULL;
}

// ============================================================================================
/*
* Read file into list. Main entry point
* Return: TRUE if file read successfully.
*/
// ============================================================================================
gint vcard_read_data( VCardFile *cardFile ) {
	g_return_if_fail( cardFile != NULL );
	cardFile->retVal = MGU_SUCCESS;
	if( mgu_check_file( cardFile->addressCache, cardFile->path ) ) {
		mgu_clear_cache( cardFile->addressCache );
		vcard_open_file( cardFile );
		if( cardFile->retVal == MGU_SUCCESS ) {
			// Read data into the list
			vcard_read_cache( cardFile );
			vcard_close_file( cardFile );

			// Mark cache
			mgu_mark_cache( cardFile->addressCache, cardFile->path );
			cardFile->addressCache->modified = FALSE;
			cardFile->addressCache->dataRead = TRUE;
		}
	}
	return cardFile->retVal;
}

/*
* Return link list of address items.
* Return: TRUE if file read successfully.
*/
GList *vcard_get_address_list( VCardFile *cardFile ) {
	g_return_if_fail( cardFile != NULL );
	return cardFile->addressCache->addressList;
}

/*
* Validate that all parameters specified.
* Return: TRUE if data is good.
*/
gboolean vcard_validate( const VCardFile *cardFile ) {
	gboolean retVal;
	g_return_if_fail( cardFile != NULL );

	retVal = TRUE;
	if( cardFile->path ) {
		if( strlen( cardFile->path ) < 1 ) retVal = FALSE;
	}
	else {
		retVal = FALSE;
	}
	if( cardFile->name ) {
		if( strlen( cardFile->name ) < 1 ) retVal = FALSE;
	}
	else {
		retVal = FALSE;
	}
	return retVal;
}

#define WORK_BUFLEN 1024

/*
* Attempt to find a valid GnomeCard file.
* Return: Filename, or home directory if not found. Filename should
*	be g_free() when done.
*/
gchar *vcard_find_gnomecard( void ) {
	gchar *homedir;
	gchar buf[ WORK_BUFLEN ];
	gchar str[ WORK_BUFLEN ];
	gchar *fileSpec;
	gint len, lenlbl, i;
	FILE *fp;

	homedir = g_get_home_dir();
	if( ! homedir ) return NULL;

	strcpy( str, homedir );
	len = strlen( str );
	if( len > 0 ) {
		if( str[ len-1 ] != G_DIR_SEPARATOR ) {
			str[ len ] = G_DIR_SEPARATOR;
			str[ ++len ] = '\0';
		}
	}
	strcat( str, GNOMECARD_DIR );
	strcat( str, G_DIR_SEPARATOR_S );
	strcat( str, GNOMECARD_FILE );

	fileSpec = NULL;
	if( ( fp = fopen( str, "r" ) ) != NULL ) {
		// Read configuration file
		lenlbl = strlen( GNOMECARD_SECTION );
		while( fgets( buf, sizeof( buf ), fp ) != NULL ) {
			if( 0 == g_strncasecmp( buf, GNOMECARD_SECTION, lenlbl ) ) {
				break;
			}
		}

		while( fgets( buf, sizeof( buf ), fp ) != NULL ) {
			g_strchomp( buf );
			if( buf[0] == '[' ) break;
			for( i = 0; i < lenlbl; i++ ) {
				if( buf[i] == '=' ) {
					if( 0 == g_strncasecmp( buf, GNOMECARD_PARAM, i ) ) {
						fileSpec = g_strdup( buf + i + 1 );
						g_strstrip( fileSpec );
					}
				}
			}
		}
		fclose( fp );
	}

	if( fileSpec == NULL ) {
		// Use the home directory
		str[ len ] = '\0';
		fileSpec = g_strdup( str );
	}

	return fileSpec;
}

/*
* Attempt to read file, testing for valid VCard format.
* Return: TRUE if file appears to be valid format.
*/
gint vcard_test_read_file( const gchar *fileSpec ) {
	gboolean haveStart;
	gchar *tagtemp = NULL, *tagname = NULL, *tagvalue = NULL, *tagtype = NULL, *tagrest = NULL, *line;
	VCardFile *cardFile;
	gint retVal, lines;

	if( ! fileSpec ) return MGU_NO_FILE;

	cardFile = vcard_create_path( fileSpec );
	cardFile->retVal = MGU_SUCCESS;
	vcard_open_file( cardFile );
	if( cardFile->retVal == MGU_SUCCESS ) {
		cardFile->retVal = MGU_BAD_FORMAT;
		haveStart = FALSE;
		lines = VCARD_TEST_LINES;
		while( lines > 0 ) {
			lines--;
			if( ( line =  vcard_get_line( cardFile ) ) == NULL ) break;

			/* Parse line */
			tagtemp = vcard_get_tagname( line, VCARD_SEP_TAG );
			if( tagtemp ) {
				tagvalue = vcard_get_tagvalue( line, VCARD_SEP_TAG );
				tagname = vcard_get_tagname( tagtemp, VCARD_SEP_TYPE );
				tagtype = vcard_get_tagvalue( tagtemp, VCARD_SEP_TYPE );
				if( tagname == NULL ) {
					tagname = tagtemp;
					tagtemp = NULL;
				}

				if( tagvalue ) {
					if( g_strcasecmp( tagtype, VCARD_TYPE_QP ) == 0 ) {
						// Quoted-Printable: could span multiple lines
						tagvalue = vcard_read_qp( cardFile, tagvalue );
						vcard_unescape_qp( tagvalue );
					}

					if( g_strcasecmp( tagname, VCARD_TAG_START ) == 0 &&  g_strcasecmp( tagvalue, VCARD_NAME ) == 0 ) {
						haveStart = TRUE;
					}
					if( g_strcasecmp( tagname, VCARD_TAG_END ) == 0 && g_strcasecmp( tagvalue, VCARD_NAME ) == 0 ) {
						// VCard is complete
						if( haveStart ) cardFile->retVal = MGU_SUCCESS;
					}
					g_free( tagvalue );
				}
				g_free( tagname );
				g_free( tagtype );
			}
		}
		vcard_close_file( cardFile );
	}
	retVal = cardFile->retVal;
	vcard_free( cardFile );
	cardFile = NULL;
	return retVal;
}

/*
* End of Source.
*/
