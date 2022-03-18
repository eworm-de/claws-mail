/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2015 the Claws Mail Team
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

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <ctype.h>
#include <errno.h>

#include "claws.h"
#include "account.h"
#include "passwordstore.h"
#include "gtk/inputdialog.h"
#include "md5.h"
#include "utils.h"
#include "log.h"
#include "session.h"
#include "prefs_common.h"

#include "managesieve.h"
#include "sieve_editor.h"
#include "sieve_prefs.h"

GSList *sessions = NULL;

static void sieve_session_destroy(Session *session);
static gint sieve_pop_send_queue(SieveSession *session);
static void sieve_session_reset(SieveSession *session);
static void command_free(SieveCommand *cmd);
static void command_abort(SieveCommand *cmd);
static void command_cb(SieveCommand *cmd, gpointer result);
static gint sieve_session_recv_chunk(SieveSession *, guint len);
static void sieve_read_chunk(SieveSession *, gchar *data, guint len);
static gint sieve_read_chunk_done(SieveSession *session);

void sieve_sessions_close()
{
	if (sessions) {
		GSList *list = sessions;
		sessions = NULL;
		g_slist_free_full(list, (GDestroyNotify)session_destroy);
	}
}

/* remove all command callbacks with a given data pointer */
void sieve_sessions_discard_callbacks(gpointer user_data)
{
	GSList *item;
	GSList *queue;
	GSList *prev = NULL;
	SieveSession *session;
	SieveCommand *cmd;

	for (item = sessions; item; item = item->next) {
		session = (SieveSession *)item->data;
		cmd = session->current_cmd;
		/* abort current command handler */
		if (cmd && cmd->data == user_data) {
			command_abort(cmd);
			session->current_cmd = NULL;
		}
		/* abort queued command handlers */
		for (queue = session->send_queue; queue; queue = queue->next) {
			cmd = (SieveCommand *)queue->data;
			if (cmd && cmd->data == user_data) {
				if (prev)
					prev->next = queue->next;
				else
					session->send_queue = NULL;
				command_abort(cmd);
				g_slist_free_1(queue);
			} else {
				prev = queue;
			}
		}
	}
}

static void command_cb(SieveCommand *cmd, gpointer result)
{
	if (cmd)
		cmd->cb(cmd->session, FALSE, result, cmd->data);
}

static void command_abort(SieveCommand *cmd)
{
	cmd->cb(cmd->session, TRUE, NULL, cmd->data);
	g_free(cmd->msg);
	g_free(cmd);
}

static void command_free(SieveCommand *cmd)
{
	g_free(cmd->msg);
	g_free(cmd);
}

void sieve_session_handle_status(SieveSession *session,
		sieve_session_error_cb_fn on_error,
		sieve_session_connected_cb_fn on_connected,
		gpointer data)
{
	session->on_error = on_error;
	session->on_connected = on_connected;
	session->cb_data = data;
}

static void sieve_error(SieveSession *session, const gchar *msg)
{
	if (session->on_error)
		session->on_error(session, msg, session->cb_data);
}

static void sieve_connected(SieveSession *session, gboolean connected)
{
	if (session->on_connected)
		session->on_connected(session, connected, session->cb_data);
}

static gboolean sieve_read_chunk_cb(SockInfo *source,
		GIOCondition condition, gpointer data)
{
	SieveSession *sieve_session = SIEVE_SESSION(data);
	Session *session = &sieve_session->session;
	gint data_len;
	gint ret;

	cm_return_val_if_fail(condition == G_IO_IN, FALSE);

	session_set_timeout(session, session->timeout_interval);

	if (session->read_buf_len == 0) {
		gint read_len = -1;

		if (session->sock)
			read_len = sock_read(session->sock,
					session->read_buf,
					SESSION_BUFFSIZE - 1);

		if (read_len == -1 &&
				session->state == SESSION_DISCONNECTED) {
			g_warning("sock_read: session disconnected");
			if (session->io_tag > 0) {
				g_source_remove(session->io_tag);
				session->io_tag = 0;
			}
			return FALSE;
		}

		if (read_len == 0) {
			g_warning("sock_read: received EOF");
			session->state = SESSION_EOF;
			return FALSE;
		}

		if (read_len < 0) {
			switch (errno) {
			case EAGAIN:
				return TRUE;
			default:
				g_warning("sock_read: %s",
						g_strerror(errno));
				session->state = SESSION_ERROR;
				return FALSE;
			}
		}

		session->read_buf_len = read_len;
	}

	data_len = MIN(session->read_buf_len,
			sieve_session->octets_remaining);
	sieve_session->octets_remaining -= data_len;
	session->read_buf_len -= data_len;
	session->read_buf_p[data_len] = '\0';

	/* progress callback */
	sieve_read_chunk(sieve_session, session->read_buf_p, data_len);

	if (session->read_buf_len == 0) {
		session->read_buf_p = session->read_buf;
	} else {
		session->read_buf_p += data_len;
	}

	/* incomplete read */
	if (sieve_session->octets_remaining > 0)
		return TRUE;

	/* complete */
	if (session->io_tag > 0) {
		g_source_remove(session->io_tag);
		session->io_tag = 0;
	}

	/* completion callback */
	ret = sieve_read_chunk_done(sieve_session);

	if (ret < 0)
		session->state = SESSION_ERROR;

	return FALSE;
}

