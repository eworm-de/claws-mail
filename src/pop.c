/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto
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
#include <time.h>
#include <errno.h>

#include "intl.h"
#include "pop.h"
#include "md5.h"
#include "prefs_account.h"
#include "utils.h"
#include "inc.h"
#include "recv.h"
/* disable sd
#include "selective_download.h"
*/
#include "log.h"

static gint pop3_greeting_recv		(Pop3Session *session,
					 const gchar *msg);
static gint pop3_getauth_user_send	(Pop3Session *session);
static gint pop3_getauth_pass_send	(Pop3Session *session);
static gint pop3_getauth_apop_send	(Pop3Session *session);
#if USE_OPENSSL
static gint pop3_stls_send		(Pop3Session *session);
static gint pop3_stls_recv		(Pop3Session *session);
#endif
static gint pop3_getrange_stat_send	(Pop3Session *session);
static gint pop3_getrange_stat_recv	(Pop3Session *session,
					 const gchar *msg);
static gint pop3_getrange_last_send	(Pop3Session *session);
static gint pop3_getrange_last_recv	(Pop3Session *session,
					 const gchar *msg);
static gint pop3_getrange_uidl_send	(Pop3Session *session);
static gint pop3_getrange_uidl_recv	(Pop3Session *session,
					 const gchar *msg);
static gint pop3_getsize_list_send	(Pop3Session *session);
static gint pop3_getsize_list_recv	(Pop3Session *session,
					 const gchar *msg);
static gint pop3_retr_send		(Pop3Session *session);
static gint pop3_retr_recv		(Pop3Session *session,
					 const gchar *data,
					 guint        len);
static gint pop3_retr_eom_recv		(Pop3Session *session,
					 const gchar *msg);
static gint pop3_delete_send		(Pop3Session *session);
static gint pop3_delete_recv		(Pop3Session *session);
static gint pop3_logout_send		(Pop3Session *session);

static void pop3_gen_send		(Pop3Session	*session,
					 const gchar	*format, ...);

static void pop3_session_destroy	(Session	*session);

static gint pop3_write_msg_to_file	(const gchar	*file,
					 const gchar	*data,
					 guint		 len);

static Pop3State pop3_lookup_next	(Pop3Session	*session);
static gint pop3_ok			(Pop3Session	*session,
					 const gchar	*msg);

static gint pop3_session_recv_msg		(Session	*session,
						 const gchar	*msg);
static gint pop3_session_recv_data_finished	(Session	*session,
						 guchar		*data,
						 guint		 len);
/* disable sd
static gboolean pop3_sd_get_next (Pop3State *state);
static void pop3_sd_new_header(Pop3State *state);
gboolean pop3_sd_state(Pop3State *state, gint cur_state, guint *next_state);
*/
static gint pop3_greeting_recv(Pop3Session *session, const gchar *msg)
{
	session->state = POP3_GREETING;

	session->greeting = g_strdup(msg);
	return PS_SUCCESS;
}

#if USE_OPENSSL
static gint pop3_stls_send(Pop3Session *session)
{
	session->state = POP3_STLS;
	pop3_gen_send(session, "STLS");
	return PS_SUCCESS;
}

static gint pop3_stls_recv(Pop3Session *session)
{
	if (session_start_tls(SESSION(session)) < 0) {
		session->error_val = PS_SOCKET;
		return -1;
	}
	return PS_SUCCESS;
}
#endif /* USE_OPENSSL */

static gint pop3_getauth_user_send(Pop3Session *session)
{
	g_return_val_if_fail(session->user != NULL, -1);

	session->state = POP3_GETAUTH_USER;
	pop3_gen_send(session, "USER %s", session->user);
	return PS_SUCCESS;
}

static gint pop3_getauth_pass_send(Pop3Session *session)
{
	g_return_val_if_fail(session->pass != NULL, -1);

	session->state = POP3_GETAUTH_PASS;
	pop3_gen_send(session, "PASS %s", session->pass);
	return PS_SUCCESS;
}

