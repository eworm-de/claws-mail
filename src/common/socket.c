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
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#if HAVE_SYS_SELECT_H
#  include <sys/select.h>
#endif

#include "socket.h"
#include "utils.h"
#include "log.h"
#if USE_OPENSSL
#  include "ssl.h"
#endif

#if USE_GIO
#error USE_GIO is currently not supported
#endif

#define BUFFSIZE	8192

typedef gint (*SockAddrFunc)	(GList		*addr_list,
				 gpointer	 data);

typedef struct _SockConnectData	SockConnectData;
typedef struct _SockLookupData	SockLookupData;
typedef struct _SockAddrData	SockAddrData;

struct _SockConnectData {
	gint id;
	gchar *hostname;
	gushort port;
	GList *addr_list;
	GList *cur_addr;
	SockLookupData *lookup_data;
	GIOChannel *channel;
	guint io_tag;
	SockConnectFunc func;
	gpointer data;
};

struct _SockLookupData {
	gchar *hostname;
	pid_t child_pid;
	GIOChannel *channel;
	guint io_tag;
	SockAddrFunc func;
	gpointer data;
};

struct _SockAddrData {
	gint addr_len;
	gpointer addr_data;
};

static guint io_timeout = 60;

static GList *sock_connect_data_list = NULL;

static gint sock_connect_with_timeout	(gint			 sock,
					 const struct sockaddr	*serv_addr,
					 gint			 addrlen,
					 guint			 timeout_secs);

#ifndef INET6
static gint sock_connect_by_hostname	(gint		 sock,
					 const gchar	*hostname,
					 gushort	 port);
#else
static gint sock_connect_by_getaddrinfo	(const gchar	*hostname,
					 gushort	 port);
#endif

static SockInfo *sockinfo_from_fd(const gchar *hostname,
				  gushort port,
				  gint sock);
static void sock_address_list_free		(GList		*addr_list);

static gboolean sock_connect_async_cb		(GIOChannel	*source,
						 GIOCondition	 condition,
						 gpointer	 data);
static gint sock_connect_async_get_address_info_cb
						(GList		*addr_list,
						 gpointer	 data);

static gint sock_connect_address_list_async	(SockConnectData *conn_data);

static gboolean sock_get_address_info_async_cb	(GIOChannel	*source,
						 GIOCondition	 condition,
						 gpointer	 data);
static SockLookupData *sock_get_address_info_async
						(const gchar	*hostname,
						 gushort	 port,
						 SockAddrFunc	 func,
						 gpointer	 data);
static gint sock_get_address_info_async_cancel	(SockLookupData	*lookup_data);


gint sock_set_io_timeout(guint sec)
{
	io_timeout = sec;
	return 0;
}

gint fd_connect_unix(const gchar *path)
{
	gint sock;
	struct sockaddr_un addr;

	sock = socket(PF_UNIX, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("sock_connect_unix(): socket");
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

	if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		close(sock);
		return -1;
	}

	return sock;
}

gint fd_open_unix(const gchar *path)
{
	gint sock;
	struct sockaddr_un addr;

	sock = socket(PF_UNIX, SOCK_STREAM, 0);

	if (sock < 0) {
		perror("sock_open_unix(): socket");
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

	if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("bind");
		close(sock);
		return -1;
	}

	if (listen(sock, 1) < 0) {
		perror("listen");
		close(sock);
		return -1;		
	}

	return sock;
}

gint fd_accept(gint sock)
{
	struct sockaddr_in caddr;
	gint caddr_len;

	caddr_len = sizeof(caddr);
	return accept(sock, (struct sockaddr *)&caddr, &caddr_len);
}


static gint set_nonblocking_mode(gint fd, gboolean nonblock)
{
	gint flags;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) {
		perror("fcntl");
		return -1;
	}

	if (nonblock)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;

	return fcntl(fd, F_SETFL, flags);
}

