/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto
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
#include "recv.h"
#include "folder.h"

#include "log.h"
#include "hooks.h"

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
					 const gchar *data,
					 guint        len);
static gint pop3_getsize_list_send	(Pop3Session *session);
static gint pop3_getsize_list_recv	(Pop3Session *session,
					 const gchar *data,
					 guint        len);
static gint pop3_retr_send		(Pop3Session *session);
static gint pop3_retr_recv		(Pop3Session *session,
					 const gchar *data,
					 guint        len);
static gint pop3_delete_send		(Pop3Session *session);
static gint pop3_delete_recv		(Pop3Session *session);
static gint pop3_logout_send		(Pop3Session *session);

static void pop3_gen_send		(Pop3Session	*session,
					 const gchar	*format, ...);

static void pop3_session_destroy	(Session	*session);

static gint pop3_write_msg_to_file	(const gchar	*file,
					 const gchar	*data,
					 guint		 len,
					 const gchar 	*prefix);

static Pop3State pop3_lookup_next	(Pop3Session	*session);
static Pop3ErrorValue pop3_ok		(Pop3Session	*session,
					 const gchar	*msg);

static gint pop3_session_recv_msg		(Session	*session,
						 const gchar	*msg);
static gint pop3_session_recv_data_finished	(Session	*session,
						 guchar		*data,
						 guint		 len);

static gchar *pop3_get_filename_for_partial_mail(Pop3Session 	*session, 
						 gchar 		*muidl);
static void pop3_delete_old_partial	(const gchar 	*file);

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
		if (session->count > last) {
			session->new_msg_exist = TRUE;
			session->cur_msg = last + 1;
		} else
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

static gint pop3_getrange_uidl_recv(Pop3Session *session, const gchar *data,
				    guint len)
{
	gchar id[IDLEN + 1];
	gchar buf[POPBUFSIZE];
	gint buf_len;
	gint num;
	time_t recv_time;
	gint partial_recv;
	const gchar *p = data;
	const gchar *lastp = data + len;
	const gchar *newline;

	while (p < lastp) {
		if ((newline = memchr(p, '\r', lastp - p)) == NULL)
			return -1;
		buf_len = MIN(newline - p, sizeof(buf) - 1);
		memcpy(buf, p, buf_len);
		buf[buf_len] = '\0';

		p = newline + 1;
		if (p < lastp && *p == '\n') p++;

		if (sscanf(buf, "%d %" Xstr(IDLEN) "s", &num, id) != 2)
			return -1;
		if (num <= 0 || num > session->count)
			return -1;

		session->msg[num].uidl = g_strdup(id);

		recv_time = (time_t)g_hash_table_lookup(
			   		session->uidl_table, id);
		session->msg[num].recv_time = recv_time;

		partial_recv = (gint)g_hash_table_lookup(
					session->partial_recv_table, id);

		if (!session->ac_prefs->getall && recv_time != RECV_TIME_NONE) {
			session->msg[num].received = 
				(partial_recv != POP3_MUST_COMPLETE_RECV);
			session->msg[num].partial_recv = partial_recv;

		}
		if (!session->new_msg_exist &&
		    (session->ac_prefs->getall || recv_time == RECV_TIME_NONE ||
		     session->ac_prefs->rmmail)) {
			session->cur_msg = num;
			session->new_msg_exist = TRUE;
		}
	}

	session->uidl_is_valid = TRUE;
	return PS_SUCCESS;
}

static gint pop3_getsize_list_send(Pop3Session *session)
{
	session->state = POP3_GETSIZE_LIST;
	pop3_gen_send(session, "LIST");
	return PS_SUCCESS;
}