static gboolean sieve_read_chunk_idle_cb(gpointer data)
{
	Session *session = SESSION(data);
	gboolean ret;

	ret = sieve_read_chunk_cb(session->sock, G_IO_IN, session);

	if (ret == TRUE)
		session->io_tag = sock_add_watch(session->sock, G_IO_IN,
				sieve_read_chunk_cb, session);

	return FALSE;
}

/* Get data of specified length.
 * If needed elsewhere, this should be put in session.c */
static gint sieve_session_recv_chunk(SieveSession *sieve_session,
		guint bytes)
{
	Session *session = &sieve_session->session;
	cm_return_val_if_fail(session->read_msg_buf->len == 0, -1);

	session->state = SESSION_RECV;
	sieve_session->octets_remaining = bytes;

	if (session->read_buf_len > 0)
		g_idle_add(sieve_read_chunk_idle_cb, session);
	else
		session->io_tag = sock_add_watch(session->sock, G_IO_IN,
						 sieve_read_chunk_cb, session);
	return 0;
}


static gint sieve_auth_recv(SieveSession *session, const gchar *msg)
{
	gchar buf[MESSAGEBUFSIZE], *tmp;

	switch (session->auth_type) {
	case SIEVEAUTH_LOGIN:
		session->state = SIEVE_AUTH_LOGIN_USER;

		if (strstr(msg, "VXNlcm5hbWU6")) {
			tmp = g_base64_encode(session->user, strlen(session->user));
			g_snprintf(buf, sizeof(buf), "\"%s\"", tmp);

			if (session_send_msg(SESSION(session), buf) < 0) {
				g_free(tmp);
				return SE_ERROR;
			}
			g_free(tmp);
			log_print(LOG_PROTOCOL, "Sieve> [USERID]\n");
		} else {
			/* Server rejects AUTH */
			if (session_send_msg(SESSION(session), "\"*\"") < 0)
				return SE_ERROR;
			log_print(LOG_PROTOCOL, "Sieve> *\n");
		}
		break;
	case SIEVEAUTH_CRAM_MD5:
		session->state = SIEVE_AUTH_CRAM_MD5;

		if (msg[0] == '"') {
			gchar *response;
			gchar *response64;
			gchar *challenge, *tmp;
			gsize challengelen;
			guchar hexdigest[33];

			tmp = g_base64_decode(msg + 1, &challengelen);
			challenge = g_strndup(tmp, challengelen);
			g_free(tmp);
			log_print(LOG_PROTOCOL, "Sieve< [Decoded: %s]\n", challenge);

			g_snprintf(buf, sizeof(buf), "%s", session->pass);
			md5_hex_hmac(hexdigest, challenge, challengelen,
				     buf, strlen(session->pass));
			g_free(challenge);

			response = g_strdup_printf
				("%s %s", session->user, hexdigest);
			log_print(LOG_PROTOCOL, "Sieve> [Encoded: %s]\n", response);

			response64 = g_base64_encode(response, strlen(response));
			g_free(response);

			response = g_strdup_printf("\"%s\"", response64);
			g_free(response64);

			if (session_send_msg(SESSION(session), response) < 0) {
				g_free(response);
				return SE_ERROR;
			}
			log_print(LOG_PROTOCOL, "Sieve> %s\n", response);
			g_free(response);
		} else {
			/* Server rejects AUTH */
			if (session_send_msg(SESSION(session), "\"*\"") < 0)
				return SE_ERROR;
			log_print(LOG_PROTOCOL, "Sieve> *\n");
		}
		break;
	default:
		/* stop sieve_auth when no correct authtype */
		if (session_send_msg(SESSION(session), "*") < 0)
			return SE_ERROR;
		log_print(LOG_PROTOCOL, "Sieve> *\n");
		break;
	}

	return SE_OK;
}

static gint sieve_auth_login_user_recv(SieveSession *session, const gchar *msg)
{
	gchar *tmp, *tmp2;

	session->state = SIEVE_AUTH_LOGIN_PASS;
	
	if (strstr(msg, "UGFzc3dvcmQ6")) {
		tmp2 = g_base64_encode(session->pass, strlen(session->pass));
		tmp = g_strdup_printf("\"%s\"", tmp2);
		g_free(tmp2);
	} else {
		/* Server rejects AUTH */
		tmp = g_strdup("\"*\"");
	}

	if (session_send_msg(SESSION(session), tmp) < 0) {
		g_free(tmp);
		return SE_ERROR;
	}
	g_free(tmp);

	log_print(LOG_PROTOCOL, "Sieve> [PASSWORD]\n");

	return SE_OK;
}