gint sock_set_nonblocking_mode(SockInfo *sock, gboolean nonblock)
{
	g_return_val_if_fail(sock != NULL, -1);

	return set_nonblocking_mode(sock->sock, nonblock);
}


static gboolean is_nonblocking_mode(gint fd)
{
	gint flags;

	flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0) {
		perror("fcntl");
		return FALSE;
	}

	return ((flags & O_NONBLOCK) != 0);
}

gboolean sock_is_nonblocking_mode(SockInfo *sock)
{
	g_return_val_if_fail(sock != NULL, FALSE);

	return is_nonblocking_mode(sock->sock);
}

static gint fd_check_io(gint fd, GIOCondition cond)
{
	struct timeval timeout;
	fd_set fds;

	if (is_nonblocking_mode(fd))
		return 0;

	timeout.tv_sec  = io_timeout;
	timeout.tv_usec = 0;

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	if (cond == G_IO_IN) {
		select(fd + 1, &fds, NULL, NULL,
		       io_timeout > 0 ? &timeout : NULL);
	} else {
		select(fd + 1, NULL, &fds, NULL,
		       io_timeout > 0 ? &timeout : NULL);
	}

	if (FD_ISSET(fd, &fds)) {
		return 0;
	} else {
		g_warning("Socket IO timeout\n");
		return -1;
	}
}

static sigjmp_buf jmpenv;

static void timeout_handler(gint sig)
{
	siglongjmp(jmpenv, 1);
}

static gint sock_connect_with_timeout(gint sock,
				      const struct sockaddr *serv_addr,
				      gint addrlen,
				      guint timeout_secs)
{
	gint ret;
	void (*prev_handler)(gint);

	alarm(0);
	prev_handler = signal(SIGALRM, timeout_handler);
	if (sigsetjmp(jmpenv, 1)) {
		alarm(0);
		signal(SIGALRM, prev_handler);
		errno = ETIMEDOUT;
		return -1;
	}
	alarm(timeout_secs);

	ret = connect(sock, serv_addr, addrlen);

	alarm(0);
	signal(SIGALRM, prev_handler);

	return ret;
}

struct hostent *my_gethostbyname(const gchar *hostname)
{
	struct hostent *hp;
	void (*prev_handler)(gint);

	alarm(0);
	prev_handler = signal(SIGALRM, timeout_handler);
	if (sigsetjmp(jmpenv, 1)) {
		alarm(0);
		signal(SIGALRM, prev_handler);
		fprintf(stderr, "%s: host lookup timed out.\n", hostname);
		errno = 0;
		return NULL;
	}
	alarm(io_timeout);

	if ((hp = gethostbyname(hostname)) == NULL) {
		alarm(0);
		signal(SIGALRM, prev_handler);
		fprintf(stderr, "%s: unknown host.\n", hostname);
		errno = 0;
		return NULL;
	}

	alarm(0);
	signal(SIGALRM, prev_handler);

	return hp;
}

#ifndef INET6
static gint my_inet_aton(const gchar *hostname, struct in_addr *inp)
{
#if HAVE_INET_ATON
	return inet_aton(hostname, inp);
#else
#if HAVE_INET_ADDR
	guint32 inaddr;

	inaddr = inet_addr(hostname);
	if (inaddr != -1) {
		memcpy(inp, &inaddr, sizeof(inaddr));
		return 1;
	} else
		return 0;
#else
	return 0;
#endif
#endif /* HAVE_INET_ATON */
}

static gint sock_connect_by_hostname(gint sock, const gchar *hostname,
				     gushort port)
{
	struct hostent *hp;
	struct sockaddr_in ad;

	memset(&ad, 0, sizeof(ad));
	ad.sin_family = AF_INET;
	ad.sin_port = htons(port);

	if (!my_inet_aton(hostname, &ad.sin_addr)) {
		if ((hp = my_gethostbyname(hostname)) == NULL) {
			fprintf(stderr, "%s: unknown host.\n", hostname);
			errno = 0;
			return -1;
		}

		if (hp->h_length != 4 && hp->h_length != 8) {
			fprintf(stderr, "illegal address length received for host %s\n", hostname);
			errno = 0;
			return -1;
		}

		memcpy(&ad.sin_addr, hp->h_addr, hp->h_length);
	}

	return sock_connect_with_timeout(sock, (struct sockaddr *)&ad,
					 sizeof(ad), io_timeout);
}

