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
 *
 * TODO (Win32): Sync rev. 1.21 (2003/07/16 11:53:42)
 * 	- uidl rx hangs waiting for next data (if data>bufsize)
 * 	- no callback after rx'ing list reply
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef WIN32
#include <fcntl.h>
#include <process.h>
#else
#include <unistd.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <sys/time.h>
#endif
#include <errno.h>

#include "session.h"
#include "utils.h"

static gint session_connect_cb		(SockInfo	*sock,
					 gpointer	 data);
static gint session_close		(Session	*session);

static gboolean session_read_msg_cb	(SockInfo	*source,
					 GIOCondition	 condition,
					 gpointer	 data);
static gboolean session_read_data_cb	(SockInfo	*source,
					 GIOCondition	 condition,
					 gpointer	 data);
static gboolean session_write_msg_cb	(SockInfo	*source,
					 GIOCondition	 condition,
					 gpointer	 data);
static gboolean session_write_data_cb	(SockInfo	*source,
					 GIOCondition	 condition,
					 gpointer	 data);


void session_init(Session *session)
{
	session->type = SESSION_UNKNOWN;
	session->sock = NULL;
	session->server = NULL;
	session->port = 0;
#if USE_OPENSSL
	session->ssl_type = SSL_NONE;
#endif
	session->state = SESSION_READY;
	session->last_access_time = time(NULL);

	gettimeofday(&session->tv_prev, NULL);

	session->conn_id = 0;

	session->io_tag = 0;

	session->read_buf = g_string_sized_new(1024);
	session->read_data_buf = g_byte_array_new();

	session->write_buf = NULL;
	session->write_buf_p = NULL;
	session->write_buf_len = 0;

	session->data = NULL;
}

/*!
 *\brief	Set up parent and child process
 *		Childloop: Read commands from parent,
 *		send to server, get answer, pass to parent
 *
 *\param	session Contains session information
 *		server to connect to
 *		port to connect to
 *
 *\return	 0 : success
 *		-1 : pipe / fork errors (parent)
 *		 1 : connection error (child)
 */
gint session_connect(Session *session, const gchar *server, gushort port)
{
	session->server = g_strdup(server);
	session->port = port;

	session->conn_id = sock_connect_async(server, port, session_connect_cb,
					      session);
	if (session->conn_id < 0) {
		g_warning("can't connect to server.");
		session_close(session);
		return -1;
	}

	return 0;
}

static gint session_connect_cb(SockInfo *sock, gpointer data)
{
	Session *session = SESSION(data);

	session->conn_id = 0;

	if (!sock) {
		g_warning("can't connect to server.");
		session->state = SESSION_ERROR;
		return -1;
	}

	session->sock = sock;

#if USE_OPENSSL
	if (session->ssl_type == SSL_TUNNEL) {
		sock_set_nonblocking_mode(sock, FALSE);
		if (!ssl_init_socket(sock)) {
			g_warning("can't initialize SSL.");
			session->state = SESSION_ERROR;
			return -1;
		}
	}
#endif

	sock_set_nonblocking_mode(sock, TRUE);

	debug_print("session (%p): connected\n", session);

	session->state = SESSION_RECV;
	session->io_tag = sock_add_watch(session->sock, G_IO_IN,
					 session_read_msg_cb,
					 session);

	return 0;
}
#ifdef WIN32
#undef _exit
#endif

/*!
 *\brief	child and parent: send DISCONNECT message to other process
 *
 *\param	session Contains session information
 *
 *\return	 0 : success
 */
gint session_disconnect(Session *session)
{
	session_close(session);
	return 0;
}

/*!
 *\brief	parent ?
 *
 *\param	session Contains session information
 */
void session_destroy(Session *session)
{
	g_return_if_fail(session != NULL);
	g_return_if_fail(session->destroy != NULL);

	session_close(session);
	session->destroy(session);
	g_free(session->server);
	g_string_free(session->read_buf, TRUE);
	g_byte_array_free(session->read_data_buf, TRUE);
	g_free(session->read_data_terminator);
	g_free(session->write_buf);

	debug_print("session (%p): destroyed\n", session);

	g_free(session);
}

gboolean session_is_connected(Session *session)
{
	return (session->state == SESSION_READY ||
		session->state == SESSION_SEND ||
		session->state == SESSION_RECV);
}