static gint sieve_auth_cram_md5(SieveSession *session)
{
	session->state = SIEVE_AUTH;
	session->auth_type = SIEVEAUTH_CRAM_MD5;

	if (session_send_msg(SESSION(session), "Authenticate \"CRAM-MD5\"") < 0)
		return SE_ERROR;
	log_print(LOG_PROTOCOL, "Sieve> Authenticate CRAM-MD5\n");

	return SE_OK;
}

static gint sieve_auth_plain(SieveSession *session)
{
	gchar buf[MESSAGEBUFSIZE], *b64buf, *out;
	gint len;

	session->state = SIEVE_AUTH_PLAIN;
	session->auth_type = SIEVEAUTH_PLAIN;

	memset(buf, 0, sizeof buf);

	/* "\0user\0password" */
	len = sprintf(buf, "%c%s%c%s", '\0', session->user, '\0', session->pass);
	b64buf = g_base64_encode(buf, len);
	out = g_strconcat("Authenticate \"PLAIN\" \"", b64buf, "\"", NULL);
	g_free(b64buf);

	if (session_send_msg(SESSION(session), out) < 0) {
		g_free(out);
		return SE_ERROR;
	}

	g_free(out);

	log_print(LOG_PROTOCOL, "Sieve> [Authenticate PLAIN]\n");

	return SE_OK;
}

static gint sieve_auth_login(SieveSession *session)
{
	session->state = SIEVE_AUTH;
	session->auth_type = SIEVEAUTH_LOGIN;

	if (session_send_msg(SESSION(session), "Authenticate \"LOGIN\"") < 0)
		return SE_ERROR;
	log_print(LOG_PROTOCOL, "Sieve> Authenticate LOGIN\n");

	return SE_OK;
}

static gint sieve_auth(SieveSession *session)
{
	SieveAuthType forced_auth_type = session->forced_auth_type;

	if (!session->use_auth) {
		session->state = SIEVE_READY;
		sieve_connected(session, TRUE);
		return SE_OK;
	}

	session->state = SIEVE_AUTH;
	sieve_error(session, _("Authenticating..."));

	if ((forced_auth_type == SIEVEAUTH_CRAM_MD5 || forced_auth_type == 0) &&
	     (session->avail_auth_type & SIEVEAUTH_CRAM_MD5) != 0)
		return sieve_auth_cram_md5(session);
	else if ((forced_auth_type == SIEVEAUTH_LOGIN || forced_auth_type == 0) &&
		  (session->avail_auth_type & SIEVEAUTH_LOGIN) != 0)
		return sieve_auth_login(session);
	else if ((forced_auth_type == SIEVEAUTH_PLAIN || forced_auth_type == 0) &&
		  (session->avail_auth_type & SIEVEAUTH_PLAIN) != 0)
		return sieve_auth_plain(session);
	else if (forced_auth_type == 0) {
		log_warning(LOG_PROTOCOL, _("No Sieve auth method available\n"));
		session->state = SIEVE_RETRY_AUTH;
		return SE_AUTHFAIL;
	} else {
		log_warning(LOG_PROTOCOL, _("Selected Sieve auth method not available\n"));
		session->state = SIEVE_RETRY_AUTH;
		return SE_AUTHFAIL;
	}

	return SE_OK;
}

static void sieve_session_putscript_cb(SieveSession *session, SieveResult *result)
{
	/* Remove script name from the beginning the response,
	 * which is added by Dovecot/Pigeonhole */
	gchar *start, *desc = result->description;
	gchar *end = NULL;
	if (!desc) {
		/* callback just for the status */
		command_cb(session->current_cmd, result);
	}
	while (desc && desc[0]) {
		if ((end = strchr(desc, '\r')) ||
		    (end = strchr(desc, '\n')))
			while (*end == '\n' || *end == '\r')
				*end++ = '\0';
		if (g_str_has_prefix(desc, "NULL_") && (start = strchr(desc+5, ':'))) {
			desc = start+1;
			while (*desc == ' ')
				desc++;
		/* TODO: match against known script name, in case it contains
		 * weird text like ": line " */
		} else if ((start = strstr(desc, ": line ")) ||
				(start = strstr(desc, ": error"))) {
			desc = start+2;
		}
		result->description = desc;
		command_cb(session->current_cmd, result);
		desc = end;
	}
}

static inline gboolean response_is_ok(const char *msg)
{
	return !strncmp(msg, "OK", 2) && (!msg[2] || msg[2] == ' ');
}

static inline gboolean response_is_no(const char *msg)
{
	return !strncmp(msg, "NO", 2) && (!msg[2] || msg[2] == ' ');
}

static inline gboolean response_is_bye(const char *msg)
{
	return !strncmp(msg, "BYE", 3) && (!msg[3] || msg[3] == ' ');
}

