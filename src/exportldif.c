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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/*
 * Export address book to LDIF file.
 */
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "mgutils.h"
#include "utils.h"
#include "exportldif.h"
#include "xmlprops.h"
#include "ldif.h"


#ifdef MKDIR_TAKES_ONE_ARG
#undef mkdir
#define mkdir(a,b) mkdir(a)
#endif

#define DFL_DIR_SYLPHEED_OUT  "sylpheed-out"
#define DFL_FILE_SYLPHEED_OUT "addressbook.ldif"

#define FMT_BUFSIZE           2048
#define XML_BUFSIZE           2048

/* Settings - properties */
#define EXML_PROPFILE_NAME    "exportldif.xml"
#define EXMLPROP_DIRECTORY    "directory"
#define EXMLPROP_FILE         "file"
#define EXMLPROP_SUFFIX       "suffix"
#define EXMLPROP_RDN_INDEX    "rdn"
#define EXMLPROP_USE_DN       "use-dn"
#define EXMLPROP_EXCL_EMAIL   "exclude-mail"

static gchar *_attrName_UID_   = "uid";
static gchar *_attrName_DName_ = "cn";
static gchar *_attrName_EMail_ = "mail";

/**
 * Create initialized LDIF export control object.
 * \return Initialized export control data.
 */
ExportLdifCtl *exportldif_create( void ) {
	ExportLdifCtl *ctl = g_new0( ExportLdifCtl, 1 );

	ctl->path = NULL;
	ctl->dirOutput = NULL;
	ctl->fileLdif = NULL;
	ctl->suffix = NULL;
	ctl->rdnIndex = EXPORT_LDIF_ID_UID;
	ctl->useDN = FALSE;
	ctl->excludeEMail = TRUE;
	ctl->retVal = MGU_SUCCESS;
	ctl->rcCreate = 0;
	ctl->settingsFile = g_strconcat(
		get_rc_dir(), G_DIR_SEPARATOR_S, EXML_PROPFILE_NAME, NULL );

	return ctl;
}

/**
 * Free up object by releasing internal memory.
 * \return ctl Export control data.
 */
void exportldif_free( ExportLdifCtl *ctl ) {
	g_return_if_fail( ctl != NULL );

	g_free( ctl->path );
	g_free( ctl->fileLdif );
	g_free( ctl->dirOutput );
	g_free( ctl->suffix );
	g_free( ctl->settingsFile );

	/* Clear pointers */
	ctl->path = NULL;
	ctl->dirOutput = NULL;
	ctl->fileLdif = NULL;
	ctl->suffix = NULL;
	ctl->rdnIndex = EXPORT_LDIF_ID_UID;
	ctl->useDN = FALSE;
	ctl->excludeEMail = FALSE;
	ctl->retVal = MGU_SUCCESS;
	ctl->rcCreate = 0;

	/* Now release object */
	g_free( ctl );
}

/**
 * Print control object.
 * \param ctl    Export control data.
 * \param stream Output stream.
 */
void exportldif_print( ExportLdifCtl *ctl, FILE *stream ) {
	fprintf( stream, "ExportLdifCtl:\n" );
	fprintf( stream, "     path: %s\n", ctl->path );
	fprintf( stream, "directory: %s\n", ctl->dirOutput );
	fprintf( stream, "     file: %s\n", ctl->fileLdif );
	fprintf( stream, "   suffix: %s\n", ctl->suffix );
	fprintf( stream, "      rdn: %d\n", ctl->rdnIndex );
	fprintf( stream, "   use DN: %s\n", ctl->useDN ? "y" : "n" );
	fprintf( stream, " ex EMail: %s\n", ctl->excludeEMail ? "y" : "n" );
	fprintf( stream, " settings: %s\n", ctl->settingsFile );
}

/**
 * Specify directory where LDIF files are created.
 * \param ctl   Export control data.
 * \param value Full directory path.
 */
void exportldif_set_output_dir( ExportLdifCtl *ctl, const gchar *value ) {
	g_return_if_fail( ctl != NULL );
	ctl->dirOutput = mgu_replace_string( ctl->dirOutput, value );
	g_strstrip( ctl->dirOutput );
}

/**
 * Specify full file specification of LDIF file.
 * \param ctl   Export control data.
 * \param value Full file specification.
 */
