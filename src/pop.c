/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <unistd.h>

#include "intl.h"
#include "pop.h"
#include "socket.h"
#include "md5.h"
#include "prefs_account.h"
#include "utils.h"
#include "inc.h"
#include "recv.h"
#include "selective_download.h"
#if USE_SSL
#  include "ssl.h"
#endif

#define LOOKUP_NEXT_MSG() \
	for (;;) { \
		gint size = state->msg[state->cur_msg].size; \
		gboolean size_limit_over = \
		    (state->ac_prefs->enable_size_limit && \
		     state->ac_prefs->size_limit > 0 && \
		     size > state->ac_prefs->size_limit * 1024); \
 \
		if (size_limit_over) \
			log_print(_("POP3: Skipping message %d (%d bytes)\n"), \
				  state->cur_msg, size); \
 \
		if (size == 0 || state->msg[state->cur_msg].received || \
		    size_limit_over) { \
			if (size > 0) \
				state->cur_total_bytes += size; \
			if (state->cur_msg == state->count) \
				return POP3_LOGOUT_SEND; \
			else \
				state->cur_msg++; \
		} else \
			break; \
	}

static gint pop3_ok(SockInfo *sock, gchar *argbuf);
static void pop3_gen_send(SockInfo *sock, const gchar *format, ...);
static gint pop3_gen_recv(SockInfo *sock, gchar *buf, gint size);
static gboolean pop3_sd_get_next (Pop3State *state);
static void pop3_sd_new_header(Pop3State *state);
gboolean pop3_sd_state(Pop3State *state, gint cur_state, guint *next_state);
static gboolean should_delete (const char *uidl, gpointer data); 

gint pop3_greeting_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gchar buf[POPBUFSIZE];

	if (pop3_ok(sock, buf) == PS_SUCCESS) {
#if USE_SSL
		if (state->ac_prefs->ssl_pop == SSL_STARTTLS) {
			state->greeting = g_strdup(buf);
			return POP3_STLS_SEND;
		}
#endif
		if (state->ac_prefs->protocol == A_APOP) {
			state->greeting = g_strdup(buf);
			return POP3_GETAUTH_APOP_SEND;
		} else
			return POP3_GETAUTH_USER_SEND;
	} else
		return -1;
}

#if USE_SSL
gint pop3_stls_send(SockInfo *sock, gpointer data)
{
	pop3_gen_send(sock, "STLS");

	return POP3_STLS_RECV;
}

gint pop3_stls_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gint ok;

	if ((ok = pop3_ok(sock, NULL)) == PS_SUCCESS) {
		if (!ssl_init_socket_with_method(sock, SSL_METHOD_TLSv1))
			return -1;
		if (state->ac_prefs->protocol == A_APOP) {
			return POP3_GETAUTH_APOP_SEND;
		} else
			return POP3_GETAUTH_USER_SEND;
	} else if (ok == PS_PROTOCOL) {
		log_warning(_("can't start TLS session\n"));
		state->error_val = PS_PROTOCOL;
		state->inc_state = INC_ERROR;
		return POP3_LOGOUT_SEND;
	} else
		return -1;
}
#endif /* USE_SSL */

gint pop3_getauth_user_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	g_return_val_if_fail(state->user != NULL, -1);

	pop3_gen_send(sock, "USER %s", state->user);

	return POP3_GETAUTH_USER_RECV;
}

gint pop3_getauth_user_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	if (pop3_ok(sock, NULL) == PS_SUCCESS)
		return POP3_GETAUTH_PASS_SEND;
	else {
		log_warning(_("error occurred on authentication\n"));
		state->error_val = PS_AUTHFAIL;
		state->inc_state = INC_AUTH_FAILED;
		return -1;
	}
}

gint pop3_getauth_pass_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	g_return_val_if_fail(state->pass != NULL, -1);

	pop3_gen_send(sock, "PASS %s", state->pass);

	return POP3_GETAUTH_PASS_RECV;
}

gint pop3_getauth_pass_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	if (pop3_ok(sock, NULL) == PS_SUCCESS) 
		return POP3_GETRANGE_STAT_SEND;
	
	else {
		log_warning(_("error occurred on authentication\n"));
		state->error_val = PS_AUTHFAIL;
		state->inc_state = INC_AUTH_FAILED;
		return -1;
	}
}