#else /* INET6 */
static gint sock_connect_by_getaddrinfo(const gchar *hostname, gushort	port)
{
	gint sock = -1, gai_error;
	struct addrinfo hints, *res, *ai;
	gchar port_str[6];

	memset(&hints, 0, sizeof(hints));
	/* hints.ai_flags = AI_CANONNAME; */
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	/* convert port from integer to string. */
	g_snprintf(port_str, sizeof(port_str), "%d", port);

	if ((gai_error = getaddrinfo(hostname, port_str, &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo for %s:%s failed: %s\n",
			hostname, port_str, gai_strerror(gai_error));
		return -1;
	}

	for (ai = res; ai != NULL; ai = ai->ai_next) {
		sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sock < 0)
			continue;

		if (sock_connect_with_timeout
			(sock, ai->ai_addr, ai->ai_addrlen, io_timeout) == 0)
			break;

		close(sock);
	}

	if (res != NULL)
		freeaddrinfo(res);

	if (ai == NULL)
		return -1;

	return sock;
}
#endif /* !INET6 */


/* Open a connection using an external program.  May be useful when
 * you need to tunnel through a SOCKS or other firewall, or to
 * establish an IMAP-over-SSH connection. */
/* TODO: Recreate this for sock_connect_thread() */
SockInfo *sock_connect_cmd(const gchar *hostname, const gchar *tunnelcmd)
{
	gint fd[2];
	int r;
		     
	if ((r = socketpair(AF_UNIX, SOCK_STREAM, 0, fd)) == -1) {
		perror("socketpair");
		return NULL;
	}
	log_message("launching tunnel command \"%s\"\n", tunnelcmd);
	if (fork() == 0) {
		close(fd[0]);
		close(0);
		close(1);
		dup(fd[1]);	/* set onto stdin */
		dup(fd[1]);
		execlp("/bin/sh", "/bin/sh", "-c", tunnelcmd, NULL);
	}

	close(fd[1]);
	return sockinfo_from_fd(hostname, 0, fd[0]);
}


SockInfo *sock_connect(const gchar *hostname, gushort port)
{
	gint sock;

#ifdef INET6
	if ((sock = sock_connect_by_getaddrinfo(hostname, port)) < 0)
		return NULL;
#else
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return NULL;
	}

	if (sock_connect_by_hostname(sock, hostname, port) < 0) {
		if (errno != 0) perror("connect");
		close(sock);
		return NULL;
	}
#endif /* INET6 */

	return sockinfo_from_fd(hostname, port, sock);
}

static void sock_address_list_free(GList *addr_list)
{
	GList *cur;

	for (cur = addr_list; cur != NULL; cur = cur->next) {
		SockAddrData *addr_data = (SockAddrData *)cur->data;
		g_free(addr_data->addr_data);
		g_free(addr_data);
	}

	g_list_free(addr_list);
}

/* asynchronous TCP connection */

static gboolean sock_connect_async_cb(GIOChannel *source,
				      GIOCondition condition, gpointer data)
{
	SockConnectData *conn_data = (SockConnectData *)data;
	gint fd;
	gint val;
	gint len;
	SockInfo *sockinfo;

	fd = g_io_channel_unix_get_fd(source);

	conn_data->io_tag = 0;
	conn_data->channel = NULL;
	g_io_channel_unref(source);

	len = sizeof(val);
	if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &val, &len) < 0) {
		perror("getsockopt");
		close(fd);
		sock_connect_address_list_async(conn_data);
		return FALSE;
	}

	if (val != 0) {
		close(fd);
		sock_connect_address_list_async(conn_data);
		return FALSE;
	}

	sockinfo = g_new0(SockInfo, 1);
	sockinfo->sock = fd;
	sockinfo->hostname = g_strdup(conn_data->hostname);
	sockinfo->port = conn_data->port;
	sockinfo->state = CONN_ESTABLISHED;

	conn_data->func(sockinfo, conn_data->data);

	sock_connect_async_cancel(conn_data->id);

	return FALSE;
}

