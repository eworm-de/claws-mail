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

#include "defs.h"

#include <glib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <sys/time.h>

#include "session.h"
#include "utils.h"

static gint session_close		(Session	*session);

static gchar *session_recv_msg		(Session	*session);

static guchar *session_read_data	(Session	*session,
					 guint		 size);

static gint session_send_data_to_sock		(Session	*session,
						 const guchar	*data,
						 guint		 size);
static guchar *session_recv_data_from_sock	(Session	*session,
						 guint		 size);

static guchar *session_recv_data_from_sock_unescape	(Session *session,
							 guint	  size,
							 guint	 *actual_size);

gboolean session_parent_input_cb	(GIOChannel	*source,
					 GIOCondition	 condition,
					 gpointer	 data);

gboolean session_child_input		(Session	*session);


/*!
 *\brief	init session members to zero
 *
 *\param	session to be initialized
 */
void session_init(Session *session)
{
	session->type = 0;
	session->sock = NULL;

	session->server = NULL;
	session->port = 0;
	session->state = SESSION_READY;
	session->last_access_time = time(NULL);
	session->data = NULL;

	session->read_ch = NULL;
	session->write_ch = NULL;
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
	pid_t pid;
	gint pipe_fds1[2], pipe_fds2[2];
	SockInfo *sock;
	gchar *str;

	session->server = g_strdup(server);
	session->port = port;

	if (pipe(pipe_fds1) < 0) {
		perror("pipe");
		return -1;
	}
	if (pipe(pipe_fds2) < 0) {
		perror("pipe");
		close(pipe_fds1[0]);
		close(pipe_fds1[1]);
		return -1;
	}

	if ((pid = fork()) < 0) {
		perror("fork");
		return -1;
	}

	if (pid != 0) {
		session->child_pid = pid;
		session->read_ch = g_io_channel_unix_new(pipe_fds2[0]);
		session->write_ch = g_io_channel_unix_new(pipe_fds1[1]);
		close(pipe_fds1[0]);
		close(pipe_fds2[1]);
		session->read_tag = g_io_add_watch(session->read_ch, G_IO_IN,
						   session_parent_input_cb,
						   session);
		return 0;
	}

	/* child process */

	session->read_ch = g_io_channel_unix_new(pipe_fds1[0]);
	session->write_ch = g_io_channel_unix_new(pipe_fds2[1]);
	close(pipe_fds1[1]);
	close(pipe_fds2[0]);

	g_print("child: connecting to %s:%d ...\n", server, port);

	if ((sock = sock_connect(server, port)) == NULL) {
		session_send_msg(session, SESSION_MSG_ERROR,
				 "can't connect to server.");
		session_close(session);
		_exit(1);
	}

#if USE_OPENSSL
	if (session->ssl_type == SSL_TUNNEL && !ssl_init_socket(sock)) {
		session_send_msg(session, SESSION_MSG_ERROR,
				 "can't initialize SSL.");
		session_close(session);
		_exit(1);
	}
#endif

	g_print("child: connected\n");

	session->sock = sock;
	session->state = SESSION_RECV;

	if ((str = sock_getline(sock)) == NULL) {
		session_send_msg(session, SESSION_MSG_ERROR,
				 "can't get server response.");
		session_close(session);
		_exit(1);
	}
	strretchomp(str);
	session_send_msg(session, SESSION_MSG_NORMAL, str);
	g_free(str);

	while (session_child_input(session) == TRUE)
		;

	session_close(session);

	g_print("child: disconnected\n");

	_exit(0);
}

/*!
 *\brief	child and parent: send DISCONNECT message to other process
 *
 *\param	session Contains session information
 *
 *\return	 0 : success
 */