gint pop3_getauth_apop_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gchar *start, *end;
	gchar *apop_str;
	gchar md5sum[33];

	g_return_val_if_fail(state->user != NULL, -1);
	g_return_val_if_fail(state->pass != NULL, -1);

	if ((start = strchr(state->greeting, '<')) == NULL) {
		log_warning(_("Required APOP timestamp not found "
			      "in greeting\n"));
		return -1;
	}

	if ((end = strchr(start, '>')) == NULL || end == start + 1) {
		log_warning(_("Timestamp syntax error in greeting\n"));
		return -1;
	}

	*(end + 1) = '\0';

	apop_str = g_strconcat(start, state->pass, NULL);
	md5_hex_digest(md5sum, apop_str);
	g_free(apop_str);

	pop3_gen_send(sock, "APOP %s %s", state->user, md5sum);

	return POP3_GETAUTH_APOP_RECV;
}

gint pop3_getauth_apop_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	if (pop3_ok(sock, NULL) == PS_SUCCESS) 
		return POP3_GETRANGE_STAT_SEND;

	else {
		log_warning(_("error occurred on authentication\n"));
		state->error_val = PS_AUTHFAIL;
		state->inc_state = INC_AUTH_FAILED;
		return -1;
	}
}

gint pop3_getrange_stat_send(SockInfo *sock, gpointer data)
{
	pop3_gen_send(sock, "STAT");

	return POP3_GETRANGE_STAT_RECV;
}

gint pop3_getrange_stat_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gchar buf[POPBUFSIZE + 1];
	gint ok;

	if ((ok = pop3_ok(sock, buf)) == PS_SUCCESS) {
		if (sscanf(buf, "%d %d", &state->count, &state->total_bytes)
		    != 2) {
			log_warning(_("POP3 protocol error\n"));
			return -1;
		} else {
			if (state->count == 0) {
				state->uidl_is_valid = TRUE;
				return POP3_LOGOUT_SEND;
			} else {
				state->msg = g_new0
					(Pop3MsgInfo, state->count + 1);
				state->cur_msg = 1;
				return POP3_GETRANGE_UIDL_SEND;
			}
		}
	} else if (ok == PS_PROTOCOL)
		return POP3_LOGOUT_SEND;
	else
		return -1;
}

gint pop3_getrange_last_send(SockInfo *sock, gpointer data)
{
	pop3_gen_send(sock, "LAST");

	return POP3_GETRANGE_LAST_RECV;
}

gint pop3_getrange_last_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gchar buf[POPBUFSIZE + 1];

	if (pop3_ok(sock, buf) == PS_SUCCESS) {
		gint last;

		if (sscanf(buf, "%d", &last) == 0) {
			log_warning(_("POP3 protocol error\n"));
			return -1;
		} else {
			if (state->count == last)
				return POP3_LOGOUT_SEND;
			else {
				state->cur_msg = last + 1;
				return POP3_GETSIZE_LIST_SEND;
			}
		}
	} else
		return POP3_GETSIZE_LIST_SEND;
}

gint pop3_getrange_uidl_send(SockInfo *sock, gpointer data)
{
	pop3_gen_send(sock, "UIDL");

	return POP3_GETRANGE_UIDL_RECV;
}

gint pop3_getrange_uidl_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gboolean new = FALSE;
	gboolean get_all = FALSE;
	gchar buf[POPBUFSIZE];
	gchar id[IDLEN + 1];
	gint next_state;

	if (!state->uidl_table) new = TRUE;
	if (state->ac_prefs->getall)
		get_all = TRUE;

	if (pop3_ok(sock, NULL) != PS_SUCCESS) {
		/* UIDL is not supported */
		if (pop3_sd_state(state, POP3_GETRANGE_UIDL_RECV, &next_state))
			return next_state;

		if (!get_all)
			return POP3_GETRANGE_LAST_SEND;
		else
			return POP3_GETSIZE_LIST_SEND;
	}

	while (sock_gets(sock, buf, sizeof(buf)) >= 0) {
		gint num;

		if (buf[0] == '.') break;
		if (sscanf(buf, "%d %" Xstr(IDLEN) "s", &num, id) != 2)
			continue;
		if (num <= 0 || num > state->count) continue;

		state->msg[num].uidl = g_strdup(id);

		if (state->uidl_table) {
			if (!get_all &&
			    g_hash_table_lookup(state->uidl_table, id) != NULL)
				state->msg[num].received = TRUE;
			else {
				if (new == FALSE) {
					state->cur_msg = num;
					new = TRUE;
				}
			}
		}

		if (should_delete(buf, (Pop3State *) state))
			state->uidl_todelete_list = g_slist_append
					(state->uidl_todelete_list, g_strdup(buf));		
		
	}

	state->uidl_is_valid = TRUE;

	if (pop3_sd_state(state, POP3_GETRANGE_UIDL_RECV, &next_state))
		return next_state;

	if (new == TRUE)
		return POP3_GETSIZE_LIST_SEND;
	else
		return POP3_LOGOUT_SEND;
}

