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
static gboolean pop3_delete_header (Pop3State *state);
static gboolean should_delete (const char *uidl, gpointer data); 

gint pop3_greeting_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gchar buf[POPBUFSIZE];

	if (pop3_ok(sock, buf) == PS_SUCCESS) {
		if (state->ac_prefs->protocol == A_APOP) {
			state->greeting = g_strdup(buf);
			return POP3_GETAUTH_APOP_SEND;
		} else
			return POP3_GETAUTH_USER_SEND;
	} else
		return -1;
}

gint pop3_getauth_user_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	g_return_val_if_fail(state->user != NULL, -1);

	inc_progress_update(state, POP3_GETAUTH_USER_SEND);

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

	if (pop3_ok(sock, NULL) == PS_SUCCESS) {
		
		if (pop3_delete_header(state) == TRUE)
			return POP3_DELETE_SEND;
		else
			return POP3_GETRANGE_STAT_SEND;
	}
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

	inc_progress_update(state, POP3_GETAUTH_APOP_SEND);

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

	if (pop3_ok(sock, NULL) == PS_SUCCESS) {
		
		if (pop3_delete_header(state) == TRUE)
			return POP3_DELETE_SEND;
		else
		return POP3_GETRANGE_STAT_SEND;
	}

	else {
		log_warning(_("error occurred on authentication\n"));
		state->error_val = PS_AUTHFAIL;
		state->inc_state = INC_AUTH_FAILED;
		return -1;
	}
}

gint pop3_getrange_stat_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	inc_progress_update(state, POP3_GETRANGE_STAT_SEND);

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
	Pop3State *state = (Pop3State *)data;

	inc_progress_update(state, POP3_GETRANGE_LAST_SEND);

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
	Pop3State *state = (Pop3State *)data;

	inc_progress_update(state, POP3_GETRANGE_UIDL_SEND);

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

	if (pop3_ok(sock, NULL) != PS_SUCCESS) return POP3_GETRANGE_LAST_SEND;

	if (!state->uidl_table) new = TRUE;
	if (state->ac_prefs->getall)
		get_all = TRUE;

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
	GDate *curdate;
	int  id = 0;
	const gchar *sdate;
	gboolean result;

	if (!state->ac_prefs->rmmail || !strchr(uidl, ' '))
		return FALSE;

	curdate = g_date_new();	
	g_return_val_if_fail(curdate, FALSE);	
	
	g_date_set_time(curdate, time(NULL));
	
	/* remove \r\n */
	uidl   = g_strndup(uidl, strlen(uidl) - 2);
	answer = g_strsplit(uidl, " ", 2);
	id     = atoi(answer[0]);

	if (NULL != (sdate = g_hash_table_lookup(state->uidl_table, answer[1]))) {
		int tdate    = atoi(sdate);
		int keep_for = atoi(state->ac_prefs->leave_time); /* FIXME: leave time should be an int */
		int today    = g_date_day_of_year(curdate);
		int nb_days  = 365;

		nb_days = g_date_is_leap_year(g_date_year(curdate)) ? 366 : 365;
		result = ( (tdate + keep_for)%nb_days <= today );
	} else
		result = FALSE;

	g_date_free(curdate);
	g_free(uidl);
	g_strfreev(answer);
	
	return result;
}

gint pop3_getsize_list_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	inc_progress_update(state, POP3_GETSIZE_LIST_SEND);

	pop3_gen_send(sock, "LIST");

	return POP3_GETSIZE_LIST_RECV;
}

gint pop3_getsize_list_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gchar buf[POPBUFSIZE];

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

	LOOKUP_NEXT_MSG();
	if (state->ac_prefs->session_type == RETR_HEADER) 
		return POP3_TOP_SEND;
	else
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
	FILE  *fp;
	gchar buf[POPBUFSIZE];
	gchar *header;
	gchar *filename, *path;
	
	inc_progress_update(state, POP3_TOP_RECV); 

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
	/* we add a Complete-Size Header Item ...
	   note: overwrites first line  --> this is dirty */
	if ( (fp = fopen(filename, "rb+")) != NULL ) {
		gchar *buf = g_strdup_printf("%s%i", SIZE_HEADER, 
					     state->msg[state->cur_msg].size);
	
		if (change_file_mode_rw(fp, filename) == 0) 
			fprintf(fp, "%s\n", buf);
		fclose(fp);
	}
	
	g_free(path);
	g_free(filename);

	if (state->cur_msg < state->count) {
		state->cur_msg++;
		return POP3_TOP_SEND;
	} else
		return POP3_LOGOUT_SEND;
}

gint pop3_retr_send(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;

	inc_progress_update(state, POP3_RETR_SEND);

	pop3_gen_send(sock, "RETR %d", state->cur_msg);

	return POP3_RETR_RECV;
}

gint pop3_retr_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	const gchar *file;
	gint ok, drop_ok;
	int keep_for=atoi(g_strdup(state->ac_prefs->leave_time));
	
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
	
		state->cur_total_bytes += state->msg[state->cur_msg].size;
		state->cur_total_num++;

		if (drop_ok == 0 && state->ac_prefs->rmmail && keep_for == 0) {
			return POP3_DELETE_SEND;
		}

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

	/* inc_progress_update(state, POP3_DELETE_SEND); */

	pop3_gen_send(sock, "DELE %d", state->cur_msg);

	return POP3_DELETE_RECV;
}

gint pop3_delete_recv(SockInfo *sock, gpointer data)
{
	Pop3State *state = (Pop3State *)data;
	gint ok;

	if ((ok = pop3_ok(sock, NULL)) == PS_SUCCESS) {
		if (state->ac_prefs->session_type == RETR_NORMAL)
			state->msg[state->cur_msg].deleted = TRUE;

		if (pop3_delete_header(state) == TRUE) 
			return POP3_DELETE_SEND;

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
	while (state->uidl_todelete_list != NULL) {
		gchar **parts;
		gint ok;
		
		parts = g_strsplit((gchar *) state->uidl_todelete_list->data, " ", 2);
		state->uidl_todelete_list = g_slist_remove
			(state->uidl_todelete_list, state->uidl_todelete_list->data);
		pop3_gen_send(sock, "DELE %s", parts[0]);
		if ((ok = pop3_ok(sock, NULL)) != PS_SUCCESS)
			log_warning(_("error occurred on DELE\n"));
	}
	
	inc_progress_update(state, POP3_LOGOUT_SEND);

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

gboolean pop3_delete_header (Pop3State *state)
{
	
	if ( (state->ac_prefs->session_type == DELE_HEADER) &&
	     (g_slist_length(state->ac_prefs->to_delete) > 0) ) {

		state->cur_msg = (gint) state->ac_prefs->to_delete->data;
		debug_print(_("next to delete %i\n"), state->cur_msg);
		state->ac_prefs->to_delete = g_slist_remove 
			(state->ac_prefs->to_delete, state->ac_prefs->to_delete->data);
		return TRUE;
	}
	return FALSE;
}