void exportldif_set_path( ExportLdifCtl *ctl, const gchar *value ) {
	g_return_if_fail( ctl != NULL );
	ctl->path = mgu_replace_string( ctl->path, value );
	g_strstrip( ctl->path );
}

/**
 * Specify file name of LDIF file.
 * \param ctl   Export control data.
 * \param value File name.
 */
void exportldif_set_file_ldif( ExportLdifCtl *ctl, const gchar *value ) {
	g_return_if_fail( ctl != NULL );
	ctl->fileLdif = mgu_replace_string( ctl->fileLdif, value );
	g_strstrip( ctl->fileLdif );
}

/**
 * Specify suffix to be used for creating DN entries.
 * \param ctl   Export control data.
 * \param value Suffix.
 */
void exportldif_set_suffix( ExportLdifCtl *ctl, const char *value ) {
	g_return_if_fail( ctl != NULL );
	ctl->suffix = mgu_replace_string( ctl->suffix, value );
	g_strstrip( ctl->suffix );
}

/**
 * Specify index of variable to be used for creating RDN entries.
 * \param ctl   Export control data.
 * \param value Index to variable, as follows:
 * <ul>
 * <li><code>EXPORT_LDIF_ID_UID</code> - Use Sylpheed UID.</li>
 * <li><code>EXPORT_LDIF_ID_DNAME</code> - Use Sylpheed display name.</li>
 * <li><code>EXPORT_LDIF_ID_EMAIL</code> - Use first E-Mail address.</li>
 * </ul>
 */
void exportldif_set_rdn( ExportLdifCtl *ctl, const gint value ) {
	g_return_if_fail( ctl != NULL );
	ctl->rdnIndex = value;
}

/**
 * Specify that <code>DN</code> attribute, if present, should be used as the
 * DN for the entry.
 * \param ctl   Export control data.
 * \param value <i>TRUE</i> if DN should be used.
 */
void exportldif_set_use_dn( ExportLdifCtl *ctl, const gboolean value ) {
	g_return_if_fail( ctl != NULL );
	ctl->useDN = value;
}

/**
 * Specify that records without E-Mail addresses should be excluded.
 * \param ctl   Export control data.
 * \param value <i>TRUE</i> if records without E-Mail should be excluded.
 */
void exportldif_set_exclude_email( ExportLdifCtl *ctl, const gboolean value ) {
	g_return_if_fail( ctl != NULL );
	ctl->excludeEMail = value;
}

/**
 * Format LDAP value name with no embedded commas.
 * \param  value Data value to format.
 * \return Formatted string, should be freed after use.
 */
static gchar *exportldif_fmt_value( gchar *value ) {
	gchar *dupval;
	gchar *src;
	gchar *dest;
	gchar ch;

	/* Duplicate incoming value */
	dest = dupval = g_strdup( value );

	/* Copy characters, ignoring commas */
	src = value;
	while( *src ) {
		ch = *src;
		if( ch != ',' ) {
			*dest = ch;
			dest++;
		}
		src++;
	}
	*dest = '\0';
	return dupval;
}

/**
 * Build DN for entry.
 * \param  ctl    Export control data.
 * \param  person Person to format.
 * \return Formatted DN entry.
 */
static gchar *exportldif_fmt_dn(
		ExportLdifCtl *ctl, const ItemPerson *person )
{
	gchar buf[ FMT_BUFSIZE ];
	gchar *retVal = NULL;
	gchar *attr = NULL;
	gchar *value = NULL;
	gchar *dupval = NULL;

	/* Process RDN */
	*buf = '\0';
	if( ctl->rdnIndex == EXPORT_LDIF_ID_UID ) {
		attr = _attrName_UID_;
		value = ADDRITEM_ID( person );
	}
	else if( ctl->rdnIndex == EXPORT_LDIF_ID_DNAME ) {
		attr = _attrName_DName_;
		value = ADDRITEM_NAME( person );
		dupval = exportldif_fmt_value( value );
	}
	else if( ctl->rdnIndex == EXPORT_LDIF_ID_EMAIL ) {
		GList *node;

		node = person->listEMail;
		if( node ) {
			ItemEMail *email = node->data;

			attr = _attrName_EMail_;
			value = email->address;
			dupval = exportldif_fmt_value( value );
		}
	}

	/* Format DN */
	if( attr ) {
		if( value ) {
			if( strlen( value ) > 0 ) {
				strcat( buf, attr );
				strcat( buf, "=" );
				if( dupval ) {
					/* Format and free duplicated value */
					strcat( buf, dupval );
					g_free( dupval );
				}
				else {
					/* Use original value */
					strcat( buf, value );
				}

				/* Append suffix */
				if( ctl->suffix ) {
					if( strlen( ctl->suffix ) > 0 ) {
						strcat( buf, "," );
						strcat( buf, ctl->suffix );
					}
				}

				retVal = g_strdup( buf );
			}
		}
	}
	return retVal;
}