static gboolean should_delete(const char *uidl, gpointer data) 
{
	/* answer[0] will contain id
	 * answer[0] will contain uidl */
	Pop3State *state = (Pop3State *) data;
	gchar **answer;
	int  id;
	gboolean result;
	int tdate, keep_for, today, nb_days;
	const gchar *sdate;
	GDate curdate;
	gchar *tuidl;

	if (!state->ac_prefs->rmmail || !strchr(uidl, ' '))
		return FALSE;

	/* remove \r\n */
	tuidl  = g_strndup(uidl, strlen(uidl) - 2);
	answer = g_strsplit(tuidl, " ", 2);
	id     = atoi(answer[0]);

	if (NULL != (sdate = g_hash_table_lookup(state->uidl_table, answer[1]))) {
		tdate    = atoi(sdate);
		keep_for = atoi(state->ac_prefs->leave_time); /* FIXME: leave time should be an int */

		g_date_clear(&curdate, 1);
		g_date_set_time(&curdate, time(NULL));
		today = g_date_day_of_year(&curdate);
		
		nb_days = g_date_is_leap_year(g_date_year(&curdate)) ? 366 : 365;
		result  = ((tdate + keep_for) % nb_days <= today);
	} else
		result = FALSE;

	g_free(tuidl);
	g_strfreev(answer);
	
	return result;
}

gint pop3_getsize_list_send(SockInfo *sock, gpointer data)
{
	pop3_gen_send(sock, "LIST");

	return POP3_GETSIZE_LIST_RECV;
}

gint pop3_getsize_list_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gchar buf[POPBUFSIZE];
	gint next_state;

	if (pop3_ok(sock, NULL) != PS_SUCCESS) return POP3_LOGOUT_SEND;

	state->cur_total_bytes = 0;

	while (sock_gets(sock, buf, sizeof(buf)) >= 0) {
		guint num, size;

		if (buf[0] == '.') break;
		if (sscanf(buf, "%u %u", &num, &size) != 2)
			return -1;

		if (num > 0 && num <= state->count)
			state->msg[num].size = size;
		if (num > 0 && num < state->cur_msg)
			state->cur_total_bytes += size;
	}

	if (pop3_sd_state(state, POP3_GETSIZE_LIST_RECV, &next_state))
		return next_state;

	LOOKUP_NEXT_MSG();	
	return POP3_RETR_SEND;
}
 
gint pop3_top_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	inc_progress_update(state, POP3_TOP_SEND); 

	pop3_gen_send(sock, "TOP %i 0", state->cur_msg );

	return POP3_TOP_RECV;
}

gint pop3_top_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gchar *filename, *path;
	gint next_state;
	
	if (pop3_ok(sock, NULL) != PS_SUCCESS) 
		return POP3_LOGOUT_SEND;

	path = g_strconcat(get_header_cache_dir(), G_DIR_SEPARATOR_S, NULL);

	if ( !is_dir_exist(path) )
		make_dir_hier(path);
	
	filename = g_strdup_printf("%s%i", path, state->cur_msg);
				   
	if (recv_write_to_file(sock, filename) < 0) {
		state->inc_state = INC_NOSPACE;
		return -1;
	}

	pop3_sd_state(state, POP3_TOP_RECV, &next_state);
	
	if (state->cur_msg < state->count) {
		state->cur_msg++;
		return POP3_TOP_SEND;
	} else
		return POP3_LOGOUT_SEND;
}

gint pop3_retr_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	pop3_gen_send(sock, "RETR %d", state->cur_msg);

	return POP3_RETR_RECV;
}

