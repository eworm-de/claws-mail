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

#ifndef __PREFS_ACCOUNT_H__
#define __PREFS_ACCOUNT_H__

#include <glib.h>

typedef struct _PrefsAccount	PrefsAccount;

#include "folder.h"

typedef enum {
	A_POP3,
	A_APOP,
	A_RPOP,
	A_IMAP4,
	A_NNTP,
	A_LOCAL
} RecvProtocol;

#if USE_GPGME
typedef enum {
	SIGN_KEY_DEFAULT,
	SIGN_KEY_BY_FROM,
	SIGN_KEY_CUSTOM
} SignKeyType;
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
	gchar *inbox;
	gchar *recv_server;
	gchar *smtp_server;
	gchar *nntp_server;
	gboolean use_nntp_auth;
	gchar *userid;
	gchar *passwd;

	/* Temporarily preserved password */
	gchar *tmp_pass;

	/* Receive */
	gboolean rmmail;
	gboolean getall;
	gboolean recv_at_getall;
	gboolean filter_on_recv;

	/* Send */
	gboolean  add_date;
	gboolean  gen_msgid;
	gboolean  add_customhdr;
	gboolean  set_autocc;
	gchar    *auto_cc;
	gboolean  set_autobcc;
	gchar    *auto_bcc;
	gboolean  set_autoreplyto;
	gchar    *auto_replyto;
	gboolean use_smtp_auth;
	gboolean pop_before_smtp;

	/* Compose */
	gchar *sig_path;

#if USE_GPGME
	/* Privacy */
	SignKeyType sign_key;
	gchar *sign_key_id;
#endif /* USE_GPGME */

	/* Advanced */
	gboolean  set_smtpport;
	gushort   smtpport;
	gboolean  set_popport;
	gushort   popport;
	gboolean  set_nntpport;
	gushort   nntpport;
	gboolean  set_domain;
	gchar    *domain;

	/* Default or not */
	gboolean is_default;
	/* Unique account ID */
	gint account_id;

	RemoteFolder *folder;
};

void prefs_account_read_config		(PrefsAccount	*ac_prefs,
					 const gchar	*label);
void prefs_account_save_config		(PrefsAccount	*ac_prefs);
void prefs_account_save_config_all	(GList		*account_list);
void prefs_account_free			(PrefsAccount	*ac_prefs);
PrefsAccount *prefs_account_open	(PrefsAccount	*ac_prefs);

#endif /* __PREFS_ACCOUNT_H__ */
