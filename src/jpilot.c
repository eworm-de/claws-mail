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
 * Functions necessary to access JPilot database files.
 * JPilot is Copyright(c) by Judd Montgomery.
 * Visit http://www.jpilot.org for more details.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_JPILOT

#include <glib.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dlfcn.h>

#ifdef HAVE_LIBPISOCK_PI_ARGS_H
#  include <libpisock/pi-args.h>
#  include <libpisock/pi-appinfo.h>
#  include <libpisock/pi-address.h>
#else
#  include <pi-args.h>
#  include <pi-appinfo.h>
#  include <pi-address.h>
#endif

#include "mgutils.h"
#include "addritem.h"
#include "addrcache.h"
#include "jpilot.h"

#define JPILOT_DBHOME_DIR   ".jpilot"
#define JPILOT_DBHOME_FILE  "AddressDB.pdb"
#define PILOT_LINK_LIB_NAME "libpisock.so"

#define IND_LABEL_LASTNAME  0 	// Index of last name in address data
#define IND_LABEL_FIRSTNAME 1 	// Index of first name in address data
#define IND_PHONE_EMAIL     4 	// Index of E-Mail address in phone labels
#define OFFSET_PHONE_LABEL  3 	// Offset to phone data in address data
#define IND_CUSTOM_LABEL    14	// Offset to custom label names
#define NUM_CUSTOM_LABEL    4 	// Number of custom labels

// Shamelessly copied from JPilot (libplugin.h)
typedef struct {
	unsigned char db_name[32];
	unsigned char flags[2];
	unsigned char version[2];
	unsigned char creation_time[4];
	unsigned char modification_time[4];
	unsigned char backup_time[4];
	unsigned char modification_number[4];
	unsigned char app_info_offset[4];
	unsigned char sort_info_offset[4];
	unsigned char type[4];/*Database ID */
	unsigned char creator_id[4];/*Application ID */
	unsigned char unique_id_seed[4];
	unsigned char next_record_list_id[4];
	unsigned char number_of_records[2];
} RawDBHeader;

// Shamelessly copied from JPilot (libplugin.h)
typedef struct {
	char db_name[32];
	unsigned int flags;
	unsigned int version;
	time_t creation_time;
	time_t modification_time;
	time_t backup_time;
	unsigned int modification_number;
	unsigned int app_info_offset;
	unsigned int sort_info_offset;
	char type[5];/*Database ID */
	char creator_id[5];/*Application ID */
	char unique_id_seed[5];
	unsigned int next_record_list_id;
	unsigned int number_of_records;
} DBHeader;

// Shamelessly copied from JPilot (libplugin.h)
typedef struct {
	unsigned char Offset[4];  /*4 bytes offset from BOF to record */
	unsigned char attrib;
	unsigned char unique_ID[3];
} record_header;

// Shamelessly copied from JPilot (libplugin.h)
typedef struct mem_rec_header_s {
	unsigned int rec_num;
	unsigned int offset;
	unsigned int unique_id;
	unsigned char attrib;
	struct mem_rec_header_s *next;
} mem_rec_header;

// Shamelessly copied from JPilot (libplugin.h)
#define SPENT_PC_RECORD_BIT	256

typedef enum {
	PALM_REC = 100L,
	MODIFIED_PALM_REC = 101L,
	DELETED_PALM_REC = 102L,
	NEW_PC_REC = 103L,
	DELETED_PC_REC = SPENT_PC_RECORD_BIT + 104L,
	DELETED_DELETED_PALM_REC = SPENT_PC_RECORD_BIT + 105L
} PCRecType;

// Shamelessly copied from JPilot (libplugin.h)
typedef struct {
	PCRecType rt;
	unsigned int unique_id;
	unsigned char attrib;
	void *buf;
	int size;
} buf_rec;

/*
* Create new pilot file object.
*/
JPilotFile *jpilot_create() {
	JPilotFile *pilotFile;
	pilotFile = g_new0( JPilotFile, 1 );
	pilotFile->name = NULL;
	pilotFile->file = NULL;
	pilotFile->path = NULL;
	pilotFile->addressCache = addrcache_create();
	pilotFile->readMetadata = FALSE;
	pilotFile->customLabels = NULL;
	pilotFile->labelInd = NULL;
	pilotFile->retVal = MGU_SUCCESS;
	pilotFile->accessFlag = FALSE;
	return pilotFile;
}

/*
* Create new pilot file object for specified file.
*/
JPilotFile *jpilot_create_path( const gchar *path ) {
	JPilotFile *pilotFile;
	pilotFile = jpilot_create();
	jpilot_set_file( pilotFile, path );
	return pilotFile;
}

/*
* Properties...
*/
void jpilot_set_name( JPilotFile* pilotFile, const gchar *value ) {
	g_return_if_fail( pilotFile != NULL );
	pilotFile->name = mgu_replace_string( pilotFile->name, value );
}
void jpilot_set_file( JPilotFile* pilotFile, const gchar *value ) {
	g_return_if_fail( pilotFile != NULL );
	addrcache_refresh( pilotFile->addressCache );
	pilotFile->readMetadata = FALSE;
	pilotFile->path = mgu_replace_string( pilotFile->path, value );
}
void jpilot_set_accessed( JPilotFile *pilotFile, const gboolean value ) {
	g_return_if_fail( pilotFile != NULL );
	pilotFile->accessFlag = value;
}