void session_set_recv_message_notify(Session *session,
				     RecvMsgNotify notify_func, gpointer data)
{
	session->recv_msg_notify = notify_func;
	session->recv_msg_notify_data = data;
}

void session_set_recv_data_progressive_notify
					(Session *session,
					 RecvDataProgressiveNotify notify_func,
					 gpointer data)
{
	session->recv_data_progressive_notify = notify_func,
	session->recv_data_progressive_notify_data = data;
}

void session_set_recv_data_notify(Session *session, RecvDataNotify notify_func,
				  gpointer data)
{
	session->recv_data_notify = notify_func;
	session->recv_data_notify_data = data;
}

void session_set_send_data_progressive_notify
					(Session *session,
					 SendDataProgressiveNotify notify_func,
					 gpointer data)
{
	session->send_data_progressive_notify = notify_func;
	session->send_data_progressive_notify_data = data;
}

void session_set_send_data_notify(Session *session, SendDataNotify notify_func,
				  gpointer data)
{
	session->send_data_notify = notify_func;
	session->send_data_notify_data = data;
}

/*!
 *\brief	child and parent cleanup (child closes first)
 *
 *\param	session Contains session information
 *
 *\return	 0 : success
 */
static gint session_close(Session *session)
{
	g_return_val_if_fail(session != NULL, -1);

	if (session->conn_id > 0) {
		sock_connect_async_cancel(session->conn_id);
		session->conn_id = 0;
	}

	if (session->io_tag > 0) {
		g_source_remove(session->io_tag);
		session->io_tag = 0;
	}

	if (session->sock) {
		sock_close(session->sock);
		session->sock = NULL;
		session->state = SESSION_DISCONNECTED;
	}

	debug_print("session (%p): closed\n", session);

	return 0;
}

#if USE_OPENSSL
gint session_start_tls(Session *session)
{
	gboolean nb_mode;

	nb_mode = sock_is_nonblocking_mode(session->sock);

	if (nb_mode)
		sock_set_nonblocking_mode(session->sock, FALSE);

	if (!ssl_init_socket_with_method(session->sock, SSL_METHOD_TLSv1)) {
		g_warning("can't start TLS session.\n");
		if (nb_mode)
			sock_set_nonblocking_mode(session->sock, TRUE);
		return -1;
	}

	if (nb_mode)
		sock_set_nonblocking_mode(session->sock, TRUE);

	return 0;
}
#endif

gint session_send_msg(Session *session, SessionMsgType type, const gchar *msg)
{
	gboolean ret;

	g_return_val_if_fail(session->write_buf == NULL, -1);
	g_return_val_if_fail(msg != NULL, -1);
	g_return_val_if_fail(msg[0] != '\0', -1);

	session->state = SESSION_SEND;
	session->write_buf = g_strconcat(msg, "\r\n", NULL);
	session->write_buf_p = session->write_buf;
	session->write_buf_len = strlen(msg) + 2;

	ret = session_write_msg_cb(session->sock, G_IO_OUT, session);

	if (ret == TRUE)
		session->io_tag = sock_add_watch(session->sock, G_IO_OUT,
						 session_write_msg_cb, session);
	else if (session->state == SESSION_ERROR)
		return -1;

	return 0;
}

gint session_recv_msg(Session *session)
{
	g_return_val_if_fail(session->read_buf->len == 0, -1);

	session->state = SESSION_RECV;

	session->io_tag = sock_add_watch(session->sock, G_IO_IN,
					 session_read_msg_cb, session);

	return 0;
}

/*!
 *\brief	parent (child?): send data to other process
 *
 *\param	session Contains session information
 *		data Data to send
 *		size Bytes to send
 *
 *\return	 0 : success
 *		-1 : error
 */
gint session_send_data(Session *session, const guchar *data, guint size)
{
	gboolean ret;

	g_return_val_if_fail(session->write_buf == NULL, -1);
	g_return_val_if_fail(data != NULL, -1);
	g_return_val_if_fail(size != 0, -1);

	session->state = SESSION_SEND;

	session->write_buf = g_malloc(size);
	session->write_buf_p = session->write_buf;
	memcpy(session->write_buf, data, size);
	session->write_buf_len = size;
	gettimeofday(&session->tv_prev, NULL);

	ret = session_write_data_cb(session->sock, G_IO_OUT, session);

	if (ret == TRUE)
		session->io_tag = sock_add_watch(session->sock, G_IO_OUT,
						 session_write_data_cb,
						 session);
	else if (session->state == SESSION_ERROR)
		return -1;

	return 0;
}