static gint pop3_getauth_apop_send(Pop3Session *session)
{
	gchar *start, *end;
	gchar *apop_str;
	gchar md5sum[33];

	g_return_val_if_fail(session->user != NULL, -1);
	g_return_val_if_fail(session->pass != NULL, -1);

	session->state = POP3_GETAUTH_APOP;

	if ((start = strchr(session->greeting, '<')) == NULL) {
		log_warning(_("Required APOP timestamp not found "
			      "in greeting\n"));
		session->error_val = PS_PROTOCOL;
		return -1;
	}

	if ((end = strchr(start, '>')) == NULL || end == start + 1) {
		log_warning(_("Timestamp syntax error in greeting\n"));
		session->error_val = PS_PROTOCOL;
		return -1;
	}

	*(end + 1) = '\0';

	apop_str = g_strconcat(start, session->pass, NULL);
	md5_hex_digest(md5sum, apop_str);
	g_free(apop_str);

	pop3_gen_send(session, "APOP %s %s", session->user, md5sum);

	return PS_SUCCESS;
}

static gint pop3_getrange_stat_send(Pop3Session *session)
{
	session->state = POP3_GETRANGE_STAT;
	pop3_gen_send(session, "STAT");
	return PS_SUCCESS;
}

static gint pop3_getrange_stat_recv(Pop3Session *session, const gchar *msg)
{
	if (sscanf(msg, "%d %d", &session->count, &session->total_bytes) != 2) {
		log_warning(_("POP3 protocol error\n"));
		session->error_val = PS_PROTOCOL;
		return -1;
	} else {
		if (session->count == 0) {
			session->uidl_is_valid = TRUE;
		} else {
			session->msg = g_new0(Pop3MsgInfo, session->count + 1);
			session->cur_msg = 1;
		}
	}

	return PS_SUCCESS;
}

static gint pop3_getrange_last_send(Pop3Session *session)
{
	session->state = POP3_GETRANGE_LAST;
	pop3_gen_send(session, "LAST");
	return PS_SUCCESS;
}

static gint pop3_getrange_last_recv(Pop3Session *session, const gchar *msg)
{
	gint last;

	if (sscanf(msg, "%d", &last) == 0) {
		log_warning(_("POP3 protocol error\n"));
		session->error_val = PS_PROTOCOL;
		return -1;
	} else {
		if (session->count > last)
			session->cur_msg = last + 1;
		else
			session->cur_msg = 0;
	}

	return PS_SUCCESS;
}

static gint pop3_getrange_uidl_send(Pop3Session *session)
{
	session->state = POP3_GETRANGE_UIDL;
	pop3_gen_send(session, "UIDL");
	return PS_SUCCESS;
}

static gint pop3_getrange_uidl_recv(Pop3Session *session, const gchar *msg)
{
	gchar id[IDLEN + 1];
	gint num;
	time_t recv_time;
	gint next_state;

	if (msg[0] == '.') {
		session->uidl_is_valid = TRUE;
		return PS_SUCCESS;
	}

	if (sscanf(msg, "%d %" Xstr(IDLEN) "s", &num, id) != 2)
		return -1;
	if (num <= 0 || num > session->count)
		return -1;

	session->msg[num].uidl = g_strdup(id);

	if (!session->uidl_table)
		return PS_CONTINUE;

	recv_time = (time_t)g_hash_table_lookup(session->uidl_table, id);
	session->msg[num].recv_time = recv_time;

	if (!session->ac_prefs->getall && recv_time != RECV_TIME_NONE)
		session->msg[num].received = TRUE;

	if (!session->new_msg_exist &&
	    (session->ac_prefs->getall || recv_time == RECV_TIME_NONE ||
	     session->ac_prefs->rmmail)) {
		session->cur_msg = num;
		session->new_msg_exist = TRUE;
	}

/* disable sd
	if (pop3_sd_state(state, POP3_GETRANGE_UIDL_RECV, &next_state))
		return next_state;
*/
	return PS_CONTINUE;
}

static gint pop3_getsize_list_send(Pop3Session *session)
{
	session->state = POP3_GETSIZE_LIST;
	pop3_gen_send(session, "LIST");
	return PS_SUCCESS;
}