gint jpilot_get_status( JPilotFile *pilotFile ) {
	g_return_val_if_fail( pilotFile != NULL, -1 );
	return pilotFile->retVal;
}
ItemFolder *jpilot_get_root_folder( JPilotFile *pilotFile ) {
	g_return_val_if_fail( pilotFile != NULL, NULL );
	return addrcache_get_root_folder( pilotFile->addressCache );
}
gchar *jpilot_get_name( JPilotFile *pilotFile ) {
	g_return_val_if_fail( pilotFile != NULL, NULL );
	return pilotFile->name;
}

/*
* Test whether file was modified since last access.
* Return: TRUE if file was modified.
*/
gboolean jpilot_get_modified( JPilotFile *pilotFile ) {
	g_return_val_if_fail( pilotFile != NULL, FALSE );
	return addrcache_check_file( pilotFile->addressCache, pilotFile->path );
}
gboolean jpilot_get_accessed( JPilotFile *pilotFile ) {
	g_return_val_if_fail( pilotFile != NULL, FALSE );
	return pilotFile->accessFlag;
}

/*
* Test whether file was read.
* Return: TRUE if file was read.
*/
gboolean jpilot_get_read_flag( JPilotFile *pilotFile ) {
	g_return_val_if_fail( pilotFile != NULL, FALSE );
	return pilotFile->addressCache->dataRead;
}

/*
* Free up custom label list.
*/
void jpilot_clear_custom_labels( JPilotFile *pilotFile ) {
	GList *node;

	g_return_if_fail( pilotFile != NULL );

	// Release custom labels
	mgu_free_dlist( pilotFile->customLabels );
	pilotFile->customLabels = NULL;

	// Release indexes
	node = pilotFile->labelInd;
	while( node ) {
		node->data = NULL;
		node = g_list_next( node );
	}
	g_list_free( pilotFile->labelInd );
	pilotFile->labelInd = NULL;

	// Force a fresh read
	addrcache_refresh( pilotFile->addressCache );
}

/*
* Append a custom label, representing an E-Mail address field to the
* custom label list.
*/
void jpilot_add_custom_label( JPilotFile *pilotFile, const gchar *labelName ) {
	g_return_if_fail( pilotFile != NULL );

	if( labelName ) {
		gchar *labelCopy = g_strdup( labelName );
		g_strstrip( labelCopy );
		if( *labelCopy == '\0' ) {
			g_free( labelCopy );
		}
		else {
			pilotFile->customLabels = g_list_append( pilotFile->customLabels, labelCopy );
			// Force a fresh read
			addrcache_refresh( pilotFile->addressCache );
		}
	}
}

/*
* Get list of custom labels.
* Return: List of labels. Must use g_free() when done.
*/
GList *jpilot_get_custom_labels( JPilotFile *pilotFile ) {
	GList *retVal = NULL;
	GList *node;

	g_return_val_if_fail( pilotFile != NULL, NULL );

	node = pilotFile->customLabels;
	while( node ) {
		retVal = g_list_append( retVal, g_strdup( node->data ) );
		node = g_list_next( node );
	}
	return retVal;
}

/*
* Free up pilot file object by releasing internal memory.
*/
void jpilot_free( JPilotFile *pilotFile ) {
	g_return_if_fail( pilotFile != NULL );

	/* Free internal stuff */
	g_free( pilotFile->path );

	// Release custom labels
	jpilot_clear_custom_labels( pilotFile );

	/* Clear cache */
	addrcache_clear( pilotFile->addressCache );
	addrcache_free( pilotFile->addressCache );
	pilotFile->addressCache = NULL;
	pilotFile->readMetadata = FALSE;
	pilotFile->accessFlag = FALSE;

	/* Now release file object */
	g_free( pilotFile );
}

/*
* Refresh internal variables to force a file read.
*/
void jpilot_force_refresh( JPilotFile *pilotFile ) {
	addrcache_refresh( pilotFile->addressCache );
}

/*
* Print object to specified stream.
*/
void jpilot_print_file( JPilotFile *pilotFile, FILE *stream ) {
	GList *node;

	g_return_if_fail( pilotFile != NULL );

	fprintf( stream, "JPilotFile:\n" );
	fprintf( stream, "file spec: '%s'\n", pilotFile->path );
	fprintf( stream, " metadata: %s\n", pilotFile->readMetadata ? "yes" : "no" );
	fprintf( stream, "  ret val: %d\n", pilotFile->retVal );

	node = pilotFile->customLabels;
	while( node ) {
		fprintf( stream, "  c label: %s\n", (gchar *)node->data );
		node = g_list_next( node );
	}

	node = pilotFile->labelInd;
	while( node ) {
		fprintf( stream, " labelind: %d\n", GPOINTER_TO_INT(node->data) );
		node = g_list_next( node );
	}

	addrcache_print( pilotFile->addressCache, stream );
	addritem_print_item_folder( pilotFile->addressCache->rootFolder, stream );
}

