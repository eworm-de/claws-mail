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
 * Some utility functions to access LDAP servers.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#ifdef USE_LDAP

#include <glib.h>
#include <string.h>
#include <sys/time.h>
#include <ldap.h>
#include <lber.h>

#define SYLDAP_TEST_FILTER   "(objectclass=*)"
#define SYLDAP_SEARCHBASE_V2 "cn=config"
#define SYLDAP_SEARCHBASE_V3 ""
#define SYLDAP_V2_TEST_ATTR  "database"
#define SYLDAP_V3_TEST_ATTR  "namingcontexts"

/**
 * Attempt to discover the base DN for a server using LDAP version 3.
 * \param  ld  LDAP handle for a connected server.
 * \param  tov Timeout value (seconds), or 0 for none, default 30 secs.
 * \return List of Base DN's, or NULL if could not read. List should be
 *         g_free() when done.
 */
static GList *ldaputil_test_v3( LDAP *ld, gint tov ) {
	GList *baseDN = NULL;
	gint rc, i;
	LDAPMessage *result, *e;
	gchar *attribs[2];
	BerElement *ber;
	gchar *attribute;
	gchar **vals;
	struct timeval timeout;

	/* Set timeout */
	timeout.tv_usec = 0L;
	if( tov > 0 ) {
		timeout.tv_sec = tov;
	}
	else {
		timeout.tv_sec = 30L;
	}

	/* Test for LDAP version 3 */
	attribs[0] = SYLDAP_V3_TEST_ATTR;
	attribs[1] = NULL;
	rc = ldap_search_ext_s(
		ld, SYLDAP_SEARCHBASE_V3, LDAP_SCOPE_BASE, SYLDAP_TEST_FILTER,
		attribs, 0, NULL, NULL, &timeout, 0, &result );

	if( rc == LDAP_SUCCESS ) {
		/* Process entries */
		for( e = ldap_first_entry( ld, result );
		     e != NULL;
		     e = ldap_next_entry( ld, e ) ) 
		{
			/* Process attributes */
			for( attribute = ldap_first_attribute( ld, e, &ber );
			     attribute != NULL;
			     attribute = ldap_next_attribute( ld, e, ber ) )
			{
				if( strcasecmp(
					attribute, SYLDAP_V3_TEST_ATTR ) == 0 )
				{
					vals = ldap_get_values( ld, e, attribute );
					if( vals != NULL ) {
						for( i = 0; vals[i] != NULL; i++ ) {
							baseDN = g_list_append(
								baseDN, g_strdup( vals[i] ) );
						}
					}
					ldap_value_free( vals );
				}
				ldap_memfree( attribute );
			}
			if( ber != NULL ) {
				ber_free( ber, 0 );
			}
			ber = NULL;
		}
	}
	ldap_msgfree( result );
	return baseDN;
}

/**
 * Attempt to discover the base DN for a server using LDAP version 2.
 * \param  ld  LDAP handle for a connected server.
 * \param  tov Timeout value (seconds), or 0 for none, default 30 secs.
 * \return List of Base DN's, or NULL if could not read. List should be
 *         g_free() when done.
 */
static GList *ldaputil_test_v2( LDAP *ld, gint tov ) {
	GList *baseDN = NULL;
	gint rc, i;
	LDAPMessage *result, *e;
	gchar *attribs[1];
	BerElement *ber;
	gchar *attribute;
	gchar **vals;
	struct timeval timeout;

	/* Set timeout */
	timeout.tv_usec = 0L;
	if( tov > 0 ) {
		timeout.tv_sec = tov;
	}
	else {
		timeout.tv_sec = 30L;
	}

	attribs[0] = NULL;
	rc = ldap_search_ext_s(
		ld, SYLDAP_SEARCHBASE_V2, LDAP_SCOPE_BASE, SYLDAP_TEST_FILTER,
		attribs, 0, NULL, NULL, &timeout, 0, &result );

	if( rc == LDAP_SUCCESS ) {
		/* Process entries */
		for( e = ldap_first_entry( ld, result );
		     e != NULL;
		     e = ldap_next_entry( ld, e ) )
		{
			/* Process attributes */
			for( attribute = ldap_first_attribute( ld, e, &ber );
			     attribute != NULL;
			     attribute = ldap_next_attribute( ld, e, ber ) )
			{
				if( strcasecmp(
					attribute,
					SYLDAP_V2_TEST_ATTR ) == 0 ) {
					vals = ldap_get_values( ld, e, attribute );
					if( vals != NULL ) {
						for( i = 0; vals[i] != NULL; i++ ) {
							char *ch;
							/*
							 * Strip the 'ldb:' from the
							 * front of the value.
							 */
							ch = ( char * ) strchr( vals[i], ':' );
							if( ch ) {
								gchar *bn = g_strdup( ++ch );
								g_strchomp( bn );
								g_strchug( bn );
								baseDN = g_list_append(
									baseDN, g_strdup( bn ) );
								g_free( bn );
							}
						}
					}
					ldap_value_free( vals );
				}
				ldap_memfree( attribute );
			}
			if( ber != NULL ) {
				ber_free( ber, 0 );
			}
			ber = NULL;
		}
	}
	ldap_msgfree( result );
	return baseDN;
}

/**
 * Attempt to discover the base DN for the server.
 * \param  host   Host name.
 * \param  port   Port number.
 * \param  bindDN Bind DN (optional).
 * \param  bindPW Bind PW (optional).
 * \param  tov    Timeout value (seconds), or 0 for none, default 30 secs.
 * \return List of Base DN's, or NULL if could not read. This list should be
 *         g_free() when done.
 */
GList *ldaputil_read_basedn(
		const gchar *host, const gint port, const gchar *bindDN,
		const gchar *bindPW, const gint tov )
{
	GList *baseDN = NULL;
	LDAP *ld;
	gint rc;

	if( host == NULL ) return baseDN;
	if( port < 1 ) return baseDN;

	/* Connect to server. */
	if( ( ld = ldap_init( host, port ) ) == NULL ) {
		return baseDN;
	}

	/* Bind to the server, if required */
	if( bindDN ) {
		if( *bindDN != '\0' ) {
			rc = ldap_simple_bind_s( ld, bindDN, bindPW );
			if( rc != LDAP_SUCCESS ) {
				ldap_unbind( ld );
				return baseDN;
			}
		}
	}

	/* Test for LDAP version 3 */
	baseDN = ldaputil_test_v3( ld, tov );
	if( baseDN == NULL ) {
		baseDN = ldaputil_test_v2( ld, tov );
	}
	ldap_unbind( ld );
	return baseDN;
}

/**
 * Attempt to connect to the server.
 * Enter:
 * \param  host Host name.
 * \param  port Port number.
 * \return <i>TRUE</i> if connected successfully.
 */
gboolean ldaputil_test_connect( const gchar *host, const gint port ) {
	gboolean retVal = FALSE;
	LDAP *ld;

	if( host == NULL ) return retVal;
	if( port < 1 ) return retVal;
	ld = ldap_open( host, port );
	if( ld != NULL ) {
		ldap_unbind( ld );
		retVal = TRUE;
	}
	return retVal;
}

/**
 * Test whether LDAP libraries installed.
 * Return: TRUE if library available.
 */
gboolean ldaputil_test_ldap_lib( void ) {
	return TRUE;
}

#endif	/* USE_LDAP */

/*
 * End of Source.
*/