gint session_disconnect(Session *session)
{
	g_print("%s: session_disconnect()\n", session->child_pid == 0 ? "child" : "parent");
	session_send_msg(session, SESSION_MSG_CONTROL, "DISCONNECT");
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

	g_print("session_destroy()\n");
	session_close(session);
	g_print("$$$ session_destroy() session->destroy called\n");
	session->destroy(session);
	g_print("$$$ session_destroy() freeing session server\n");
	g_free(session->server);
	g_print("$$$ session_destroy() freeing session\n");
	g_free(session);
	g_print("$$$ session_destroy() done\n");
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

	g_print("%s: session_close()\n", session->child_pid == 0 ? "child" : "parent");

	if (session->read_tag > 0) {
		g_source_remove(session->read_tag);
		session->read_tag = 0;
	}
	
	g_print("$$$ %s: closing read channel\n", session->child_pid == 0 ? "child" : "parent");
	if (session->read_ch) {
		g_io_channel_close(session->read_ch);
		g_io_channel_unref(session->read_ch);
		session->read_ch = NULL;
	}
	
	g_print("$$$ %s: closing write channel\n", session->child_pid == 0 ? "child" : "parent");
	if (session->write_ch) {
		g_io_channel_close(session->write_ch);
		g_io_channel_unref(session->write_ch);
		session->write_ch = NULL;
	}

	g_print("$$$ %s: closing socket\n", session->child_pid == 0 ? "child" : "parent");
	if (session->sock) {
		sock_close(session->sock);
		session->sock = NULL;
		session->state = SESSION_DISCONNECTED;
	}

	if (session->child_pid) {
		g_print("$$$ %s: closing child\n", session->child_pid == 0 ? "child" : "parent");
		kill(session->child_pid, SIGTERM);
		waitpid(session->child_pid, NULL, 0);
		session->child_pid = 0;
	}

	g_print("$$$ %s: exiting session_close\n", session->child_pid == 0 ? "child" : "parent");
	return 0;
}

/*!
 *\brief	child and parent: send control message to other process
 *
 *\param	session Contains session information
 *		type Kind of data (commands or message data)
 *		msg Data
 *
 *\return	 0 : success
 *		-1 : error
 */
gint session_send_msg(Session *session, SessionMsgType type, const gchar *msg)
{
	gchar *prefix;
	gchar *str;
	guint size;
	guint bytes_written;

	switch (type) {
	case SESSION_MSG_NORMAL:
		prefix = "MESSAGE"; break;
	case SESSION_MSG_SEND_DATA:
		prefix = "SENDDATA"; break;
	case SESSION_MSG_RECV_DATA:
		prefix = "RECVDATA"; break;
	case SESSION_MSG_CONTROL:
		prefix = "CONTROL"; break;
	case SESSION_MSG_ERROR:
		prefix = "ERROR"; break;
	default:
		return -1;
	}

	str = g_strdup_printf("%s %s\n", prefix, msg);
	/* g_print("%s: sending message: %s", session->child_pid == 0 ? "child" : "parent", str); */
	size = strlen(str);

	while (size > 0) {
		if (g_io_channel_write(session->write_ch, str, size,
				       &bytes_written)
		    != G_IO_ERROR_NONE || bytes_written == 0) {
			g_warning("%s: sending message failed.\n",
				  session->child_pid == 0 ? "child" : "parent");
			return -1;
		}
		size -= bytes_written;
	}

	return 0;
}

/*!
 *\brief	child and parent receive function
 *
 *\param	session Contains session information
 *
 *\return	Message read by current session
 */
static gchar *session_recv_msg(Session *session)
{
	gchar buf[BUFFSIZE];
	gchar *str = NULL;
	guint size = 1;
	guint bytes_read;

	for (;;) {
		if (g_io_channel_read(session->read_ch, buf, sizeof(buf) - 1,
				      &bytes_read)
		    != G_IO_ERROR_NONE || bytes_read == 0) {
			g_warning("%s: receiving message failed.\n",
				  session->child_pid == 0 ? "child" : "parent");
			g_free(str);
			str = NULL;
			break;
		}

		size += bytes_read;
		buf[bytes_read] = '\0';

		if (!str)
			str = g_strdup(buf);
		else {
			str = g_realloc(str, size);
			strcat(str, buf);
		}
		if (str[size - 2] == '\n') {
			str[size - 2] = '\0';

			g_print("%s: received message: %s\n", session->child_pid == 0 ? "child" : "parent", str);

			break;
		}
	}

	return str;
}