gint session_recv_data(Session *session, guint size, const gchar *terminator)
{
	g_return_val_if_fail(session->read_data_buf->len == 0, -1);

	session->state = SESSION_RECV;

	g_free(session->read_data_terminator);
	session->read_data_terminator = g_strdup(terminator);
	gettimeofday(&session->tv_prev, NULL);

	session->io_tag = sock_add_watch(session->sock, G_IO_IN,
					 session_read_data_cb, session);

	return 0;
}

static gboolean session_read_msg_cb(SockInfo *source, GIOCondition condition,
				    gpointer data)
{
	Session *session = SESSION(data);
	gchar buf[SESSION_BUFFSIZE];
	gint read_len;
	gint to_read_len;
	gchar *newline;
	gchar *msg;
	gint ret;

#ifdef WIN32 /* XXX:tm nonblock */
	g_return_val_if_fail(condition == G_IO_IN, TRUE); /* Keep watching... cond==0 sometimes(if data follows)? */
	Sleep(0);
#else
	g_return_val_if_fail(condition == G_IO_IN, FALSE);
#endif

	read_len = sock_peek(session->sock, buf, sizeof(buf) - 1);

	if (read_len < 0) {
		switch (errno) {
		case EAGAIN:
			return TRUE;
		default:
			g_warning("sock_peek: %s\n", g_strerror(errno));
			session->state = SESSION_ERROR;
			return FALSE;
		}
	}

	if ((newline = memchr(buf, '\n', read_len)) != NULL)
		to_read_len = newline - buf + 1;
	else
		to_read_len = read_len;

	read_len = sock_read(session->sock, buf, to_read_len);

	/* this should always succeed */
	if (read_len < 0) {
		g_warning("sock_read: %s\n", g_strerror(errno));
		session->state = SESSION_ERROR;
		return FALSE;
	}

	buf[read_len] = '\0';

	/* incomplete read */
	if (read_len == 0 || buf[read_len - 1] != '\n') {
		g_string_append(session->read_buf, buf);
		return TRUE;
	}

	/* complete */
	strretchomp(buf);
	g_string_append(session->read_buf, buf);

	if (session->io_tag > 0) {
		g_source_remove(session->io_tag);
		session->io_tag = 0;
	}

	/* callback */
	msg = g_strdup(session->read_buf->str);
	g_string_truncate(session->read_buf, 0);

	ret = session->recv_msg(session, msg);
	session->recv_msg_notify(session, msg, session->recv_msg_notify_data);

	g_free(msg);

	if (ret < 0)
		session->state = SESSION_ERROR;

	return FALSE;
}

static gboolean session_read_data_cb(SockInfo *source, GIOCondition condition,
				     gpointer data)
{
	Session *session = SESSION(data);
	gchar buf[SESSION_BUFFSIZE];
	GByteArray *data_buf;
	gint read_len;
	gint terminator_len;
	gboolean complete = FALSE;
	guint data_len;
	gint ret;

#ifdef WIN32 /* XXX:tm nonblock */
	g_return_val_if_fail(condition == G_IO_IN, TRUE);
#endif
	g_return_val_if_fail(condition == G_IO_IN, FALSE);

	read_len = sock_read(session->sock, buf, sizeof(buf));
	if (read_len < 0) {
		switch (errno) {
		case EAGAIN:
			return TRUE;
		default:
			g_warning("sock_read: %s\n", g_strerror(errno));
			session->state = SESSION_ERROR;
			return FALSE;
		}
	}

	data_buf = session->read_data_buf;

	g_byte_array_append(data_buf, buf, read_len);
	terminator_len = strlen(session->read_data_terminator);

	/* check if data is terminated */
	if (read_len > 0 && data_buf->len >= terminator_len) {
		if (memcmp(data_buf->data, session->read_data_terminator,
			   terminator_len) == 0)
			complete = TRUE;
		else if (data_buf->len >= terminator_len + 2 &&
			 memcmp(data_buf->data + data_buf->len -
				(terminator_len + 2), "\r\n", 2) == 0 &&
			 memcmp(data_buf->data + data_buf->len -
				terminator_len, session->read_data_terminator,
				terminator_len) == 0)
			complete = TRUE;
	}

	/* incomplete read */
	if (!complete) {
		struct timeval tv_cur;

		gettimeofday(&tv_cur, NULL);
		if (tv_cur.tv_sec - session->tv_prev.tv_sec > 0 ||
		    tv_cur.tv_usec - session->tv_prev.tv_usec >
		    UI_REFRESH_INTERVAL) {
			session->recv_data_progressive_notify
				(session, data_buf->len, 0,
				 session->recv_data_progressive_notify_data);
			gettimeofday(&session->tv_prev, NULL);
		}
		return TRUE;
	}

	/* complete */
	if (session->io_tag > 0) {
		g_source_remove(session->io_tag);
		session->io_tag = 0;
	}

	data_len = data_buf->len - terminator_len;

	/* callback */
	ret = session->recv_data_finished(session, (gchar *)data_buf->data,
					  data_len);

	g_byte_array_set_size(data_buf, 0);

	session->recv_data_notify(session, data_len,
				  session->recv_data_notify_data);

	if (ret < 0)
		session->state = SESSION_ERROR;

	return FALSE;
}