/*
* Print summary of object to specified stream.
*/
void jpilot_print_short( JPilotFile *pilotFile, FILE *stream ) {
	GList *node;
	g_return_if_fail( pilotFile != NULL );
	fprintf( stream, "JPilotFile:\n" );
	fprintf( stream, "file spec: '%s'\n", pilotFile->path );
	fprintf( stream, " metadata: %s\n", pilotFile->readMetadata ? "yes" : "no" );
	fprintf( stream, "  ret val: %d\n", pilotFile->retVal );

	node = pilotFile->customLabels;
	while( node ) {
		fprintf( stream, "  c label: %s\n", (gchar *)node->data );
		node = g_list_next( node );
	}

	node = pilotFile->labelInd;
	while( node ) {
		fprintf( stream, " labelind: %d\n", GPOINTER_TO_INT(node->data) );
		node = g_list_next( node );
	}
	addrcache_print( pilotFile->addressCache, stream );
}

// Shamelessly copied from JPilot (libplugin.c)
static unsigned int bytes_to_bin(unsigned char *bytes, unsigned int num_bytes) {
unsigned int i, n;
	n=0;
	for (i=0;i<num_bytes;i++) {
		n = n*256+bytes[i];
	}
	return n;
}

// Shamelessly copied from JPilot (utils.c)
/*These next 2 functions were copied from pi-file.c in the pilot-link app */
/* Exact value of "Jan 1, 1970 0:00:00 GMT" - "Jan 1, 1904 0:00:00 GMT" */
#define PILOT_TIME_DELTA (unsigned)(2082844800)

time_t pilot_time_to_unix_time ( unsigned long raw_time ) {
   return (time_t)(raw_time - PILOT_TIME_DELTA);
}

// Shamelessly copied from JPilot (libplugin.c)
static int raw_header_to_header(RawDBHeader *rdbh, DBHeader *dbh) {
	unsigned long temp;

	strncpy(dbh->db_name, rdbh->db_name, 31);
	dbh->db_name[31] = '\0';
	dbh->flags = bytes_to_bin(rdbh->flags, 2);
	dbh->version = bytes_to_bin(rdbh->version, 2);
	temp = bytes_to_bin(rdbh->creation_time, 4);
	dbh->creation_time = pilot_time_to_unix_time(temp);
	temp = bytes_to_bin(rdbh->modification_time, 4);
	dbh->modification_time = pilot_time_to_unix_time(temp);
	temp = bytes_to_bin(rdbh->backup_time, 4);
	dbh->backup_time = pilot_time_to_unix_time(temp);
	dbh->modification_number = bytes_to_bin(rdbh->modification_number, 4);
	dbh->app_info_offset = bytes_to_bin(rdbh->app_info_offset, 4);
	dbh->sort_info_offset = bytes_to_bin(rdbh->sort_info_offset, 4);
	strncpy(dbh->type, rdbh->type, 4);
	dbh->type[4] = '\0';
	strncpy(dbh->creator_id, rdbh->creator_id, 4);
	dbh->creator_id[4] = '\0';
	strncpy(dbh->unique_id_seed, rdbh->unique_id_seed, 4);
	dbh->unique_id_seed[4] = '\0';
	dbh->next_record_list_id = bytes_to_bin(rdbh->next_record_list_id, 4);
	dbh->number_of_records = bytes_to_bin(rdbh->number_of_records, 2);
	return 0;
}

// Shamelessly copied from JPilot (libplugin.c)
/*returns 1 if found */
/*        0 if eof */
static int find_next_offset( mem_rec_header *mem_rh, long fpos,
	unsigned int *next_offset, unsigned char *attrib, unsigned int *unique_id )
{
	mem_rec_header *temp_mem_rh;
	unsigned char found = 0;
	unsigned long found_at;

	found_at=0xFFFFFF;
	for (temp_mem_rh=mem_rh; temp_mem_rh; temp_mem_rh = temp_mem_rh->next) {
		if ((temp_mem_rh->offset > fpos) && (temp_mem_rh->offset < found_at)) {
			found_at = temp_mem_rh->offset;
			/* *attrib = temp_mem_rh->attrib; */
			/* *unique_id = temp_mem_rh->unique_id; */
		}
		if ((temp_mem_rh->offset == fpos)) {
			found = 1;
			*attrib = temp_mem_rh->attrib;
			*unique_id = temp_mem_rh->unique_id;
		}
	}
	*next_offset = found_at;
	return found;
}

// Shamelessly copied from JPilot (libplugin.c)
static void free_mem_rec_header(mem_rec_header **mem_rh) {
	mem_rec_header *h, *next_h;
	for (h=*mem_rh; h; h=next_h) {
		next_h=h->next;
		free(h);
	}
	*mem_rh = NULL;
}

// Shamelessly copied from JPilot (libplugin.c)
static int jpilot_free_db_list( GList **br_list ) {
	GList *temp_list, *first;
	buf_rec *br;

	/* Go to first entry in the list */
	first=NULL;
	for( temp_list = *br_list; temp_list; temp_list = temp_list->prev ) {
		first = temp_list;
	}
	for (temp_list = first; temp_list; temp_list = temp_list->next) {
		if (temp_list->data) {
			br=temp_list->data;
			if (br->buf) {
				free(br->buf);
				temp_list->data=NULL;
			}
			free(br);
		}
	}
	g_list_free(*br_list);
	*br_list=NULL;
	return 0;
}