#if USE_OPENSSL
gint session_start_tls(Session *session)
{
	gchar *ctl_msg;

	session_send_msg(session, SESSION_MSG_CONTROL, "STARTTLS");
	ctl_msg = session_recv_msg(session);
	if (!ctl_msg || strcmp(ctl_msg, "CONTROL STARTTLSOK") != 0) {
		g_free(ctl_msg);
		return -1;
	}
	g_free(ctl_msg);

	return 0;
}
#endif

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
	gchar *msg;
	guint bytes_written;
	GIOError err;

	session_send_msg(session, SESSION_MSG_SEND_DATA, itos(size));
	if ((msg = session_recv_msg(session)) == NULL)
		return -1;
	g_free(msg);

	while (size > 0) {
		if ((err = g_io_channel_write(session->write_ch, (guchar *)data,
					      size, &bytes_written))
		    != G_IO_ERROR_NONE || bytes_written == 0) {
			g_warning("%s: sending data failed: %d\n",
				  session->child_pid == 0 ? "child" : "parent",
				  err);
			return -1;
		}
		size -= bytes_written;
		g_print("%s: sent %d bytes of data\n", session->child_pid == 0 ? "child" : "parent", bytes_written);
	}

	return 0;
}

gint session_recv_data(Session *session, guint size, gboolean unescape_dot)
{
	if (unescape_dot) {
		gchar buf[BUFFSIZE];

		g_snprintf(buf, sizeof(buf), "%d UNESCAPE", size);
		session_send_msg(session, SESSION_MSG_RECV_DATA, buf);
	} else
		session_send_msg(session, SESSION_MSG_RECV_DATA, itos(size));
	return 0;
}

/*!
 *\brief	child (parent?): read data from other process
 *
 *\param	session Contains session information
 *		size Bytes to read
 *
 *\return	data read from session
 */
static guchar *session_read_data(Session *session, guint size)
{
	guchar *data;
	guchar *cur;
	guint bytes_read;
	GIOError err;

	cur = data = g_malloc(size);

	while (size > 0) {
		if ((err = g_io_channel_read(session->read_ch, cur, size,
					     &bytes_read))
		    != G_IO_ERROR_NONE || bytes_read == 0) {
			g_warning("%s: reading data failed: %d\n",
				  session->child_pid == 0 ? "child" : "parent",
				  err);
			g_free(data);
			return NULL;
		}
		size -= bytes_read;
		cur += bytes_read;
		g_print("%s: received %d bytes of data\n", session->child_pid == 0 ? "child" : "parent", bytes_read);
	}

	return data;
}

#define MAX_CHUNK_SIZE 4096

/*!
 *\brief	child: Send session data to server
 *
 *\param	session Contains session information
 *		data Data to send to server
 *		size Bytes to send
 *
 *\return	 0 : success
 *		-1 : error
 */
static gint session_send_data_to_sock(Session *session, const guchar *data,
				      guint size)
{
	const guchar *cur = data;
	gint bytes_written;
	gint total_write_len = 0;
	guint left = size;
	gchar buf[BUFFSIZE];
	gchar *msg;
	struct timeval tv_prev, tv_cur;

	gettimeofday(&tv_prev, NULL);

	while (1) {
		bytes_written = sock_write(session->sock, cur,
					   MIN(left, MAX_CHUNK_SIZE));
		if (bytes_written <= 0)
			return -1;
		left -= bytes_written;
		cur += bytes_written;
		total_write_len += bytes_written;
		if (left == 0)
			break;

		gettimeofday(&tv_cur, NULL);
		if (tv_cur.tv_sec - tv_prev.tv_sec > 0 ||
		    tv_cur.tv_usec - tv_prev.tv_usec > UI_REFRESH_INTERVAL) {
			g_snprintf(buf, sizeof(buf), "DATASENDINPROG %d %d",
				   total_write_len, size);
			session_send_msg(session, SESSION_MSG_CONTROL, buf);
			if ((msg = session_recv_msg(session)) == NULL)
				return -1;
			g_free(msg);
			gettimeofday(&tv_prev, NULL);
		}
	}

	return 0;
}

/*!
 *\brief	child: Read answer/data from server
 *
 *\param	session Contains session information
 *		size Max bytes to receive
 *
 *\return	Server answer
 */
