/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999,2000 Hiroyuki Yamamoto
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
#include "utils.h"

static gint verbose = 1;

static void nntp_gen_send(gint sock, const gchar *format, ...);
static gint nntp_gen_recv(gint sock, gchar *buf, gint size);

gint nntp_open(const gchar *server, gushort port, gchar *buf)
{
	SockInfo *sockinfo;
	gint sock;

	if ((sockinfo = sock_connect(server, port)) == NULL) {
		log_warning(_("Can't connect to NNTP server: %s:%d\n"),
			    server, port);
		return -1;
	}
	sock = sockinfo->sock;
	sock_sockinfo_free(sockinfo);

	if (nntp_ok(sock, buf) == NN_SUCCESS)
		return sock;
	else {
		sock_close(sock);
		return -1;
	}
}

gint nntp_group(gint sock, const gchar *group,
		gint *num, gint *first, gint *last)
{
	gint ok;
	gint resp;
	gchar buf[NNTPBUFSIZE];

	nntp_gen_send(sock, "GROUP %s", group);

	if ((ok = nntp_ok(sock, buf)) != NN_SUCCESS)
		return ok;

	if (sscanf(buf, "%d %d %d %d", &resp, num, first, last)
	    != 4) {
		log_warning(_("protocol error: %s\n"), buf);
		return NN_PROTOCOL;
	}

	return NN_SUCCESS;
}

gint nntp_get_article(gint sock, const gchar *cmd, gint num, gchar **msgid)
{
	gint ok;
	gchar buf[NNTPBUFSIZE];

	if (num > 0)
		nntp_gen_send(sock, "%s %d", cmd, num);
	else
		nntp_gen_send(sock, cmd);

	if ((ok = nntp_ok(sock, buf)) != NN_SUCCESS)
		return ok;

	extract_parenthesis(buf, '<', '>');
	if (buf[0] == '\0') {
		log_warning(_("protocol error\n"));
		return NN_PROTOCOL;
	}
	*msgid = g_strdup(buf);

	return NN_SUCCESS;
}

gint nntp_article(gint sock, gint num, gchar **msgid)
{
	return nntp_get_article(sock, "ARTICLE", num, msgid);
}

gint nntp_body(gint sock, gint num, gchar **msgid)
{
	return nntp_get_article(sock, "BODY", num, msgid);
}

gint nntp_head(gint sock, gint num, gchar **msgid)
{
	return nntp_get_article(sock, "HEAD", num, msgid);
}

gint nntp_stat(gint sock, gint num, gchar **msgid)
{
	return nntp_get_article(sock, "STAT", num, msgid);
}

gint nntp_next(gint sock, gint *num, gchar **msgid)
{
	gint ok;
	gint resp;
	gchar buf[NNTPBUFSIZE];

	nntp_gen_send(sock, "NEXT");

	if ((ok = nntp_ok(sock, buf)) != NN_SUCCESS)
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

gint nntp_xover(gint sock, gint first, gint last)
{
	gint ok;
	gchar buf[NNTPBUFSIZE];

	nntp_gen_send(sock, "XOVER %d-%d", first, last);
	if ((ok = nntp_ok(sock, buf)) != NN_SUCCESS)
		return ok;

	return NN_SUCCESS;
}

gint nntp_post(gint sock, FILE *fp)
{
	gint ok;
	gchar buf[NNTPBUFSIZE];

	nntp_gen_send(sock, "POST");
	if ((ok = nntp_ok(sock, buf)) != NN_SUCCESS)
		return ok;

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		strretchomp(buf);
		if (buf[0] == '.') {
			if (sock_write(sock, ".", 1) < 0) {
				log_warning(_("Error occurred while posting\n"));
				return NN_SOCKET;
			}
		}

		if (sock_puts(sock, buf) < 0) {
			log_warning(_("Error occurred while posting\n"));
			return NN_SOCKET;
		}
	}

	sock_write(sock, ".\r\n", 3);
	if ((ok = nntp_ok(sock, buf)) != NN_SUCCESS)
		return ok;

	return NN_SUCCESS;
}

gint nntp_newgroups(gint sock)
{
	return NN_SUCCESS;
}

gint nntp_newnews(gint sock)
{
	return NN_SUCCESS;
}

gint nntp_mode(gint sock, gboolean stream)
{
	gint ok;

	nntp_gen_send(sock, "MODE %s", stream ? "STREAM" : "READER");
	ok = nntp_ok(sock, NULL);

	return ok;
}

gint nntp_ok(gint sock, gchar *argbuf)
{
	gint ok;
	gchar buf[NNTPBUFSIZE];

	if ((ok = nntp_gen_recv(sock, buf, sizeof(buf))) == NN_SUCCESS) {
		if (strlen(buf) < 4)
			return NN_ERROR;

		if ((buf[0] == '1' || buf[0] == '2' || buf[0] == '3') &&
		    buf[3] == ' ') {
			if (argbuf)
				strcpy(argbuf, buf);

			return NN_SUCCESS;
		} else
			return NN_ERROR;
	}

	return ok;
}

static void nntp_gen_send(gint sock, const gchar *format, ...)
{
	gchar buf[NNTPBUFSIZE];
	va_list args;

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	if (verbose)
		log_print("NNTP> %s\n", buf);

	strcat(buf, "\r\n");
	sock_write(sock, buf, strlen(buf));
}

static gint nntp_gen_recv(gint sock, gchar *buf, gint size)
{
	if (sock_read(sock, buf, size) == -1)
		return NN_SOCKET;

	strretchomp(buf);

	if (verbose)
		log_print("NNTP< %s\n", buf);

	return NN_SUCCESS;
}