static void sieve_got_capability(SieveSession *session, gchar *cap_name,
		gchar *cap_value)
{
	if (strcmp(cap_name, "SASL") == 0) {
		SieveAuthType auth_type = 0;
		gchar *auth, *end;
		for (auth = cap_value; auth && auth[0]; auth = end) {
			if ((end = strchr(auth, ' ')))
				*end++ = '\0';
			if (strcmp(auth, "PLAIN") == 0) {
				auth_type |= SIEVEAUTH_PLAIN;
			} else if (strcmp(auth, "CRAM-MD5") == 0) {
				auth_type |= SIEVEAUTH_CRAM_MD5;
			} else if (strcmp(auth, "LOGIN") == 0) {
				auth_type |= SIEVEAUTH_LOGIN;
			}
		}
		session->avail_auth_type = auth_type;

	} else if (strcmp(cap_name, "STARTTLS") == 0) {
		session->capability.starttls = TRUE;
	}
}

static void log_send(SieveSession *session, SieveCommand *cmd)
{
	gchar *end, *msg = cmd->msg;
	if (cmd->next_state == SIEVE_PUTSCRIPT && (end = strchr(msg, '\n'))) {
		/* Don't log the script data */
		msg = g_strndup(msg, end - msg);
		log_print(LOG_PROTOCOL, "Sieve> %s\n", msg);
		g_free(msg);
		msg = "[Data]";
	}
	log_print(LOG_PROTOCOL, "Sieve> %s\n", msg);
}

static gint sieve_pop_send_queue(SieveSession *session)
{
	SieveCommand *cmd;
	GSList *send_queue = session->send_queue;

	if (session->current_cmd) {
		command_free(session->current_cmd);
		session->current_cmd = NULL;
	}

	if (!send_queue)
		return SE_OK;

	cmd = (SieveCommand *)send_queue->data;
	session->send_queue = g_slist_next(send_queue);
	g_slist_free_1(send_queue);

	log_send(session, cmd);
	session->state = cmd->next_state;
	session->current_cmd = cmd;
	if (session_send_msg(SESSION(session), cmd->msg) < 0)
		return SE_ERROR;

	return SE_OK;
}

static void parse_split(gchar *line, gchar **first_word, gchar **second_word)
{
	gchar *first = line;
	gchar *second;
	gchar *end;

	/* get first */
	if (line[0] == '"' && ((second = strchr(line + 1, '"')))) {
		*second++ = '\0';
		first++;
		if (second[0] == ' ')
			second++;
	} else if ((second = strchr(line, ' '))) {
		*second++ = '\0';
	}

	/* unquote second */
	if (second && second[0] == '"' &&
			((end = strchr(second + 1, '"')))) {
		second++;
		*end = '\0';
	}

	*first_word = first;
	*second_word = second;
}

static void unquote_inplace(gchar *str)
{
	gchar *src, *dest;
	if (*str != '"')
		return;
	for (src = str+1, dest = str; src && *src && *src != '"'; src++) {
		if (*src == '\\') {
			src++;
		}
		*dest++ = *src;
	}
	*dest = '\0';
}

static void parse_response(gchar *msg, SieveResult *result)
{
	gchar *end;

	cm_return_if_fail(msg != NULL);

	/* response status */
	if (isalpha(msg[0])) {
		end = strchr(msg, ' ');
		if (end) {
			*end++ = '\0';
			while (*end == ' ')
				end++;
		}
		result->success = strcmp(msg, "OK") == 0;
		result->has_status = TRUE;
		msg = end;
	} else {
		result->has_status = FALSE;
	}

	/* response code */
	if (msg && msg[0] == '(' && (end = strchr(msg, ')'))) {
		msg++;
		*end++ = '\0';
		result->code =
			strcmp(msg, "WARNINGS") == 0 ? SIEVE_CODE_WARNINGS :
			strcmp(msg, "TRYLATER") == 0 ? SIEVE_CODE_TRYLATER :
			SIEVE_CODE_UNKNOWN;
		while (*end == ' ')
			end++;
		msg = end;
	} else {
		result->code = SIEVE_CODE_NONE;
	}

	/* s2c octets */
	if (msg && msg[0] == '{' && (end = strchr(msg, '}'))) {
		msg++;
		*end++ = '\0';
		if (msg[0] == '0' && msg+1 == end) {
			result->has_octets = TRUE;
			result->octets = 0;
		} else {
			result->has_octets =
				(result->octets = g_ascii_strtoll(msg, NULL, 10)) != 0;
		}
		while (*end == ' ')
			end++;
		msg = end;
	} else {
		result->has_octets = FALSE;
		result->octets = 0;
	}

	/* text */
	if (msg && *msg) {
		unquote_inplace(msg);
		result->description = msg;
	} else {
		result->description = NULL;
	}
}