static gint sock_connect_async_get_address_info_cb(GList *addr_list,
						   gpointer data)
{
	SockConnectData *conn_data = (SockConnectData *)data;

	conn_data->addr_list = addr_list;
	conn_data->cur_addr = addr_list;
	conn_data->lookup_data = NULL;

	return sock_connect_address_list_async(conn_data);
}

gint sock_connect_async(const gchar *hostname, gushort port,
			SockConnectFunc func, gpointer data)
{
	static gint id = 1;
	SockConnectData *conn_data;

	conn_data = g_new0(SockConnectData, 1);
	conn_data->id = id++;
	conn_data->hostname = g_strdup(hostname);
	conn_data->port = port;
	conn_data->addr_list = NULL;
	conn_data->cur_addr = NULL;
	conn_data->io_tag = 0;
	conn_data->func = func;
	conn_data->data = data;

	conn_data->lookup_data = sock_get_address_info_async
		(hostname, port, sock_connect_async_get_address_info_cb,
		 conn_data);

	if (conn_data->lookup_data == NULL) {
		g_free(conn_data->hostname);
		g_free(conn_data);
		return -1;
	}

	sock_connect_data_list = g_list_append(sock_connect_data_list,
					       conn_data);

	return conn_data->id;
}

gint sock_connect_async_cancel(gint id)
{
	SockConnectData *conn_data = NULL;
	GList *cur;

	for (cur = sock_connect_data_list; cur != NULL; cur = cur->next) {
		if (((SockConnectData *)cur->data)->id == id) {
			conn_data = (SockConnectData *)cur->data;
			break;
		}
	}

	if (conn_data) {
		sock_connect_data_list = g_list_remove(sock_connect_data_list,
						       conn_data);

		if (conn_data->lookup_data)
			sock_get_address_info_async_cancel
				(conn_data->lookup_data);

		if (conn_data->io_tag > 0)
			g_source_remove(conn_data->io_tag);
		if (conn_data->channel) {
			g_io_channel_close(conn_data->channel);
			g_io_channel_unref(conn_data->channel);
		}

		sock_address_list_free(conn_data->addr_list);
		g_free(conn_data->hostname);
		g_free(conn_data);
	} else {
		g_warning("sock_connect_async_cancel: id %d not found.\n", id);
		return -1;
	}

	return 0;
}