static guchar *session_recv_data_from_sock(Session *session, guint size)
{
	guchar *data;
	guchar *cur;
	gint bytes_read;
	gint total_read_len = 0;
	guint left = size;
	gchar buf[BUFFSIZE];
	gchar *msg;
	struct timeval tv_prev, tv_cur;

	gettimeofday(&tv_prev, NULL);

	cur = data = g_malloc(size);

	while (1) {
		bytes_read = sock_read(session->sock, cur, left);
		if (bytes_read <= 0) {
			g_free(data);
			return NULL;
		}
		g_print("child: received %d bytes of data from sock\n", bytes_read);
		left -= bytes_read;
		cur += bytes_read;
		total_read_len += bytes_read;
		if (left == 0)
			break;

		gettimeofday(&tv_cur, NULL);
		if (tv_cur.tv_sec - tv_prev.tv_sec > 0 ||
		    tv_cur.tv_usec - tv_prev.tv_usec > UI_REFRESH_INTERVAL) {
			g_snprintf(buf, sizeof(buf), "DATARECVINPROG %d %d",
				   total_read_len, size);
			session_send_msg(session, SESSION_MSG_CONTROL, buf);
			if ((msg = session_recv_msg(session)) == NULL) {
				g_free(data);
				return NULL;
			}
			g_free(msg);
			gettimeofday(&tv_prev, NULL);
		}
	}

	return data;
}

static guchar *session_recv_data_from_sock_unescape(Session *session,
						    guint size,
						    guint *actual_size)
{
	GString *data;
	guchar *ret_data;
	gint bytes_read;
	gchar buf[BUFFSIZE];
	gchar *msg;
	struct timeval tv_prev, tv_cur;

	gettimeofday(&tv_prev, NULL);

	data = g_string_sized_new(size + 1);
	*actual_size = 0;

	while (1) {
		bytes_read = sock_gets(session->sock, buf, sizeof(buf));
		if (bytes_read <= 0) {
			g_string_free(data, TRUE);
			return NULL;
		}

		if (buf[0] == '.' && buf[1] == '\r' && buf[2] == '\n')
			break;
		if (buf[0] == '.' && buf[1] == '.')
			g_string_append(data, buf + 1);
		else
			g_string_append(data, buf);

		gettimeofday(&tv_cur, NULL);
		if (tv_cur.tv_sec - tv_prev.tv_sec > 0 ||
		    tv_cur.tv_usec - tv_prev.tv_usec > UI_REFRESH_INTERVAL) {
			g_snprintf(buf, sizeof(buf), "DATARECVINPROG %d %d",
				   data->len, MAX(data->len, size));
			session_send_msg(session, SESSION_MSG_CONTROL, buf);
			if ((msg = session_recv_msg(session)) == NULL) {
				g_string_free(data, TRUE);
				return NULL;
			}
			g_free(msg);
			gettimeofday(&tv_prev, NULL);
		}
	}

	ret_data = data->str;
	*actual_size = data->len;
	g_string_free(data, FALSE);

	return ret_data;
}

/*!
 *\brief	Return if message is an internal command or server data
 *
 *\param	str Message to analyze
 *
 *\return	Type of message
 */
static SessionMsgType session_get_msg_type(const gchar *str)
{
	if (!strncmp(str, "MESSAGE ", 8))
		return SESSION_MSG_NORMAL;
	else if (!strncmp(str, "SENDDATA ", 9))
		return SESSION_MSG_SEND_DATA;
	else if (!strncmp(str, "RECVDATA ", 9))
		return SESSION_MSG_RECV_DATA;
	else if (!strncmp(str, "CONTROL ", 8))
		return SESSION_MSG_CONTROL;
	else if (!strncmp(str, "ERROR ", 6))
		return SESSION_MSG_ERROR;
	else
		return SESSION_MSG_UNKNOWN;
}

/*!
 *\brief	parent: Received data from child
 *
 *\param	source Channel watching child pipe
 *		condition Unused (IN, HUP, OUT)
 *		data Contains session information
 *
 *\return	FALSE to remove watching channel
 */
