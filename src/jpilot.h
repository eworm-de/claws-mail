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
 * Definitions for accessing JPilot database files.
 * JPilot is Copyright(c) by Judd Montgomery.
 * Visit http://www.jpilot.org for more details.
 */

#ifndef __JPILOT_H__
#define __JPILOT_H__

#ifdef USE_JPILOT

#include <pi-address.h>

#include <time.h>
#include <stdio.h>
#include <glib.h>

#include "mgutils.h"

typedef struct _JPilotFile JPilotFile;

struct _JPilotFile {
	gchar        *name;
	FILE         *file;
	gchar        *path;
	AddressCache *addressCache;
	struct AddressAppInfo addrInfo;
	gboolean     readMetadata;
	GSList       *customLabels;
	GSList       *labelInd;
	gint         retVal;
	GList        *categoryList;
	GList        *catAddrList;
};

// Limits
#define JPILOT_NUM_LABELS	22	// Number of labels
#define JPILOT_NUM_PHONELABELS  8 	// Number of phone number labels
#define JPILOT_NUM_CATEG	16	// Number of categories
#define JPILOT_LEN_LABEL	15	// Max length of label
#define JPILOT_LEN_CATEG	15	// Max length of category
#define JPILOT_NUM_ADDR_PHONE   5	// Number of phone entries a person can have

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

/* Function prototypes */
JPilotFile *jpilot_create();
JPilotFile *jpilot_create_path( const gchar *path );
void jpilot_free( JPilotFile *pilotFile );
void jpilot_force_refresh( JPilotFile *pilotFile );
gboolean jpilot_get_modified( JPilotFile *pilotFile );
void jpilot_print_file( JPilotFile *jpilotFile, FILE *stream );
void jpilot_print_list( GSList *list, FILE *stream );
gint jpilot_read_file( JPilotFile *pilotFile );

GList *jpilot_get_address_list( JPilotFile *pilotFile );
GSList *jpilot_load_label( JPilotFile *pilotFile, GSList *labelList );
GSList *jpilot_get_category_list( JPilotFile *pilotFile );
gchar *jpilot_get_category_name( JPilotFile *pilotFile, gint catID );
GSList *jpilot_load_phone_label( JPilotFile *pilotFile, GSList *labelList );
GList *jpilot_load_custom_label( JPilotFile *pilotFile, GList *labelList );

GList *jpilot_get_category_items( JPilotFile *pilotFile );
GList *jpilot_get_address_list_cat( JPilotFile *pilotFile, const gint catID );

void jpilot_set_file( JPilotFile* pilotFile, const gchar *path );
gboolean jpilot_validate( const JPilotFile *pilotFile );
gchar *jpilot_find_pilotdb( void );
gint jpilot_test_read_data( const gchar *fileSpec );

void jpilot_clear_custom_labels( JPilotFile *pilotFile );
void jpilot_add_custom_label( JPilotFile *pilotFile, const gchar *labelName );
GList *jpilot_get_custom_labels( JPilotFile *pilotFile );
gboolean jpilot_test_custom_label( JPilotFile *pilotFile, const gchar *labelName );
gboolean jpilot_setup_labels( JPilotFile *pilotFile );
gboolean jpilot_test_pilot_lib();

#endif /* USE_JPILOT */

#endif /* __JPILOT_H__ */