// Shamelessly copied from JPilot (libplugin.c)
// Read file size.
static int jpilot_get_info_size( FILE *in, int *size ) {
	RawDBHeader rdbh;
	DBHeader dbh;
	unsigned int offset;
	record_header rh;

	fseek(in, 0, SEEK_SET);
	fread(&rdbh, sizeof(RawDBHeader), 1, in);
	if (feof(in)) {
		// fprintf( stderr, "error reading file in 'jpilot_get_info_size'\n" );
		return MGU_EOF;
	}

	raw_header_to_header(&rdbh, &dbh);
	if (dbh.app_info_offset==0) {
		*size=0;
		return MGU_SUCCESS;
	}
	if (dbh.sort_info_offset!=0) {
		*size = dbh.sort_info_offset - dbh.app_info_offset;
		return MGU_SUCCESS;
	}
	if (dbh.number_of_records==0) {
		fseek(in, 0, SEEK_END);
		*size=ftell(in) - dbh.app_info_offset;
		return MGU_SUCCESS;
	}

	fread(&rh, sizeof(record_header), 1, in);
	offset = ((rh.Offset[0]*256+rh.Offset[1])*256+rh.Offset[2])*256+rh.Offset[3];
	*size=offset - dbh.app_info_offset;

	return MGU_SUCCESS;
}

// Read address file into address list. Based on JPilot's
// libplugin.c (jp_get_app_info)
static gint jpilot_get_file_info( JPilotFile *pilotFile, unsigned char **buf, int *buf_size ) {
	FILE *in;
 	int num;
	unsigned int rec_size;
	RawDBHeader rdbh;
	DBHeader dbh;

	if( ( !buf_size ) || ( ! buf ) ) {
		return MGU_BAD_ARGS;
	}

	*buf = NULL;
	*buf_size=0;

	if( pilotFile->path ) {
		in = fopen( pilotFile->path, "r" );
		if( !in ) {
			// fprintf( stderr, "can't open %s\n", pilotFile->path );
			return MGU_OPEN_FILE;
		}
	}
	else {
		// fprintf( stderr, "file not specified\n" );
		return MGU_NO_FILE;
	}

	num = fread( &rdbh, sizeof( RawDBHeader ), 1, in );
	if( num != 1 ) {
	  	if( ferror(in) ) {
  			// fprintf( stderr, "error reading %s\n", pilotFile->path );
			fclose(in);
			return MGU_ERROR_READ;
		}
	}
	if (feof(in)) {
		fclose(in);
		return MGU_EOF;
	}

	// Convert header into something recognizable
	raw_header_to_header(&rdbh, &dbh);

	num = jpilot_get_info_size(in, &rec_size);
	if (num) {
		fclose(in);
		return MGU_ERROR_READ;
	}

	fseek(in, dbh.app_info_offset, SEEK_SET);
	*buf = ( char * ) malloc(rec_size);
	if (!(*buf)) {
		// fprintf( stderr, "jpilot_get_file_info(): Out of memory\n" );
		fclose(in);
		return MGU_OO_MEMORY;
	}
	num = fread(*buf, rec_size, 1, in);
	if (num != 1) {
		if (ferror(in)) {
			fclose(in);
			free(*buf);
			// fprintf( stderr, "Error reading %s\n", pilotFile->path );
			return MGU_ERROR_READ;
		}
	}
	fclose(in);

	*buf_size = rec_size;

	return MGU_SUCCESS;
}

