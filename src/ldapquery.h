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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
 * Functions necessary to define an LDAP query.
 */

#ifndef __LDAPQUERY_H__
#define __LDAPQUERY_H__

#ifdef USE_LDAP

#include <glib.h>
#include <stdio.h>
#include <sys/time.h>
#include <pthread.h>

#include "ldapctrl.h"
#include "ldapserver.h"
#include "addritem.h"
#include "addrcache.h"

/*
 * Constants.
 */
#define LDAPQUERY_NONE     0
#define LDAPQUERY_STATIC   1
#define LDAPQUERY_DYNAMIC  2

/* Error codes */
#define LDAPRC_SUCCESS    0
#define LDAPRC_CONNECT    -1
#define LDAPRC_INIT       -2
#define LDAPRC_BIND       -3
#define LDAPRC_SEARCH     -4
#define LDAPRC_TIMEOUT    -5
#define LDAPRC_CRITERIA   -6
#define LDAPRC_NOENTRIES  -7

typedef struct _LdapQuery LdapQuery;
struct _LdapQuery {
	LdapControl *control;
	gint        retVal;
	gint        queryType;
	gchar       *queryName;
	gchar       *searchValue;
	gint        queryID;
	gint        entriesRead;
	gint        elapsedTime;
	gboolean    stopFlag;
	gboolean    busyFlag;
	gboolean    agedFlag;
	gboolean    completed;
	time_t      touchTime;
	pthread_t   *thread;
	pthread_mutex_t *mutexStop;
	pthread_mutex_t *mutexBusy;
	pthread_mutex_t *mutexEntry;
	void        (*callBackStart)( void * );
	void        (*callBackEntry)( void *, void * );
	void        (*callBackEnd)( void * );
	ItemFolder  *folder;		/* Reference to folder in cache */
	LdapServer  *server;		/* Reference to (parent) LDAP server */
};

/* Function prototypes */
void ldapqry_initialize		( void );
LdapQuery *ldapqry_create	( void );
void ldapqry_set_control	( LdapQuery *qry, LdapControl *ctl );
void ldapqry_set_name		( LdapQuery* qry, const gchar *value );
void ldapqry_set_search_value	( LdapQuery *qry, const gchar *value );
void ldapqry_set_error_status	( LdapQuery* qry, const gint value );
void ldapqry_set_query_type	( LdapQuery* qry, const gint value );
void ldapqry_set_query_id	( LdapQuery* qry, const gint value );
void ldapqry_set_entries_read	( LdapQuery* qry, const gint value );
void ldapqry_set_callback_start	( LdapQuery *qry, void *func );
void ldapqry_set_callback_entry	( LdapQuery *qry, void *func );
void ldapqry_set_callback_end	( LdapQuery *qry, void *func );
void ldapqry_clear		( LdapQuery *qry );
void ldapqry_free		( LdapQuery *qry );
void ldapqry_print		( const LdapQuery *qry, FILE *stream );
void ldapqry_set_stop_flag	( LdapQuery *qry, const gboolean value );
gboolean ldapqry_get_stop_flag	( LdapQuery *qry );
void ldapqry_set_busy_flag	( LdapQuery *qry, const gboolean value );
gboolean ldapqry_get_busy_flag	( LdapQuery *qry );
void ldapqry_set_aged_flag	( LdapQuery *qry, const gboolean value );
gboolean ldapqry_get_aged_flag	( LdapQuery *qry );

gboolean ldapqry_check_search	( LdapQuery *qry );
void ldapqry_touch		( LdapQuery *qry );
gint ldapqry_search		( LdapQuery *qry );
gint ldapqry_read_data_th	( LdapQuery *qry );
void ldapqry_join_thread	( LdapQuery *qry );
void ldapqry_cancel		( LdapQuery *qry );
void ldapqry_age		( LdapQuery *qry, gint maxAge );
void ldapqry_delete_folder	( LdapQuery *qry );

#endif	/* USE_LDAP */

#endif /* __LDAPQUERY_H__ */
