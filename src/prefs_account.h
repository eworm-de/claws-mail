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

#ifndef __PREFS_ACCOUNT_H__
#define __PREFS_ACCOUNT_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

typedef struct _PrefsAccount	PrefsAccount;

#include "folder.h"
#include "smtp.h"

#ifdef USE_GPGME
#  include "rfc2015.h"
#endif

typedef enum {
	A_POP3,
	A_APOP,
	A_RPOP,
	A_IMAP4,
	A_NNTP,
	A_LOCAL
} RecvProtocol;

typedef enum {
	/* login and retrieve messages, as before */
	STYPE_NORMAL,
	/* send TOP to server and retrieve all available Headers */
	STYPE_PREVIEW_ALL,
	/* send TOP to server and retrieve new Headers */
	STYPE_PREVIEW_NEW,
	/* delete selected Headers on Server */
	STYPE_DELETE, 
	/* download + remove Mail from Server */
	STYPE_DOWNLOAD,
	/* just login (pop before smtp) */
	STYPE_POP_BEFORE_SMTP,
} Pop3SessionType;

#if USE_GPGME
typedef enum {
	SIGN_KEY_DEFAULT,
	SIGN_KEY_BY_FROM,
	SIGN_KEY_CUSTOM
} SignKeyType;

typedef enum {
	GNUPG_MODE_DETACH,
	GNUPG_MODE_INLINE
} DefaultGnuPGMode;
#endif /* USE_GPGME */

struct _PrefsAccount
{
	gchar *account_name;

	/* Personal info */
	gchar *name;
	gchar *address;
	gchar *organization;

	/* Server info */
	RecvProtocol protocol;
	gchar *recv_server;
	gchar *smtp_server;
	gchar *nntp_server;
	gboolean use_nntp_auth;
	gchar *userid;
	gchar *passwd;

	gchar * local_mbox;
	gboolean use_mail_command;
	gchar * mail_command;

#if USE_SSL
	SSLType ssl_pop;
	SSLType ssl_imap;
	SSLType ssl_nntp;
	SSLType ssl_smtp;
#endif /* USE_SSL */

	/* Temporarily preserved password */
	gchar *tmp_pass;

	/* Receive */
	gboolean rmmail;
	gint msg_leave_time;
	gboolean getall;
	gboolean recv_at_getall;
	gboolean sd_rmmail_on_download;
	gboolean sd_filter_on_recv;
	gboolean enable_size_limit;
	gint size_limit;
	gboolean filter_on_recv;
	gchar *inbox;

	/* selective Download */
	gint   session;
	GSList *msg_list;

	/* Send */
	gboolean add_date;
	gboolean gen_msgid;
	gboolean add_customhdr;
	gboolean use_smtp_auth;
	SMTPAuthType smtp_auth_type;
	gchar *smtp_userid;
	gchar *smtp_passwd;

	/* Temporarily preserved password */
	gchar *tmp_smtp_pass;

	gboolean pop_before_smtp;
	gint pop_before_smtp_timeout;
	time_t last_pop_login_time;

	GSList *customhdr_list;

	/* Compose */
	gchar *sig_path;
	gboolean  set_autocc;
	gchar    *auto_cc;
	gboolean  set_autobcc;
	gchar    *auto_bcc;
	gboolean  set_autoreplyto;
	gchar    *auto_replyto;

#if USE_GPGME
	/* Privacy */
	gboolean default_encrypt;
	gboolean default_sign;
	gboolean default_gnupg_mode;
	SignKeyType sign_key;
	gchar *sign_key_id;
#endif /* USE_GPGME */

	/* Advanced */
	gboolean  set_smtpport;
	gushort   smtpport;
	gboolean  set_popport;
	gushort   popport;
	gboolean  set_imapport;
	gushort   imapport;
	gboolean  set_nntpport;
	gushort   nntpport;
	gboolean  set_domain;
	gchar    *domain;
	gboolean  mark_crosspost_read;
	gint	  crosspost_col;

	/* Use this command to open a socket, rather than doing so
	 * directly.  Good if you want to perhaps use a special socks
	 * tunnel command, or run IMAP-over-SSH.  In this case the
	 * server, port etc are only for the user's own information
	 * and are not used.  username and password are used to
	 * authenticate the account only if necessary, since some
	 * tunnels will implicitly authenticate by running e.g. imapd
	 * as a particular user. */
	gboolean  set_tunnelcmd;
	gchar     *tunnelcmd;

	gchar *imap_dir;

	gboolean set_sent_folder;
	gchar *sent_folder;
	gboolean set_draft_folder;
	gchar *draft_folder;
	gboolean set_trash_folder;
	gchar *trash_folder;

	/* Default or not */
	gboolean is_default;
	/* Unique account ID */
	gint account_id;

	RemoteFolder *folder;
};

PrefsAccount *prefs_account_new		(void);

void prefs_account_read_config		(PrefsAccount	*ac_prefs,
					 const gchar	*label);
void prefs_account_save_config_all	(GList		*account_list);

void prefs_account_free			(PrefsAccount	*ac_prefs);

PrefsAccount *prefs_account_open	(PrefsAccount	*ac_prefs);

#endif /* __PREFS_ACCOUNT_H__ */