static gint sieve_session_recv_msg(Session *session, const gchar *msg)
{
	SieveSession *sieve_session = SIEVE_SESSION(session);
	SieveResult result;
	gint ret = SE_OK;

	log_print(LOG_PROTOCOL, "Sieve< %s\n", msg);
	if (response_is_bye(msg)) {
		gchar *status;
		parse_response((gchar *)msg, &result);
		if (!result.description)
			status = g_strdup(_("Disconnected"));
		else if (g_str_has_prefix(result.description, "Disconnected"))
			status = g_strdup(result.description);
		else
			status = g_strdup_printf(_("Disconnected: %s"), result.description);
		sieve_session->error = SE_ERROR;
		sieve_error(sieve_session, status);
		sieve_session->state = SIEVE_DISCONNECTED;
		g_free(status);
		return -1;
	}

	switch (sieve_session->state) {
	case SIEVE_CAPABILITIES:
		if (response_is_ok(msg)) {
			/* capabilities list done */

#ifdef USE_GNUTLS
			if (sieve_session->tls_init_done == FALSE &&
					sieve_session->config->tls_type != SIEVE_TLS_NO) {
				if (sieve_session->capability.starttls) {
					if (session_send_msg(session, "STARTTLS") < 0)
						sieve_session->state = SIEVE_ERROR;
					else
						sieve_session->state = SIEVE_STARTTLS;
				} else if (sieve_session->config->tls_type == SIEVE_TLS_YES) {
					log_warning(LOG_PROTOCOL, "Sieve: does not support STARTTLS\n");
					sieve_session->state = SIEVE_ERROR;
				} else {
					log_warning(LOG_PROTOCOL, "Sieve: continuing unencrypted\n");
					sieve_session->state = SIEVE_READY;
				}
				break;
			}
#endif
			/* authenticate after getting capabilities */
			if (!sieve_session->authenticated) {
				ret = sieve_auth(sieve_session);
			} else {
				sieve_session->state = SIEVE_READY;
				sieve_connected(sieve_session, TRUE);
			}
		} else {
			/* got a capability */
			gchar *cap_name, *cap_value;
			parse_split((gchar *)msg, &cap_name, &cap_value);
			sieve_got_capability(sieve_session, cap_name, cap_value);
		}
		break;
	case SIEVE_READY:
		if (!msg[0])
			break;
		log_warning(LOG_PROTOCOL,
				_("unhandled message on Sieve session: %s\n"), msg);
		break;
	case SIEVE_STARTTLS:
#ifdef USE_GNUTLS
		if (session_start_tls(session) < 0) {
			sieve_session->state = SIEVE_ERROR;
			sieve_session->error = SE_ERROR;
			sieve_error(sieve_session, _("STARTTLS failed"));
			return -1;
		}
		sieve_session->tls_init_done = TRUE;
		sieve_session->state = SIEVE_CAPABILITIES;
#endif
		break;
	case SIEVE_AUTH:
		ret = sieve_auth_recv(sieve_session, msg);
		break;
	case SIEVE_AUTH_LOGIN_USER:
		ret = sieve_auth_login_user_recv(sieve_session, msg);
		break;
	case SIEVE_AUTH_PLAIN:
	case SIEVE_AUTH_LOGIN_PASS:
	case SIEVE_AUTH_CRAM_MD5:
		if (response_is_no(msg)) {
			log_print(LOG_PROTOCOL, "Sieve auth failed\n");
			sieve_session->state = SIEVE_RETRY_AUTH;
			ret = SE_AUTHFAIL;
		} else if (response_is_ok(msg)) {
			log_print(LOG_PROTOCOL, "Sieve auth completed\n");
			sieve_error(sieve_session, "");
			sieve_session->authenticated = TRUE;
			sieve_session->state = SIEVE_READY;
			sieve_connected(sieve_session, TRUE);
		}
		break;
	case SIEVE_NOOP:
		if (!response_is_ok(msg)) {
			sieve_session->state = SIEVE_ERROR;
		}
		sieve_session->state = SIEVE_READY;
		break;
	case SIEVE_LISTSCRIPTS:
		if (response_is_no(msg)) {
			/* got an error. probably not authenticated. */
			command_cb(sieve_session->current_cmd, NULL);
			sieve_session->state = SIEVE_READY;
		} else if (response_is_ok(msg)) {
			/* end of list */
			sieve_session->state = SIEVE_READY;
			sieve_session->error = SE_OK;
			command_cb(sieve_session->current_cmd,
					(gpointer)&(SieveScript){0});
		} else {
			/* got a script name */
			SieveScript script;
			gchar *script_status;

			parse_split((gchar *)msg, &script.name, &script_status);
			script.active = (script_status &&
					strcasecmp(script_status, "active") == 0);

			command_cb(sieve_session->current_cmd,
					(gpointer)&script);
		}
		break;
	case SIEVE_RENAMESCRIPT:
		if (response_is_no(msg)) {
			/* error */
			command_cb(sieve_session->current_cmd, NULL);
		} else if (response_is_ok(msg)) {
			command_cb(sieve_session->current_cmd, (void*)TRUE);
		} else {
			log_warning(LOG_PROTOCOL, _("error occurred on SIEVE session\n"));
		}
		sieve_session->state = SIEVE_READY;
		break;
	case SIEVE_SETACTIVE:
		parse_response((gchar *)msg, &result);
		if (result.success) {
			/* clear status possibly set when setting another
			 * script active. TODO: give textual feedback */
			sieve_error(sieve_session, "");

			command_cb(sieve_session->current_cmd, NULL);
		} else if (result.description) {
			command_cb(sieve_session->current_cmd,
					result.description);
		} else {
			log_warning(LOG_PROTOCOL, _("error occurred on SIEVE session\n"));
		}
		if (result.has_octets) {
			return sieve_session_recv_chunk(sieve_session,
					result.octets);
		} else {
			sieve_session->state = SIEVE_READY;
		}
		break;
	case SIEVE_GETSCRIPT:
		if (response_is_no(msg)) {
			command_cb(sieve_session->current_cmd, (void *)-1);
			sieve_session->state = SIEVE_READY;
		} else {
			parse_response((gchar *)msg, &result);
			sieve_session->state = SIEVE_GETSCRIPT_DATA;
			return sieve_session_recv_chunk(sieve_session,
					result.octets);
		}
		break;
	case SIEVE_GETSCRIPT_DATA:
		if (!msg[0])
			break;
		sieve_session->state = SIEVE_READY;
		if (response_is_ok(msg)) {
			command_cb(sieve_session->current_cmd, NULL);
		} else if (msg[0]) {
			log_warning(LOG_PROTOCOL, _("error occurred on SIEVE session\n"));
		}
		break;
	case SIEVE_PUTSCRIPT:
		if (!msg[0])
			break;
		parse_response((gchar *)msg, &result);
		sieve_session_putscript_cb(sieve_session, &result);
		if (result.has_octets) {
			return sieve_session_recv_chunk(sieve_session,
					result.octets);
		} else {
			sieve_session->state = SIEVE_READY;
		}
		break;
	case SIEVE_DELETESCRIPT:
		parse_response((gchar *)msg, &result);
		if (!result.success) {
			command_cb(sieve_session->current_cmd,
					result.description);
		} else {
			command_cb(sieve_session->current_cmd, NULL);
		}
		sieve_session->state = SIEVE_READY;
		break;
	case SIEVE_ERROR:
		log_warning(LOG_PROTOCOL, _("error occurred on Sieve session. data: %s\n"), msg);
		sieve_session->error = SE_ERROR;
		break;
	case SIEVE_RETRY_AUTH:
		log_warning(LOG_PROTOCOL, _("unhandled message on Sieve session: %s\n"),
					msg);
		ret = sieve_auth(sieve_session);
		break;
	default:
		log_warning(LOG_PROTOCOL, _("unhandled message on Sieve session: %d\n"),
					sieve_session->state);
		sieve_session->error = SE_ERROR;
		return -1;
	}

	if (ret == SE_OK && sieve_session->state == SIEVE_READY)
		ret = sieve_pop_send_queue(sieve_session);

	if (ret == SE_OK) {
		return session_recv_msg(session);
	} else if (ret == SE_AUTHFAIL) {
		sieve_error(sieve_session, _("Auth failed"));
		sieve_session->state = SIEVE_ERROR;
		sieve_session->error = SE_ERROR;
	}

	return 0;
}

