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

#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "esmtp.h"
#include "smtp.h"
#include "socket.h"
#include "utils.h"
#include "md5.h"
#include "base64.h"

#define MSGBUFSIZE	8192

static gint verbose = 1;
static gchar esmtp_response[MSGBUFSIZE];

gint esmtp_ehlo(SockInfo *sock, const gchar *hostname)
{
	sock_printf(sock, "EHLO %s\r\n", hostname);
	if (verbose)
		log_print("ESMTP> EHLO %s\n", hostname);

	return esmtp_ok(sock);
}

gint esmtp_starttls(SockInfo *sock)
{
	sock_printf(sock, "STARTTLS\r\n");
	if (verbose)
		log_print("ESMTP> STARTTLS\n");

	return esmtp_ok(sock);
}

gint esmtp_auth_cram_md5(SockInfo *sock)
{
	sock_printf(sock, "AUTH CRAM-MD5\r\n");
	if (verbose)
		log_print("ESMTP> AUTH CRAM-MD5\n");

	return esmtp_ok(sock);
}

gint esmtp_auth_login(SockInfo *sock)
{
	sock_printf(sock, "AUTH LOGIN\r\n");
	if (verbose)
		log_print("ESMTP> AUTH LOGIN\n");

	return esmtp_ok(sock);
}

gint esmtp_auth(SockInfo *sock, SMTPAuthType authtype,
		const gchar *userid, const gchar *passwd)
{
	gchar buf[MSGBUFSIZE];
	guchar hexdigest[33];
	gchar *challenge, *response, *response64;
	gint challengelen;

	switch (authtype) {
	case SMTPAUTH_LOGIN:
		if (!strncmp(esmtp_response, "334 ", 4))
			to64frombits(buf, userid, strlen(userid));
		else
			/* Server rejects AUTH */
			g_snprintf(buf, sizeof(buf), "*");

		sock_printf(sock, "%s\r\n", buf);
		if (verbose) log_print("ESMTP> USERID\n");

		esmtp_ok(sock); /* to read the answer from the server */

		if (!strncmp(esmtp_response, "334 ", 4))
			to64frombits(buf, passwd, strlen(passwd));
		else
			/* Server rejects AUTH */
			g_snprintf(buf, sizeof(buf), "*");

		sock_printf(sock, "%s\r\n", buf);
		if (verbose) log_print("ESMTP> PASSWORD\n");
		break;
	case SMTPAUTH_CRAM_MD5:
		if (!strncmp(esmtp_response, "334 ", 4)) {
			/* remove 334 from esmtp_response */
			gchar *p = esmtp_response + 4;

			challenge = g_malloc(strlen(p) + 1);
			challengelen = from64tobits(challenge, p);
			challenge[challengelen] = '\0';
			if (verbose)
				log_print("ESMTP< [Decoded: %s]\n", challenge);

			g_snprintf(buf, sizeof(buf), "%s", passwd);
			md5_hex_hmac(hexdigest, challenge, challengelen,
				     buf, strlen(passwd));
			g_free(challenge);

			response = g_strdup_printf("%s %s", userid, hexdigest);
			if (verbose)
				log_print("ESMTP> [Encoded: %s]\n", response);

			response64 = g_malloc((strlen(response) + 3) * 2 + 1);
			to64frombits(response64, response, strlen(response));
			g_free(response);

			sock_printf(sock, "%s\r\n", response64);
			if (verbose) log_print("ESMTP> %s\n", response64);
			g_free(response64);
		} else {
			/* Server rejects AUTH */
			g_snprintf(buf, sizeof(buf), "*");
			sock_printf(sock, "%s\r\n", buf);
			if (verbose)
				log_print("ESMTP> %s\n",buf);
		}
		break;
	case SMTPAUTH_DIGEST_MD5:
        default:
        	/* stop esmtp_auth when no correct authtype */
		g_snprintf(buf, sizeof(buf), "*");
		sock_printf(sock, "%s\r\n", buf);
		if (verbose) log_print("ESMTP> %s\n", buf);
		break;
	}

	return esmtp_ok(sock);
}

gint esmtp_ok(SockInfo *sock)
{
	while (sock_gets(sock, esmtp_response, sizeof(esmtp_response) - 1)
	       != -1) {
		if (strlen(esmtp_response) < 4)
			return SM_ERROR;
		strretchomp(esmtp_response);

		if (verbose)
			log_print("ESMTP< %s\n", esmtp_response);

		if ((esmtp_response[0] == '1' || esmtp_response[0] == '2' ||
		     esmtp_response[0] == '3') && esmtp_response[3] == ' ')
			return SM_OK;
		else if (esmtp_response[3] != '-')
			return SM_ERROR;
		else if (esmtp_response[0] == '5' &&
			 esmtp_response[1] == '0' &&
			 (esmtp_response[2] == '4' ||
			  esmtp_response[2] == '3' ||
			  esmtp_response[2] == '1'))
			return SM_ERROR;
	}

	return SM_UNRECOVERABLE;
}
