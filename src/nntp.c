/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2001 Hiroyuki Yamamoto
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

#include "intl.h"
#include "nntp.h"
#include "socket.h"
#include "ssl.h"
#include "utils.h"

static gint verbose = 1;

static void nntp_gen_send	(NNTPSockInfo	*sock,
				 const gchar	*format,
				 ...);
static gint nntp_gen_recv	(NNTPSockInfo	*sock,
				 gchar		*buf,
				 gint		 size);
static gint nntp_gen_command	(NNTPSockInfo	*sock,
				 gchar		*argbuf,
				 const gchar	*format,
				 ...);

#if USE_SSL
NNTPSockInfo *nntp_open(const gchar *server, gushort port, gchar *buf, gboolean use_ssl)
#else
NNTPSockInfo *nntp_open(const gchar *server, gushort port, gchar *buf)
#endif
{
	SockInfo *sock;
	NNTPSockInfo *nntp_sock;

	if ((sock = sock_connect(server, port)) == NULL) {
		log_warning(_("Can't connect to NNTP server: %s:%d\n"),
			    server, port);
		return NULL;
	}

#if USE_SSL
	if (use_ssl && !ssl_init_socket(sock)) {
		 sock_close(sock);
		 return NULL;
	}
#endif
	
	nntp_sock = g_new0(NNTPSockInfo, 1);
	nntp_sock->sock = sock;

	if (nntp_ok(nntp_sock, buf) == NN_SUCCESS)
		return nntp_sock;
	else {
		sock_close(sock);
		g_free(nntp_sock);
		return NULL;
	}
}

#if USE_SSL
NNTPSockInfo *nntp_open_auth(const gchar *server, gushort port, gchar *buf,
			     const gchar *userid, const gchar *passwd, gboolean use_ssl)
#else
NNTPSockInfo *nntp_open_auth(const gchar *server, gushort port, gchar *buf,
			     const gchar *userid, const gchar *passwd)
#endif
{
	NNTPSockInfo *sock;

#if USE_SSL
	sock = nntp_open(server, port, buf, use_ssl);
#else
	sock = nntp_open(server, port, buf);
#endif

	if (!sock) return NULL;

	sock->userid = g_strdup(userid);
	sock->passwd = g_strdup(passwd);

	return sock;
}

void nntp_close(NNTPSockInfo *sock)
{
	if (!sock) return;

	sock_close(sock->sock);
	g_free(sock->userid);
	g_free(sock->passwd);
	g_free(sock);
}

gint nntp_group(NNTPSockInfo *sock, const gchar *group,
		gint *num, gint *first, gint *last)
{
	gint ok;
	gint resp;
	gchar buf[NNTPBUFSIZE];

	ok = nntp_gen_command(sock, buf, "GROUP %s", group);

	if (ok != NN_SUCCESS) {
		ok = nntp_gen_command(sock, buf, "MODE READER");
		if (ok == NN_SUCCESS)
			ok = nntp_gen_command(sock, buf, "GROUP %s", group);
	}	
	if (ok != NN_SUCCESS)
		return ok;

	if (sscanf(buf, "%d %d %d %d", &resp, num, first, last)
	    != 4) {
		log_warning(_("protocol error: %s\n"), buf);
		return NN_PROTOCOL;
	}

	return NN_SUCCESS;
}

gint nntp_get_article(NNTPSockInfo *sock, const gchar *cmd, gint num,
		      gchar **msgid)
{
	gint ok;
	gchar buf[NNTPBUFSIZE];

	if (num > 0)
		ok = nntp_gen_command(sock, buf, "%s %d", cmd, num);
	else
		ok = nntp_gen_command(sock, buf, cmd);

	if (ok != NN_SUCCESS)
		return ok;

	extract_parenthesis(buf, '<', '>');
	if (buf[0] == '\0') {
		log_warning(_("protocol error\n"));
		return NN_PROTOCOL;
	}
	*msgid = g_strdup(buf);

	return NN_SUCCESS;
}

gint nntp_article(NNTPSockInfo *sock, gint num, gchar **msgid)
{
	return nntp_get_article(sock, "ARTICLE", num, msgid);
}

gint nntp_body(NNTPSockInfo *sock, gint num, gchar **msgid)
{
	return nntp_get_article(sock, "BODY", num, msgid);
}

gint nntp_head(NNTPSockInfo *sock, gint num, gchar **msgid)
{
	return nntp_get_article(sock, "HEAD", num, msgid);
}

gint nntp_stat(NNTPSockInfo *sock, gint num, gchar **msgid)
{
	return nntp_get_article(sock, "STAT", num, msgid);
}

gint nntp_next(NNTPSockInfo *sock, gint *num, gchar **msgid)
{
	gint ok;
	gint resp;
	gchar buf[NNTPBUFSIZE];

	ok = nntp_gen_command(sock, buf, "NEXT");

	if (ok != NN_SUCCESS)
		return ok;

	if (sscanf(buf, "%d %d", &resp, num) != 2) {
		log_warning(_("protocol error: %s\n"), buf);
		return NN_PROTOCOL;
	}

	extract_parenthesis(buf, '<', '>');
	if (buf[0] == '\0') {
		log_warning(_("protocol error\n"));
		return NN_PROTOCOL;
	}
	*msgid = g_strdup(buf);

	return NN_SUCCESS;
}