#define	FULLNAME_BUFSIZE   256
#define	EMAIL_BUFSIZE      256
// Read address file into address cache. Based on JPilot's
// jp_read_DB_files (from libplugin.c)
static gint jpilot_read_file( JPilotFile *pilotFile ) {
	FILE *in;
	gchar *buf;
	gint num_records, recs_returned, i, num;
	guint offset, prev_offset, next_offset, rec_size;
	gint out_of_order;
	glong fpos;  /*file position indicator */
	guchar attrib;
	guint unique_id;
	gint cat_id = 0;
	mem_rec_header *mem_rh, *temp_mem_rh, *last_mem_rh;
	record_header rh;
	RawDBHeader rdbh;
	DBHeader dbh;
	gint retVal;
	struct Address addr;
	struct AddressAppInfo *ai;
	gchar **addrEnt;
	gint inum, k;
	gchar fullName[ FULLNAME_BUFSIZE ];
	gchar bufEMail[ EMAIL_BUFSIZE ];
	gchar* extID;
	ItemPerson *person;
	ItemEMail *email;
	gint *indPhoneLbl;
	gchar *labelEntry;
	GList *node;
	ItemFolder *folderInd[ JPILOT_NUM_CATEG ];

	retVal = MGU_SUCCESS;
	mem_rh = last_mem_rh = NULL;
	recs_returned = 0;

	// Pointer to address metadata.
	ai = & pilotFile->addrInfo;

	// Open file for read
	if( pilotFile->path ) {
		in = fopen( pilotFile->path, "r" );
		if( !in ) {
			// fprintf( stderr, "can't open %s\n", pilotFile->path );
			return MGU_OPEN_FILE;
		}
	}
	else {
		// fprintf( stderr, "file not specified\n" );
		return MGU_NO_FILE;
	}

	/* Read the database header */
	num = fread(&rdbh, sizeof(RawDBHeader), 1, in);
	if (num != 1) {
		if (ferror(in)) {
			// fprintf( stderr, "error reading '%s'\n", pilotFile->path );
			fclose(in);
			return MGU_ERROR_READ;
		}
		if (feof(in)) {
	 		return MGU_EOF;
	 	}
	}

	raw_header_to_header(&rdbh, &dbh);

	/*Read each record entry header */
	num_records = dbh.number_of_records;
	out_of_order = 0;
	prev_offset = 0;

	for (i=1; i<num_records+1; i++) {
		num = fread( &rh, sizeof( record_header ), 1, in );
		if (num != 1) {
			if (ferror(in)) {
				// fprintf( stderr, "error reading '%s'\n", pilotFile->path );
				retVal = MGU_ERROR_READ;
				break;
			}
			if (feof(in)) {
				return MGU_EOF;
			}
		}

		offset = ((rh.Offset[0]*256+rh.Offset[1])*256+rh.Offset[2])*256+rh.Offset[3];
		if (offset < prev_offset) {
			out_of_order = 1;
		}
		prev_offset = offset;

		temp_mem_rh = (mem_rec_header *)malloc(sizeof(mem_rec_header));
		if (!temp_mem_rh) {
			// fprintf( stderr, "jpilot_read_db_file(): Out of memory 1\n" );
			retVal = MGU_OO_MEMORY;
			break;
		}

		temp_mem_rh->next = NULL;
		temp_mem_rh->rec_num = i;
		temp_mem_rh->offset = offset;
		temp_mem_rh->attrib = rh.attrib;
		temp_mem_rh->unique_id = (rh.unique_ID[0]*256+rh.unique_ID[1])*256+rh.unique_ID[2];

		if (mem_rh == NULL) {
			mem_rh = temp_mem_rh;
			last_mem_rh = temp_mem_rh;
		}
		else {
			last_mem_rh->next = temp_mem_rh;
			last_mem_rh = temp_mem_rh;
		}

	}	// for( ;; )

	temp_mem_rh = mem_rh;

	if (num_records) {
		if (out_of_order) {
			find_next_offset(mem_rh, 0, &next_offset, &attrib, &unique_id);
		}
		else {
			if (mem_rh) {
				next_offset = mem_rh->offset;
				attrib = mem_rh->attrib;
				unique_id = mem_rh->unique_id;
			}
		}
		fseek(in, next_offset, SEEK_SET);

		// Build array of pointers to categories;
		i = 0;
		node = addrcache_get_list_folder( pilotFile->addressCache );
		while( node ) {
			if( i < JPILOT_NUM_CATEG ) {
				folderInd[i] = node->data;
			}
			node = g_list_next( node );
			i++;
		}

		// Now go load all records		
		while(!feof(in)) {
			//struct CategoryAppInfo *cat = &ai->category;

			fpos = ftell(in);
			if (out_of_order) {
				find_next_offset(mem_rh, fpos, &next_offset, &attrib, &unique_id);
			}
			else {
				next_offset = 0xFFFFFF;
				if (temp_mem_rh) {
					attrib = temp_mem_rh->attrib;
					unique_id = temp_mem_rh->unique_id;
      					cat_id = attrib & 0x0F;
					if (temp_mem_rh->next) {
						temp_mem_rh = temp_mem_rh->next;
						next_offset = temp_mem_rh->offset;
					}
				}
			}
			rec_size = next_offset - fpos;

			buf = ( char * ) malloc(rec_size);
			if (!buf) break;
			num = fread(buf, rec_size, 1, in);
			if ((num != 1)) {
				if (ferror(in)) {
					// fprintf( stderr, "Error reading %s 5\n", pilotFile );
					free(buf);
					retVal = MGU_ERROR_READ;
					break;
				}
			}

			// Retrieve address
			inum = unpack_Address( & addr, buf, rec_size );
			if( inum > 0 ) {
				addrEnt = addr.entry;

				*fullName = *bufEMail = '\0';
				if( addrEnt[ IND_LABEL_FIRSTNAME ] ) {
					strcat( fullName, addrEnt[ IND_LABEL_FIRSTNAME ] );
				}

				if( addrEnt[ IND_LABEL_LASTNAME ] ) {
					strcat( fullName, " " );
					strcat( fullName, addrEnt[ IND_LABEL_LASTNAME ] );
				}
				g_strchug( fullName );
				g_strchomp( fullName );

				person = addritem_create_item_person();
				addritem_person_set_common_name( person, fullName );
				addritem_person_set_first_name( person, addrEnt[ IND_LABEL_FIRSTNAME ] );
				addritem_person_set_last_name( person, addrEnt[ IND_LABEL_LASTNAME ] );
				addrcache_id_person( pilotFile->addressCache, person );

				extID = g_strdup_printf( "%d", unique_id );
				addritem_person_set_external_id( person, extID );
				g_free( extID );
				extID = NULL;

				// Add entry for each email address listed under phone labels.
				indPhoneLbl = addr.phoneLabel;
				for( k = 0; k < JPILOT_NUM_ADDR_PHONE; k++ ) {
					gint ind;
					ind = indPhoneLbl[k];
					/*
					fprintf( stdout, "%d : %d : %20s : %s\n", k, ind,
						ai->phoneLabels[ind], addrEnt[3+k] );
					*/
					if( indPhoneLbl[k] == IND_PHONE_EMAIL ) {
						labelEntry = addrEnt[ OFFSET_PHONE_LABEL + k ];
						if( labelEntry ) {
							strcpy( bufEMail, labelEntry );
							g_strchug( bufEMail );
							g_strchomp( bufEMail );

							email = addritem_create_item_email();
							addritem_email_set_address( email, bufEMail );
							addrcache_id_email( pilotFile->addressCache, email );
							addrcache_person_add_email(
								pilotFile->addressCache, person, email );
						}
					}
				}

				// Add entry for each custom label
				node = pilotFile->labelInd;
				while( node ) {
					gint ind;
					ind = GPOINTER_TO_INT( node->data );
					if( ind > -1 ) {
						/*
						fprintf( stdout, "%d : %20s : %s\n", ind, ai->labels[ind],
							addrEnt[ind] );
						*/
						labelEntry = addrEnt[ind];
						if( labelEntry ) {
							strcpy( bufEMail, labelEntry );
							g_strchug( bufEMail );
							g_strchomp( bufEMail );

							email = addritem_create_item_email();
							addritem_email_set_address( email, bufEMail );
							addritem_email_set_remarks( email, ai->labels[ind] );
							addrcache_id_email( pilotFile->addressCache, email );
							addrcache_person_add_email(
								pilotFile->addressCache, person, email );
						}

					}

					node = g_list_next( node );
				}

				if( person->listEMail ) {
					if( cat_id > -1 && cat_id < JPILOT_NUM_CATEG ) {
						// Add to specified category
						addrcache_folder_add_person(
							pilotFile->addressCache, folderInd[cat_id], person );
					}
					else {
						// Add to root folder
						addrcache_add_person( pilotFile->addressCache, person );
					}
				}
				else {
					addritem_free_item_person( person );
					person = NULL;
				}

			}
			recs_returned++;
		}
	}
	fclose(in);
 	free_mem_rec_header(&mem_rh);
	return retVal;
}