static gint sock_connect_address_list_async(SockConnectData *conn_data)
{
	SockAddrData *addr_data;
	struct sockaddr_in ad;
#ifdef INET6
	struct sockaddr_in6 ad6;
#endif
	struct sockaddr *sa;
	gint sa_size;
	gint sock = -1;

	for (; conn_data->cur_addr != NULL;
	     conn_data->cur_addr = conn_data->cur_addr->next) {
		addr_data = (SockAddrData *)conn_data->cur_addr->data;

#ifdef INET6
		if (addr_data->addr_len != 4 && addr_data->addr_len != 16)
			continue;

		if (addr_data->addr_len == 4) {
			/* IPv4 address */
			if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
				perror("socket");
				conn_data->cur_addr = NULL;
				break;
			}

			memset(&ad, 0, sizeof(ad));
			ad.sin_family = AF_INET;
			ad.sin_port = htons(conn_data->port);
			memcpy(&ad.sin_addr, addr_data->addr_data,
			       addr_data->addr_len);

			sa = (struct sockaddr *)&ad;
			sa_size = sizeof(ad);
		} else {
			/* IPv6 address */
			if ((sock = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
				perror("socket");
				conn_data->cur_addr = NULL;
				break;
			}

			memset(&ad6, 0, sizeof(ad6));
			ad6.sin6_family = AF_INET6;
			ad6.sin6_port = htons(conn_data->port);
			memcpy(&ad6.sin6_addr, addr_data->addr_data,
			       addr_data->addr_len);

			sa = (struct sockaddr *)&ad6;
			sa_size = sizeof(ad6);
		}
#else /* !INET6 */
		/* IPv4 only */
		if (addr_data->addr_len != 4)
			continue;

		if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
			perror("socket");
			conn_data->cur_addr = NULL;
			break;
		}

		memset(&ad, 0, sizeof(ad));
		ad.sin_family = AF_INET;
		ad.sin_port = htons(conn_data->port);
		memcpy(&ad.sin_addr, addr_data->addr_data, addr_data->addr_len);

		sa = (struct sockaddr *)&ad;
		sa_size = sizeof(ad);
#endif

		set_nonblocking_mode(sock, TRUE);

		if (connect(sock, sa, sa_size) < 0) {
			if (EINPROGRESS == errno) {
				break;
			} else {
				perror("connect");
				close(sock);
			}
		} else
			break;
	}

	if (conn_data->cur_addr == NULL) {
		g_warning("sock_connect_address_list_async: "
			  "connection to %s:%d failed\n",
			  conn_data->hostname, conn_data->port);
		conn_data->func(NULL, conn_data->data);
		sock_connect_async_cancel(conn_data->id);
		return -1;
	}

	conn_data->cur_addr = conn_data->cur_addr->next;

	conn_data->channel = g_io_channel_unix_new(sock);
	conn_data->io_tag = g_io_add_watch(conn_data->channel, G_IO_IN|G_IO_OUT,
					   sock_connect_async_cb, conn_data);

	return 0;
}

/* asynchronous DNS lookup */

static gboolean sock_get_address_info_async_cb(GIOChannel *source,
					       GIOCondition condition,
					       gpointer data)
{
	SockLookupData *lookup_data = (SockLookupData *)data;
	GList *addr_list = NULL;
	SockAddrData *addr_data;
	guint bytes_read;
	guint8 addr_len;
	gchar buf[16];

	for (;;) {
		if (g_io_channel_read(source, &addr_len, 1, &bytes_read)
		    != G_IO_ERROR_NONE) {
			g_warning("sock_get_address_info_async_cb: "
				  "address length read error\n");
			break;
		}

		if (bytes_read == 0)
			break;

		if (addr_len == 'e') {
			g_warning("DNS lookup failed\n");
			break;
		} else if (addr_len != 4 && addr_len != 16) {
			g_warning("illegal address length: %d\n", addr_len);
			break;
		}

		if (g_io_channel_read(source, buf, addr_len, &bytes_read)
		    != G_IO_ERROR_NONE) {
			g_warning("sock_get_address_info_async_cb: "
				  "address data read error\n");
			break;
		}

		if (bytes_read != addr_len) {
			g_warning("sock_get_address_info_async_cb: "
				  "incomplete address data\n");
			break;
		}

		addr_data = g_new0(SockAddrData, 1);
		addr_data->addr_len = addr_len;
		addr_data->addr_data = g_malloc(addr_len);
		memcpy(addr_data->addr_data, buf, addr_len);

		addr_list = g_list_append(addr_list, addr_data);
	}

	g_io_channel_close(source);
	g_io_channel_unref(source);

	kill(lookup_data->child_pid, SIGKILL);
	waitpid(lookup_data->child_pid, NULL, 0);

	lookup_data->func(addr_list, lookup_data->data);

	g_free(lookup_data->hostname);
	g_free(lookup_data);

	return FALSE;
}