static gint pop3_getsize_list_recv(Pop3Session *session, const gchar *data,
				   guint len)
{
	gchar buf[POPBUFSIZE];
	gint buf_len;
	guint num, size;
	const gchar *p = data;
	const gchar *lastp = data + len;
	const gchar *newline;

	while (p < lastp) {
		if ((newline = memchr(p, '\r', lastp - p)) == NULL)
			return -1;
		buf_len = MIN(newline - p, sizeof(buf) - 1);
		memcpy(buf, p, buf_len);
		buf[buf_len] = '\0';

		p = newline + 1;
		if (p < lastp && *p == '\n') p++;

		if (sscanf(buf, "%u %u", &num, &size) != 2) {
			session->error_val = PS_PROTOCOL;
			return -1;
		}

		if (num > 0 && num <= session->count)
			session->msg[num].size = size;
		if (num > 0 && num < session->cur_msg)
			session->cur_total_bytes += size;
	}

	return PS_SUCCESS;
}

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
	MailReceiveData mail_receive_data;

	mail_receive_data.session = session;
	mail_receive_data.data = g_strndup(data, len);
	hooks_invoke(MAIL_RECEIVE_HOOKLIST, &mail_receive_data);

	file = get_tmp_file();
	if (pop3_write_msg_to_file(file, mail_receive_data.data,
		strlen(mail_receive_data.data), NULL) < 0) {
		g_free(file);
		g_free(mail_receive_data.data);
		session->error_val = PS_IOERR;
		return -1;
	}
	g_free(mail_receive_data.data);

	if (session->msg[session->cur_msg].partial_recv 
	    == POP3_MUST_COMPLETE_RECV) {
		gchar *old_file = pop3_get_filename_for_partial_mail(
				session, session->msg[session->cur_msg].uidl);
		
		if (old_file) {
			pop3_delete_old_partial (old_file);
			g_free(old_file);
		}
	} 

	/* drop_ok: 0: success 1: don't receive -1: error */
	drop_ok = session->drop_message(session, file);

	g_free(file);
	if (drop_ok < 0) {
		session->error_val = PS_IOERR;
		return -1;
	}
	
	session->cur_total_bytes += session->msg[session->cur_msg].size;
	session->cur_total_recv_bytes += session->msg[session->cur_msg].size;
	session->cur_total_num++;

	session->msg[session->cur_msg].received = TRUE;
	session->msg[session->cur_msg].partial_recv = POP3_TOTALLY_RECEIVED;

	session->msg[session->cur_msg].recv_time =
		drop_ok == 1 ? RECV_TIME_KEEP : session->current_time;

	return PS_SUCCESS;
}

static gint pop3_top_send(Pop3Session *session, gint max_size)
{
	gint num_lines = (max_size*1024)/82; /* consider lines to be 80 chars */
	session->state = POP3_TOP;
	pop3_gen_send(session, "TOP %d %d", session->cur_msg, num_lines);
	return PS_SUCCESS;
}