static gint sieve_recv_message(Session *session, const gchar *msg,
		gpointer user_data)
{
	return 0;
}

static void sieve_read_chunk(SieveSession *session, gchar *data, guint len)
{
	log_print(LOG_PROTOCOL, "Sieve< [%u bytes]\n", len);

	switch (session->state) {
	case SIEVE_GETSCRIPT_DATA:
		command_cb(session->current_cmd, (gchar *)data);
		break;
	case SIEVE_SETACTIVE:
		/* Dovecot shows a script's warnings when making it active */
		/* TODO: append message in case it is very long*/
		strretchomp(data);
		sieve_error(session, data);
		break;
	case SIEVE_PUTSCRIPT: {
		SieveResult result = {.description = (gchar *)data};
		sieve_session_putscript_cb(session, &result);
		break;
	}
	default:
		log_warning(LOG_PROTOCOL,
				_("error occurred on SIEVE session\n"));
	}
}

static gint sieve_read_chunk_done(SieveSession *session)
{
	gint ret = SE_OK;

	switch (session->state) {
	case SIEVE_GETSCRIPT_DATA:
		/* wait for ending "OK" response */
		break;
	case SIEVE_SETACTIVE:
	case SIEVE_PUTSCRIPT:
		session->state = SIEVE_READY;
		break;
	default:
		log_warning(LOG_PROTOCOL,
				_("error occurred on SIEVE session\n"));
	}

	if (ret == SE_OK && session->state == SIEVE_READY)
		ret = sieve_pop_send_queue(session);

	if (ret == SE_OK)
		return session_recv_msg(SESSION(session));

	return 0;
}

static gint sieve_cmd_noop(SieveSession *session)
{
	log_print(LOG_PROTOCOL, "Sieve> NOOP\n");
	session->state = SIEVE_NOOP;
	if (session_send_msg(SESSION(session), "NOOP") < 0) {
		session->state = SIEVE_ERROR;
		session->error = SE_ERROR;
		return 1;
	}
	return 0;
}