static gint pop3_getsize_list_recv(Pop3Session *session, const gchar *msg)
{
	guint num, size;
	gint next_state;

	if (msg[0] == '.')
		return PS_SUCCESS;

	if (sscanf(msg, "%u %u", &num, &size) != 2) {
		session->error_val = PS_PROTOCOL;
		return -1;
	}

	if (num > 0 && num <= session->count)
		session->msg[num].size = size;
	if (num > 0 && num < session->cur_msg)
		session->cur_total_bytes += size;

/* disable sd
	if (pop3_sd_state(state, POP3_GETSIZE_LIST_RECV, &next_state))
		return next_state;
*/
	return PS_CONTINUE;
}
/* disable sd 
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
	gint write_val;
	if (pop3_ok(sock, NULL) != PS_SUCCESS) 
		return POP3_LOGOUT_SEND;

	path = g_strconcat(get_header_cache_dir(), G_DIR_SEPARATOR_S, NULL);

	if ( !is_dir_exist(path) )
		make_dir_hier(path);
	
	filename = g_strdup_printf("%s%i", path, state->cur_msg);
				   
	if ( (write_val = recv_write_to_file(sock, filename)) < 0) {
		state->error_val = (write_val == -1 ? PS_IOERR : PS_SOCKET);
		g_free(path);
		g_free(filename);
		return -1;
	}
	
	g_free(path);
	g_free(filename);
	
	pop3_sd_state(state, POP3_TOP_RECV, &next_state);
	
	if (state->cur_msg < state->count) {
		state->cur_msg++;
		return POP3_TOP_SEND;
	} else
		return POP3_LOGOUT_SEND;
}
*/
static gint pop3_retr_send(Pop3Session *session)
{
	session->state = POP3_RETR;
	pop3_gen_send(session, "RETR %d", session->cur_msg);
	return PS_SUCCESS;
}

static gint pop3_retr_recv(Pop3Session *session, const gchar *data, guint len)
{
	gchar *file;
	gint drop_ok;
	gint next_state;

	file = get_tmp_file();
	if (pop3_write_msg_to_file(file, data, len) < 0) {
		g_free(file);
		session->error_val = PS_IOERR;
		return -1;
	}

	/* drop_ok: 0: success 1: don't receive -1: error */
	drop_ok = inc_drop_message(file, session);
	g_free(file);
	if (drop_ok < 0) {
		session->error_val = PS_ERROR;
		return -1;
	}
/* disable sd
	if (pop3_sd_state(state, POP3_RETR_RECV, &next_state))
		return next_state;
*/	
	session->cur_total_bytes += session->msg[session->cur_msg].size;
	session->cur_total_recv_bytes += session->msg[session->cur_msg].size;
	session->cur_total_num++;

	session->msg[session->cur_msg].received = TRUE;
	session->msg[session->cur_msg].recv_time =
		drop_ok == 1 ? RECV_TIME_KEEP : session->current_time;

	return PS_SUCCESS;
}

static gint pop3_retr_eom_recv(Pop3Session *session, const gchar *msg)
{
	if (msg[0] == '.' && msg[1] == '\0')
		return PS_SUCCESS;
	else {
		g_warning("pop3_retr_eom_recv(): "
			  "invalid end of message: '%s'.\n"
			  "Maybe given size and actual one is different?\n",
			  msg);
		return PS_PROTOCOL;
	}
}

static gint pop3_delete_send(Pop3Session *session)
{
	session->state = POP3_DELETE;
	pop3_gen_send(session, "DELE %d", session->cur_msg);
	return PS_SUCCESS;
}

static gint pop3_delete_recv(Pop3Session *session)
{
	session->msg[session->cur_msg].deleted = TRUE;
	return PS_SUCCESS;
}

static gint pop3_logout_send(Pop3Session *session)
{
	session->state = POP3_LOGOUT;
	pop3_gen_send(session, "QUIT");
	return PS_SUCCESS;
}