static gint pop3_top_recv(Pop3Session *session, const gchar *data, guint len)
{
	gchar *file;
	gint drop_ok;
	MailReceiveData mail_receive_data;
	gchar *partial_notice = NULL;
	
	mail_receive_data.session = session;
	mail_receive_data.data = g_strndup(data, len);
	hooks_invoke(MAIL_RECEIVE_HOOKLIST, &mail_receive_data);

	partial_notice = g_strdup_printf("SC-Partially-Retrieved: %s\n"
					 "SC-Account-Server: %s\n"
					 "SC-Account-Login: %s\n"
					 "SC-Message-Size: %d",
					 session->msg[session->cur_msg].uidl,
					 session->ac_prefs->recv_server,
			   		 session->ac_prefs->userid,
					 session->msg[session->cur_msg].size);
	file = get_tmp_file();
	if (pop3_write_msg_to_file(file, mail_receive_data.data,
		strlen(mail_receive_data.data), partial_notice) < 0) {
		g_free(file);
		g_free(mail_receive_data.data);
		session->error_val = PS_IOERR;
		g_free(partial_notice);
		return -1;
	}
	g_free(mail_receive_data.data);
	g_free(partial_notice);

	/* drop_ok: 0: success 1: don't receive -1: error */
	drop_ok = session->drop_message(session, file);
	g_free(file);
	if (drop_ok < 0) {
		session->error_val = PS_IOERR;
		return -1;
	}

	session->cur_total_bytes += session->msg[session->cur_msg].size;
	session->cur_total_recv_bytes += session->msg[session->cur_msg].size;
	session->cur_total_num++;

	session->msg[session->cur_msg].received = TRUE;
	session->msg[session->cur_msg].partial_recv = POP3_PARTIALLY_RECEIVED;
	session->msg[session->cur_msg].recv_time =
		drop_ok == 1 ? RECV_TIME_KEEP : session->current_time;

	return PS_SUCCESS;
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

Session *pop3_session_new(PrefsAccount *account)
{
	Pop3Session *session;

	g_return_val_if_fail(account != NULL, NULL);

	session = g_new0(Pop3Session, 1);

	session_init(SESSION(session));

	SESSION(session)->type = SESSION_POP3;

	SESSION(session)->recv_msg = pop3_session_recv_msg;
	SESSION(session)->recv_data_finished = pop3_session_recv_data_finished;
	SESSION(session)->send_data_finished = NULL;

	SESSION(session)->destroy = pop3_session_destroy;

	session->state = POP3_READY;
	session->ac_prefs = account;
	session->pop_before_smtp = FALSE;
	pop3_get_uidl_table(account, session);
	session->current_time = time(NULL);
	session->error_val = PS_SUCCESS;
	session->error_msg = NULL;

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

	if (pop3_session->partial_recv_table) {
		hash_free_strings(pop3_session->partial_recv_table);
		g_hash_table_destroy(pop3_session->partial_recv_table);
	}

	g_free(pop3_session->greeting);
	g_free(pop3_session->user);
	g_free(pop3_session->pass);
	g_free(pop3_session->error_msg);
}

void pop3_get_uidl_table(PrefsAccount *ac_prefs, Pop3Session *session)
{
	GHashTable *table;
	GHashTable *partial_recv_table;
	gchar *path;
	FILE *fp;
	gchar buf[POPBUFSIZE];
	gchar uidl[POPBUFSIZE];
	time_t recv_time;
	time_t now;
	gint partial_recv;
	
	table = g_hash_table_new(g_str_hash, g_str_equal);
	partial_recv_table = g_hash_table_new(g_str_hash, g_str_equal);

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
			session->uidl_table = table;
			session->partial_recv_table = partial_recv_table;
			return;
		}
	}
	g_free(path);

	now = time(NULL);

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		gchar tmp[POPBUFSIZE];
		strretchomp(buf);
		recv_time = RECV_TIME_NONE;
		partial_recv = POP3_TOTALLY_RECEIVED;
		
		if (sscanf(buf, "%s\t%ld\t%s", uidl, &recv_time, &tmp) < 2) {
			if (sscanf(buf, "%s", uidl) != 1)
				continue;
			else {
				recv_time = now;
			}
		}
		if (recv_time == RECV_TIME_NONE)
			recv_time = RECV_TIME_RECEIVED;
		g_hash_table_insert(table, g_strdup(uidl),
				    GINT_TO_POINTER(recv_time));
		if (strlen(tmp) == 1)
			partial_recv = atoi(tmp); /* totally received ?*/
		else
			partial_recv = POP3_MUST_COMPLETE_RECV;

		g_hash_table_insert(partial_recv_table, g_strdup(uidl),
				    GINT_TO_POINTER(partial_recv));
	}

	fclose(fp);
	session->uidl_table = table;
	session->partial_recv_table = partial_recv_table;
	
	return;
}

int pop3_msg_in_uidl_list(const gchar *server, const gchar *login, 
			  const gchar *muidl)
{
	gchar *path;
	FILE *fp;
	gchar buf[POPBUFSIZE];
	gchar uidl[POPBUFSIZE];
	time_t recv_time;
	time_t now;
	gint partial_recv;
	
	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			   "uidl", G_DIR_SEPARATOR_S, server,
			   "-", login, NULL);
	if ((fp = fopen(path, "rb")) == NULL) {
		if (ENOENT != errno) FILE_OP_ERROR(path, "fopen");
		g_free(path);
		path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
				   "uidl-", server,
				   "-", login, NULL);
		if ((fp = fopen(path, "rb")) == NULL) {
			if (ENOENT != errno) FILE_OP_ERROR(path, "fopen");
			g_free(path);
			return FALSE;
		}
	}
	g_free(path);

	now = time(NULL);

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		gchar tmp[POPBUFSIZE];
		strretchomp(buf);
		recv_time = RECV_TIME_NONE;
		partial_recv = POP3_TOTALLY_RECEIVED;
		
		if (sscanf(buf, "%s\t%ld\t%s", uidl, &recv_time, &tmp) < 2) {
			if (sscanf(buf, "%s", uidl) != 1)
				continue;
			else {
				recv_time = now;
			}
		}
		if (!strcmp(uidl, muidl)) {
			fclose(fp);
			return TRUE;
		}
	}

	fclose(fp);	
	return FALSE;
}