static gboolean sieve_ping(gpointer data)
{
	Session *session = SESSION(data);
	SieveSession *sieve_session = SIEVE_SESSION(session);

	if (sieve_session->state == SIEVE_ERROR || session->state == SESSION_ERROR)
		return FALSE;
	if (sieve_session->state != SIEVE_READY)
		return TRUE;

	return sieve_cmd_noop(sieve_session) == 0;
}

static void sieve_session_destroy(Session *session)
{
	SieveSession *sieve_session = SIEVE_SESSION(session);
	g_free(sieve_session->pass);
	if (sieve_session->current_cmd)
		command_abort(sieve_session->current_cmd);
	sessions = g_slist_remove(sessions, (gconstpointer)session);
	g_slist_free_full(sieve_session->send_queue,
			(GDestroyNotify)command_abort);
	if (sieve_session->config)
		sieve_prefs_account_free_config(sieve_session->config);
}

static void sieve_connect_finished(Session *session, gboolean success)
{
	if (!success) {
		sieve_connected(SIEVE_SESSION(session), FALSE);
	}
}

static gint sieve_session_connect(SieveSession *session)
{
	PrefsAccount *ac = session->account;
	ProxyInfo *proxy_info = NULL;

	session->state = SIEVE_CAPABILITIES;
	session->authenticated = FALSE;
#ifdef USE_GNUTLS
	session->tls_init_done = FALSE;
#endif

	if (ac->use_proxy) {
		if (ac->use_default_proxy) {
			proxy_info = (ProxyInfo *)&(prefs_common_get_prefs()->proxy_info);
			if (proxy_info->use_proxy_auth)
				proxy_info->proxy_pass = passwd_store_get(PWS_CORE, PWS_CORE_PROXY,
					PWS_CORE_PROXY_PASS);
		} else {
			proxy_info = (ProxyInfo *)&(ac->proxy_info);
			if (proxy_info->use_proxy_auth)
				proxy_info->proxy_pass = passwd_store_get_account(ac->account_id,
					PWS_ACCOUNT_PROXY_PASS);
		}
	}
	SESSION(session)->proxy_info = proxy_info;

	return session_connect(SESSION(session), session->host,
			session->port);
}

static SieveSession *sieve_session_new(PrefsAccount *account)
{
	SieveSession *session;
	session = g_new0(SieveSession, 1);
	session_init(SESSION(session), account, FALSE);

	session->account = account;

	SESSION(session)->recv_msg = sieve_session_recv_msg;
	SESSION(session)->destroy = sieve_session_destroy;
	SESSION(session)->connect_finished = sieve_connect_finished;
	session_set_recv_message_notify(SESSION(session), sieve_recv_message, NULL);

	session->config = NULL;
	sieve_session_reset(session);
	return session;
}

static void sieve_session_reset(SieveSession *session)
{
	PrefsAccount *account = session->account;
	SieveAccountConfig *config = sieve_prefs_account_get_config(account);
	gboolean reuse_auth = (config->auth == SIEVEAUTH_REUSE);

	g_slist_free_full(session->send_queue, (GDestroyNotify)command_abort);

	session_disconnect(SESSION(session));

	SESSION(session)->ssl_cert_auto_accept = account->ssl_certs_auto_accept;
	SESSION(session)->nonblocking = account->use_nonblocking_ssl;
	session->authenticated = FALSE;
	session->current_cmd = NULL;
	session->send_queue = NULL;
	session->state = SIEVE_CAPABILITIES;
#ifdef USE_GNUTLS
	session->tls_init_done = FALSE;
	SESSION(session)->use_tls_sni = account->use_tls_sni;
#endif
	session->avail_auth_type = 0;
	session->auth_type = 0;
	if (session->config)
		sieve_prefs_account_free_config(session->config);
	session->config = config;
	session->host = config->use_host ? config->host : account->recv_server;
	session->port = config->use_port ? config->port : SIEVE_PORT;
	session->user = reuse_auth ? account->userid : session->config->userid;
	session->forced_auth_type = config->auth_type;
	session_register_ping(SESSION(session), sieve_ping);

	if (session->pass)
		g_free(session->pass);
	if (config->auth == SIEVEAUTH_NONE) {
		session->pass = NULL;
	} else if (reuse_auth && (session->pass = passwd_store_get_account(
                                account->account_id, PWS_ACCOUNT_RECV))) {
	} else if ((session->pass = passwd_store_get_account(
                                account->account_id, "sieve"))) {
	} else if (password_get(session->user, session->host, "sieve",
				session->port, &session->pass)) {
	} else {
		session->pass = input_dialog_query_password_keep(session->host,
				session->user, &(session->pass));
	}
	if (!session->pass) {
		session->pass = g_strdup("");
		session->use_auth = FALSE;
	} else {
		session->use_auth = TRUE;
	}

#ifdef USE_GNUTLS
	SESSION(session)->ssl_type =
		(config->tls_type == SIEVE_TLS_NO) ? SSL_NONE : SSL_STARTTLS;
#endif
}