gint pop3_retr_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	const gchar *file;
	gint ok, drop_ok;
	gint next_state;
	int keep_for;
	
	if ((ok = pop3_ok(sock, NULL)) == PS_SUCCESS) {
		if (recv_write_to_file(sock, (file = get_tmp_file())) < 0) {
			if (state->inc_state == INC_SUCCESS)
				state->inc_state = INC_NOSPACE;
			return -1;
		}

		if ((drop_ok = inc_drop_message(file, state)) < 0) {
			state->inc_state = INC_ERROR;
			return -1;
		}

		if (pop3_sd_state(state, POP3_RETR_RECV, &next_state))
			return next_state;
	
		state->cur_total_bytes += state->msg[state->cur_msg].size;
		state->cur_total_num++;

		keep_for = (state->ac_prefs && state->ac_prefs->leave_time) ? 
			   atoi(state->ac_prefs->leave_time) : 0;

		if (drop_ok == 0 && state->ac_prefs->rmmail && keep_for == 0)
			return POP3_DELETE_SEND;

		state->msg[state->cur_msg].received = TRUE;

		if (state->cur_msg < state->count) {
			state->cur_msg++;
			LOOKUP_NEXT_MSG();
			return POP3_RETR_SEND;
		} else
			return POP3_LOGOUT_SEND;
	} else if (ok == PS_PROTOCOL)
		return POP3_LOGOUT_SEND;
	else
		return -1;
}

gint pop3_delete_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	pop3_gen_send(sock, "DELE %d", state->cur_msg);

	return POP3_DELETE_RECV;
}

gint pop3_delete_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gint next_state;
	gint ok;

	if ((ok = pop3_ok(sock, NULL)) == PS_SUCCESS) {

		state->msg[state->cur_msg].deleted = TRUE;
		
		if (pop3_sd_state(state, POP3_DELETE_RECV, &next_state))
			return next_state;	

		if (state->cur_msg < state->count) {
			state->cur_msg++;
			LOOKUP_NEXT_MSG();
			return POP3_RETR_SEND;
		} else
			return POP3_LOGOUT_SEND;
	} else if (ok == PS_PROTOCOL)
		return POP3_LOGOUT_SEND;
	else
		return -1;
}

gint pop3_logout_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gchar **parts;
	
	while (state->uidl_todelete_list != NULL) {
		/*
		 * FIXME: doesn't feel right - no checks for parts
		 */
		parts = g_strsplit((gchar *) state->uidl_todelete_list->data, " ", 2);
		state->uidl_todelete_list = g_slist_remove
			(state->uidl_todelete_list, state->uidl_todelete_list->data);
		pop3_gen_send(sock, "DELE %s", parts[0]);
		if (pop3_ok(sock, NULL) != PS_SUCCESS)
			log_warning(_("error occurred on DELE\n"));
		g_strfreev(parts);	
	}
	
	pop3_gen_send(sock, "QUIT");

	return POP3_LOGOUT_RECV;
}

gint pop3_logout_recv(SockInfo *sock, gpointer data)
{
	if (pop3_ok(sock, NULL) == PS_SUCCESS)
		return -1;
	else
		return -1;
}

static gint pop3_ok(SockInfo *sock, gchar *argbuf)
{
	gint ok;
	gchar buf[POPBUFSIZE + 1];
	gchar *bufp;

	if ((ok = pop3_gen_recv(sock, buf, sizeof(buf))) == PS_SUCCESS) {
		bufp = buf;
		if (*bufp == '+' || *bufp == '-')
			bufp++;
		else
			return PS_PROTOCOL;

		while (isalpha(*bufp))
			bufp++;

		if (*bufp)
			*(bufp++) = '\0';

		if (!strcmp(buf, "+OK"))
			ok = PS_SUCCESS;
		else if (!strncmp(buf, "-ERR", 4)) {
			if (strstr(bufp, "lock") ||
				 strstr(bufp, "Lock") ||
				 strstr(bufp, "LOCK") ||
				 strstr(bufp, "wait"))
				ok = PS_LOCKBUSY;
			else
				ok = PS_PROTOCOL;

			if (*bufp)
				fprintf(stderr, "POP3: %s\n", bufp);
		} else
			ok = PS_PROTOCOL;

		if (argbuf)
			strcpy(argbuf, bufp);
	}

	return ok;
}

static void pop3_gen_send(SockInfo *sock, const gchar *format, ...)
{
	gchar buf[POPBUFSIZE + 1];
	va_list args;

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf) - 2, format, args);
	va_end(args);

	if (!strncasecmp(buf, "PASS ", 5))
		log_print("POP3> PASS ********\n");
	else
		log_print("POP3> %s\n", buf);

	strcat(buf, "\r\n");
	sock_write(sock, buf, strlen(buf));
}