static gchar *pop3_get_filename_for_partial_mail(Pop3Session *session, 
						 gchar *muidl)
{
	gchar *path;
	gchar *result = NULL;
	FILE *fp;
	gchar buf[POPBUFSIZE];
	gchar uidl[POPBUFSIZE];
	time_t recv_time;
	time_t now;
	gint partial_recv;
	
	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			   "uidl", G_DIR_SEPARATOR_S, 
			   session->ac_prefs->recv_server,
			   "-", session->ac_prefs->userid, NULL);
	if ((fp = fopen(path, "rb")) == NULL) {
		if (ENOENT != errno) FILE_OP_ERROR(path, "fopen");
		g_free(path);
		path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
				   "uidl-", session->ac_prefs->recv_server,
				   "-", session->ac_prefs->userid, NULL);
		if ((fp = fopen(path, "rb")) == NULL) {
			if (ENOENT != errno) FILE_OP_ERROR(path, "fopen");
			g_free(path);
			return result;
		}
	}
	g_free(path);

	now = time(NULL);

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		gchar tmp[POPBUFSIZE];
		strretchomp(buf);
		recv_time = RECV_TIME_NONE;
		partial_recv = POP3_TOTALLY_RECEIVED;
		
		if (sscanf(buf, "%s\t%ld\t%s", uidl, &recv_time, &tmp) < 2) {
			if (sscanf(buf, "%s", uidl) != 1)
				continue;
			else {
				recv_time = now;
			}
		}
		if (!strcmp(muidl, uidl)) {
			result = strdup(tmp);
			break;
		}
	}

	fclose(fp);
	
	return result;
}

static int pop3_uidl_mark_mail(const gchar *server, const gchar *login, 
			  const gchar *muidl, const gchar *filename, 
			  const gchar *folder_item_id, guint msgnum, 
			  int download)
{
	gchar *path;
	gchar *pathnew;
	FILE *fp;
	FILE *fpnew;
	gchar buf[POPBUFSIZE];
	gchar uidl[POPBUFSIZE];
	time_t recv_time;
	time_t now;
	int len;
	int start = TRUE;
	gchar partial_recv[POPBUFSIZE];
	
	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			   "uidl", G_DIR_SEPARATOR_S, server,
			   "-", login, NULL);
	if ((fp = fopen(path, "rb")) == NULL) {
		perror("fopen1");
		if (ENOENT != errno) FILE_OP_ERROR(path, "fopen");
		g_free(path);
		path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
				   "uidl-", server,
				   "-", login, NULL);
		if ((fp = fopen(path, "rb")) == NULL) {
			if (ENOENT != errno) FILE_OP_ERROR(path, "fopen");
			g_free(path);
		}
		return -1;
	}

	pathnew = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			   "uidl", G_DIR_SEPARATOR_S, server,
			   "-", login, ".new", NULL);
	if ((fpnew = fopen(pathnew, "wb")) == NULL) {
		perror("fopen2");
		fclose(fp);
		g_free(pathnew);
		return -1;	
	}
	
	now = time(NULL);

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		strretchomp(buf);
		recv_time = RECV_TIME_NONE;
		sprintf(partial_recv,"0");
		
		if (sscanf(buf, "%s\t%ld\t%s", 
			   uidl, &recv_time, &partial_recv) < 2) {
			if (sscanf(buf, "%s", uidl) != 1)
				continue;
			else {
				recv_time = now;
			}
		}
		if (strcmp(muidl, uidl)) {
			fprintf(fpnew, "%s\t%ld\t%s\n", 
				uidl, recv_time, partial_recv);
		} else {
			gchar *stat = NULL;
			if (download == POP3_PARTIAL_DLOAD_DLOAD) {
				stat = g_strdup_printf("%s:%d",
					folder_item_id, msgnum);
			}
			else if (download == POP3_PARTIAL_DLOAD_UNKN)
				stat = strdup("1");
			else if (download == POP3_PARTIAL_DLOAD_DELE)
				stat = strdup("0");
			
			fprintf(fpnew, "%s\t%ld\t%s\n", 
				uidl, recv_time, stat);
			g_free(stat);
		}
	}
	fclose(fpnew);
	fclose(fp);

	move_file(pathnew, path, TRUE);

	g_free(path);
	g_free(pathnew);
	
	if ((fp = fopen(filename,"rb")) == NULL) {
		perror("fopen3");
		return -1;
	}
	pathnew = g_strdup_printf("%s.new", filename);
	if ((fpnew = fopen(pathnew, "wb")) == NULL) {
		perror("fopen4");
		fclose(fp);
		g_free(pathnew);
		return -1;
	}
	
	while ((len = fread(buf, sizeof(gchar), sizeof(buf), fp)) > 0) {
		buf[len]='\0';
		if (start) {
			start = FALSE;
			if (download != POP3_PARTIAL_DLOAD_UNKN)
				fprintf(fpnew, "SC-Marked-For-Download: %d\n", 
					download);
			
			if(strlen(buf) > strlen("SC-Marked-For-Download: x\n")
			&& !strncmp(buf, "SC-Marked-For-Download:", 
			            strlen("SC-Marked-For-Download:"))) {
				fprintf(fpnew, "%s", 
				 buf+strlen("SC-Marked-For-Download: x\n"));
				continue;
			}
		}
		fprintf(fpnew, "%s", buf);
	}
	fclose(fpnew);
	fclose(fp);
	unlink(filename);
	rename(pathnew, filename);
	
	g_free(pathnew);
	return 0;
}