/*
* Read metadata from file.
*/
static gint jpilot_read_metadata( JPilotFile *pilotFile ) {
	gint retVal;
	unsigned int rec_size;
	unsigned char *buf;
	int num;

	g_return_val_if_fail( pilotFile != NULL, -1 );

	pilotFile->readMetadata = FALSE;
	addrcache_clear( pilotFile->addressCache );

	// Read file info
	retVal = jpilot_get_file_info( pilotFile, &buf, &rec_size);
	if( retVal != MGU_SUCCESS ) {
		pilotFile->retVal = retVal;
		return pilotFile->retVal;
	}

	num = unpack_AddressAppInfo( &pilotFile->addrInfo, buf, rec_size );
	if( buf ) {
		free(buf);
	}
	if( num <= 0 ) {
		// fprintf( stderr, "error reading '%s'\n", pilotFile->path );
		pilotFile->retVal = MGU_ERROR_READ;
		return pilotFile->retVal;
	}

	pilotFile->readMetadata = TRUE;
	pilotFile->retVal = MGU_SUCCESS;
	return pilotFile->retVal;
}

/*
* Setup labels and indexes from metadata.
* Return: TRUE is setup successfully.
*/
static gboolean jpilot_setup_labels( JPilotFile *pilotFile ) {
	gboolean retVal = FALSE;
	struct AddressAppInfo *ai;
	GList *node;

	g_return_val_if_fail( pilotFile != NULL, -1 );

	// Release indexes
	node = pilotFile->labelInd;
	while( node ) {
		node->data = NULL;
		node = g_list_next( node );
	}
	pilotFile->labelInd = NULL;

	if( pilotFile->readMetadata ) {
		ai = & pilotFile->addrInfo;
		node = pilotFile->customLabels;
		while( node ) {
			gchar *lbl = node->data;
			gint ind = -1;
			gint i;
			for( i = 0; i < JPILOT_NUM_LABELS; i++ ) {
				gchar *labelName = ai->labels[i];
				if( g_strcasecmp( labelName, lbl ) == 0 ) {
					ind = i;
					break;
				}
			}
			pilotFile->labelInd = g_list_append( pilotFile->labelInd, GINT_TO_POINTER(ind) );
			node = g_list_next( node );
		}
		retVal = TRUE;
	}
	return retVal;
}

/*
* Load list with character strings of label names.
*/
GList *jpilot_load_label( JPilotFile *pilotFile, GList *labelList ) {
	int i;

	g_return_val_if_fail( pilotFile != NULL, NULL );

	if( pilotFile->readMetadata ) {
		struct AddressAppInfo *ai = & pilotFile->addrInfo;
		for( i = 0; i < JPILOT_NUM_LABELS; i++ ) {
			gchar *labelName = ai->labels[i];
			if( labelName ) {
				labelList = g_list_append( labelList, g_strdup( labelName ) );
			}
			else {
				labelList = g_list_append( labelList, g_strdup( "" ) );
			}
		}
	}
	return labelList;
}

/*
* Return category name for specified category ID.
* Enter:  Category ID.
* Return: Name, or empty string if not invalid ID. Name should be g_free() when done.
*/
gchar *jpilot_get_category_name( JPilotFile *pilotFile, gint catID ) {
	gchar *catName = NULL;

	g_return_val_if_fail( pilotFile != NULL, NULL );

	if( pilotFile->readMetadata ) {
		struct AddressAppInfo *ai = & pilotFile->addrInfo;
		struct CategoryAppInfo *cat = &	ai->category;
		if( catID < 0 || catID > JPILOT_NUM_CATEG ) {
		}
		else {
			catName = g_strdup( cat->name[catID] );
		}
	}
	if( ! catName ) catName = g_strdup( "" );
	return catName;
}