/**
 * Find DN by searching attribute list.
 * \param  ctl    Export control data.
 * \param  person Person to format.
 * \return Formatted DN entry, should be freed after use.
 */
static gchar *exportldif_find_dn(
			ExportLdifCtl *ctl, const ItemPerson *person )
{
	gchar *retVal = NULL;
	const GList *node;

	node = person->listAttrib;
	while( node ) {
		UserAttribute *attrib = node->data;

		node = g_list_next( node );
		if( g_utf8_collate( attrib->name, LDIF_TAG_DN ) == 0 ) {
			retVal = g_strdup( attrib->value );
			break;
		}
	}
	return retVal;
}

/**
 * Format E-Mail entries for person.
 * \param  person Person to format.
 * \param  stream Output stream.
 * \return <i>TRUE</i> if entry formatted.
 */
static gboolean exportldif_fmt_email( const ItemPerson *person, FILE *stream ) {
	gboolean retVal = FALSE;
	const GList *node;

	node = person->listEMail;
	while( node ) {
		ItemEMail *email = node->data;

		node = g_list_next( node );
		ldif_write_value( stream, LDIF_TAG_EMAIL, email->address );
		retVal = TRUE;
	}
	return retVal;
}

/**
 * Test for E-Mail entries for person.
 * \param  person Person to test.
 * \return <i>TRUE</i> if person has E-Mail address.
 */
static gboolean exportldif_test_email( const ItemPerson *person )
{
	gboolean retVal = FALSE;
	const GList *node;

	node = person->listEMail;
	while( node ) {
		ItemEMail *email = node->data;

		node = g_list_next( node );
		if( email->address ) {
			if( strlen( email->address ) > 0 ) {
				retVal = TRUE;
				break;
			}
		}
		retVal = TRUE;
	}
	return retVal;
}

/**
 * Format persons in an address book folder.
 * \param  ctl    Export control data.
 * \param  stream Output stream.
 * \param  folder Folder to format.
 * \return <i>TRUE</i> if no persons were formatted.
 */
static gboolean exportldif_fmt_person(
		ExportLdifCtl *ctl, FILE *stream, const ItemFolder *folder )
{
	gboolean retVal = TRUE;
	const GList *node;

	if( folder->listPerson == NULL ) return retVal;

	node = folder->listPerson;
	while( node ) {
		AddrItemObject *aio = node->data;
		node = g_list_next( node );

		if( aio && aio->type == ITEMTYPE_PERSON ) {
			ItemPerson *person = ( ItemPerson * ) aio;
			gboolean classPerson = FALSE;
			gboolean classInetP = FALSE;
			gchar *dn = NULL;

			/* Check for E-Mail */
			if( exportldif_test_email( person ) ) {
				classInetP = TRUE;
			}
			else {
				/* Bail if no E-Mail address */
				if( ctl->excludeEMail ) continue;
			}

			/* Format DN */
			if( ctl->useDN ) {
				dn = exportldif_find_dn( ctl, person );
			}
			if( dn == NULL ) {
				dn = exportldif_fmt_dn( ctl, person );
			}
			if( dn == NULL ) continue;
			ldif_write_value( stream, LDIF_TAG_DN, dn );
			g_free( dn );

			/*
			 * Test for schema requirements. This is a simple
			 * test and does not trap all LDAP schema errors.
			 * These can be detected when the LDIF file is
			 * loaded into an LDAP server.
			 */
			if( person->lastName ) {
				if( strlen( person->lastName ) > 0 ) {
					classPerson = TRUE;
					classInetP = TRUE;
				}
			}

			if( classPerson ) {
				ldif_write_value( stream,
					LDIF_TAG_OBJECTCLASS, LDIF_CLASS_PERSON );
			}
			if( classInetP ) {
				ldif_write_value( stream,
					LDIF_TAG_OBJECTCLASS, LDIF_CLASS_INET_PERSON );
			}

			/* Format person attributes */
			ldif_write_value(
				stream, LDIF_TAG_COMMONNAME, ADDRITEM_NAME( person ) );
			ldif_write_value(
				stream, LDIF_TAG_LASTNAME, person->lastName );
			ldif_write_value(
				stream, LDIF_TAG_FIRSTNAME, person->firstName );
			ldif_write_value(
				stream, LDIF_TAG_NICKNAME, person->nickName );

			/* Format E-Mail */
			exportldif_fmt_email( person, stream );

			/* End record */
			ldif_write_eor( stream );

			retVal = FALSE;
		}
	}

	return retVal;
}