static SockLookupData *sock_get_address_info_async(const gchar *hostname,
						   gushort port,
						   SockAddrFunc func,
						   gpointer data)
{
	SockLookupData *lookup_data = NULL;
	gint pipe_fds[2];
	pid_t pid;

	if (pipe(pipe_fds) < 0) {
		perror("pipe");
		func(NULL, data);
		return NULL;
	}

	if ((pid = fork()) < 0) {
		perror("fork");
		func(NULL, data);
		return NULL;
	}

	/* child process */
	if (pid == 0) {
#ifdef INET6
		gint gai_err;
		struct addrinfo hints, *res, *ai;
		gchar port_str[6];
#else /* !INET6 */
		struct hostent *hp;
		gchar **addr_list_p;
#endif /* INET6 */
		guint8 addr_len;

		close(pipe_fds[0]);

#ifdef INET6
		memset(&hints, 0, sizeof(hints));
		/* hints.ai_flags = AI_CANONNAME; */
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		g_snprintf(port_str, sizeof(port_str), "%d", port);

		gai_err = getaddrinfo(hostname, port_str, &hints, &res);
		if (gai_err != 0) {
			g_warning("getaddrinfo for %s:%s failed: %s\n",
				  hostname, port_str, gai_strerror(gai_err));
			fd_write(pipe_fds[1], "e", 1);
			close(pipe_fds[1]);
			_exit(1);
		}

		for (ai = res; ai != NULL; ai = ai->ai_next) {
			if (ai->ai_family == AF_INET) {
				struct sockaddr_in *ad;

				ad = (struct sockaddr_in *)ai->ai_addr;
				addr_len = sizeof(ad->sin_addr);

				fd_write(pipe_fds[1], &addr_len, 1);
				fd_write_all(pipe_fds[1],
					     (gchar *)&ad->sin_addr,
					     sizeof(ad->sin_addr));
			} else if (ai->ai_family == AF_INET6) {
				struct sockaddr_in6 *ad;

				ad = (struct sockaddr_in6 *)ai->ai_addr;
				addr_len = sizeof(ad->sin6_addr);

				fd_write(pipe_fds[1], &addr_len, 1);
				fd_write_all(pipe_fds[1],
					     (gchar *)&ad->sin6_addr,
					     sizeof(ad->sin6_addr));
			}
		}

		if (res != NULL)
			freeaddrinfo(res);
#else /* !INET6 */
		hp = my_gethostbyname(hostname);
		if (hp == NULL) {
			fd_write(pipe_fds[1], "e", 1);
			close(pipe_fds[1]);
			_exit(1);
		}

		addr_len = (guint8)hp->h_length;

		for (addr_list_p = hp->h_addr_list; *addr_list_p != NULL;
		     addr_list_p++) {
			fd_write(pipe_fds[1], &addr_len, 1);
			fd_write_all(pipe_fds[1], *addr_list_p, hp->h_length);
		}
#endif /* INET6 */

		close(pipe_fds[1]);

		_exit(0);
	} else {
		close(pipe_fds[1]);

		lookup_data = g_new0(SockLookupData, 1);
		lookup_data->hostname = g_strdup(hostname);
		lookup_data->child_pid = pid;
		lookup_data->func = func;
		lookup_data->data = data;

		lookup_data->channel = g_io_channel_unix_new(pipe_fds[0]);
		lookup_data->io_tag = g_io_add_watch
			(lookup_data->channel, G_IO_IN,
			 sock_get_address_info_async_cb, lookup_data);
	}

	return lookup_data;
}

static gint sock_get_address_info_async_cancel(SockLookupData *lookup_data)
{
	if (lookup_data->io_tag > 0)
		g_source_remove(lookup_data->io_tag);
	if (lookup_data->channel) {
		g_io_channel_close(lookup_data->channel);
		g_io_channel_unref(lookup_data->channel);
	}

	if (lookup_data->child_pid > 0) {
		kill(lookup_data->child_pid, SIGKILL);
		waitpid(lookup_data->child_pid, NULL, 0);
	}

	g_free(lookup_data->hostname);
	g_free(lookup_data);

	g_print("DNS lookup cancelled\n");

	return 0;
}


