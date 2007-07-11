/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2003-2007 Match Grun and the Claws Mail team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 * 
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
#include <errno.h>

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
static GList *ldaputil_test_v3( LDAP *ld, gint tov, gint *errcode ) {
	GList *baseDN = NULL;
	gint rc, i;
	LDAPMessage *result = NULL, *e;
	gchar *attribs[2];
	BerElement *ber;
	gchar *attribute;
	struct berval **vals;
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
					vals = ldap_get_values_len( ld, e, attribute );
					if( vals != NULL ) {
						for( i = 0; vals[i] != NULL; i++ ) {
							baseDN = g_list_append(
								baseDN, g_strndup( vals[i]->bv_val, vals[i]->bv_len ) );
						}
					}
					ldap_value_free_len( vals );
				}
				ldap_memfree( attribute );
			}
			if( ber != NULL ) {
				ber_free( ber, 0 );
			}
			ber = NULL;
		}
	} 
	if (errcode)
		*errcode = rc;
	if (result)
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
	LDAPMessage *result = NULL, *e;
	gchar *attribs[1];
	BerElement *ber;
	gchar *attribute;
	struct berval **vals;
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
					vals = ldap_get_values_len( ld, e, attribute );
					if( vals != NULL ) {
						for( i = 0; vals[i] != NULL; i++ ) {
							char *ch, *tmp;
							/*
							 * Strip the 'ldb:' from the
							 * front of the value.
							 */
							tmp = g_strndup( vals[i]->bv_val, vals[i]->bv_len);
							ch = ( char * ) strchr( tmp, ':' );
							if( ch ) {
								gchar *bn = g_strdup( ++ch );
								g_strchomp( bn );
								g_strchug( bn );
								baseDN = g_list_append(
									baseDN, g_strdup( bn ) );
								g_free( bn );
							}
							g_free(tmp);
						}
					}
					ldap_value_free_len( vals );
				}
				ldap_memfree( attribute );
			}
			if( ber != NULL ) {
				ber_free( ber, 0 );
			}
			ber = NULL;
		}
	}
	if (result)
		ldap_msgfree( result );
	return baseDN;
}

int claws_ldap_simple_bind_s( LDAP *ld, LDAP_CONST char *dn, LDAP_CONST char *passwd )
{
	struct berval cred;

	if ( passwd != NULL ) {
		cred.bv_val = (char *) passwd;
		cred.bv_len = strlen( passwd );
	} else {
		cred.bv_val = "";
		cred.bv_len = 0;
	}

	return ldap_sasl_bind_s( ld, dn, LDAP_SASL_SIMPLE, &cred,
		NULL, NULL, NULL );
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
		const gchar *bindPW, const gint tov, int ssl, int tls )
{
	GList *baseDN = NULL;
	LDAP *ld = NULL;
	gint rc;
	gchar *uri = NULL;
#ifdef USE_LDAP_TLS
	gint version;
#endif

	if( host == NULL ) return baseDN;
	if( port < 1 ) return baseDN;

	/* Connect to server. */

	uri = g_strdup_printf("ldap%s://%s:%d",
			ssl?"s":"",
			host, port);
	rc = ldap_initialize(&ld, uri);
	g_free(uri);
	
	if( ld == NULL ) {
		return baseDN;
	}
#ifdef USE_LDAP_TLS
	if( tls && !ssl ) {
		/* Handle TLS */
		version = LDAP_VERSION3;
		rc = ldap_set_option( ld, LDAP_OPT_PROTOCOL_VERSION, &version );
		if( rc != LDAP_OPT_SUCCESS ) {
			ldap_unbind_ext( ld, NULL, NULL );
			return baseDN;
		}
		rc = ldap_start_tls_s( ld, NULL, NULL );
		if (rc != 0) {
			ldap_unbind_ext( ld, NULL, NULL );
			return baseDN;
		}
	}
#endif

	/* Bind to the server, if required */
	if( bindDN ) {
		if( *bindDN != '\0' ) {
			rc = claws_ldap_simple_bind_s( ld, bindDN, bindPW );
			if( rc != LDAP_SUCCESS ) {
				ldap_unbind_ext( ld, NULL, NULL );
				return baseDN;
			}
		}
	}

	/* Test for LDAP version 3 */
	baseDN = ldaputil_test_v3( ld, tov, &rc );

	if( baseDN == NULL && !LDAP_API_ERROR(rc) ) {
		baseDN = ldaputil_test_v2( ld, tov );
	}
	if (ld && !LDAP_API_ERROR(rc))
		ldap_unbind_ext( ld, NULL, NULL );
	
	return baseDN;
}

/**
 * Attempt to connect to the server.
 * Enter:
 * \param  host Host name.
 * \param  port Port number.
 * \return <i>TRUE</i> if connected successfully.
 */
gboolean ldaputil_test_connect( const gchar *host, const gint port, int ssl, int tls ) {
	gboolean retVal = FALSE;
	LDAP *ld;
#ifdef USE_LDAP_TLS
	gint rc;
	gint version;
#endif
	gchar *uri = NULL;

	if( host == NULL ) return retVal;
	if( port < 1 ) return retVal;
	
	uri = g_strdup_printf("ldap%s://%s:%d",
				ssl?"s":"",
				host, port);
	ldap_initialize(&ld, uri);
	g_free(uri);
	if (ld == NULL)
		return FALSE;

#ifdef USE_LDAP_TLS
	if (ssl) {
		GList *dummy = ldaputil_test_v3( ld, 10, &rc );
		if (dummy)
			g_list_free(dummy);
		if (LDAP_API_ERROR(rc))
			return FALSE;
	}

	if( tls && !ssl ) {
		/* Handle TLS */
		version = LDAP_VERSION3;
		rc = ldap_set_option( ld, LDAP_OPT_PROTOCOL_VERSION, &version );
		if( rc != LDAP_OPT_SUCCESS ) {
			ldap_unbind_ext( ld, NULL, NULL );
			return FALSE;
		}

		rc = ldap_start_tls_s( ld, NULL, NULL );
		if (rc != 0) {
			ldap_unbind_ext( ld, NULL, NULL );
			return FALSE;
		}
	}
#endif
	if( ld != NULL ) {
		ldap_unbind_ext( ld, NULL, NULL );
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