/**
 * Format an address book folder.
 * \param  ctl    Export control data.
 * \param  stream Output stream.
 * \param  folder Folder to format.
 * \return <i>TRUE</i> if no persons were formatted.
 */
static void exportldif_fmt_folder(
		ExportLdifCtl *ctl, FILE *stream, const ItemFolder *folder )
{
	const GList *node;

	/* Export entries in this folder */
	exportldif_fmt_person( ctl, stream, folder );

	/* Export entries in sub-folders */
	node = folder->listFolder;
	while( node ) {
		AddrItemObject *aio = node->data;

		node = g_list_next( node );
		if( aio && aio->type == ITEMTYPE_FOLDER ) {
			ItemFolder *subFolder = ( ItemFolder * ) aio;
			exportldif_fmt_folder( ctl, stream, subFolder );
		}
	}
}

/**
 * Export address book to LDIF file.
 * \param  ctl   Export control data.
 * \param  cache Address book/data source cache.
 * \return Status.
 */
void exportldif_process( ExportLdifCtl *ctl, AddressCache *cache )
{
	ItemFolder *rootFolder;
	FILE *ldifFile;

	ldifFile = g_fopen( ctl->path, "wb" );
	if( ! ldifFile ) {
		/* Cannot open file */
		ctl->retVal = MGU_OPEN_FILE;
		return;
	}

	rootFolder = cache->rootFolder;
	exportldif_fmt_folder( ctl, ldifFile, rootFolder );
	fclose( ldifFile );
	ctl->retVal = MGU_SUCCESS;
}

/**
 * Build full export file specification.
 * \param ctl  Export control data.
 */
static void exportldif_build_filespec( ExportLdifCtl *ctl ) {
	gchar *fileSpec;

	fileSpec = g_strconcat(
		ctl->dirOutput, G_DIR_SEPARATOR_S, ctl->fileLdif, NULL );
	ctl->path = mgu_replace_string( ctl->path, fileSpec );
	g_free( fileSpec );
}

/**
 * Parse directory and filename from full export file specification.
 * \param ctl      Export control data.
 * \param fileSpec File spec.
 */
void exportldif_parse_filespec( ExportLdifCtl *ctl, gchar *fileSpec ) {
	gchar *t;
	gchar *base = g_path_get_basename(fileSpec);

	ctl->fileLdif =
		mgu_replace_string( ctl->fileLdif, base );
	g_free(base);
	t = g_path_get_dirname( fileSpec );
	ctl->dirOutput = mgu_replace_string( ctl->dirOutput, t );
	g_free( t );
	ctl->path = mgu_replace_string( ctl->path, fileSpec );
}

/**
 * Test whether output directory exists.
 * \param  ctl Export control data.
 * \return TRUE if exists.
 */
gboolean exportldif_test_dir( ExportLdifCtl *ctl ) {
	gboolean retVal;
	DIR *dp;

	retVal = FALSE;
	if((dp = opendir( ctl->dirOutput )) != NULL) {
		retVal = TRUE;
		closedir( dp );
	}
	return retVal;
}

/**
 * Create output directory.
 * \param  ctl Export control data.
 * \return TRUE if directory created.
 */