static SockInfo *sockinfo_from_fd(const gchar *hostname,
				  gushort port,
				  gint sock)
{
	SockInfo *sockinfo;

	sockinfo = g_new0(SockInfo, 1);
	sockinfo->sock = sock;
	sockinfo->hostname = g_strdup(hostname);
	sockinfo->port = port;
	sockinfo->state = CONN_ESTABLISHED;

	usleep(100000);

	return sockinfo;
}

gint sock_printf(SockInfo *sock, const gchar *format, ...)
{
	va_list args;
	gchar buf[BUFFSIZE];

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	return sock_write_all(sock, buf, strlen(buf));
}

gint fd_read(gint fd, gchar *buf, gint len)
{
	if (fd_check_io(fd, G_IO_IN) < 0)
		return -1;

	return read(fd, buf, len);
}

#if USE_OPENSSL
gint ssl_read(SSL *ssl, gchar *buf, gint len)
{
	return SSL_read(ssl, buf, len);
}
#endif

gint sock_read(SockInfo *sock, gchar *buf, gint len)
{
	gint ret;

	g_return_val_if_fail(sock != NULL, -1);

#if USE_OPENSSL
	if (sock->ssl)
		ret = ssl_read(sock->ssl, buf, len);
	else
#endif
		ret = fd_read(sock->sock, buf, len);
	
	if (ret < 0)
		sock->state = CONN_DISCONNECTED;
	return ret;
}

gint fd_write(gint fd, const gchar *buf, gint len)
{
	if (fd_check_io(fd, G_IO_OUT) < 0)
		return -1;

	return write(fd, buf, len);
}

#if USE_OPENSSL
gint ssl_write(SSL *ssl, const gchar *buf, gint len)
{
	return SSL_write(ssl, buf, len);
}
#endif

gint sock_write(SockInfo *sock, const gchar *buf, gint len)
{
	gint ret;

	g_return_val_if_fail(sock != NULL, -1);

#if USE_OPENSSL
	if (sock->ssl)
		ret = ssl_write(sock->ssl, buf, len);
	else
#endif
		ret = fd_write(sock->sock, buf, len);

	if (ret < 0)
		sock->state = CONN_DISCONNECTED;
	return ret;
}

gint fd_write_all(gint fd, const gchar *buf, gint len)
{
	gint n, wrlen = 0;

	while (len) {
		if (fd_check_io(fd, G_IO_OUT) < 0)
			return -1;
		signal(SIGPIPE, SIG_IGN);
		n = write(fd, buf, len);
		if (n <= 0) {
			log_error("write on fd%d: %s\n", fd, strerror(errno));
			return -1;
		}
		len -= n;
		wrlen += n;
		buf += n;
	}

	return wrlen;
}

#if USE_OPENSSL
gint ssl_write_all(SSL *ssl, const gchar *buf, gint len)
{
	gint n, wrlen = 0;

	while (len) {
		n = SSL_write(ssl, buf, len);
		if (n <= 0)
			return -1;
		len -= n;
		wrlen += n;
		buf += n;
	}

	return wrlen;
}
#endif

gint sock_write_all(SockInfo *sock, const gchar *buf, gint len)
{
	gint ret;

	g_return_val_if_fail(sock != NULL, -1);

#if USE_OPENSSL
	if (sock->ssl)
		ret = ssl_write_all(sock->ssl, buf, len);
	else
#endif
		ret = fd_write_all(sock->sock, buf, len);

	if (ret < 0)
		sock->state = CONN_DISCONNECTED;
	return ret;
}

gint fd_recv(gint fd, gchar *buf, gint len, gint flags)
{
	if (fd_check_io(fd, G_IO_IN) < 0)
		return -1;

	return recv(fd, buf, len, flags);
}

