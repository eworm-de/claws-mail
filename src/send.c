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

#include "defs.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "intl.h"
#include "send.h"
#include "socket.h"
#include "smtp.h"
#include "prefs_account.h"
#include "account.h"
#include "compose.h"
#include "procmsg.h"
#include "procheader.h"
#include "utils.h"

#define SMTP_PORT	25

static gint send_message_smtp	(GSList *to_list, const gchar *from,
				 const gchar *server, gushort port,
				 const gchar *domain, const gchar *userid,
				 const gchar *passwd, gboolean use_smtp_auth,
				 FILE *fp);
static SockInfo *send_smtp_open	(const gchar *server, gushort port,
				 const gchar *domain, gboolean use_smtp_auth);
static gint send_message_with_command(GSList *to_list, gchar * mailcmd,
				      FILE * fp);

#define SEND_EXIT_IF_ERROR(f, s) \
{ \
	if (!(f)) { \
		log_warning("Error occurred while %s\n", s); \
		sock_close(smtp_sock); \
		smtp_sock = NULL; \
		return -1; \
	} \
}

#define SEND_EXIT_IF_NOTOK(f, s) \
{ \
	if ((f) != SM_OK) { \
		log_warning("Error occurred while %s\n", s); \
		if (smtp_quit(smtp_sock) != SM_OK) \
			log_warning("Error occurred while sending QUIT\n"); \
		sock_close(smtp_sock); \
		smtp_sock = NULL; \
		return -1; \
	} \
}