/*
* Load list with character strings of phone label names.
*/
GList *jpilot_load_phone_label( JPilotFile *pilotFile, GList *labelList ) {
	gint i;

	g_return_val_if_fail( pilotFile != NULL, NULL );

	if( pilotFile->readMetadata ) {
		struct AddressAppInfo *ai = & pilotFile->addrInfo;
		for( i = 0; i < JPILOT_NUM_PHONELABELS; i++ ) {
			gchar	*labelName = ai->phoneLabels[i];
			if( labelName ) {
				labelList = g_list_append( labelList, g_strdup( labelName ) );
			}
			else {
				labelList = g_list_append( labelList, g_strdup( "" ) );
			}
		}
	}
	return labelList;
}

/*
* Load list with character strings of label names. Only none blank names
* are loaded.
*/
GList *jpilot_load_custom_label( JPilotFile *pilotFile, GList *labelList ) {
	gint i;

	g_return_val_if_fail( pilotFile != NULL, NULL );

	if( pilotFile->readMetadata ) {
		struct AddressAppInfo *ai = & pilotFile->addrInfo;
		for( i = 0; i < NUM_CUSTOM_LABEL; i++ ) {
			gchar *labelName = ai->labels[i+IND_CUSTOM_LABEL];
			if( labelName ) {
				g_strchomp( labelName );
				g_strchug( labelName );
				if( *labelName != '\0' ) {
					labelList = g_list_append( labelList, g_strdup( labelName ) );
				}
			}
		}
	}
	return labelList;
}

/*
* Load list with character strings of category names.
*/
GList *jpilot_get_category_list( JPilotFile *pilotFile ) {
	GList *catList = NULL;
	gint i;

	g_return_val_if_fail( pilotFile != NULL, NULL );

	if( pilotFile->readMetadata ) {
		struct AddressAppInfo *ai = & pilotFile->addrInfo;
		struct CategoryAppInfo *cat = &	ai->category;
		for( i = 0; i < JPILOT_NUM_CATEG; i++ ) {
			gchar *catName = cat->name[i];
			if( catName ) {
				catList = g_list_append( catList, g_strdup( catName ) );
			}
			else {
				catList = g_list_append( catList, g_strdup( "" ) );
			}
		}
	}
	return catList;
}

/*
* Build folder for each category.
*/
static void jpilot_build_category_list( JPilotFile *pilotFile ) {
	struct AddressAppInfo *ai = & pilotFile->addrInfo;
	struct CategoryAppInfo *cat = &	ai->category;
	gint i;

	for( i = 0; i < JPILOT_NUM_CATEG; i++ ) {
		ItemFolder *folder = addritem_create_item_folder();
		addritem_folder_set_name( folder, cat->name[i] );
		addrcache_id_folder( pilotFile->addressCache, folder );
		addrcache_add_folder( pilotFile->addressCache, folder );
	}
}

/*
* Remove empty folders (categories).
*/
static void jpilot_remove_empty( JPilotFile *pilotFile ) {
	GList *listFolder;
	GList *remList;
	GList *node;
	gint i = 0;

	listFolder = addrcache_get_list_folder( pilotFile->addressCache );
	node = listFolder;
	remList = NULL;
	while( node ) {
		ItemFolder *folder = node->data;
		if( ADDRITEM_NAME(folder) == NULL || *ADDRITEM_NAME(folder) == '\0' ) {
			if( folder->listPerson ) {
				// Give name to folder
				gchar name[20];
				sprintf( name, "? %d", i );
				addritem_folder_set_name( folder, name );
			}
			else {
				// Mark for removal
				remList = g_list_append( remList, folder );
			}
		}
		node = g_list_next( node );
		i++;
	}
	node = remList;
	while( node ) {
		ItemFolder *folder = node->data;
		addrcache_remove_folder( pilotFile->addressCache, folder );
		node = g_list_next( node );
	}
	g_list_free( remList );
}

// ============================================================================================
/*
* Read file into list. Main entry point
* Return: TRUE if file read successfully.
*/
// ============================================================================================
gint jpilot_read_data( JPilotFile *pilotFile ) {
	g_return_val_if_fail( pilotFile != NULL, -1 );

	pilotFile->retVal = MGU_SUCCESS;
	pilotFile->accessFlag = FALSE;
	if( addrcache_check_file( pilotFile->addressCache, pilotFile->path ) ) {
		addrcache_clear( pilotFile->addressCache );
		jpilot_read_metadata( pilotFile );
		if( pilotFile->retVal == MGU_SUCCESS ) {
			jpilot_setup_labels( pilotFile );
			jpilot_build_category_list( pilotFile );
			pilotFile->retVal = jpilot_read_file( pilotFile );
			if( pilotFile->retVal == MGU_SUCCESS ) {
				jpilot_remove_empty( pilotFile );
				addrcache_mark_file( pilotFile->addressCache, pilotFile->path );
				pilotFile->addressCache->modified = FALSE;
				pilotFile->addressCache->dataRead = TRUE;
			}
		}
	}
	return pilotFile->retVal;
}