gint fd_gets(gint fd, gchar *buf, gint len)
{
	gchar *newline, *bp = buf;
	gint n;

	if (--len < 1)
		return -1;
	do {
		if ((n = fd_recv(fd, bp, len, MSG_PEEK)) <= 0)
			return -1;
		if ((newline = memchr(bp, '\n', n)) != NULL)
			n = newline - bp + 1;
		if ((n = fd_read(fd, bp, n)) < 0)
			return -1;
		bp += n;
		len -= n;
	} while (!newline && len);

	*bp = '\0';
	return bp - buf;
}

#if USE_OPENSSL
gint ssl_gets(SSL *ssl, gchar *buf, gint len)
{
	gchar *newline, *bp = buf;
	gint n;

	if (--len < 1)
		return -1;
	do {
		if ((n = SSL_peek(ssl, bp, len)) <= 0)
			return -1;
		if ((newline = memchr(bp, '\n', n)) != NULL)
			n = newline - bp + 1;
		if ((n = SSL_read(ssl, bp, n)) < 0)
			return -1;
		bp += n;
		len -= n;
	} while (!newline && len);

	*bp = '\0';
	return bp - buf;
}
#endif

gint sock_gets(SockInfo *sock, gchar *buf, gint len)
{
	gint ret;

	g_return_val_if_fail(sock != NULL, -1);

#if USE_OPENSSL
	if (sock->ssl)
		return ssl_gets(sock->ssl, buf, len);
	else
#endif
		return fd_gets(sock->sock, buf, len);

	if (ret < 0)
		sock->state = CONN_DISCONNECTED;
	return ret;
}

gint fd_getline(gint fd, gchar **str)
{
	gchar buf[BUFFSIZE];
	gint len;
	gulong size = 1;

	while ((len = fd_gets(fd, buf, sizeof(buf))) > 0) {
		size += len;
		if (!*str)
			*str = g_strdup(buf);
		else {
			*str = g_realloc(*str, size);
			strcat(*str, buf);
		}
		if (buf[len - 1] == '\n')
			break;
	}
	if (len == -1 && *str)
		g_free(*str);

	return len;
}

#if USE_OPENSSL
gint ssl_getline(SSL *ssl, gchar **str)
{
	gchar buf[BUFFSIZE];
	gint len;
	gulong size = 1;

	while ((len = ssl_gets(ssl, buf, sizeof(buf))) > 0) {
		size += len;
		if (!*str)
			*str = g_strdup(buf);
		else {
			*str = g_realloc(*str, size);
			strcat(*str, buf);
		}
		if (buf[len - 1] == '\n')
			break;
	}
	if (len == -1 && *str)
		g_free(*str);

	return len;
}
#endif

gchar *sock_getline(SockInfo *sock)
{
	gint ret;
	gchar *str = NULL;

	g_return_val_if_fail(sock != NULL, NULL);

#if USE_OPENSSL
	if (sock->ssl)
		ret = ssl_getline(sock->ssl, &str);
	else
#endif
		ret = fd_getline(sock->sock, &str);

	if (ret < 0)
		sock->state = CONN_DISCONNECTED;
	return str;
}

gint sock_puts(SockInfo *sock, const gchar *buf)
{
	gint ret;

	if ((ret = sock_write_all(sock, buf, strlen(buf))) < 0)
		return ret;
	return sock_write_all(sock, "\r\n", 2);
}

/* peek at the socket data without actually reading it */
gint sock_peek(SockInfo *sock, gchar *buf, gint len)
{
	g_return_val_if_fail(sock != NULL, -1);

#if USE_OPENSSL
	if (sock->ssl)
		return SSL_peek(sock->ssl, buf, len);
#endif
	return fd_recv(sock->sock, buf, len, MSG_PEEK);
}

gint sock_close(SockInfo *sock)
{
	gint ret;

	if (!sock)
		return 0;

#if USE_OPENSSL
	if (sock->ssl)
		ssl_done_socket(sock);
#endif
	ret = fd_close(sock->sock); 
	g_free(sock->hostname);
	g_free(sock);

	return ret;
}

gint fd_close(gint fd)
{
	return close(fd);
}