/* When an account config is changed, reset associated sessions. */
void sieve_account_prefs_updated(PrefsAccount *account)
{
	GSList *item;
	SieveSession *session;

	for (item = sessions; item; item = item->next) {
		session = (SieveSession *)item->data;
		if (session->account == account) {
			log_print(LOG_PROTOCOL, "Sieve: resetting session\n");
			sieve_session_reset(session);
		}
	}
}

SieveSession *sieve_session_get_for_account(PrefsAccount *account)
{
	SieveSession *session;
	GSList *item;

	/* find existing */
	for (item = sessions; item; item = item->next) {
		session = (SieveSession *)item->data;
		if (session->account == account) {
			return session;
		}
	}

	/* create new */
	session = sieve_session_new(account);
	sessions = g_slist_prepend(sessions, session);

	return session;
}

static void sieve_queue_send(SieveSession *session, SieveState next_state,
		gchar *msg, sieve_session_data_cb_fn cb, gpointer data)
{
	gboolean queue = FALSE;
	SieveCommand *cmd = g_new0(SieveCommand, 1);
	cmd->session = session;
	cmd->next_state = next_state;
	cmd->msg = msg;
	cmd->data = data;
	cmd->cb = cb;

	if (!session_is_connected(SESSION(session))) {
		log_print(LOG_PROTOCOL, "Sieve: connecting to %s:%hu\n",
				session->host, session->port);
		if (sieve_session_connect(session) < 0) {
			sieve_connect_finished(SESSION(session), FALSE);
		}
		queue = TRUE;
	} else if (session->state == SIEVE_RETRY_AUTH) {
		log_print(LOG_PROTOCOL, _("Sieve: retrying auth\n"));
		if (sieve_auth(session) == SE_AUTHFAIL)
			sieve_error(session, _("Auth method not available"));
		queue = TRUE;
	} else if (session->state != SIEVE_READY) {
		log_print(LOG_PROTOCOL, "Sieve: in state %d\n", session->state);
		queue = TRUE;
	}

	if (queue) {
		session->send_queue = g_slist_prepend(session->send_queue, cmd);
	} else {
		if (session->current_cmd)
			command_free(session->current_cmd);
		session->current_cmd = cmd;
		session->state = next_state;
		log_send(session, cmd);
		if (session_send_msg(SESSION(session), cmd->msg) < 0) {
			log_warning(LOG_PROTOCOL,
				_("sending error on Sieve session: %s\n"), cmd->msg);
		}
	}
}

void sieve_session_list_scripts(SieveSession *session,
		sieve_session_data_cb_fn cb, gpointer data)
{
	gchar *msg = g_strdup("LISTSCRIPTS");
	sieve_queue_send(session, SIEVE_LISTSCRIPTS, msg, cb, data);
}

void sieve_session_set_active_script(SieveSession *session,
		const gchar *filter_name,
		sieve_session_data_cb_fn cb, gpointer data)
{
	gchar *msg = g_strdup_printf("SETACTIVE \"%s\"",
			filter_name ? filter_name : "");
	if (!msg) {
		cb(session, FALSE, (void*)FALSE, data);
		return;
	}

	sieve_queue_send(session, SIEVE_SETACTIVE, msg, cb, data);
}

void sieve_session_rename_script(SieveSession *session,
		const gchar *name_old, const char *name_new,
		sieve_session_data_cb_fn cb, gpointer data)
{
	gchar *msg = g_strdup_printf("RENAMESCRIPT \"%s\" \"%s\"",
			name_old, name_new);

	sieve_queue_send(session, SIEVE_RENAMESCRIPT, msg, cb, data);
}

void sieve_session_get_script(SieveSession *session, const gchar *filter_name,
		sieve_session_data_cb_fn cb, gpointer data)
{
	gchar *msg = g_strdup_printf("GETSCRIPT \"%s\"",
			filter_name);

	sieve_queue_send(session, SIEVE_GETSCRIPT, msg, cb, data);
}

void sieve_session_put_script(SieveSession *session, const gchar *filter_name,
		gint len, const gchar *script_contents,
		sieve_session_data_cb_fn cb, gpointer data)
{
	/* TODO: refactor so don't have to copy the whole script here */
	gchar *msg = g_strdup_printf("PUTSCRIPT \"%s\" {%u+}%s%s",
			filter_name, len, len > 0 ? "\r\n" : "",
			script_contents);

	sieve_queue_send(session, SIEVE_PUTSCRIPT, msg, cb, data);
}

void sieve_session_check_script(SieveSession *session,
		gint len, const gchar *script_contents,
		sieve_session_data_cb_fn cb, gpointer data)
{
	gchar *msg = g_strdup_printf("CHECKSCRIPT {%u+}%s%s",
			len, len > 0 ? "\r\n" : "", script_contents);

	sieve_queue_send(session, SIEVE_PUTSCRIPT, msg, cb, data);
}

void sieve_session_delete_script(SieveSession *session,
		const gchar *filter_name,
		sieve_session_data_cb_fn cb, gpointer data)
{
	gchar *msg = g_strdup_printf("DELETESCRIPT \"%s\"",
			filter_name);

	sieve_queue_send(session, SIEVE_DELETESCRIPT, msg, cb, data);
}