/* Partial download:
 * A mail which has been completely downloaded will have no special headers,
 * and its entry in the uidl file will end by 0 (POP3_TOTALLY_RECEIVED);
 *
 * A mail which has been partially downloaded will have some special headers,
 * and its entry in the uidl file will first be 1 (POP3_PARTIALLY_RECEIVED);
 * the special headers will be including "SC-Marked-For-Download" which can 
 * have three values:
 * 0 (POP3_PARTIAL_DLOAD_UNKN) meaning that the user has not yet chosen to
 *  download the mail or let it be deleted - this header is absent until the
 *  user first chooses an action
 * 1 (POP3_PARTIAL_DLOAD_DLOAD) meaning that the user wants to finish 
 *  downloading the mail
 * 2 (POP3_PARTIAL_DLOAD_DELE) meaning that the user does not want to finish
 *  downloading the mail
 * When updating this header to POP3_PARTIAL_DLOAD_DLOAD, the uidl line of
 * this mail will end with the mail's physical path, which Sylpheed will remove
 * after having downloaded the complete mail. msg->partial_recv will equal
 * 2 (POP3_MUST_COMPLETE_RECV).
 * When updating this header to POP3_PARTIAL_DLOAD_DELE, the uidl line of
 * this mail will be 0 (POP3_TOTALLY_RECEIVED), which will let Sylpheed delete
 * this mail from the server as soon as the leave_time preference specifies.
 */
 
int pop3_mark_for_delete(const gchar *server, const gchar *login, 
			 const gchar *muidl, const gchar *file, 
			 const gchar *folder_item_id, guint msgnum)
{
	return pop3_uidl_mark_mail(server, login, muidl, file, folder_item_id, 
		msgnum, POP3_PARTIAL_DLOAD_DELE);
}

int pop3_mark_for_download(const gchar *server, const gchar *login, 
			   const gchar *muidl, const gchar *file, 
			   const gchar *folder_item_id, guint msgnum)
{
	return pop3_uidl_mark_mail(server, login, muidl, file, folder_item_id, 
		msgnum, POP3_PARTIAL_DLOAD_DLOAD);
}

int pop3_unmark(const gchar *server, const gchar *login, 
		const gchar *muidl, const gchar *file, 
		const gchar *folder_item_id, guint msgnum)
{
	return pop3_uidl_mark_mail(server, login, muidl, file, folder_item_id, 
		msgnum, POP3_PARTIAL_DLOAD_UNKN);
}