/*
* Return link list of persons.
*/
GList *jpilot_get_list_person( JPilotFile *pilotFile ) {
	g_return_val_if_fail( pilotFile != NULL, NULL );
	return addrcache_get_list_person( pilotFile->addressCache );
}

/*
* Return link list of folders. This is always NULL since there are
* no folders in GnomeCard.
* Return: NULL.
*/
GList *jpilot_get_list_folder( JPilotFile *pilotFile ) {
	g_return_val_if_fail( pilotFile != NULL, NULL );
	return addrcache_get_list_folder( pilotFile->addressCache );
}

/*
* Return link list of all persons. Note that the list contains references
* to items. Do *NOT* attempt to use the addrbook_free_xxx() functions...
* this will destroy the addressbook data!
* Return: List of items, or NULL if none.
*/
GList *jpilot_get_all_persons( JPilotFile *pilotFile ) {
	g_return_val_if_fail( pilotFile != NULL, NULL );
	return addrcache_get_all_persons( pilotFile->addressCache );
}

/*
* Check label list for specified label.
*/
gint jpilot_check_label( struct AddressAppInfo *ai, gchar *lblCheck ) {
	gint i;
	gchar *lblName;

	if( lblCheck == NULL ) return -1;
	if( strlen( lblCheck ) < 1 ) return -1;
	for( i = 0; i < JPILOT_NUM_LABELS; i++ ) {
		lblName = ai->labels[i];
		if( lblName ) {
			if( strlen( lblName ) ) {
				if( g_strcasecmp( lblName, lblCheck ) == 0 ) return i;
			}
		}
	}
	return -2;
}

/*
* Validate that all parameters specified.
* Return: TRUE if data is good.
*/
gboolean jpilot_validate( const JPilotFile *pilotFile ) {
	gboolean retVal;

	g_return_val_if_fail( pilotFile != NULL, FALSE );

	retVal = TRUE;
	if( pilotFile->path ) {
		if( strlen( pilotFile->path ) < 1 ) retVal = FALSE;
	}
	else {
		retVal = FALSE;
	}
	if( pilotFile->name ) {
		if( strlen( pilotFile->name ) < 1 ) retVal = FALSE;
	}
	else {
		retVal = FALSE;
	}
	return retVal;
}

#define WORK_BUFLEN 1024

/*
* Attempt to find a valid JPilot file.
* Return: Filename, or home directory if not found, or empty string if
* no home. Filename should be g_free() when done.
*/
gchar *jpilot_find_pilotdb( void ) {
	gchar *homedir;
	gchar str[ WORK_BUFLEN ];
	gint len;
	FILE *fp;

	homedir = g_get_home_dir();
	if( ! homedir ) return g_strdup( "" );

	strcpy( str, homedir );
	len = strlen( str );
	if( len > 0 ) {
		if( str[ len-1 ] != G_DIR_SEPARATOR ) {
			str[ len ] = G_DIR_SEPARATOR;
			str[ ++len ] = '\0';
		}
	}
	strcat( str, JPILOT_DBHOME_DIR );
	strcat( str, G_DIR_SEPARATOR_S );
	strcat( str, JPILOT_DBHOME_FILE );

	// Attempt to open
	if( ( fp = fopen( str, "r" ) ) != NULL ) {
		fclose( fp );
	}
	else {
		// Truncate filename
		str[ len ] = '\0';
	}
	return g_strdup( str );
}

/*
* Attempt to read file, testing for valid JPilot format.
* Return: TRUE if file appears to be valid format.
*/
gint jpilot_test_read_file( const gchar *fileSpec ) {
	JPilotFile *pilotFile;
	gint retVal;

	if( fileSpec ) {
		pilotFile = jpilot_create_path( fileSpec );
		retVal = jpilot_read_metadata( pilotFile );
		jpilot_free( pilotFile );
		pilotFile = NULL;
	}
	else {
		retVal = MGU_NO_FILE;
	}
	return retVal;
}

/*
* Check whether label is in custom labels.
* Return: TRUE if found.
*/
gboolean jpilot_test_custom_label( JPilotFile *pilotFile, const gchar *labelName ) {
	gboolean retVal;
	GList *node;

	g_return_val_if_fail( pilotFile != NULL, FALSE );

	retVal = FALSE;
	if( labelName ) {
		node = pilotFile->customLabels;
		while( node ) {
			if( g_strcasecmp( labelName, node->data ) == 0 ) {
				retVal = TRUE;
				break;
			}
			node = g_list_next( node );
		}
	}
	return retVal;
}

/*
* Test whether pilot link library installed.
* Return: TRUE if library available.
*/
gboolean jpilot_test_pilot_lib( void ) {
	void *handle, *fun;

	handle = dlopen( PILOT_LINK_LIB_NAME, RTLD_LAZY );
	if( ! handle ) {
		return FALSE;
	}

	// Test for symbols we need
	fun = dlsym( handle, "unpack_Address" );
	if( ! fun ) {
		dlclose( handle );
		return FALSE;
	}

	fun = dlsym( handle, "unpack_AddressAppInfo" );
	if( ! fun ) {
		dlclose( handle );
		return FALSE;
	}
	dlclose( handle );
	return TRUE;
}

#endif	/* USE_JPILOT */

/*
* End of Source.
*/