gboolean session_parent_input_cb(GIOChannel *source, GIOCondition condition,
				 gpointer data)
{
	Session *session = SESSION(data);
	gchar *msg;
	gchar *msg_data;
	gint len;
	gint total;
	guchar *recv_data;
	guint size;
	gint ret;

	if ((msg = session_recv_msg(session)) == NULL) {
		session->state = SESSION_ERROR;
		return FALSE;
	}

	switch (session_get_msg_type(msg)) {
	case SESSION_MSG_NORMAL:
		msg_data = msg + strlen("MESSAGE ");
		ret = session->recv_msg(session, msg_data);
		g_print("$$$ > session_parent_input_cb(data: %lx)\n", 
			session->recv_msg_notify_data);
		session->recv_msg_notify(session, msg_data,
					 session->recv_msg_notify_data);
		g_print("$$$ < session_parent_input_cb(data: %lx)\n", 
			session->recv_msg_notify_data);
		if (ret > 0)
			session_send_msg(session, SESSION_MSG_CONTROL,
					 "CONTINUE");
		else if (ret < 0) {
			session->state = SESSION_ERROR;
			g_free(msg);
			return FALSE;
		}
		break;
	case SESSION_MSG_SEND_DATA:
		msg_data = msg + strlen("SENDDATA ");
		size = atoi(msg_data);
		session_send_msg(session, SESSION_MSG_CONTROL, "ACCEPTDATA");
		recv_data = session_read_data(session, size);
		if (!recv_data) {
			session->state = SESSION_ERROR;
			g_free(msg);
			return FALSE;
		}
		ret = session->recv_data_finished(session, recv_data, size);
		g_free(recv_data);
		session->recv_data_notify(session, size,
					  session->recv_data_notify_data);
		if (ret > 0)
			session_send_msg(session, SESSION_MSG_CONTROL,
					 "CONTINUE");
		else if (ret < 0) {
			session->state = SESSION_ERROR;
			g_free(msg);
			return FALSE;
		}
		break;
	case SESSION_MSG_RECV_DATA:
		break;
	case SESSION_MSG_CONTROL:
		msg_data = msg + strlen("CONTROL ");
		if (!strncmp(msg_data, "DATARECVINPROG ", 15)) {
			ret = sscanf(msg_data,
				     "DATARECVINPROG %d %d", &len, &total);
			if (ret != 2) {
				g_warning("wrong control message: %s\n", msg);
				session->state = SESSION_ERROR;
				g_free(msg);
				return FALSE;
			}
			session_send_msg(session, SESSION_MSG_CONTROL,
					 "CONTINUE");
			session->recv_data_progressive_notify
				(session, len, total,
				 session->recv_data_progressive_notify_data);
		} else if (!strncmp(msg_data, "DATASENDINPROG ", 15)) {
			ret = sscanf(msg_data,
				     "DATASENDINPROG %d %d", &len, &total);
			if (ret != 2) {
				g_warning("wrong control message: %s\n", msg);
				session->state = SESSION_ERROR;
				g_free(msg);
				return FALSE;
			}
			session_send_msg(session, SESSION_MSG_CONTROL,
					 "CONTINUE");
			session->send_data_progressive_notify
				(session, len, total,
				 session->send_data_progressive_notify_data);
		} else if (!strncmp(msg_data, "DATASENT ", 9)) {
			len = atoi(msg_data + 9);
			ret = session->send_data_finished(session, len);
			session->send_data_notify
				(session, len, session->send_data_notify_data);
		} else if (!strcmp(msg_data, "DISCONNECTED")) {
			session->state = SESSION_DISCONNECTED;
			g_free(msg);
			return FALSE;
		} else {
			g_warning("wrong control message: %s\n", msg);
			session->state = SESSION_ERROR;
			g_free(msg);
			return FALSE;
		}
		break;
	case SESSION_MSG_ERROR:
	default:
		g_warning("error from child: %s\n", msg + strlen("ERROR "));
		session->state = SESSION_ERROR;
		g_free(msg);
		return FALSE;
	}

	g_free(msg);
	return TRUE;
}

/*!
 *\brief	child: Receive control message from parent,
 *		transfer data from/to server
 *
 *\param	session Contains session information
 *
 *\return	TRUE if more data is available
 */