static void pop3_delete_old_partial(const gchar *file) 
{
	gchar *id = strdup(file);
	gchar *snum = strrchr(file, ':');
	int num = 0;
	FolderItem *item = NULL;

	debug_print("too big message updated,should remove %s\n", file);

	if (snum) {
		snum++;
	} else {
		g_free(id);
		return; /* not a real problem */
	}

	num = atoi(snum);

	if (strrchr(id, ':'))
		*(strrchr(id, ':'))='\0';

	item = folder_find_item_from_identifier(id);
	if (item) {
		debug_print("removing %d in %s\n", num, id);
		folder_item_remove_msg(item, num);
	} 
	g_free(id);
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
		if (msg->uidl && msg->received && !msg->deleted) {
			fprintf(fp, "%s\t%ld\t%d\n", 
				msg->uidl, msg->recv_time, msg->partial_recv);
		}
	}

	if (fclose(fp) == EOF) FILE_OP_ERROR(path, "fclose");
	g_free(path);

	return 0;
}

static gint pop3_write_msg_to_file(const gchar *file, const gchar *data,
				   guint len, const gchar *prefix)
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

	if (prefix != NULL) {
		fprintf(fp, prefix);
		fprintf(fp, "\n");
	}
	
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

		if (prev - data < len - 1 && *prev == '.' && *(prev + 1) == '.')
			prev++;

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
		    msg->partial_recv == POP3_TOTALLY_RECEIVED &&
		    session->current_time - msg->recv_time >=
		    ac->msg_leave_time * 24 * 60 * 60) {
			log_message
				(_("POP3: Deleting expired message "
				   "%d\n"), session->cur_msg);
			pop3_delete_send(session);
			return POP3_DELETE;
		}

		if (size_limit_over) {
			if (!msg->received && msg->partial_recv != 
			    POP3_MUST_COMPLETE_RECV) {
				pop3_top_send(session, ac->size_limit);
				return POP3_TOP;
			} else if (msg->partial_recv == POP3_MUST_COMPLETE_RECV)
				break;

			log_message
				(_("POP3: Skipping message %d (%d bytes)\n"),
				   session->cur_msg, size);
		}
		
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

static Pop3ErrorValue pop3_ok(Pop3Session *session, const gchar *msg)
{
	Pop3ErrorValue ok;

	log_print("POP3< %s\n", msg);

	if (!strncmp(msg, "+OK", 3))
		ok = PS_SUCCESS;
	else if (!strncmp(msg, "-ERR", 4)) {
		if (strstr(msg + 4, "lock") ||
		    strstr(msg + 4, "Lock") ||
		    strstr(msg + 4, "LOCK") ||
		    strstr(msg + 4, "wait")) {
			log_warning(_("mailbox is locked\n"));
			ok = PS_LOCKBUSY;
		} else if (strcasestr(msg + 4, "timeout")) {
			log_warning(_("session timeout\n"));
			ok = PS_ERROR;
		} else {
			switch (session->state) {
#if USE_OPENSSL
			case POP3_STLS:
				log_warning(_("can't start TLS session\n"));
				ok = PS_ERROR;
				break;
#endif
			case POP3_GETAUTH_USER:
			case POP3_GETAUTH_PASS:
			case POP3_GETAUTH_APOP:
				log_warning(_("error occurred on authentication\n"));
				ok = PS_AUTHFAIL;
				break;
			case POP3_GETRANGE_LAST:
			case POP3_GETRANGE_UIDL:
			case POP3_TOP:
				log_warning(_("command not supported\n"));
				ok = PS_NOTSUPPORTED;
				break;
				
			default:
				log_warning(_("error occurred on POP3 session\n"));
				ok = PS_ERROR;
			}
		}

		g_free(session->error_msg);
		session->error_msg = g_strdup(msg);
		fprintf(stderr, "POP3: %s\n", msg);
	} else
		ok = PS_PROTOCOL;

	session->error_val = ok;
	return ok;
}