static gint session_write_buf(Session *session)
{
	gint write_len;
	gint to_write_len;

	g_return_val_if_fail(session->write_buf != NULL, -1);
	g_return_val_if_fail(session->write_buf_p != NULL, -1);
	g_return_val_if_fail(session->write_buf_len > 0, -1);

	to_write_len = session->write_buf_len -
		(session->write_buf_p - session->write_buf);
	to_write_len = MIN(to_write_len, SESSION_BUFFSIZE);

	write_len = sock_write(session->sock, session->write_buf_p,
			       to_write_len);

	if (write_len < 0) {
		switch (errno) {
		case EAGAIN:
			write_len = 0;
			break;
		default:
			g_warning("sock_write: %s\n", g_strerror(errno));
			session->state = SESSION_ERROR;
			return -1;
		}
	}

	/* incomplete write */
	if (session->write_buf_p - session->write_buf + write_len <
	    session->write_buf_len) {
		session->write_buf_p += write_len;
		return 1;
	}

	g_free(session->write_buf);
	session->write_buf = NULL;
	session->write_buf_p = NULL;
	session->write_buf_len = 0;

	return 0;
}

static gboolean session_write_msg_cb(SockInfo *source, GIOCondition condition,
				     gpointer data)
{
	Session *session = SESSION(data);
	gint ret;

	g_return_val_if_fail(condition == G_IO_OUT, FALSE);
	g_return_val_if_fail(session->write_buf != NULL, FALSE);
	g_return_val_if_fail(session->write_buf_p != NULL, FALSE);
	g_return_val_if_fail(session->write_buf_len > 0, FALSE);

	ret = session_write_buf(session);

	if (ret < 0) {
		session->state = SESSION_ERROR;
		return FALSE;
	} else if (ret > 0)
		return TRUE;

	if (session->io_tag > 0) {
		g_source_remove(session->io_tag);
		session->io_tag = 0;
	}

	session_recv_msg(session);

	return FALSE;
}

static gboolean session_write_data_cb(SockInfo *source,
				      GIOCondition condition, gpointer data)
{
	Session *session = SESSION(data);
	guint write_buf_len;
	gint ret;

	g_return_val_if_fail(condition == G_IO_OUT, FALSE);
	g_return_val_if_fail(session->write_buf != NULL, FALSE);
	g_return_val_if_fail(session->write_buf_p != NULL, FALSE);
	g_return_val_if_fail(session->write_buf_len > 0, FALSE);

	write_buf_len = session->write_buf_len;

	ret = session_write_buf(session);

	if (ret < 0) {
		session->state = SESSION_ERROR;
		return FALSE;
	} else if (ret > 0) {
		struct timeval tv_cur;

		gettimeofday(&tv_cur, NULL);
		if (tv_cur.tv_sec - session->tv_prev.tv_sec > 0 ||
		    tv_cur.tv_usec - session->tv_prev.tv_usec >
		    UI_REFRESH_INTERVAL) {
			session->send_data_progressive_notify
				(session,
				 session->write_buf_p - session->write_buf,
				 write_buf_len,
				 session->send_data_progressive_notify_data);
			gettimeofday(&session->tv_prev, NULL);
		}
		return TRUE;
	}

	if (session->io_tag > 0) {
		g_source_remove(session->io_tag);
		session->io_tag = 0;
	}

	/* callback */
	ret = session->send_data_finished(session, write_buf_len);
	session->send_data_notify(session, write_buf_len,
				  session->send_data_notify_data);

	return FALSE;
}