gboolean session_child_input(Session *session)
{
	gchar buf[BUFFSIZE];
	gchar *msg;
	gchar *msg_data;
	gchar *str;
	guchar *send_data;
	guchar *recv_data;
	guint size;
	guint actual_size;

	if ((msg = session_recv_msg(session)) == NULL) {
		session_send_msg(session, SESSION_MSG_ERROR,
				 "receiving message failed.");
		session_close(session);
		session->state = SESSION_ERROR;
		return FALSE;
	}

	switch (session_get_msg_type(msg)) {
	case SESSION_MSG_NORMAL:
		msg_data = msg + strlen("MESSAGE ");
		session->state = SESSION_SEND;
		sock_puts(session->sock, msg_data);
		session->state = SESSION_RECV;
		str = sock_getline(session->sock);
		if (!str) {
			session_send_msg(session, SESSION_MSG_ERROR,
					 "receiving message failed.");
			session_close(session);
			session->state = SESSION_ERROR;
			g_free(msg);
			return FALSE;
		}
		strretchomp(str);
		session_send_msg(session, SESSION_MSG_NORMAL, str);
		g_free(str);
		break;
	case SESSION_MSG_SEND_DATA:
		msg_data = msg + strlen("SENDDATA ");
		size = atoi(msg_data);
		session_send_msg(session, SESSION_MSG_CONTROL, "ACCEPTDATA");
		send_data = session_read_data(session, size);
		if (!send_data) {
			session_send_msg(session, SESSION_MSG_ERROR,
					 "sending data failed.");
			session_close(session);
			session->state = SESSION_ERROR;
			g_free(msg);
			return FALSE;
		}
		session->state = SESSION_SEND;
		if (session_send_data_to_sock(session, send_data, size) < 0) {
			session_send_msg(session, SESSION_MSG_ERROR,
					 "sending data failed.");
			session_close(session);
			session->state = SESSION_ERROR;
			g_free(send_data);
			g_free(msg);
			return FALSE;
		}
		g_free(send_data);
		g_snprintf(buf, sizeof(buf), "DATASENT %d", size);
		session_send_msg(session, SESSION_MSG_CONTROL, buf);
		break;
	case SESSION_MSG_RECV_DATA:
		msg_data = msg + strlen("RECVDATA ");
		size = atoi(msg_data);
		session->state = SESSION_RECV;
		if (strstr(msg_data, "UNESCAPE") != NULL) {
			recv_data = session_recv_data_from_sock_unescape
				(session, size, &actual_size);
			size = actual_size;
		} else
			recv_data = session_recv_data_from_sock(session, size);
		if (!recv_data) {
			session_send_msg(session, SESSION_MSG_ERROR,
					 "receiving data failed.");
			session_close(session);
			session->state = SESSION_ERROR;
			g_free(msg);
			return FALSE;
		}
		if (session_send_data(session, recv_data, size) < 0) {
			session_close(session);
			session->state = SESSION_ERROR;
			g_free(recv_data);
			g_free(msg);
			return FALSE;
		}
		g_free(recv_data);
		break;
	case SESSION_MSG_CONTROL:
		msg_data = msg + strlen("CONTROL ");
		if (!strcmp(msg_data, "CONTINUE")) {
			session->state = SESSION_RECV;
			str = sock_getline(session->sock);
			if (!str) {
				session_send_msg(session, SESSION_MSG_ERROR,
						 "receiving message failed.");
				session_close(session);
				session->state = SESSION_ERROR;
				g_free(msg);
				return FALSE;
			}
			strretchomp(str);
			session_send_msg(session, SESSION_MSG_NORMAL, str);
			g_free(str);
			break;
#if USE_OPENSSL
		} else if (!strcmp(msg_data, "STARTTLS")) {
			if (!ssl_init_socket_with_method(session->sock,
							 SSL_METHOD_TLSv1)) {
				session_send_msg(session, SESSION_MSG_ERROR,
						 "can't start TLS session.");
				session_close(session);
				session->state = SESSION_ERROR;
				g_free(msg);
				return FALSE;
			}
			session_send_msg(session, SESSION_MSG_CONTROL,
					 "STARTTLSOK");
			break;
#endif
		} else if (!strcmp(msg_data, "DISCONNECT")) {
			sock_close(session->sock);
			session->sock = NULL;
			session->state = SESSION_DISCONNECTED;
			session_send_msg(session, SESSION_MSG_CONTROL,
					 "DISCONNECTED");
			g_free(msg);
			return FALSE;
		} else {
			session_send_msg(session, SESSION_MSG_ERROR,
					 "wrong control message.");
			session_close(session);
			session->state = SESSION_ERROR;
			g_free(msg);
			return FALSE;
		}
		break;
	case SESSION_MSG_ERROR:
	default:
		session_send_msg(session, SESSION_MSG_ERROR,
				 "error received from parent.");
		session_close(session);
		session->state = SESSION_ERROR;
		g_free(msg);
		return FALSE;
	}

	g_free(msg);
	return TRUE;
}
