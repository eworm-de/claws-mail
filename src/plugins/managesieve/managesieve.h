/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2015 the Claws Mail team
 * Copyright (C) 2014-2015 Charles Lehner
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

#ifndef MANAGESIEVE_H
#define MANAGESIEVE_H

#include "prefs_account.h"
#include "socket.h"
#include "session.h"

#define SIEVE_SESSION(obj)	((SieveSession *)obj)
#define SIEVE_PORT 4190

typedef struct SieveSession SieveSession;
typedef struct SieveCommand SieveCommand;
typedef struct SieveScript SieveScript;
typedef struct SieveResult SieveResult;

typedef enum
{
	SE_OK			= 0,
	SE_ERROR			= 128,
	SE_UNRECOVERABLE	= 129,
	SE_AUTHFAIL		= 130
} SieveErrorValue;

typedef enum
{
	SIEVEAUTH_NONE		= 0,
	SIEVEAUTH_REUSE		= 1,
	SIEVEAUTH_CUSTOM	= 2
} SieveAuth;

typedef enum
{
	SIEVEAUTH_AUTO			= 0,
	SIEVEAUTH_PLAIN			= 1 << 0,
	SIEVEAUTH_LOGIN			= 1 << 1,
	SIEVEAUTH_CRAM_MD5		= 1 << 2,
} SieveAuthType;

typedef enum
{
	SIEVE_CAPABILITIES,
	SIEVE_READY,
	SIEVE_LISTSCRIPTS,
	SIEVE_STARTTLS,
	SIEVE_NOOP,
	SIEVE_RETRY_AUTH,
	SIEVE_AUTH,
	SIEVE_AUTH_PLAIN,
	SIEVE_AUTH_LOGIN_USER,
	SIEVE_AUTH_LOGIN_PASS,
	SIEVE_AUTH_CRAM_MD5,
	SIEVE_RENAMESCRIPT,
	SIEVE_SETACTIVE,
	SIEVE_GETSCRIPT,
	SIEVE_GETSCRIPT_DATA,
	SIEVE_PUTSCRIPT,
	SIEVE_DELETESCRIPT,
	SIEVE_ERROR,
	SIEVE_DISCONNECTED,

	N_SIEVE_PHASE
} SieveState;

typedef enum
{
	SIEVE_CODE_NONE,
	SIEVE_CODE_WARNINGS,
	SIEVE_CODE_TRYLATER,
	SIEVE_CODE_UNKNOWN
} SieveResponseCode;

typedef enum {
	SIEVE_TLS_NO,
	SIEVE_TLS_MAYBE,
	SIEVE_TLS_YES
} SieveTLSType;

typedef void (*sieve_session_cb_fn) (SieveSession *session, gpointer data);
typedef void (*sieve_session_data_cb_fn) (SieveSession *session,
		gboolean aborted, gpointer cb_data, gpointer user_data);
typedef void (*sieve_session_error_cb_fn) (SieveSession *session,
		const gchar *msg, gpointer user_data);
typedef void (*sieve_session_connected_cb_fn) (SieveSession *session,
		gboolean connected, gpointer user_data);

struct SieveSession
{
	Session session;
	PrefsAccount *account;
	struct SieveAccountConfig *config;
	SieveState state;
	gboolean authenticated;
	GSList *send_queue;
	SieveErrorValue error;
	SieveCommand *current_cmd;
	guint octets_remaining;

	gboolean use_auth;
	SieveAuthType avail_auth_type;
	SieveAuthType forced_auth_type;
	SieveAuthType auth_type;

	gchar *host;
	gushort port;
	gchar *user;
	gchar *pass;

#ifdef USE_GNUTLS
	gboolean tls_init_done;
#endif

	struct {
		gboolean starttls;
	} capability;

	sieve_session_error_cb_fn on_error;
	sieve_session_connected_cb_fn on_connected;
	gpointer cb_data;
};

struct SieveCommand {
	SieveSession *session;
	SieveState next_state;
	gchar *msg;
	sieve_session_data_cb_fn cb;
	gpointer data;
};

struct SieveScript {
	gchar *name;
	gboolean active;
};

struct SieveResult {
	gboolean has_status;
	gboolean success;
	SieveResponseCode code;
	gchar *description;
	gboolean has_octets;
	guint octets;
};

void sieve_sessions_close();

void sieve_account_prefs_updated(PrefsAccount *account);
SieveSession *sieve_session_get_for_account(PrefsAccount *account);
void sieve_sessions_discard_callbacks(gpointer user_data);
void sieve_session_handle_status(SieveSession *session,
		sieve_session_error_cb_fn on_error,
		sieve_session_connected_cb_fn on_connected,
		gpointer data);
void sieve_session_list_scripts(SieveSession *session,
		sieve_session_data_cb_fn got_script_name_cb, gpointer data);
void sieve_session_set_active_script(SieveSession *session,
		const gchar *filter_name,
		sieve_session_data_cb_fn cb, gpointer data);
void sieve_session_rename_script(SieveSession *session,
		const gchar *name_old, const char *name_new,
		sieve_session_data_cb_fn cb, gpointer data);
void sieve_session_get_script(SieveSession *session, const gchar *filter_name,
		sieve_session_data_cb_fn cb, gpointer data);
void sieve_session_put_script(SieveSession *session, const gchar *filter_name,
		gint len, const gchar *script_contents,
		sieve_session_data_cb_fn cb, gpointer data);
void sieve_session_check_script(SieveSession *session,
		gint len, const gchar *script_contents,
		sieve_session_data_cb_fn cb, gpointer data);
void sieve_session_delete_script(SieveSession *session,
		const gchar *filter_name,
		sieve_session_data_cb_fn cb, gpointer data);

#endif /* MANAGESIEVE_H */
