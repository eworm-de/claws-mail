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

#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "smtp.h"
#include "esmtp.h"
#include "socket.h"
#include "utils.h"

#define MSGBUFSIZE	8192

static gint verbose = 1;
static gchar smtp_response[MSGBUFSIZE];

gint smtp_helo(SockInfo *sock, const gchar *hostname, gboolean esmtp)
{
	if (esmtp)
		return esmtp_ehlo(sock, hostname);
	else {
		sock_printf(sock, "HELO %s\r\n", hostname);
		if (verbose)
			log_print("SMTP> HELO %s\n", hostname);

		return smtp_ok(sock);
	}
}

gint smtp_from(SockInfo *sock, const gchar *from,
	       const gchar *userid, const gchar *passwd,
	       gboolean use_smtp_auth)
{
	gchar buf[MSGBUFSIZE];
	SMTPAuthType authtype;

	if (use_smtp_auth) {
		/* exist AUTH-Type CRAM_MD5 */
		if (!smtp_auth_methods[SMTPAUTH_CRAM_MD5]
		    || esmtp_auth_cram_md5(sock) == SM_ERROR) {
			/* exist AUTH-Type LOGIN */
			if (!smtp_auth_methods[SMTPAUTH_LOGIN]
			    || esmtp_auth_login(sock) == SM_ERROR)
				return SM_ERROR;
			else
				authtype = SMTPAUTH_LOGIN;
		} else
			authtype = SMTPAUTH_CRAM_MD5;

		if (esmtp_auth(sock, authtype, userid, passwd) != SM_OK)
			return SM_AUTHFAIL;
	}

	if (strchr(from, '<'))
		g_snprintf(buf, sizeof(buf), "MAIL FROM: %s", from);
	else
		g_snprintf(buf, sizeof(buf), "MAIL FROM: <%s>", from);

	sock_printf(sock, "%s\r\n", buf);
	if (verbose)
		log_print("SMTP> %s\n", buf);

	return smtp_ok(sock);
}

gint smtp_rcpt(SockInfo *sock, const gchar *to)
{
	gchar buf[MSGBUFSIZE];

	if (strchr(to, '<'))
		g_snprintf(buf, sizeof(buf), "RCPT TO: %s", to);
	else
		g_snprintf(buf, sizeof(buf), "RCPT TO: <%s>", to);

	sock_printf(sock, "%s\r\n", buf);
	if (verbose)
		log_print("SMTP> %s\n", buf);

	return smtp_ok(sock);
}

gint smtp_data(SockInfo *sock)
{
	sock_printf(sock, "DATA\r\n");
	if (verbose)
		log_print("SMTP> DATA\n");

	return smtp_ok(sock);
}

gint smtp_rset(SockInfo *sock)
{
	sock_printf(sock, "RSET\r\n");
	if (verbose)
		log_print("SMTP> RSET\n");

	return smtp_ok(sock);
}

gint smtp_quit(SockInfo *sock)
{
	sock_printf(sock, "QUIT\r\n");
	if (verbose)
		log_print("SMTP> QUIT\n");

	return smtp_ok(sock);
}

gint smtp_eom(SockInfo *sock)
{
	sock_printf(sock, ".\r\n");
	if (verbose)
		log_print("SMTP> . (EOM)\n");

	return smtp_ok(sock);
}

gint smtp_ok(SockInfo *sock)
{
	while ((sock_gets(sock, smtp_response, sizeof(smtp_response) - 1))
	       != -1) {
		if (strlen(smtp_response) < 4)
			return SM_ERROR;
		strretchomp(smtp_response);

		if (verbose)
			log_print("SMTP< %s\n", smtp_response);

		if ((smtp_response[0] == '1' || smtp_response[0] == '2' ||
		     smtp_response[0] == '3') &&
		     (smtp_response[3] == ' ' || smtp_response[3] == '\0'))
			return SM_OK;
		else if (smtp_response[3] != '-')
			return SM_ERROR;
		else if (smtp_response[0] == '5' &&
			 smtp_response[1] == '0' &&
			 (smtp_response[2] == '4' ||
			  smtp_response[2] == '3' ||
			  smtp_response[2] == '1'))
			return SM_ERROR;
	}

	return SM_UNRECOVERABLE;
}