gint send_message(const gchar *file, PrefsAccount *ac_prefs, GSList *to_list)
{
	FILE *fp;
	gint val;
	gushort port;
	gchar *domain;

	g_return_val_if_fail(file != NULL, -1);
	g_return_val_if_fail(ac_prefs != NULL, -1);
	g_return_val_if_fail(to_list != NULL, -1);

	if ((fp = fopen(file, "r")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	if (ac_prefs->protocol == A_LOCAL_CMD)
		{
			val = send_message_with_command(to_list,
							ac_prefs->mail_command,
							fp);
		}
	else
		{
			port = ac_prefs->set_smtpport
				? ac_prefs->smtpport : SMTP_PORT;
			domain = ac_prefs->set_domain
				? ac_prefs->domain : NULL;
			
			val = send_message_smtp(to_list, ac_prefs->address,
						ac_prefs->smtp_server, port,
						domain, ac_prefs->userid,
						ac_prefs->passwd,
						ac_prefs->use_smtp_auth, fp);
		}

	fclose(fp);
	return val;
}

enum
{
	Q_SENDER     = 0,
	Q_SMTPSERVER = 1,
	Q_RECIPIENTS = 2
};

static gint send_message_with_command(GSList *to_list, gchar * mailcmd,
				      FILE * fp)
{
	FILE * p;
	int len;
	gchar * cmdline;
	GSList *cur;
	gchar buf[BUFFSIZE];

	len = strlen(mailcmd);
	for (cur = to_list; cur != NULL; cur = cur->next)
		len += strlen((gchar *)cur->data) + 1;

	cmdline = g_new(gchar, len + 1);
	strcpy(cmdline, mailcmd);

	for (cur = to_list; cur != NULL; cur = cur->next)
		{
			cmdline = strncat(cmdline, " ", len);
			cmdline = strncat(cmdline, (gchar *)cur->data, len);
		}

	log_message(_("Using command to send mail: %s ...\n"), cmdline);

	p = popen(cmdline, "w");
	if (p != NULL)
		{
			while (fgets(buf, sizeof(buf), fp) != NULL) {
				strretchomp(buf);
				
				/* escape when a dot appears on the top */
				if (buf[0] == '.')
					fputs(".", p);
				
				fputs(buf, p);
				fputs("\n", p);
			}
		}
	pclose(p);

	log_message(_("Mail sent successfully ...\n"));

	return 0;
}

gint send_message_queue(const gchar *file)
{
	static HeaderEntry qentry[] = {{"S:",   NULL, FALSE},
				       {"SSV:", NULL, FALSE},
				       {"R:",   NULL, FALSE},
				       {NULL,   NULL, FALSE}};
	FILE *fp;
	gint val;
	gchar *from = NULL;
	gchar *server = NULL;
	GSList *to_list = NULL;
	gchar buf[BUFFSIZE];
	gint hnum;

	g_return_val_if_fail(file != NULL, -1);

	if ((fp = fopen(file, "r")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	while ((hnum = procheader_get_one_field(buf, sizeof(buf), fp, qentry))
	       != -1) {
		gchar *p = buf + strlen(qentry[hnum].name);

		switch (hnum) {
		case Q_SENDER:
			if (!from) from = g_strdup(p);
			break;
		case Q_SMTPSERVER:
			if (!server) server = g_strdup(p);
			break;
		case Q_RECIPIENTS:
			to_list = address_list_append(to_list, p);
			break;
		default:
		}
	}

	if (!to_list || !from || !server) {
		g_warning(_("Queued message header is broken.\n"));
		val = -1;
	} else {
		gushort port;
		gchar *domain;
		PrefsAccount *ac;

		ac = account_find_from_smtp_server(from, server);
		if (!ac) {
			g_warning(_("Account not found. Using current account...\n"));
			ac = cur_account;
		}

		if (ac) {
			port = ac->set_smtpport ? ac->smtpport : SMTP_PORT;
			domain = ac->set_domain ? ac->domain : NULL;
			val = send_message_smtp
				(to_list, from, server, port, domain,
				 ac->userid, ac->passwd, ac->use_smtp_auth, fp);
		} else {
			g_warning(_("Account not found.\n"));
			val = send_message_smtp
				(to_list, from, server, SMTP_PORT, NULL,
				 NULL, NULL, FALSE, fp);
		}
	}

	slist_free_strings(to_list);
	g_slist_free(to_list);
	g_free(from);
	g_free(server);
	fclose(fp);

	return val;
}

static gint send_message_smtp(GSList *to_list, const gchar *from,
			      const gchar *server, gushort port,
			      const gchar *domain, const gchar *userid,
			      const gchar* passwd, gboolean use_smtp_auth,
			      FILE *fp)
{
	SockInfo *smtp_sock;
	gchar buf[BUFFSIZE];
	GSList *cur;

	g_return_val_if_fail(to_list != NULL, -1);
	g_return_val_if_fail(from != NULL, -1);
	g_return_val_if_fail(server != NULL, -1);
	g_return_val_if_fail(fp != NULL, -1);

	log_message(_("Connecting to SMTP server: %s ...\n"), server);

	SEND_EXIT_IF_ERROR((smtp_sock = send_smtp_open
				(server, port, domain, use_smtp_auth)),
			   "connecting to server");
	SEND_EXIT_IF_NOTOK
		(smtp_from(smtp_sock, from, userid, passwd, use_smtp_auth),
		 "sending MAIL FROM");
	for (cur = to_list; cur != NULL; cur = cur->next)
		SEND_EXIT_IF_NOTOK(smtp_rcpt(smtp_sock, (gchar *)cur->data),
				   "sending RCPT TO");
	SEND_EXIT_IF_NOTOK(smtp_data(smtp_sock), "sending DATA");

	/* send main part */
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		strretchomp(buf);

		/* escape when a dot appears on the top */
		if (buf[0] == '.')
			SEND_EXIT_IF_ERROR(sock_write(smtp_sock, ".", 1),
					   "sending data");

		SEND_EXIT_IF_ERROR(sock_puts(smtp_sock, buf), "sending data");
	}

	SEND_EXIT_IF_NOTOK(smtp_eom(smtp_sock), "terminating data");
	SEND_EXIT_IF_NOTOK(smtp_quit(smtp_sock), "sending QUIT");

	sock_close(smtp_sock);
	return 0;
}

static SockInfo *send_smtp_open(const gchar *server, gushort port,
			   const gchar *domain, gboolean use_smtp_auth)
{
	SockInfo *sock;
	gint val;

	g_return_val_if_fail(server != NULL, NULL);

	if ((sock = sock_connect(server, port)) == NULL) {
		log_warning(_("Can't connect to SMTP server: %s:%d\n"),
			    server, port);
		return NULL;
	}

	if (smtp_ok(sock) == SM_OK) {
		val = smtp_helo(sock, domain ? domain : get_domain_name(),
				use_smtp_auth);
		if (val == SM_OK) return sock;
	}

	log_warning(_("Error occurred while sending HELO\n"));
	sock_close(sock);

	return NULL;
}