gboolean exportldif_create_dir( ExportLdifCtl *ctl ) {
	gboolean retVal = FALSE;

	ctl->rcCreate = 0;
	if( mkdir( ctl->dirOutput, S_IRWXU ) == 0 ) {
		retVal = TRUE;
	}
	else {
		ctl->rcCreate = errno;
	}
	return retVal;
}

/**
 * Retrieve create directory error message.
 * \param  ctl Export control data.
 * \return Message.
 */
gchar *exportldif_get_create_msg( ExportLdifCtl *ctl ) {
	gchar *msg;

	if( ctl->rcCreate == EEXIST ) {
		msg = _( "Name already exists but is not a directory." );
	}
	else if( ctl->rcCreate == EACCES ) {
		msg = _( "No permissions to create directory." );
	}
	else if( ctl->rcCreate == ENAMETOOLONG ) {
		msg = _( "Name is too long." );
	}
	else {
		msg = _( "Not specified." );
	}
	return msg;
}

/**
 * Set default values.
 * \param  ctl Export control data.
 */
static void exportldif_default_values( ExportLdifCtl *ctl ) {
	gchar *str;

	str = g_strconcat(
		g_get_home_dir(), G_DIR_SEPARATOR_S,
		DFL_DIR_SYLPHEED_OUT, NULL );

	ctl->dirOutput = mgu_replace_string( ctl->dirOutput, str );
	g_free( str );

	ctl->fileLdif =
		mgu_replace_string( ctl->fileLdif, DFL_FILE_SYLPHEED_OUT );
	ctl->suffix = mgu_replace_string( ctl->suffix, "" );

	ctl->rdnIndex = EXPORT_LDIF_ID_UID;
	ctl->useDN = FALSE;
	ctl->retVal = MGU_SUCCESS;
}

/**
 * Load settings from XML properties file.
 * \param  ctl Export control data.
 */
void exportldif_load_settings( ExportLdifCtl *ctl ) {
	XmlProperty *props;
	gint rc;
	gchar buf[ XML_BUFSIZE ];

	props = xmlprops_create();
	xmlprops_set_path( props, ctl->settingsFile );
	rc = xmlprops_load_file( props );
	if( rc == 0 ) {
		/* Read settings */
		*buf = '\0';
		xmlprops_get_property_s( props, EXMLPROP_DIRECTORY, buf );
		ctl->dirOutput = mgu_replace_string( ctl->dirOutput, buf );

		*buf = '\0';
		xmlprops_get_property_s( props, EXMLPROP_FILE, buf );
		ctl->fileLdif = mgu_replace_string( ctl->fileLdif, buf );

		*buf = '\0';
		xmlprops_get_property_s( props, EXMLPROP_SUFFIX, buf );
		ctl->suffix = mgu_replace_string( ctl->suffix, buf );

		ctl->rdnIndex =
			xmlprops_get_property_i( props, EXMLPROP_RDN_INDEX );
		ctl->useDN =
			xmlprops_get_property_b( props, EXMLPROP_USE_DN );
		ctl->excludeEMail =
			xmlprops_get_property_b( props, EXMLPROP_EXCL_EMAIL );
	}
	else {
		/* Set default values */
		exportldif_default_values( ctl );
	}
	exportldif_build_filespec( ctl );
	/* exportldif_print( ctl, stdout ); */

	xmlprops_free( props );
}

/**
 * Save settings to XML properties file.
 * \param  ctl Export control data.
 */
void exportldif_save_settings( ExportLdifCtl *ctl ) {
	XmlProperty *props;

	props = xmlprops_create();
	xmlprops_set_path( props, ctl->settingsFile );

	xmlprops_set_property( props, EXMLPROP_DIRECTORY, ctl->dirOutput );
	xmlprops_set_property( props, EXMLPROP_FILE, ctl->fileLdif );
	xmlprops_set_property( props, EXMLPROP_SUFFIX, ctl->suffix );
	xmlprops_set_property_i( props, EXMLPROP_RDN_INDEX, ctl->rdnIndex );
	xmlprops_set_property_b( props, EXMLPROP_USE_DN, ctl->useDN );
	xmlprops_set_property_b( props, EXMLPROP_EXCL_EMAIL, ctl->excludeEMail );
	xmlprops_save_file( props );
	xmlprops_free( props );
}

/*
 * ============================================================================
 * End of Source.
 * ============================================================================
 */