static void pop3_gen_send(Pop3Session *session, const gchar *format, ...)
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

	session_send_msg(SESSION(session), SESSION_MSG_NORMAL, buf);
}
/* disable sd
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
		debug_print("received ?: msg %i, received: %i\n",new_msg->index, new_msg->received); 
	}
}

gboolean pop3_sd_state(Pop3State *state, gint cur_state, guint *next_state) 
{
	gint session = state->ac_prefs->session;
	guint goto_state = -1;

	switch (cur_state) { 
	case POP3_GETRANGE_UIDL_RECV:
		switch (session) {
		case STYPE_POP_BEFORE_SMTP:
			goto_state = POP3_LOGOUT_SEND;
			break;
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
*/
Session *pop3_session_new(PrefsAccount *account)
{
	Pop3Session *session;

	g_return_val_if_fail(account != NULL, NULL);

	session = g_new0(Pop3Session, 1);

	SESSION(session)->type = SESSION_POP3;
	SESSION(session)->server = NULL;
	SESSION(session)->port = 0;
	SESSION(session)->sock = NULL;
	SESSION(session)->state = SESSION_READY;
	SESSION(session)->data = NULL;

	SESSION(session)->recv_msg = pop3_session_recv_msg;
	SESSION(session)->recv_data_finished = pop3_session_recv_data_finished;
	SESSION(session)->send_data_finished = NULL;

	SESSION(session)->destroy = pop3_session_destroy;

	session->state = POP3_READY;
	session->ac_prefs = account;
	session->uidl_table = pop3_get_uidl_table(account);
	session->current_time = time(NULL);
	session->error_val = PS_SUCCESS;

	return SESSION(session);
}

static void pop3_session_destroy(Session *session)
{
	Pop3Session *pop3_session = POP3_SESSION(session);
	gint n;

	g_return_if_fail(session != NULL);

	for (n = 1; n <= pop3_session->count; n++)
		g_free(pop3_session->msg[n].uidl);
	g_free(pop3_session->msg);

	if (pop3_session->uidl_table) {
		hash_free_strings(pop3_session->uidl_table);
		g_hash_table_destroy(pop3_session->uidl_table);
	}

	g_free(pop3_session->greeting);
	g_free(pop3_session->user);
	g_free(pop3_session->pass);
}

GHashTable *pop3_get_uidl_table(PrefsAccount *ac_prefs)
{
	GHashTable *table;
	gchar *path;
	FILE *fp;
	gchar buf[POPBUFSIZE];
	gchar uidl[POPBUFSIZE];
	time_t recv_time;
	time_t now;

	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			   "uidl", G_DIR_SEPARATOR_S, ac_prefs->recv_server,
			   "-", ac_prefs->userid, NULL);
	if ((fp = fopen(path, "rb")) == NULL) {
		if (ENOENT != errno) FILE_OP_ERROR(path, "fopen");
		g_free(path);
		path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
				   "uidl-", ac_prefs->recv_server,
				   "-", ac_prefs->userid, NULL);
		if ((fp = fopen(path, "rb")) == NULL) {
			if (ENOENT != errno) FILE_OP_ERROR(path, "fopen");
			g_free(path);
			return NULL;
		}
	}
	g_free(path);

	table = g_hash_table_new(g_str_hash, g_str_equal);

	now = time(NULL);

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		strretchomp(buf);
		recv_time = RECV_TIME_NONE;
		if (sscanf(buf, "%s\t%ld", uidl, &recv_time) != 2) {
			if (sscanf(buf, "%s", uidl) != 1)
				continue;
			else
				recv_time = now;
		}
		if (recv_time == RECV_TIME_NONE)
			recv_time = RECV_TIME_RECEIVED;
		g_hash_table_insert(table, g_strdup(uidl),
				    GINT_TO_POINTER(recv_time));
	}

	fclose(fp);
	return table;
}

gint pop3_write_uidl_list(Pop3Session *session)
{
	gchar *path;
	FILE *fp;
	Pop3MsgInfo *msg;
	gint n;

	if (!session->uidl_is_valid) return 0;

	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			   "uidl", G_DIR_SEPARATOR_S,
			   session->ac_prefs->recv_server,
			   "-", session->ac_prefs->userid, NULL);
	if ((fp = fopen(path, "wb")) == NULL) {
		FILE_OP_ERROR(path, "fopen");
		g_free(path);
		return -1;
	}

	for (n = 1; n <= session->count; n++) {
		msg = &session->msg[n];
		if (msg->uidl && msg->received && !msg->deleted)
			fprintf(fp, "%s\t%ld\n", msg->uidl, msg->recv_time);
	}

	if (fclose(fp) == EOF) FILE_OP_ERROR(path, "fclose");
	g_free(path);

	return 0;
}