static gint pop3_session_recv_msg(Session *session, const gchar *msg)
{
	Pop3Session *pop3_session = POP3_SESSION(session);
	Pop3ErrorValue val = PS_SUCCESS;
	const guchar *body;

	body = msg;
	if (pop3_session->state != POP3_GETRANGE_UIDL_RECV &&
	    pop3_session->state != POP3_GETSIZE_LIST_RECV) {
		val = pop3_ok(pop3_session, msg);
		if (val != PS_SUCCESS) {
			if (val != PS_NOTSUPPORTED) {
				pop3_session->state = POP3_ERROR;
				return -1;
			}
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
		if (!pop3_session->pop_before_smtp)
			pop3_getrange_stat_send(pop3_session);
		else
			pop3_logout_send(pop3_session);
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
		if (val == PS_NOTSUPPORTED)
			pop3_session->error_val = PS_SUCCESS;
		else if (pop3_getrange_last_recv(pop3_session, body) < 0)
			return -1;
		if (pop3_session->cur_msg > 0)
			pop3_getsize_list_send(pop3_session);
		else
			pop3_logout_send(pop3_session);
		break;
	case POP3_GETRANGE_UIDL:
		if (val == PS_NOTSUPPORTED) {
			pop3_session->error_val = PS_SUCCESS;
			pop3_getrange_last_send(pop3_session);
		} else {
			pop3_session->state = POP3_GETRANGE_UIDL_RECV;
			session_recv_data(session, 0, ".\r\n");
		}
		break;
	case POP3_GETSIZE_LIST:
		pop3_session->state = POP3_GETSIZE_LIST_RECV;
		session_recv_data(session, 0, ".\r\n");
		break;
	case POP3_RETR:
		pop3_session->state = POP3_RETR_RECV;
		session_recv_data(session, 0, ".\r\n");
		break;
	case POP3_TOP:
		if (val == PS_NOTSUPPORTED) {
			pop3_session->error_val = PS_SUCCESS;
		} else {
			pop3_session->state = POP3_TOP_RECV;
			session_recv_data(session, 0, ".\r\n");
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
	Pop3ErrorValue val = PS_SUCCESS;

	switch (pop3_session->state) {
	case POP3_GETRANGE_UIDL_RECV:
		val = pop3_getrange_uidl_recv(pop3_session, data, len);
		if (val == PS_SUCCESS) {
			if (pop3_session->new_msg_exist)
				pop3_getsize_list_send(pop3_session);
			else
				pop3_logout_send(pop3_session);
		} else
			return -1;
		break;
	case POP3_GETSIZE_LIST_RECV:
		val = pop3_getsize_list_recv(pop3_session, data, len);
		if (val == PS_SUCCESS) {
			if (pop3_lookup_next(pop3_session) == POP3_ERROR)
				return -1;
		} else
			return -1;
		break;
	case POP3_RETR_RECV:
		if (pop3_retr_recv(pop3_session, data, len) < 0)
			return -1;

		if (pop3_session->ac_prefs->rmmail &&
		    pop3_session->ac_prefs->msg_leave_time == 0 &&
		    pop3_session->msg[pop3_session->cur_msg].recv_time
		    != RECV_TIME_KEEP)
			pop3_delete_send(pop3_session);
		else if (pop3_session->cur_msg == pop3_session->count)
			pop3_logout_send(pop3_session);
		else {
			pop3_session->cur_msg++;
			if (pop3_lookup_next(pop3_session) == POP3_ERROR)
				return -1;
		}
		break;
	case POP3_TOP_RECV:
		if (pop3_top_recv(pop3_session, data, len) < 0)
			return -1;

		if (pop3_session->cur_msg == pop3_session->count)
			pop3_logout_send(pop3_session);
		else {
			pop3_session->cur_msg++;
			if (pop3_lookup_next(pop3_session) == POP3_ERROR)
				return -1;
		}
		break;
	case POP3_TOP:
		log_warning(_("TOP command unsupported\n"));
		if (pop3_session->cur_msg == pop3_session->count)
			pop3_logout_send(pop3_session);
		else {
			pop3_session->cur_msg++;
			if (pop3_lookup_next(pop3_session) == POP3_ERROR)
				return -1;
		}
		break;
	case POP3_ERROR:
	default:
		return -1;
	}

	return 0;
}