static gint pop3_gen_recv(SockInfo *sock, gchar *buf, gint size)
{
	if (sock_gets(sock, buf, size) < 0) {
		return PS_SOCKET;
	} else {
		strretchomp(buf);
		log_print("POP3< %s\n", buf);

		return PS_SUCCESS;
	}
}

static void pop3_sd_new_header(Pop3State *state)
{
	HeaderItems *new_msg;
	if (state->cur_msg <= state->count) {
		new_msg = g_new0(HeaderItems, 1); 
		
		new_msg->index              = state->cur_msg;
		new_msg->state              = SD_UNCHECKED;
		new_msg->size               = state->msg[state->cur_msg].size; 
		new_msg->received           = state->msg[state->cur_msg].received;
		new_msg->del_by_old_session = FALSE;
		
		state->ac_prefs->msg_list = g_slist_append(state->ac_prefs->msg_list, 
							   new_msg);
	}
}

gboolean pop3_sd_state(Pop3State *state, gint cur_state, guint *next_state) 
{
	gint session = state->ac_prefs->session;
	guint goto_state = -1;

	switch (cur_state) { 
	case POP3_GETRANGE_UIDL_RECV:
		switch (session) {
		case STYPE_DOWNLOAD:
		case STYPE_DELETE:
		case STYPE_PREVIEW_ALL:
			goto_state = POP3_GETSIZE_LIST_SEND;
		default:
			break;
		}
		break;
	case POP3_GETSIZE_LIST_RECV:
		switch (session) {
		case STYPE_PREVIEW_ALL:
			state->cur_msg = 1;
		case STYPE_PREVIEW_NEW:
			goto_state = POP3_TOP_SEND;
			break;
		case STYPE_DELETE:
			if (pop3_sd_get_next(state))
				goto_state = POP3_DELETE_SEND;		
			else
				goto_state = POP3_LOGOUT_SEND;
			break;
		case STYPE_DOWNLOAD:
			if (pop3_sd_get_next(state))
				goto_state = POP3_RETR_SEND;
			else
				goto_state = POP3_LOGOUT_SEND;
		default:
			break;
		}
		break;
	case POP3_TOP_RECV: 
		switch (session) { 
		case STYPE_PREVIEW_ALL:
		case STYPE_PREVIEW_NEW:
			pop3_sd_new_header(state);
		default:
			break;
		}
		break;
	case POP3_RETR_RECV:
		switch (session) {
		case STYPE_DOWNLOAD:
			if (state->ac_prefs->sd_rmmail_on_download) 
				goto_state = POP3_DELETE_SEND;
			else {
				if (pop3_sd_get_next(state)) 
					goto_state = POP3_RETR_SEND;
				else
					goto_state = POP3_LOGOUT_SEND;
			}
		default:	
			break;
		}
		break;
	case POP3_DELETE_RECV:
		switch (session) {
		case STYPE_DELETE:
			if (pop3_sd_get_next(state)) 
				goto_state = POP3_DELETE_SEND;
			else
				goto_state =  POP3_LOGOUT_SEND;
			break;
		case STYPE_DOWNLOAD:
			if (pop3_sd_get_next(state)) 
				goto_state = POP3_RETR_SEND;
			else
				goto_state = POP3_LOGOUT_SEND;
		default:
			break;
		}
	default:
		break;
		
	}		  

	*next_state = goto_state;
	if (goto_state != -1)
		return TRUE;
	else 
		return FALSE;
}

gboolean pop3_sd_get_next(Pop3State *state)
{
	GSList *cur;
	gint deleted_msgs = 0;
	
	switch (state->ac_prefs->session) {
	case STYPE_DOWNLOAD:
	case STYPE_DELETE: 	
		for (cur = state->ac_prefs->msg_list; cur != NULL; cur = cur->next) {
			HeaderItems *items = (HeaderItems*)cur->data;

			if (items->del_by_old_session)
				deleted_msgs++;

			switch (items->state) {
			case SD_REMOVE:
				items->state = SD_REMOVED;
				break;
			case SD_DOWNLOAD:
				items->state = SD_DOWNLOADED;
				break;
			case SD_CHECKED:
				state->cur_msg = items->index - deleted_msgs;
				if (state->ac_prefs->session == STYPE_DELETE)
					items->state = SD_REMOVE;
				else
					items->state = SD_DOWNLOAD;
				return TRUE;
			default:
				break;
			}
		}
		return FALSE;
	default:
		return FALSE;
	}
}