static gint pop3_write_msg_to_file(const gchar *file, const gchar *data,
				   guint len)
{
	FILE *fp;
	const gchar *prev, *cur;

	g_return_val_if_fail(file != NULL, -1);

	if ((fp = fopen(file, "wb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	if (change_file_mode_rw(fp, file) < 0)
		FILE_OP_ERROR(file, "chmod");

	/* +------------------+----------------+--------------------------+ *
	 * ^data              ^prev            ^cur             data+len-1^ */

	prev = data;
	while ((cur = memchr(prev, '\r', len - (prev - data))) != NULL) {
		if ((cur > prev && fwrite(prev, cur - prev, 1, fp) < 1) ||
		    fputc('\n', fp) == EOF) {
			FILE_OP_ERROR(file, "fwrite");
			g_warning("can't write to file: %s\n", file);
			fclose(fp);
			unlink(file);
			return -1;
		}

		if (cur == data + len - 1) {
			prev = cur + 1;
			break;
		}

		if (*(cur + 1) == '\n')
			prev = cur + 2;
		else
			prev = cur + 1;

		if (prev - data >= len)
			break;
	}

	if (prev - data < len &&
	    fwrite(prev, len - (prev - data), 1, fp) < 1) {
		FILE_OP_ERROR(file, "fwrite");
		g_warning("can't write to file: %s\n", file);
		fclose(fp);
		unlink(file);
		return -1;
	}
	if (data[len - 1] != '\r' && data[len - 1] != '\n') {
		if (fputc('\n', fp) == EOF) {
			FILE_OP_ERROR(file, "fputc");
			g_warning("can't write to file: %s\n", file);
			fclose(fp);
			unlink(file);
			return -1;
		}
	}

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(file, "fclose");
		unlink(file);
		return -1;
	}

	return 0;
}

static Pop3State pop3_lookup_next(Pop3Session *session)
{
	Pop3MsgInfo *msg;
	PrefsAccount *ac = session->ac_prefs;
	gint size;
	gboolean size_limit_over;

	for (;;) {
		msg = &session->msg[session->cur_msg];
		size = msg->size;
		size_limit_over =
		    (ac->enable_size_limit &&
		     ac->size_limit > 0 &&
		     size > ac->size_limit * 1024);

		if (ac->rmmail &&
		    msg->recv_time != RECV_TIME_NONE &&
		    msg->recv_time != RECV_TIME_KEEP &&
		    session->current_time - msg->recv_time >=
		    ac->msg_leave_time * 24 * 60 * 60) {
			log_print(_("POP3: Deleting expired message %d\n"),
				  session->cur_msg);
			pop3_delete_send(session);
			return POP3_DELETE;
		}

		if (size_limit_over)
			log_print
				(_("POP3: Skipping message %d (%d bytes)\n"),
				  session->cur_msg, size);

		if (size == 0 || msg->received || size_limit_over) {
			session->cur_total_bytes += size;
			if (session->cur_msg == session->count) {
				pop3_logout_send(session);
				return POP3_LOGOUT;
			} else
				session->cur_msg++;
		} else
			break;
	}

	pop3_retr_send(session);
	return POP3_RETR;
}

static gint pop3_ok(Pop3Session *session, const gchar *msg)
{
	gint ok;

	if (!strncmp(msg, "+OK", 3))
		ok = PS_SUCCESS;
	else if (!strncmp(msg, "-ERR", 4)) {
		if (strstr(msg + 4, "lock") ||
		    strstr(msg + 4, "Lock") ||
		    strstr(msg + 4, "LOCK") ||
		    strstr(msg + 4, "wait"))
			ok = PS_LOCKBUSY;
		else
			ok = PS_PROTOCOL;

		fprintf(stderr, "POP3: %s\n", msg);
	} else
		ok = PS_PROTOCOL;

	return ok;
}

static gint pop3_session_recv_msg(Session *session, const gchar *msg)
{
	Pop3Session *pop3_session = POP3_SESSION(session);
	gint val;
	const gchar *body;

	body = msg;
	if (pop3_session->state != POP3_GETRANGE_UIDL_RECV &&
	    pop3_session->state != POP3_GETSIZE_LIST_RECV &&
	    pop3_session->state != POP3_RETR_EOM) {
		log_print("POP3< %s\n", msg);
		val = pop3_ok(pop3_session, msg);
		if (val != PS_SUCCESS) {
			pop3_session->state = POP3_ERROR;
			pop3_session->error_val = val;
			return -1;
		}

		if (*body == '+' || *body == '-')
			body++;
		while (isalpha(*body))
			body++;
		while (isspace(*body))
			body++;
	}

	switch (pop3_session->state) {
	case POP3_READY:
	case POP3_GREETING:
		pop3_greeting_recv(pop3_session, body);
#if USE_OPENSSL
		if (pop3_session->ac_prefs->ssl_pop == SSL_STARTTLS)
			pop3_stls_send(pop3_session);
		else
#endif
		if (pop3_session->ac_prefs->protocol == A_APOP)
			pop3_getauth_apop_send(pop3_session);
		else
			pop3_getauth_user_send(pop3_session);
		break;
#if USE_OPENSSL
	case POP3_STLS:
		if (pop3_stls_recv(pop3_session) != PS_SUCCESS)
			return -1;
		if (pop3_session->ac_prefs->protocol == A_APOP)
			pop3_getauth_apop_send(pop3_session);
		else
			pop3_getauth_user_send(pop3_session);
		break;
#endif
	case POP3_GETAUTH_USER:
		pop3_getauth_pass_send(pop3_session);
		break;
	case POP3_GETAUTH_PASS:
	case POP3_GETAUTH_APOP:
		pop3_getrange_stat_send(pop3_session);
		break;
	case POP3_GETRANGE_STAT:
		if (pop3_getrange_stat_recv(pop3_session, body) < 0)
			return -1;
		if (pop3_session->count > 0)
			pop3_getrange_uidl_send(pop3_session);
		else
			pop3_logout_send(pop3_session);
		break;
	case POP3_GETRANGE_LAST:
		if (pop3_getrange_last_recv(pop3_session, body) < 0)
			return -1;
		if (pop3_session->cur_msg > 0)
			pop3_getsize_list_send(pop3_session);
		else
			pop3_logout_send(pop3_session);
		break;
	case POP3_GETRANGE_UIDL:
		pop3_session->state = POP3_GETRANGE_UIDL_RECV;
		return 1;
	case POP3_GETRANGE_UIDL_RECV:
		val = pop3_getrange_uidl_recv(pop3_session, body);
		if (val == PS_CONTINUE)
			return 1;
		else if (val == PS_SUCCESS) {
			if (pop3_session->new_msg_exist)
				pop3_getsize_list_send(pop3_session);
			else
				pop3_logout_send(pop3_session);
		} else
			return -1;
		break;
	case POP3_GETSIZE_LIST:
		pop3_session->state = POP3_GETSIZE_LIST_RECV;
		return 1;
	case POP3_GETSIZE_LIST_RECV:
		val = pop3_getsize_list_recv(pop3_session, body);
		if (val == PS_CONTINUE)
			return 1;
		else if (val == PS_SUCCESS) {
			if (pop3_lookup_next(pop3_session) == POP3_ERROR)
				return -1;
		} else
			return -1;
		break;
	case POP3_RETR:
		pop3_session->state = POP3_RETR_RECV;
		session_recv_data
			(session,
			 pop3_session->msg[pop3_session->cur_msg].size, TRUE);
		break;
	case POP3_RETR_EOM:
		if (pop3_retr_eom_recv(pop3_session, body) != PS_SUCCESS)
			return -1;
		if (pop3_session->ac_prefs->rmmail &&
		    pop3_session->ac_prefs->msg_leave_time == 0)
			pop3_delete_send(pop3_session);
		else if (pop3_session->cur_msg == pop3_session->count)
			pop3_logout_send(pop3_session);
		else {
			pop3_session->cur_msg++;
			if (pop3_lookup_next(pop3_session) == POP3_ERROR)
				return -1;
		}
		break;
	case POP3_DELETE:
		pop3_delete_recv(pop3_session);
		if (pop3_session->cur_msg == pop3_session->count)
			pop3_logout_send(pop3_session);
		else {
			pop3_session->cur_msg++;
			if (pop3_lookup_next(pop3_session) == POP3_ERROR)
				return -1;
		}
		break;
	case POP3_LOGOUT:
		session_disconnect(session);
		break;
	case POP3_ERROR:
	default:
		return -1;
	}

	return 0;
}

static gint pop3_session_recv_data_finished(Session *session, guchar *data,
					    guint len)
{
	Pop3Session *pop3_session = POP3_SESSION(session);

	if (len == 0)
		return -1;

	if (pop3_retr_recv(pop3_session, data, len) < 0)
		return -1;

	pop3_session->state = POP3_RETR_EOM;

	return 1;
}