gint nntp_xover(NNTPSockInfo *sock, gint first, gint last)
{
	gint ok;
	gchar buf[NNTPBUFSIZE];

	ok = nntp_gen_command(sock, buf, "XOVER %d-%d", first, last);
	if (ok != NN_SUCCESS)
		return ok;

	return NN_SUCCESS;
}

gint nntp_xhdr(NNTPSockInfo *sock, const gchar *header, gint first, gint last)
{
	gint ok;
	gchar buf[NNTPBUFSIZE];

	ok = nntp_gen_command(sock, buf, "XHDR %s %d-%d", header, first, last);
	if (ok != NN_SUCCESS)
		return ok;

	return NN_SUCCESS;
}

gint nntp_list(NNTPSockInfo *sock)
{
	return nntp_gen_command(sock, NULL, "LIST");
}

gint nntp_post(NNTPSockInfo *sock, FILE *fp)
{
	gint ok;
	gchar buf[NNTPBUFSIZE];

	ok = nntp_gen_command(sock, buf, "POST");
	if (ok != NN_SUCCESS)
		return ok;

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		strretchomp(buf);
		if (buf[0] == '.') {
			if (sock_write(sock->sock, ".", 1) < 0) {
				log_warning(_("Error occurred while posting\n"));
				return NN_SOCKET;
			}
		}

		if (sock_puts(sock->sock, buf) < 0) {
			log_warning(_("Error occurred while posting\n"));
			return NN_SOCKET;
		}
	}

	sock_write(sock->sock, ".\r\n", 3);
	if ((ok = nntp_ok(sock, buf)) != NN_SUCCESS)
		return ok;

	return NN_SUCCESS;
}

gint nntp_newgroups(NNTPSockInfo *sock)
{
	return NN_SUCCESS;
}

gint nntp_newnews(NNTPSockInfo *sock)
{
	return NN_SUCCESS;
}

gint nntp_mode(NNTPSockInfo *sock, gboolean stream)
{
	gint ok;

	if (sock->auth_failed)
		return NN_AUTHREQ;

	ok = nntp_gen_command(sock, NULL, "MODE %s",
			      stream ? "STREAM" : "READER");

	return ok;
}

gint nntp_ok(NNTPSockInfo *sock, gchar *argbuf)
{
	gint ok;
	gchar buf[NNTPBUFSIZE];

	if ((ok = nntp_gen_recv(sock, buf, sizeof(buf))) == NN_SUCCESS) {
		if (strlen(buf) < 3)
			return NN_ERROR;

		if ((buf[0] == '1' || buf[0] == '2' || buf[0] == '3') &&
		    (buf[3] == ' ' || buf[3] == '\0')) {
			if (argbuf)
				strcpy(argbuf, buf);

			if (!strncmp(buf, "381", 3))
				return NN_AUTHCONT;

			return NN_SUCCESS;
		} else if (!strncmp(buf, "480", 3))
			return NN_AUTHREQ;
		else
			return NN_ERROR;
	}

	return ok;
}

static void nntp_gen_send(NNTPSockInfo *sock, const gchar *format, ...)
{
	gchar buf[NNTPBUFSIZE];
	va_list args;

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	if (verbose) {
		if (!g_strncasecmp(buf, "AUTHINFO PASS", 13))
			log_print("NNTP> AUTHINFO PASS ********\n");
		else
			log_print("NNTP> %s\n", buf);
	}

	strcat(buf, "\r\n");
	sock_write(sock->sock, buf, strlen(buf));
}

static gint nntp_gen_recv(NNTPSockInfo *sock, gchar *buf, gint size)
{
	if (sock_gets(sock->sock, buf, size) == -1)
		return NN_SOCKET;

	strretchomp(buf);

	if (verbose)
		log_print("NNTP< %s\n", buf);

	return NN_SUCCESS;
}

static gint nntp_gen_command(NNTPSockInfo *sock, gchar *argbuf,
			     const gchar *format, ...)
{
	gchar buf[NNTPBUFSIZE];
	va_list args;
	gint ok;

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	nntp_gen_send(sock, "%s", buf);
	ok = nntp_ok(sock, argbuf);
	if (ok == NN_AUTHREQ) {
		if (!sock->userid || !sock->passwd) {
			sock->auth_failed = TRUE;
			return ok;
		}

		nntp_gen_send(sock, "AUTHINFO USER %s", sock->userid);
		ok = nntp_ok(sock, NULL);
		if (ok == NN_AUTHCONT) {
			nntp_gen_send(sock, "AUTHINFO PASS %s", sock->passwd);
			ok = nntp_ok(sock, NULL);
		}
		if (ok != NN_SUCCESS) {
			sock->auth_failed = TRUE;
			return ok;
		}

		nntp_gen_send(sock, "%s", buf);
		ok = nntp_ok(sock, argbuf);
	}

	return ok;
}
