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

/*
** Function: hmac_md5
** taken from the file rfc2104.txt
*/
void hmac_md5(guchar *text, gint text_len, guchar *key, gint key_len,
	      guchar *digest)
{
	MD5_CTX context;
	guchar k_ipad[65];	/* inner padding -
				 * key XORd with ipad
				 */
	guchar k_opad[65];	/* outer padding -
				 * key XORd with opad
				 */
	guchar tk[16];
	gint i;

	/* if key is longer than 64 bytes reset it to key=MD5(key) */
	if (key_len > 64) {
		MD5_CTX tctx;

		MD5Init(&tctx);
		MD5Update(&tctx, key, key_len);
		MD5Final(tk, &tctx);

		key = tk;
		key_len = 16;
	}

	/*
	 * the HMAC_MD5 transform looks like:
	 *
	 * MD5(K XOR opad, MD5(K XOR ipad, text))
	 *
	 * where K is an n byte key
	 * ipad is the byte 0x36 repeated 64 times
	 * opad is the byte 0x5c repeated 64 times
	 * and text is the data being protected
	 */

	/* start out by storing key in pads */
	bzero(k_ipad, sizeof k_ipad);
	bzero(k_opad, sizeof k_opad);
	bcopy(key, k_ipad, key_len);
	bcopy(key, k_opad, key_len);

	/* XOR key with ipad and opad values */
	for (i = 0; i < 64; i++) {
		k_ipad[i] ^= 0x36;
		k_opad[i] ^= 0x5c;
	}

	/*
	 * perform inner MD5
	 */
	MD5Init(&context);		     /* init context for 1st
					      * pass */
	MD5Update(&context, k_ipad, 64);     /* start with inner pad */
	MD5Update(&context, text, text_len); /* then text of datagram */
	MD5Final(digest, &context);          /* finish up 1st pass */

	/*
	 * perform outer MD5
	 */
	MD5Init(&context);		     /* init context for 2nd
					      * pass */
	MD5Update(&context, k_opad, 64);     /* start with outer pad */
	MD5Update(&context, digest, 16);     /* then results of 1st
					      * hash */
	MD5Final(digest, &context);	     /* finish up 2nd pass */
}

void md5_hex_hmac(gchar *hexdigest, guchar *text, gint text_len,
		  guchar *key, gint key_len)
{
	guchar digest[16];
	gint i;

	hmac_md5(text, text_len, key, key_len, digest);
	for(i = 0; i < 16; i++)
		sprintf(hexdigest + 2 * i, "%02x", digest[i]);
}

gint esmtp_auth_cram_md5(gint sock)
{
	gchar buf[MSGBUFSIZE];

	g_snprintf(buf, sizeof(buf), "AUTH CRAM-MD5");
	sock_printf(sock, "%s\r\n", buf);

	if (verbose)
		log_print("ESMTP> %s\n", buf);

	return esmtp_ok(sock);
}

gint esmtp_auth_login(gint sock)
{
	gchar buf[MSGBUFSIZE];

	g_snprintf(buf, sizeof(buf), "AUTH LOGIN");
	sock_printf(sock, "%s\r\n", buf);

	if (verbose)
		log_print("ESMTP> %s\n", buf);

	return esmtp_ok(sock);
}

gint esmtp_auth(gint sock, SMTPAuthType authtype,
		const gchar *userid, const gchar *passwd,
		gboolean use_smtp_auth)
{
	gchar buf[MSGBUFSIZE];
	guchar hexdigest[33];
	gchar *challenge, *response, *response64;
	gint challengelen;
	//const gchar delimiters[]=" ";
	gchar *token;

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
			g_snprintf(buf, sizeof(buf), "%s",esmtp_response);
			token = strtok(buf, " ");
			token = strtok(NULL, " ");
			challenge = g_malloc(strlen(token)+1);
			challengelen = from64tobits(challenge,token);
			if (verbose)
				log_print("ESMTP< Decoded: %s\n", challenge);

			g_snprintf(buf, sizeof(buf), "%s", passwd);
			md5_hex_hmac(hexdigest, challenge, challengelen,
				     buf, strlen(passwd));

			response = g_strdup_printf("%s %s", userid, hexdigest);
			if (verbose)
				log_print("ESMTP> Encoded %s\n",response);

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

gint esmtp_ok(gint sock)
{
	while (sock_read(sock, esmtp_response, sizeof(esmtp_response) - 1)
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
			 (esmtp_response[3] == '4' ||
			  esmtp_response[3] == '3' ||
			  esmtp_response[3] == '1'))
			return SM_ERROR;
	}

	return SM_UNRECOVERABLE;
}
