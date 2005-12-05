/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/filesel.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "main.h"
#include "prefs_gtk.h"
#include "prefs_account.h"
#include "prefs_common.h"
#include "prefs_customheader.h"
#include "account.h"
#include "mainwindow.h"
#include "manage_window.h"
#include "foldersel.h"
#include "inc.h"
#include "menu.h"
#include "gtkutils.h"
#include "utils.h"
#include "alertpanel.h"
#include "colorlabel.h"
#include "smtp.h"
#include "imap.h"
#include "remotefolder.h"
#include "base64.h"

static gboolean cancelled;
static gboolean new_account;

static PrefsAccount tmp_ac_prefs;

static GtkWidget *notebook;
static GtkWidget *sigfile_radiobtn;
static GtkWidget *sigcmd_radiobtn;
static GtkWidget *entry_sigpath;
static GtkWidget *signature_browse_button;

static GSList *prefs_pages = NULL;

static struct Basic {
	GtkWidget *acname_entry;
	GtkWidget *default_chkbtn;

	GtkWidget *name_entry;
	GtkWidget *addr_entry;
	GtkWidget *org_entry;

	GtkWidget *serv_frame;
	GtkWidget *serv_table;
	GtkWidget *protocol_optmenu;
	GtkWidget *recvserv_label;
	GtkWidget *smtpserv_label;
	GtkWidget *nntpserv_label;
	GtkWidget *localmbox_label;
	GtkWidget *mailcmd_label;
	GtkWidget *recvserv_entry;
	GtkWidget *smtpserv_entry;
	GtkWidget *nntpserv_entry;
	GtkWidget *nntpauth_chkbtn;
	GtkWidget *nntpauth_onconnect_chkbtn;
	GtkWidget *localmbox_entry;
	GtkWidget *mailcmd_chkbtn;
	GtkWidget *mailcmd_entry;
	GtkWidget *uid_label;
	GtkWidget *pass_label;
	GtkWidget *uid_entry;
	GtkWidget *pass_entry;
} basic;

static struct Receive {
	GtkWidget *pop3_frame;
	GtkWidget *use_apop_chkbtn;
	GtkWidget *rmmail_chkbtn;
	GtkWidget *leave_time_entry;
	GtkWidget *getall_chkbtn;
	GtkWidget *size_limit_chkbtn;
	GtkWidget *size_limit_entry;
	GtkWidget *inbox_label;
	GtkWidget *inbox_entry;
	GtkWidget *inbox_btn;

	GtkWidget *local_frame;
	GtkWidget *local_inbox_label;
	GtkWidget *local_inbox_entry;
	GtkWidget *local_inbox_btn;

	GtkWidget *filter_on_recv_chkbtn;
	GtkWidget *recvatgetall_chkbtn;
	
	GtkWidget *imap_frame;
	GtkWidget *imap_auth_type_optmenu;
	GtkWidget *imapdir_label;
	GtkWidget *imapdir_entry;

	GtkWidget *frame_maxarticle;
	GtkWidget *maxarticle_label;
	GtkWidget *maxarticle_spinbtn;
	GtkObject *maxarticle_spinbtn_adj;
} receive;

static struct Send {
	GtkWidget *date_chkbtn;
	GtkWidget *msgid_chkbtn;
	GtkWidget *customhdr_chkbtn;
	GtkWidget *smtp_auth_chkbtn;
	GtkWidget *smtp_auth_type_optmenu;
	GtkWidget *smtp_uid_entry;
	GtkWidget *smtp_pass_entry;
	GtkWidget *pop_bfr_smtp_chkbtn;
	GtkWidget *pop_bfr_smtp_tm_entry;
} p_send;

static struct Compose {
	GtkWidget *sigfile_radiobtn;
	GtkWidget *entry_sigpath;
	GtkWidget *checkbtn_autosig;
	GtkWidget *entry_sigsep;
	GtkWidget *autocc_chkbtn;
	GtkWidget *autocc_entry;
	GtkWidget *autobcc_chkbtn;
	GtkWidget *autobcc_entry;
	GtkWidget *autoreplyto_chkbtn;
	GtkWidget *autoreplyto_entry;
} compose;

static struct Privacy {
	GtkWidget *default_privacy_system;
	GtkWidget *default_encrypt_chkbtn;
	GtkWidget *default_encrypt_reply_chkbtn;
	GtkWidget *default_sign_chkbtn;
	GtkWidget *save_clear_text_chkbtn;
} privacy;

#if USE_OPENSSL
static struct SSLPrefs {
	GtkWidget *pop_frame;
	GtkWidget *pop_nossl_radiobtn;
	GtkWidget *pop_ssltunnel_radiobtn;
	GtkWidget *pop_starttls_radiobtn;

	GtkWidget *imap_frame;
	GtkWidget *imap_nossl_radiobtn;
	GtkWidget *imap_ssltunnel_radiobtn;
	GtkWidget *imap_starttls_radiobtn;

	GtkWidget *nntp_frame;
	GtkWidget *nntp_nossl_radiobtn;
	GtkWidget *nntp_ssltunnel_radiobtn;

	GtkWidget *send_frame;
	GtkWidget *smtp_nossl_radiobtn;
	GtkWidget *smtp_ssltunnel_radiobtn;
	GtkWidget *smtp_starttls_radiobtn;

	GtkWidget *use_nonblocking_ssl_chkbtn;
} ssl;
#endif /* USE_OPENSSL */

static struct Advanced {
	GtkWidget *smtpport_chkbtn;
	GtkWidget *smtpport_entry;
	GtkWidget *popport_hbox;
	GtkWidget *popport_chkbtn;
	GtkWidget *popport_entry;
	GtkWidget *imapport_hbox;
	GtkWidget *imapport_chkbtn;
	GtkWidget *imapport_entry;
	GtkWidget *nntpport_hbox;
	GtkWidget *nntpport_chkbtn;
	GtkWidget *nntpport_entry;
	GtkWidget *domain_chkbtn;
	GtkWidget *domain_entry;
	GtkWidget *crosspost_chkbtn;
 	GtkWidget *crosspost_colormenu;

	GtkWidget *tunnelcmd_chkbtn;
	GtkWidget *tunnelcmd_entry;

	GtkWidget *sent_folder_chkbtn;
	GtkWidget *sent_folder_entry;
	GtkWidget *queue_folder_chkbtn;
	GtkWidget *queue_folder_entry;
	GtkWidget *draft_folder_chkbtn;
	GtkWidget *draft_folder_entry;
	GtkWidget *trash_folder_chkbtn;
	GtkWidget *trash_folder_entry;
} advanced;

static void prefs_account_fix_size			(void);

static void prefs_account_protocol_set_data_from_optmenu(PrefParam *pparam);
static void prefs_account_protocol_set_optmenu		(PrefParam *pparam);
static void prefs_account_protocol_activated		(GtkMenuItem *menuitem);

static void prefs_account_set_string_from_optmenu	(PrefParam *pparam);
static void prefs_account_set_optmenu_from_string	(PrefParam *pparam);

static void prefs_account_imap_auth_type_set_data_from_optmenu
							(PrefParam *pparam);
static void prefs_account_imap_auth_type_set_optmenu	(PrefParam *pparam);
static void prefs_account_smtp_auth_type_set_data_from_optmenu
							(PrefParam *pparam);
static void prefs_account_smtp_auth_type_set_optmenu	(PrefParam *pparam);

static void prefs_account_enum_set_data_from_radiobtn	(PrefParam *pparam);
static void prefs_account_enum_set_radiobtn		(PrefParam *pparam);

static void prefs_account_crosspost_set_data_from_colormenu(PrefParam *pparam);
static void prefs_account_crosspost_set_colormenu(PrefParam *pparam);

static void prefs_account_nntpauth_toggled(GtkToggleButton *button,
					   gpointer user_data);
static void prefs_account_mailcmd_toggled(GtkToggleButton *button,
					  gpointer user_data);

static gchar *privacy_prefs;

static PrefParam param[] = {
	/* Basic */
	{"account_name", NULL, &tmp_ac_prefs.account_name, P_STRING,
	 &basic.acname_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"is_default", "FALSE", &tmp_ac_prefs.is_default, P_BOOL,
	 &basic.default_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"name", NULL, &tmp_ac_prefs.name, P_STRING,
	 &basic.name_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"address", NULL, &tmp_ac_prefs.address, P_STRING,
	 &basic.addr_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"organization", NULL, &tmp_ac_prefs.organization, P_STRING,
	 &basic.org_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"protocol", NULL, &tmp_ac_prefs.protocol, P_ENUM,
	 &basic.protocol_optmenu,
	 prefs_account_protocol_set_data_from_optmenu,
	 prefs_account_protocol_set_optmenu},

	{"receive_server", NULL, &tmp_ac_prefs.recv_server, P_STRING,
	 &basic.recvserv_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"smtp_server", NULL, &tmp_ac_prefs.smtp_server, P_STRING,
	 &basic.smtpserv_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"nntp_server", NULL, &tmp_ac_prefs.nntp_server, P_STRING,
	 &basic.nntpserv_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"local_mbox", "/var/mail", &tmp_ac_prefs.local_mbox, P_STRING,
	 &basic.localmbox_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"use_mail_command", "FALSE", &tmp_ac_prefs.use_mail_command, P_BOOL,
	 &basic.mailcmd_chkbtn, prefs_set_data_from_toggle, prefs_set_toggle},

	{"mail_command", DEFAULT_SENDMAIL_CMD, &tmp_ac_prefs.mail_command, P_STRING,
	 &basic.mailcmd_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"use_nntp_auth", "FALSE", &tmp_ac_prefs.use_nntp_auth, P_BOOL,
	 &basic.nntpauth_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	
	{"use_nntp_auth_onconnect", "FALSE", &tmp_ac_prefs.use_nntp_auth_onconnect, P_BOOL,
	 &basic.nntpauth_onconnect_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"user_id", "ENV_USER", &tmp_ac_prefs.userid, P_STRING,
	 &basic.uid_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"password", NULL, &tmp_ac_prefs.passwd, P_PASSWORD,
	 &basic.pass_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"inbox", "#mh/Mailbox/inbox", &tmp_ac_prefs.inbox, P_STRING,
	 &receive.inbox_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"local_inbox", "#mh/Mailbox/inbox", &tmp_ac_prefs.local_inbox, P_STRING,
	 &receive.local_inbox_entry, prefs_set_data_from_entry, prefs_set_entry},

	/* Receive */
	{"use_apop_auth", "FALSE", &tmp_ac_prefs.use_apop_auth, P_BOOL,
	 &receive.use_apop_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"remove_mail", "TRUE", &tmp_ac_prefs.rmmail, P_BOOL,
	 &receive.rmmail_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"message_leave_time", "0", &tmp_ac_prefs.msg_leave_time, P_INT,
	 &receive.leave_time_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"get_all_mail", "FALSE", &tmp_ac_prefs.getall, P_BOOL,
	 &receive.getall_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"enable_size_limit", "FALSE", &tmp_ac_prefs.enable_size_limit, P_BOOL,
	 &receive.size_limit_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"size_limit", "1024", &tmp_ac_prefs.size_limit, P_INT,
	 &receive.size_limit_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"filter_on_receive", "TRUE", &tmp_ac_prefs.filter_on_recv, P_BOOL,
	 &receive.filter_on_recv_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"imap_auth_method", "0", &tmp_ac_prefs.imap_auth_type, P_ENUM,
	 &receive.imap_auth_type_optmenu,
	 prefs_account_imap_auth_type_set_data_from_optmenu,
	 prefs_account_imap_auth_type_set_optmenu},

	{"receive_at_get_all", "TRUE", &tmp_ac_prefs.recv_at_getall, P_BOOL,
	 &receive.recvatgetall_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"max_news_articles", "300", &tmp_ac_prefs.max_articles, P_INT,
	 &receive.maxarticle_spinbtn,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},

	/* Send */
	{"add_date", "TRUE", &tmp_ac_prefs.add_date, P_BOOL,
	 &p_send.date_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"generate_msgid", "TRUE", &tmp_ac_prefs.gen_msgid, P_BOOL,
	 &p_send.msgid_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"add_custom_header", "FALSE", &tmp_ac_prefs.add_customhdr, P_BOOL,
	 &p_send.customhdr_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"use_smtp_auth", "FALSE", &tmp_ac_prefs.use_smtp_auth, P_BOOL,
	 &p_send.smtp_auth_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"smtp_auth_method", "0", &tmp_ac_prefs.smtp_auth_type, P_ENUM,
	 &p_send.smtp_auth_type_optmenu,
	 prefs_account_smtp_auth_type_set_data_from_optmenu,
	 prefs_account_smtp_auth_type_set_optmenu},

	{"smtp_user_id", NULL, &tmp_ac_prefs.smtp_userid, P_STRING,
	 &p_send.smtp_uid_entry, prefs_set_data_from_entry, prefs_set_entry},
	{"smtp_password", NULL, &tmp_ac_prefs.smtp_passwd, P_PASSWORD,
	 &p_send.smtp_pass_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"pop_before_smtp", "FALSE", &tmp_ac_prefs.pop_before_smtp, P_BOOL,
	 &p_send.pop_bfr_smtp_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"pop_before_smtp_timeout", "5", &tmp_ac_prefs.pop_before_smtp_timeout, P_INT,
	 &p_send.pop_bfr_smtp_tm_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	/* Compose */
	{"signature_type", "0", &tmp_ac_prefs.sig_type, P_ENUM,
	 &compose.sigfile_radiobtn,
	 prefs_account_enum_set_data_from_radiobtn,
	 prefs_account_enum_set_radiobtn},
	{"signature_path", "~" G_DIR_SEPARATOR_S DEFAULT_SIGNATURE,
	 &tmp_ac_prefs.sig_path, P_STRING, &compose.entry_sigpath,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"auto_signature", "TRUE", &tmp_ac_prefs.auto_sig, P_BOOL,
	 &compose.checkbtn_autosig,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	 
	{"signature_separator", "-- ", &tmp_ac_prefs.sig_sep, P_STRING,
	 &compose.entry_sigsep, 
	 prefs_set_data_from_entry, prefs_set_entry},

	{"set_autocc", "FALSE", &tmp_ac_prefs.set_autocc, P_BOOL,
	 &compose.autocc_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"auto_cc", NULL, &tmp_ac_prefs.auto_cc, P_STRING,
	 &compose.autocc_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"set_autobcc", "FALSE", &tmp_ac_prefs.set_autobcc, P_BOOL,
	 &compose.autobcc_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"auto_bcc", NULL, &tmp_ac_prefs.auto_bcc, P_STRING,
	 &compose.autobcc_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"set_autoreplyto", "FALSE", &tmp_ac_prefs.set_autoreplyto, P_BOOL,
	 &compose.autoreplyto_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"auto_replyto", NULL, &tmp_ac_prefs.auto_replyto, P_STRING,
	 &compose.autoreplyto_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	/* Privacy */
	{"default_privacy_system", "", &tmp_ac_prefs.default_privacy_system, P_STRING,
	 &privacy.default_privacy_system,
	 prefs_account_set_string_from_optmenu, prefs_account_set_optmenu_from_string},
	{"default_encrypt", "FALSE", &tmp_ac_prefs.default_encrypt, P_BOOL,
	 &privacy.default_encrypt_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"default_encrypt_reply", "TRUE", &tmp_ac_prefs.default_encrypt_reply, P_BOOL,
	 &privacy.default_encrypt_reply_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"default_sign", "FALSE", &tmp_ac_prefs.default_sign, P_BOOL,
	 &privacy.default_sign_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"save_clear_text", "FALSE", &tmp_ac_prefs.save_encrypted_as_clear_text, P_BOOL,
	 &privacy.save_clear_text_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"privacy_prefs", "", &privacy_prefs, P_STRING,
	 NULL, NULL, NULL},
#if USE_OPENSSL
	/* SSL */
	{"ssl_pop", "0", &tmp_ac_prefs.ssl_pop, P_ENUM,
	 &ssl.pop_nossl_radiobtn,
	 prefs_account_enum_set_data_from_radiobtn,
	 prefs_account_enum_set_radiobtn},
	{"ssl_imap", "0", &tmp_ac_prefs.ssl_imap, P_ENUM,
	 &ssl.imap_nossl_radiobtn,
	 prefs_account_enum_set_data_from_radiobtn,
	 prefs_account_enum_set_radiobtn},
	{"ssl_nntp", "0", &tmp_ac_prefs.ssl_nntp, P_ENUM,
	 &ssl.nntp_nossl_radiobtn,
	 prefs_account_enum_set_data_from_radiobtn,
	 prefs_account_enum_set_radiobtn},
	{"ssl_smtp", "0", &tmp_ac_prefs.ssl_smtp, P_ENUM,
	 &ssl.smtp_nossl_radiobtn,
	 prefs_account_enum_set_data_from_radiobtn,
	 prefs_account_enum_set_radiobtn},

	{"use_nonblocking_ssl", "1", &tmp_ac_prefs.use_nonblocking_ssl, P_BOOL,
	 &ssl.use_nonblocking_ssl_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},
#else
	/* SSL */
	{"ssl_pop", "0", &tmp_ac_prefs.ssl_pop, P_ENUM,
	 NULL, NULL, NULL},
	{"ssl_imap", "0", &tmp_ac_prefs.ssl_imap, P_ENUM,
	 NULL, NULL, NULL},
	{"ssl_nntp", "0", &tmp_ac_prefs.ssl_nntp, P_ENUM,
	 NULL, NULL, NULL},
	{"ssl_smtp", "0", &tmp_ac_prefs.ssl_smtp, P_ENUM,
	 NULL, NULL, NULL},

	{"use_nonblocking_ssl", "1", &tmp_ac_prefs.use_nonblocking_ssl, P_BOOL,
	 NULL, NULL, NULL},
#endif /* USE_OPENSSL */

	/* Advanced */
	{"set_smtpport", "FALSE", &tmp_ac_prefs.set_smtpport, P_BOOL,
	 &advanced.smtpport_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"smtp_port", "25", &tmp_ac_prefs.smtpport, P_USHORT,
	 &advanced.smtpport_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"set_popport", "FALSE", &tmp_ac_prefs.set_popport, P_BOOL,
	 &advanced.popport_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"pop_port", "110", &tmp_ac_prefs.popport, P_USHORT,
	 &advanced.popport_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"set_imapport", "FALSE", &tmp_ac_prefs.set_imapport, P_BOOL,
	 &advanced.imapport_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"imap_port", "143", &tmp_ac_prefs.imapport, P_USHORT,
	 &advanced.imapport_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"set_nntpport", "FALSE", &tmp_ac_prefs.set_nntpport, P_BOOL,
	 &advanced.nntpport_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"nntp_port", "119", &tmp_ac_prefs.nntpport, P_USHORT,
	 &advanced.nntpport_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"set_domain", "FALSE", &tmp_ac_prefs.set_domain, P_BOOL,
	 &advanced.domain_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"domain", NULL, &tmp_ac_prefs.domain, P_STRING,
	 &advanced.domain_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"set_tunnelcmd", "FALSE", &tmp_ac_prefs.set_tunnelcmd, P_BOOL,
	 &advanced.tunnelcmd_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"tunnelcmd", NULL, &tmp_ac_prefs.tunnelcmd, P_STRING,
	 &advanced.tunnelcmd_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"mark_crosspost_read", "FALSE", &tmp_ac_prefs.mark_crosspost_read, P_BOOL,
	 &advanced.crosspost_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"crosspost_color", NULL, &tmp_ac_prefs.crosspost_col, P_ENUM,
	 &advanced.crosspost_colormenu,
	 prefs_account_crosspost_set_data_from_colormenu,
	 prefs_account_crosspost_set_colormenu},

	{"imap_directory", NULL, &tmp_ac_prefs.imap_dir, P_STRING,
	 &receive.imapdir_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"set_sent_folder", "FALSE", &tmp_ac_prefs.set_sent_folder, P_BOOL,
	 &advanced.sent_folder_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"sent_folder", NULL, &tmp_ac_prefs.sent_folder, P_STRING,
	 &advanced.sent_folder_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"set_queue_folder", "FALSE", &tmp_ac_prefs.set_queue_folder, P_BOOL,
	 &advanced.queue_folder_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"queue_folder", NULL, &tmp_ac_prefs.queue_folder, P_STRING,
	 &advanced.queue_folder_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"set_draft_folder", "FALSE", &tmp_ac_prefs.set_draft_folder, P_BOOL,
	 &advanced.draft_folder_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"draft_folder", NULL, &tmp_ac_prefs.draft_folder, P_STRING,
	 &advanced.draft_folder_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"set_trash_folder", "FALSE", &tmp_ac_prefs.set_trash_folder, P_BOOL,
	 &advanced.trash_folder_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"trash_folder", NULL, &tmp_ac_prefs.trash_folder, P_STRING,
	 &advanced.trash_folder_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

static gint prefs_account_get_new_id		(void);

static void prefs_account_create		(void);
static void prefs_account_basic_create		(void);
static void prefs_account_receive_create	(void);
static void prefs_account_send_create		(void);
static void prefs_account_compose_create	(void);
static void prefs_account_privacy_create	(void);
#if USE_OPENSSL
static void prefs_account_ssl_create		(void);
#endif /* USE_OPENSSL */
static void prefs_account_advanced_create	(void);

static void prefs_account_select_folder_cb	(GtkWidget	*widget,
						 gpointer	 data);

static void prefs_account_sigfile_radiobtn_cb	(GtkWidget	*widget,
						 gpointer	 data);

static void prefs_account_sigcmd_radiobtn_cb	(GtkWidget	*widget,
						 gpointer	 data);

static void prefs_account_signature_browse_cb	(GtkWidget	*widget,
						 gpointer	 data);

static void pop_bfr_smtp_tm_set_sens		(GtkWidget	*widget,
						 gpointer	 data);

static void prefs_account_edit_custom_header	(void);

static gint prefs_account_apply			(void);

typedef struct AccountPage
{
        PrefsPage page;
 
        GtkWidget *vbox;
} AccountPage;

static AccountPage account_page;

static void privacy_system_activated(GtkMenuItem *menuitem)
{
	const gchar* system_id;
	gboolean privacy_enabled = FALSE;
	
	system_id = g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID);
	
	privacy_enabled = strcmp(system_id, "");

	gtk_widget_set_sensitive (privacy.default_encrypt_chkbtn, privacy_enabled);
	gtk_widget_set_sensitive (privacy.default_encrypt_reply_chkbtn, privacy_enabled);
	gtk_widget_set_sensitive (privacy.default_sign_chkbtn, privacy_enabled);
	gtk_widget_set_sensitive (privacy.save_clear_text_chkbtn, privacy_enabled);
}

void update_privacy_system_menu() {
	GtkWidget *menu;
	GtkWidget *menuitem;
	GSList *system_ids, *cur;

	menu = gtk_menu_new();

	menuitem = gtk_menu_item_new_with_label(_("None"));
	gtk_widget_show(menuitem);
	g_object_set_data(G_OBJECT(menuitem), MENU_VAL_ID, "");
	gtk_menu_append(GTK_MENU(menu), menuitem);

	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(privacy_system_activated),
			 NULL);

	system_ids = privacy_get_system_ids();
	for (cur = system_ids; cur != NULL; cur = g_slist_next(cur)) {
		gchar *id = (gchar *) cur->data;
		const gchar *name;
		
		name = privacy_system_get_name(id);
		menuitem = gtk_menu_item_new_with_label(name);
		gtk_widget_show(menuitem);
		g_object_set_data_full(G_OBJECT(menuitem), MENU_VAL_ID, id, g_free);
		gtk_menu_append(GTK_MENU(menu), menuitem);

		g_signal_connect(G_OBJECT(menuitem), "activate",
				 G_CALLBACK(privacy_system_activated),
				 NULL);

		
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(privacy.default_privacy_system), menu);
}

static void create_widget_func(PrefsPage * _page,
                                           GtkWindow * window,
                                           gpointer data)
{
	AccountPage *page = (AccountPage *) _page;
	PrefsAccount *ac_prefs = (PrefsAccount *) data;
	GtkWidget *vbox;

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_widget_show(vbox);

	if (notebook == NULL)
		prefs_account_create();
	gtk_box_pack_start (GTK_BOX (vbox), notebook, TRUE, TRUE, 0);
	gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook), 0);

	update_privacy_system_menu();

	tmp_ac_prefs = *ac_prefs;

	if (new_account) {
		PrefsAccount *def_ac;
		gchar *buf;

		prefs_set_dialog_to_default(param);
		buf = g_strdup_printf(_("Account%d"), ac_prefs->account_id);
		gtk_entry_set_text(GTK_ENTRY(basic.acname_entry), buf);
		g_free(buf);
		def_ac = account_get_default();
		if (def_ac) {
			gtk_entry_set_text(GTK_ENTRY(basic.name_entry),
					   def_ac->name ? def_ac->name : "");
			gtk_entry_set_text(GTK_ENTRY(basic.addr_entry),
					   def_ac->address ? def_ac->address : "");
			gtk_entry_set_text(GTK_ENTRY(basic.org_entry),
					   def_ac->organization ? def_ac->organization : "");
		}
		menu_set_sensitive_all
			(GTK_MENU_SHELL
				(gtk_option_menu_get_menu
					(GTK_OPTION_MENU
						(basic.protocol_optmenu))),
			 TRUE);
	} else
		prefs_set_dialog(param);

	pop_bfr_smtp_tm_set_sens (NULL, NULL);
	
	page->vbox = vbox;

	page->page.widget = vbox;
}

static void destroy_widget_func(PrefsPage *_page)
{
	AccountPage *page = (AccountPage *) _page;

	gtk_container_remove(GTK_CONTAINER (page->vbox), notebook);
}

static gboolean can_close_func(PrefsPage *page_)
{	
	return prefs_account_apply() >= 0;
}

static void save_func(PrefsPage * _page)
{
	if (prefs_account_apply() >= 0)
		cancelled = FALSE;
}

void prefs_account_init()
{
	static gchar *path[2];

	path[0] = _("Account");
	path[2] = NULL;
        
	account_page.page.path = path;
	account_page.page.weight = 1000.0;
	account_page.page.create_widget = create_widget_func;
	account_page.page.destroy_widget = destroy_widget_func;
	account_page.page.save_page = save_func;
	account_page.page.can_close = can_close_func;

	prefs_account_register_page((PrefsPage *) &account_page);
}

PrefsAccount *prefs_account_new(void)
{
	PrefsAccount *ac_prefs;

	ac_prefs = g_new0(PrefsAccount, 1);
	memset(&tmp_ac_prefs, 0, sizeof(PrefsAccount));
	prefs_set_default(param);
	*ac_prefs = tmp_ac_prefs;
	ac_prefs->account_id = prefs_account_get_new_id();

	ac_prefs->privacy_prefs = g_hash_table_new(g_str_hash, g_str_equal);

	return ac_prefs;
}

void prefs_account_read_config(PrefsAccount *ac_prefs, const gchar *label)
{
	const gchar *p = label;
	gchar *rcpath;
	gint id;
	gchar **strv, **cur;

	g_return_if_fail(ac_prefs != NULL);
	g_return_if_fail(label != NULL);

	memset(&tmp_ac_prefs, 0, sizeof(PrefsAccount));
	tmp_ac_prefs.privacy_prefs = ac_prefs->privacy_prefs;

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, ACCOUNT_RC, NULL);
	prefs_read_config(param, label, rcpath, NULL);
	g_free(rcpath);

	*ac_prefs = tmp_ac_prefs;
	while (*p && !g_ascii_isdigit(*p)) p++;
	id = atoi(p);
	if (id < 0) g_warning("wrong account id: %d\n", id);
	ac_prefs->account_id = id;

	if (ac_prefs->protocol == A_APOP) {
		debug_print("converting protocol A_APOP to new prefs.\n");
		ac_prefs->protocol = A_POP3;
		ac_prefs->use_apop_auth = TRUE;
	}

	if (privacy_prefs != NULL) {
		strv = g_strsplit(privacy_prefs, ",", 0);
		for (cur = strv; *cur != NULL; cur++) {
			gchar *encvalue, *value;

			encvalue = strchr(*cur, '=');
			if (encvalue == NULL)
				continue;
			encvalue[0] = '\0';
			encvalue++;

			value = g_malloc0(strlen(encvalue));
			if (base64_decode(value, encvalue, strlen(encvalue)) > 0)
				g_hash_table_insert(ac_prefs->privacy_prefs, g_strdup(*cur), g_strdup(value));
			g_free(value);
		}
		g_strfreev(strv);
		g_free(privacy_prefs);
		privacy_prefs = NULL;
	}

	prefs_custom_header_read_config(ac_prefs);
}

static void create_privacy_prefs(gpointer key, gpointer _value, gpointer user_data)
{
	GString *str = (GString *) user_data;
	gchar *encvalue;
	gchar *value = (gchar *) _value;

	if (str->len > 0)
		g_string_append_c(str, ',');

	encvalue = g_malloc0(B64LEN(strlen(value)) + 1);
	base64_encode(encvalue, (gchar *) value, strlen(value));
	g_string_append_printf(str, "%s=%s", (gchar *) key, encvalue);
	g_free(encvalue);
}

void prefs_account_write_config_all(GList *account_list)
{
	GList *cur;
	gchar *rcpath;
	PrefFile *pfile;

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, ACCOUNT_RC, NULL);
	if ((pfile = prefs_write_open(rcpath)) == NULL) {
		g_free(rcpath);
		return;
	}
	g_free(rcpath);

	for (cur = account_list; cur != NULL; cur = cur->next) {
		GString *str;

		tmp_ac_prefs = *(PrefsAccount *)cur->data;
		if (fprintf(pfile->fp, "[Account: %d]\n",
			    tmp_ac_prefs.account_id) <= 0)
			return;

		str = g_string_sized_new(32);
		g_hash_table_foreach(tmp_ac_prefs.privacy_prefs, create_privacy_prefs, str);
		privacy_prefs = str->str;		    
		g_string_free(str, FALSE);

		if (prefs_write_param(param, pfile->fp) < 0) {
			g_warning("failed to write configuration to file\n");
			prefs_file_close_revert(pfile);
			g_free(privacy_prefs);
			privacy_prefs = NULL;
		    	return;
 		}
		g_free(privacy_prefs);
		privacy_prefs = NULL;

		if (cur->next) {
			if (fputc('\n', pfile->fp) == EOF) {
				FILE_OP_ERROR(rcpath, "fputc");
				prefs_file_close_revert(pfile);
				return;
			}
		}
	}

	if (prefs_file_close(pfile) < 0)
		g_warning("failed to write configuration to file\n");
}

static gboolean free_privacy_prefs(gpointer key, gpointer value, gpointer user_data)
{
	g_free(key);
	g_free(value);

	return TRUE;
}

void prefs_account_free(PrefsAccount *ac_prefs)
{
	if (!ac_prefs) return;

	g_hash_table_foreach_remove(ac_prefs->privacy_prefs, free_privacy_prefs, NULL);

	tmp_ac_prefs = *ac_prefs;
	prefs_free(param);
}

const gchar *prefs_account_get_privacy_prefs(PrefsAccount *account, gchar *id)
{
	return g_hash_table_lookup(account->privacy_prefs, id);
}

void prefs_account_set_privacy_prefs(PrefsAccount *account, gchar *id, gchar *new_value)
{
	gchar *orig_key = NULL, *value;

	if (g_hash_table_lookup_extended(account->privacy_prefs, id, (gpointer *)(gchar *) &orig_key, (gpointer *)(gchar *) &value)) {
		g_hash_table_remove(account->privacy_prefs, id);

		g_free(orig_key);
		g_free(value);
	}

	if (new_value != NULL)
		g_hash_table_insert(account->privacy_prefs, g_strdup(id), g_strdup(new_value));
}

static gint prefs_account_get_new_id(void)
{
	GList *ac_list;
	PrefsAccount *ac;
	static gint last_id = 0;

	for (ac_list = account_get_list(); ac_list != NULL;
	     ac_list = ac_list->next) {
		ac = (PrefsAccount *)ac_list->data;
		if (last_id < ac->account_id)
			last_id = ac->account_id;
	}

	return last_id + 1;
}

void destroy_dialog(gpointer data)
{
	PrefsAccount *ac_prefs = (PrefsAccount *) data;
	if (!cancelled)
		*ac_prefs = tmp_ac_prefs;

	gtk_main_quit();
}

PrefsAccount *prefs_account_open(PrefsAccount *ac_prefs)
{
	gchar *title;

	if (prefs_rc_is_readonly(ACCOUNT_RC))
		return ac_prefs;

	debug_print("Opening account preferences window...\n");

	inc_lock();

	cancelled = TRUE;

	if (!ac_prefs) {
		ac_prefs = prefs_account_new();
		new_account = TRUE;
	} else
		new_account = FALSE;

	if (new_account)
		title = g_strdup (_("Preferences for new account"));
	else
		title = g_strdup_printf (_("%s - Account preferences"),
				ac_prefs->account_name);

	prefswindow_open_full(title, prefs_pages, ac_prefs, destroy_dialog,
			&prefs_common.editaccountwin_width, &prefs_common.editaccountwin_height);
	g_free(title);
	gtk_main();

	inc_unlock();

	if (cancelled && new_account) {
		prefs_account_free(ac_prefs);
		return NULL;
	} else 
		return ac_prefs;
}

static void prefs_account_create(void)
{
	gint page = 0;

	debug_print("Creating account preferences window...\n");

	notebook = gtk_notebook_new ();
	gtk_widget_show(notebook);
	gtk_container_set_border_width (GTK_CONTAINER (notebook), 2);
	/* GTK_WIDGET_UNSET_FLAGS (notebook, GTK_CAN_FOCUS); */
	gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
	
	gtk_notebook_popup_enable (GTK_NOTEBOOK (notebook));

	gtk_widget_ref(notebook);

	/* create all widgets on notebook */
	prefs_account_basic_create();
	SET_NOTEBOOK_LABEL(notebook, _("Basic"), page++);
	prefs_account_receive_create();
	SET_NOTEBOOK_LABEL(notebook, _("Receive"), page++);
	prefs_account_send_create();
	SET_NOTEBOOK_LABEL(notebook, _("Send"), page++);
	prefs_account_compose_create();
	SET_NOTEBOOK_LABEL(notebook, _("Compose"), page++);
	prefs_account_privacy_create();
	SET_NOTEBOOK_LABEL(notebook, _("Privacy"), page++);
#if USE_OPENSSL
	prefs_account_ssl_create();
	SET_NOTEBOOK_LABEL(notebook, _("SSL"), page++);
#endif /* USE_OPENSSL */
	prefs_account_advanced_create();
	SET_NOTEBOOK_LABEL(notebook, _("Advanced"), page++);

	prefs_account_fix_size();
}

/**
 * prefs_account_fix_size:
 * 
 * Fix the window size after creating widgets by selecting "Local"
 * protocol (currently it has the largest size of parameter widgets).
 * Without this the window gets too large.
 **/
static void prefs_account_fix_size(void)
{
	GtkOptionMenu *optmenu = GTK_OPTION_MENU (basic.protocol_optmenu);
	GtkWidget *menu;
	GtkWidget *menuitem;

	gtk_option_menu_set_history (optmenu, 4); /* local */
	menu = gtk_option_menu_get_menu (optmenu);
	menuitem = gtk_menu_get_active (GTK_MENU (menu));
	gtk_menu_item_activate (GTK_MENU_ITEM (menuitem));
}

#define SET_ACTIVATE(menuitem) \
{ \
	g_signal_connect(G_OBJECT(menuitem), "activate", \
			 G_CALLBACK(prefs_account_protocol_activated), \
			 NULL); \
}

#define TABLE_YPAD 2

static void prefs_account_basic_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *acname_entry;
	GtkWidget *default_chkbtn;
	GtkWidget *frame1;
	GtkWidget *table1;
	GtkWidget *name_entry;
	GtkWidget *addr_entry;
	GtkWidget *org_entry;

	GtkWidget *serv_frame;
	GtkWidget *vbox2;
	GtkWidget *optmenu;
	GtkWidget *optmenu_menu;
	GtkWidget *menuitem;
	GtkWidget *serv_table;
	GtkWidget *recvserv_label;
	GtkWidget *smtpserv_label;
	GtkWidget *nntpserv_label;
	GtkWidget *localmbox_label;
	GtkWidget *mailcmd_label;
	GtkWidget *recvserv_entry;
	GtkWidget *smtpserv_entry;
	GtkWidget *nntpserv_entry;
	GtkWidget *nntpauth_chkbtn;
	GtkWidget *nntpauth_onconnect_chkbtn;
	GtkWidget *localmbox_entry;
	GtkWidget *mailcmd_chkbtn;
	GtkWidget *mailcmd_entry;
	GtkWidget *uid_label;
	GtkWidget *pass_label;
	GtkWidget *uid_entry;
	GtkWidget *pass_entry;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox, FALSE, FALSE, 0);

	label = gtk_label_new (_("Name of account"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	acname_entry = gtk_entry_new ();
	gtk_widget_show (acname_entry);
	gtk_widget_set_size_request (acname_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_box_pack_start (GTK_BOX (hbox), acname_entry, TRUE, TRUE, 0);

	default_chkbtn = gtk_check_button_new_with_label (_("Set as default"));
	gtk_widget_show (default_chkbtn);
	gtk_box_pack_end (GTK_BOX (hbox), default_chkbtn, FALSE, FALSE, 0);

	PACK_FRAME (vbox1, frame1, _("Personal information"));

	table1 = gtk_table_new (3, 2, FALSE);
	gtk_widget_show (table1);
	gtk_container_add (GTK_CONTAINER (frame1), table1);
	gtk_container_set_border_width (GTK_CONTAINER (table1), 8);
	gtk_table_set_row_spacings (GTK_TABLE (table1), VSPACING_NARROW);
	gtk_table_set_col_spacings (GTK_TABLE (table1), 8);

	label = gtk_label_new (_("Full name"));
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table1), label, 0, 1, 0, 1,
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

	label = gtk_label_new (_("Mail address"));
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table1), label, 0, 1, 1, 2,
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

	label = gtk_label_new (_("Organization"));
	gtk_widget_show (label);
	gtk_table_attach (GTK_TABLE (table1), label, 0, 1, 2, 3,
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);

	name_entry = gtk_entry_new ();
	gtk_widget_show (name_entry);
	gtk_table_attach (GTK_TABLE (table1), name_entry, 1, 2, 0, 1,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	addr_entry = gtk_entry_new ();
	gtk_widget_show (addr_entry);
	gtk_table_attach (GTK_TABLE (table1), addr_entry, 1, 2, 1, 2,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	org_entry = gtk_entry_new ();
	gtk_widget_show (org_entry);
	gtk_table_attach (GTK_TABLE (table1), org_entry, 1, 2, 2, 3,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	PACK_FRAME (vbox1, serv_frame, _("Server information"));

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (serv_frame), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);

	label = gtk_label_new (_("Protocol"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	optmenu = gtk_option_menu_new ();
	gtk_widget_show (optmenu);
	gtk_box_pack_start (GTK_BOX (hbox), optmenu, FALSE, FALSE, 0);

	optmenu_menu = gtk_menu_new ();

	MENUITEM_ADD (optmenu_menu, menuitem, _("POP3"),  A_POP3);
	SET_ACTIVATE (menuitem);
	MENUITEM_ADD (optmenu_menu, menuitem, _("IMAP4"), A_IMAP4);
	SET_ACTIVATE (menuitem);
	MENUITEM_ADD (optmenu_menu, menuitem, _("News (NNTP)"), A_NNTP);
	SET_ACTIVATE (menuitem);
	MENUITEM_ADD (optmenu_menu, menuitem, _("Local mbox file"), A_LOCAL);
	SET_ACTIVATE (menuitem);
	MENUITEM_ADD (optmenu_menu, menuitem, _("None (SMTP only)"), A_NONE);
	SET_ACTIVATE (menuitem);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (optmenu), optmenu_menu);

	serv_table = gtk_table_new (6, 4, FALSE);
	gtk_widget_show (serv_table);
	gtk_box_pack_start (GTK_BOX (vbox2), serv_table, FALSE, FALSE, 0);
	gtk_table_set_row_spacings (GTK_TABLE (serv_table), VSPACING_NARROW);
	gtk_table_set_row_spacing (GTK_TABLE (serv_table), 3, 0);
	gtk_table_set_col_spacings (GTK_TABLE (serv_table), 8);

	nntpserv_entry = gtk_entry_new ();
	gtk_widget_show (nntpserv_entry);
	gtk_table_attach (GTK_TABLE (serv_table), nntpserv_entry, 1, 4, 0, 1,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);
/*  	gtk_table_set_row_spacing (GTK_TABLE (serv_table), 0, 0); */

	nntpauth_chkbtn = gtk_check_button_new_with_label
		(_("This server requires authentication"));
	gtk_widget_show (nntpauth_chkbtn);
	
	gtk_table_attach (GTK_TABLE (serv_table), nntpauth_chkbtn, 0, 2, 6, 7,
			  GTK_FILL, 0, 0, 0);

	nntpauth_onconnect_chkbtn = gtk_check_button_new_with_label
		(_("Authenticate on connect"));
	gtk_widget_show (nntpauth_onconnect_chkbtn);

	gtk_table_attach (GTK_TABLE (serv_table), nntpauth_onconnect_chkbtn, 2, 4, 6, 7,
			  GTK_FILL, 0, 0, 0);

	recvserv_entry = gtk_entry_new ();
	gtk_widget_show (recvserv_entry);
	gtk_table_attach (GTK_TABLE (serv_table), recvserv_entry, 1, 4, 2, 3,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	localmbox_entry = gtk_entry_new ();
	gtk_widget_show (localmbox_entry);
	gtk_table_attach (GTK_TABLE (serv_table), localmbox_entry, 1, 4, 3, 4,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	smtpserv_entry = gtk_entry_new ();
	gtk_widget_show (smtpserv_entry);
	gtk_table_attach (GTK_TABLE (serv_table), smtpserv_entry, 1, 4, 4, 5,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	mailcmd_entry = gtk_entry_new ();
	gtk_widget_show (mailcmd_entry);
	gtk_table_attach (GTK_TABLE (serv_table), mailcmd_entry, 1, 4, 6, 7,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	uid_entry = gtk_entry_new ();
	gtk_widget_show (uid_entry);
	gtk_widget_set_size_request (uid_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_table_attach (GTK_TABLE (serv_table), uid_entry, 1, 2, 7, 8,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	pass_entry = gtk_entry_new ();
	gtk_widget_show (pass_entry);
	gtk_widget_set_size_request (pass_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_table_attach (GTK_TABLE (serv_table), pass_entry, 3, 4, 7, 8,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);
	gtk_entry_set_visibility (GTK_ENTRY (pass_entry), FALSE);

	nntpserv_label = gtk_label_new (_("News server"));
	gtk_widget_show (nntpserv_label);
	gtk_table_attach (GTK_TABLE (serv_table), nntpserv_label, 0, 1, 0, 1,
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (nntpserv_label), 1, 0.5);

	recvserv_label = gtk_label_new (_("Server for receiving"));
	gtk_widget_show (recvserv_label);
	gtk_table_attach (GTK_TABLE (serv_table), recvserv_label, 0, 1, 2, 3,
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (recvserv_label), 1, 0.5);

	localmbox_label = gtk_label_new (_("Local mailbox"));
	gtk_widget_show (localmbox_label);
	gtk_table_attach (GTK_TABLE (serv_table), localmbox_label, 0, 1, 3, 4,
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (localmbox_label), 1, 0.5);
/*  	gtk_table_set_row_spacing (GTK_TABLE (serv_table), 2, 0); */

	smtpserv_label = gtk_label_new (_("SMTP server (send)"));
	gtk_widget_show (smtpserv_label);
	gtk_table_attach (GTK_TABLE (serv_table), smtpserv_label, 0, 1, 4, 5,
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (smtpserv_label), 1, 0.5);
/*  	gtk_table_set_row_spacing (GTK_TABLE (serv_table), 2, 0); */

	mailcmd_chkbtn = gtk_check_button_new_with_label
		(_("Use mail command rather than SMTP server"));
	gtk_widget_show (mailcmd_chkbtn);
	gtk_table_attach (GTK_TABLE (serv_table), mailcmd_chkbtn, 0, 4, 5, 6,
			  GTK_EXPAND | GTK_FILL,
			  0, 0, TABLE_YPAD);
	g_signal_connect(G_OBJECT(mailcmd_chkbtn), "toggled",
			 G_CALLBACK(prefs_account_mailcmd_toggled),
			 NULL);

	mailcmd_label = gtk_label_new (_("command to send mails"));
	gtk_widget_show (mailcmd_label);
	gtk_table_attach (GTK_TABLE (serv_table), mailcmd_label, 0, 1, 6, 7,
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (mailcmd_label), 1, 0.5);
/*  	gtk_table_set_row_spacing (GTK_TABLE (serv_table), 2, 0); */

	uid_label = gtk_label_new (_("User ID"));
	gtk_widget_show (uid_label);
	gtk_table_attach (GTK_TABLE (serv_table), uid_label, 0, 1, 7, 8,
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (uid_label), 1, 0.5);

	pass_label = gtk_label_new (_("Password"));
	gtk_widget_show (pass_label);
	gtk_table_attach (GTK_TABLE (serv_table), pass_label, 2, 3, 7, 8,
			  0, 0, 0, 0);

	SET_TOGGLE_SENSITIVITY (nntpauth_chkbtn, uid_label);
	SET_TOGGLE_SENSITIVITY (nntpauth_chkbtn, pass_label);
	SET_TOGGLE_SENSITIVITY (nntpauth_chkbtn, uid_entry);
	SET_TOGGLE_SENSITIVITY (nntpauth_chkbtn, pass_entry);
	SET_TOGGLE_SENSITIVITY (nntpauth_chkbtn, nntpauth_onconnect_chkbtn);

	basic.acname_entry   = acname_entry;
	basic.default_chkbtn = default_chkbtn;

	basic.name_entry = name_entry;
	basic.addr_entry = addr_entry;
	basic.org_entry  = org_entry;

	basic.serv_frame       = serv_frame;
	basic.serv_table       = serv_table;
	basic.protocol_optmenu = optmenu;
	basic.recvserv_label   = recvserv_label;
	basic.recvserv_entry   = recvserv_entry;
	basic.smtpserv_label   = smtpserv_label;
	basic.smtpserv_entry   = smtpserv_entry;
	basic.nntpserv_label   = nntpserv_label;
	basic.nntpserv_entry   = nntpserv_entry;
	basic.nntpauth_chkbtn  = nntpauth_chkbtn;
	basic.nntpauth_onconnect_chkbtn  = nntpauth_onconnect_chkbtn;
	basic.localmbox_label   = localmbox_label;
	basic.localmbox_entry   = localmbox_entry;
	basic.mailcmd_chkbtn   = mailcmd_chkbtn;
	basic.mailcmd_label   = mailcmd_label;
	basic.mailcmd_entry   = mailcmd_entry;
	basic.uid_label        = uid_label;
	basic.pass_label       = pass_label;
	basic.uid_entry        = uid_entry;
	basic.pass_entry       = pass_entry;
}

static void prefs_account_receive_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *frame1;
	GtkWidget *vbox2;
	GtkWidget *use_apop_chkbtn;
	GtkWidget *rmmail_chkbtn;
	GtkWidget *hbox_spc;
	GtkTooltips *leave_time_tooltip;
	GtkWidget *leave_time_label;
	GtkWidget *leave_time_entry;
	GtkWidget *getall_chkbtn;
	GtkWidget *hbox1;
	GtkWidget *size_limit_chkbtn;
	GtkWidget *size_limit_entry;
	GtkWidget *label;
	GtkWidget *filter_on_recv_chkbtn;
	GtkWidget *vbox3;
	GtkWidget *inbox_label;
	GtkWidget *inbox_entry;
	GtkWidget *inbox_btn;
	GtkTooltips *inbox_tooltip;
	GtkWidget *imap_frame;
 	GtkWidget *imapdir_label;
	GtkWidget *imapdir_entry;
	GtkWidget *local_frame;
	GtkWidget *local_vbox;
	GtkWidget *local_hbox;
	GtkWidget *local_inbox_label;
	GtkWidget *local_inbox_entry;
	GtkWidget *local_inbox_btn;

	GtkWidget *optmenu;
	GtkWidget *optmenu_menu;
	GtkWidget *menuitem;
	GtkWidget *recvatgetall_chkbtn;

	GtkWidget *hbox2;
	GtkWidget *frame2;
	GtkWidget *maxarticle_label;
	GtkWidget *maxarticle_spinbtn;
	GtkObject *maxarticle_spinbtn_adj;
	GtkTooltips *maxarticle_tool_tip;

	inbox_tooltip = gtk_tooltips_new();

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	PACK_FRAME (vbox1, local_frame, _("Local"));

	local_vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (local_vbox);
	gtk_container_add (GTK_CONTAINER (local_frame), local_vbox);
	gtk_container_set_border_width (GTK_CONTAINER (local_vbox), VBOX_BORDER);

	local_hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (local_hbox);
	gtk_box_pack_start (GTK_BOX (local_vbox), local_hbox, FALSE, FALSE, 0);

	local_inbox_label = gtk_label_new (_("Default inbox"));
	gtk_widget_show (local_inbox_label);
	gtk_box_pack_start (GTK_BOX (local_hbox), local_inbox_label, FALSE, FALSE, 0);

	local_inbox_entry = gtk_entry_new ();
	gtk_widget_show (local_inbox_entry);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(inbox_tooltip), local_inbox_entry,
			     _("Unfiltered messages will be stored in this folder"),
			     NULL);
	gtk_widget_set_size_request (local_inbox_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_box_pack_start (GTK_BOX (local_hbox), local_inbox_entry, TRUE, TRUE, 0);

	local_inbox_btn = gtkut_get_browse_file_btn(_("_Browse"));
	gtk_widget_show (local_inbox_btn);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(inbox_tooltip), local_inbox_btn,
			     _("Unfiltered messages will be stored in this folder"),
			     NULL);
	gtk_box_pack_start (GTK_BOX (local_hbox), local_inbox_btn, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (local_inbox_btn), "clicked",
			  G_CALLBACK (prefs_account_select_folder_cb),
			  local_inbox_entry);

	PACK_FRAME (vbox1, frame1, _("POP3"));

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (frame1), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), VBOX_BORDER);

	PACK_CHECK_BUTTON (vbox2, use_apop_chkbtn,
			   _("Use secure authentication (APOP)"));

	PACK_CHECK_BUTTON (vbox2, rmmail_chkbtn,
			   _("Remove messages on server when received"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

	hbox_spc = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (hbox1), hbox_spc, FALSE, FALSE, 0);
	gtk_widget_set_size_request (hbox_spc, 12, -1);

	leave_time_label = gtk_label_new (_("Remove after"));
	gtk_widget_show (leave_time_label);
	gtk_box_pack_start (GTK_BOX (hbox1), leave_time_label, FALSE, FALSE, 0);

	leave_time_tooltip = gtk_tooltips_new();

	leave_time_entry = gtk_entry_new ();
	gtk_widget_show (leave_time_entry);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(leave_time_tooltip), leave_time_entry,
			     _("0 days: remove immediately"), NULL);
	gtk_widget_set_size_request (leave_time_entry, 64, -1);
	gtk_box_pack_start (GTK_BOX (hbox1), leave_time_entry, FALSE, FALSE, 0);

	leave_time_label = gtk_label_new (_("days"));
	gtk_widget_show (leave_time_label);
	gtk_box_pack_start (GTK_BOX (hbox1), leave_time_label, FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY (rmmail_chkbtn, hbox1);

	PACK_CHECK_BUTTON (vbox2, getall_chkbtn,
			   _("Download all messages on server"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox1, size_limit_chkbtn, _("Receive size limit"));

	size_limit_entry = gtk_entry_new ();
	gtk_widget_show (size_limit_entry);
	gtk_widget_set_size_request (size_limit_entry, 64, -1);
	gtk_box_pack_start (GTK_BOX (hbox1), size_limit_entry, FALSE, FALSE, 0);

	label = gtk_label_new (_("KB"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY (size_limit_chkbtn, size_limit_entry);

	PACK_VSPACER(vbox2, vbox3, VSPACING_NARROW_2);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

	inbox_label = gtk_label_new (_("Default inbox"));
	gtk_widget_show (inbox_label);
	gtk_box_pack_start (GTK_BOX (hbox1), inbox_label, FALSE, FALSE, 0);

	inbox_entry = gtk_entry_new ();
	gtk_widget_show (inbox_entry);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(inbox_tooltip), inbox_entry,
			     _("Unfiltered messages will be stored in this folder"),
			     NULL);
	gtk_widget_set_size_request (inbox_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_box_pack_start (GTK_BOX (hbox1), inbox_entry, TRUE, TRUE, 0);

	inbox_btn = gtkut_get_browse_file_btn(_("_Browse"));
	gtk_widget_show (inbox_btn);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(inbox_tooltip), inbox_btn,
			     _("Unfiltered messages will be stored in this folder"),
			     NULL);
	gtk_box_pack_start (GTK_BOX (hbox1), inbox_btn, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (inbox_btn), "clicked",
			  G_CALLBACK (prefs_account_select_folder_cb),
			  inbox_entry);

	PACK_FRAME(vbox1, frame2, _("NNTP"));

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (frame2), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), VBOX_BORDER);

	hbox2 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox2);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox2, FALSE, FALSE, 0);

	maxarticle_label = gtk_label_new
		(_("Maximum number of articles to download"));
	gtk_widget_show (maxarticle_label);
	gtk_box_pack_start (GTK_BOX (hbox2), maxarticle_label, FALSE, FALSE, 0);

	maxarticle_tool_tip = gtk_tooltips_new();

	maxarticle_spinbtn_adj =
		gtk_adjustment_new (300, 0, 10000, 10, 100, 100);
	maxarticle_spinbtn = gtk_spin_button_new
		(GTK_ADJUSTMENT (maxarticle_spinbtn_adj), 10, 0);
	gtk_widget_show (maxarticle_spinbtn);
	gtk_tooltips_set_tip(maxarticle_tool_tip, maxarticle_spinbtn,
			     _("unlimited if 0 is specified"), NULL);
	gtk_box_pack_start (GTK_BOX (hbox2), maxarticle_spinbtn,
			    FALSE, FALSE, 0);
	gtk_widget_set_size_request (maxarticle_spinbtn, 64, -1);
	gtk_spin_button_set_numeric
		(GTK_SPIN_BUTTON (maxarticle_spinbtn), TRUE);

	PACK_FRAME (vbox1, imap_frame, _("IMAP4"));

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (imap_frame), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), VBOX_BORDER);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

	label = gtk_label_new (_("Authentication method"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);

	optmenu = gtk_option_menu_new ();
	gtk_widget_show (optmenu);
	gtk_box_pack_start (GTK_BOX (hbox1), optmenu, FALSE, FALSE, 0);

	optmenu_menu = gtk_menu_new ();

	MENUITEM_ADD (optmenu_menu, menuitem, _("Automatic"), 0);
	MENUITEM_ADD (optmenu_menu, menuitem, "LOGIN", IMAP_AUTH_LOGIN);
	MENUITEM_ADD (optmenu_menu, menuitem, "CRAM-MD5", IMAP_AUTH_CRAM_MD5);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (optmenu), optmenu_menu);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 4);

	imapdir_label = gtk_label_new (_("IMAP server directory"));
	gtk_widget_show (imapdir_label);
	gtk_box_pack_start (GTK_BOX (hbox1), imapdir_label, FALSE, FALSE, 0);

	imapdir_label = gtk_label_new(_("(usually empty)"));
	gtk_widget_show (imapdir_label);
	gtkut_widget_set_small_font_size (imapdir_label);
	gtk_box_pack_start (GTK_BOX (hbox1), imapdir_label, FALSE, FALSE, 0);

	imapdir_entry = gtk_entry_new();
	gtk_widget_show (imapdir_entry);
	gtk_box_pack_start (GTK_BOX (hbox1), imapdir_entry, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (vbox1, filter_on_recv_chkbtn,
			   _("Filter messages on receiving"));

	PACK_CHECK_BUTTON
		(vbox1, recvatgetall_chkbtn,
		 _("'Get all' checks for new messages on this account"));

	receive.pop3_frame               = frame1;
	receive.use_apop_chkbtn          = use_apop_chkbtn;
	receive.rmmail_chkbtn            = rmmail_chkbtn;
	receive.leave_time_entry         = leave_time_entry;
	receive.getall_chkbtn            = getall_chkbtn;
	receive.size_limit_chkbtn        = size_limit_chkbtn;
	receive.size_limit_entry         = size_limit_entry;
	receive.filter_on_recv_chkbtn    = filter_on_recv_chkbtn;
	receive.inbox_label              = inbox_label;
	receive.inbox_entry              = inbox_entry;
	receive.inbox_btn                = inbox_btn;

	receive.imap_frame               = imap_frame;
	receive.imap_auth_type_optmenu   = optmenu;

	receive.imapdir_label		= imapdir_label;
	receive.imapdir_entry           = imapdir_entry;

	receive.local_frame		= local_frame;
	receive.local_inbox_label	= local_inbox_label;
	receive.local_inbox_entry	= local_inbox_entry;
	receive.local_inbox_btn		= local_inbox_btn;

	receive.recvatgetall_chkbtn      = recvatgetall_chkbtn;

	receive.frame_maxarticle	= frame2;
	receive.maxarticle_spinbtn     	= maxarticle_spinbtn;
	receive.maxarticle_spinbtn_adj 	= maxarticle_spinbtn_adj;
}

static void prefs_account_send_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *frame;
	GtkWidget *date_chkbtn;
	GtkWidget *msgid_chkbtn;
	GtkWidget *hbox;
	GtkWidget *customhdr_chkbtn;
	GtkWidget *customhdr_edit_btn;
	GtkWidget *vbox3;
	GtkWidget *smtp_auth_chkbtn;
	GtkWidget *optmenu;
	GtkWidget *optmenu_menu;
	GtkWidget *menuitem;
	GtkWidget *vbox4;
	GtkWidget *hbox_spc;
	GtkWidget *label;
	GtkWidget *smtp_uid_entry;
	GtkWidget *smtp_pass_entry;
	GtkWidget *vbox_spc;
	GtkWidget *pop_bfr_smtp_chkbtn;
	GtkWidget *pop_bfr_smtp_tm_entry;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	PACK_FRAME (vbox1, frame, _("Header"));

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (frame), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);

	PACK_CHECK_BUTTON (vbox2, date_chkbtn, _("Add Date"));
	PACK_CHECK_BUTTON (vbox2, msgid_chkbtn, _("Generate Message-ID"));

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox, customhdr_chkbtn,
			   _("Add user-defined header"));

	customhdr_edit_btn = gtk_button_new_with_label (_(" Edit... "));
	gtk_widget_show (customhdr_edit_btn);
	gtk_box_pack_start (GTK_BOX (hbox), customhdr_edit_btn,
			    FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (customhdr_edit_btn), "clicked",
			  G_CALLBACK (prefs_account_edit_custom_header),
			  NULL);

	SET_TOGGLE_SENSITIVITY (customhdr_chkbtn, customhdr_edit_btn);

	PACK_FRAME (vbox1, frame, _("Authentication"));

	vbox3 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox3);
	gtk_container_add (GTK_CONTAINER (frame), vbox3);
	gtk_container_set_border_width (GTK_CONTAINER (vbox3), 8);

	PACK_CHECK_BUTTON (vbox3, smtp_auth_chkbtn,
		_("SMTP Authentication (SMTP AUTH)"));

	vbox4 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox4);
	gtk_box_pack_start (GTK_BOX (vbox3), vbox4, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox4), hbox, FALSE, FALSE, 0);

	hbox_spc = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (hbox), hbox_spc, FALSE, FALSE, 0);
	gtk_widget_set_size_request (hbox_spc, 12, -1);

	label = gtk_label_new (_("Authentication method"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	optmenu = gtk_option_menu_new ();
	gtk_widget_show (optmenu);
	gtk_box_pack_start (GTK_BOX (hbox), optmenu, FALSE, FALSE, 0);

	optmenu_menu = gtk_menu_new ();

	MENUITEM_ADD (optmenu_menu, menuitem, _("Automatic"), 0);
	MENUITEM_ADD (optmenu_menu, menuitem, "PLAIN", SMTPAUTH_PLAIN);
	MENUITEM_ADD (optmenu_menu, menuitem, "LOGIN", SMTPAUTH_LOGIN);
	MENUITEM_ADD (optmenu_menu, menuitem, "CRAM-MD5", SMTPAUTH_CRAM_MD5);
	MENUITEM_ADD (optmenu_menu, menuitem, "DIGEST-MD5", SMTPAUTH_DIGEST_MD5);
	gtk_widget_set_sensitive (menuitem, FALSE);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (optmenu), optmenu_menu);

	PACK_VSPACER(vbox4, vbox_spc, VSPACING_NARROW_2);

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox4), hbox, FALSE, FALSE, 0);

	hbox_spc = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (hbox), hbox_spc, FALSE, FALSE, 0);
	gtk_widget_set_size_request (hbox_spc, 12, -1);

	label = gtk_label_new (_("User ID"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	smtp_uid_entry = gtk_entry_new ();
	gtk_widget_show (smtp_uid_entry);
	gtk_widget_set_size_request (smtp_uid_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_box_pack_start (GTK_BOX (hbox), smtp_uid_entry, TRUE, TRUE, 0);

	label = gtk_label_new (_("Password"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	smtp_pass_entry = gtk_entry_new ();
	gtk_widget_show (smtp_pass_entry);
	gtk_widget_set_size_request (smtp_pass_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_box_pack_start (GTK_BOX (hbox), smtp_pass_entry, TRUE, TRUE, 0);
	gtk_entry_set_visibility (GTK_ENTRY (smtp_pass_entry), FALSE);

	PACK_VSPACER(vbox4, vbox_spc, VSPACING_NARROW_2);

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox4), hbox, FALSE, FALSE, 0);

	hbox_spc = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (hbox), hbox_spc, FALSE, FALSE, 0);
	gtk_widget_set_size_request (hbox_spc, 12, -1);

	label = gtk_label_new
		(_("If you leave these entries empty, the same "
		   "user ID and password as receiving will be used."));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtkut_widget_set_small_font_size (label);

	SET_TOGGLE_SENSITIVITY (smtp_auth_chkbtn, vbox4);

	PACK_CHECK_BUTTON (vbox3, pop_bfr_smtp_chkbtn,
		_("Authenticate with POP3 before sending"));
	
	g_signal_connect (G_OBJECT (pop_bfr_smtp_chkbtn), "clicked",
			  G_CALLBACK (pop_bfr_smtp_tm_set_sens),
			  NULL);

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox3), hbox, FALSE, FALSE, 0);

	hbox_spc = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (hbox), hbox_spc, FALSE, FALSE, 0);
	gtk_widget_set_size_request (hbox_spc, 12, -1);

	label = gtk_label_new(_("POP authentication timeout: "));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	pop_bfr_smtp_tm_entry = gtk_entry_new ();
	gtk_widget_show (pop_bfr_smtp_tm_entry);
	gtk_widget_set_size_request (pop_bfr_smtp_tm_entry, 30, -1);
	gtk_box_pack_start (GTK_BOX (hbox), pop_bfr_smtp_tm_entry, FALSE, FALSE, 0);

	label = gtk_label_new(_("minutes"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);


	
	p_send.date_chkbtn      = date_chkbtn;
	p_send.msgid_chkbtn     = msgid_chkbtn;
	p_send.customhdr_chkbtn = customhdr_chkbtn;

	p_send.smtp_auth_chkbtn       = smtp_auth_chkbtn;
	p_send.smtp_auth_type_optmenu = optmenu;
	p_send.smtp_uid_entry         = smtp_uid_entry;
	p_send.smtp_pass_entry        = smtp_pass_entry;
	p_send.pop_bfr_smtp_chkbtn    = pop_bfr_smtp_chkbtn;
	p_send.pop_bfr_smtp_tm_entry  = pop_bfr_smtp_tm_entry;
}

static void prefs_account_compose_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *sig_hbox;
	GtkWidget *hbox1;
	GtkWidget *hbox2;
	GtkWidget *frame_sig;
	GtkWidget *vbox_sig;
	GtkWidget *label_sigpath;
	GtkWidget *checkbtn_autosig;
	GtkWidget *label_sigsep;
	GtkWidget *entry_sigsep;
	GtkWidget *frame;
	GtkWidget *table;
	GtkWidget *autocc_chkbtn;
	GtkWidget *autocc_entry;
	GtkWidget *autobcc_chkbtn;
	GtkWidget *autobcc_entry;
	GtkWidget *autoreplyto_chkbtn;
	GtkWidget *autoreplyto_entry;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	PACK_FRAME(vbox1, frame_sig, _("Signature"));

	vbox_sig = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox_sig);
	gtk_container_add (GTK_CONTAINER (frame_sig), vbox_sig);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_sig), 8);

	PACK_CHECK_BUTTON (vbox_sig, checkbtn_autosig,
			   _("Insert signature automatically"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_sig), hbox1, TRUE, TRUE, 0);
	label_sigsep = gtk_label_new (_("Signature separator"));
	gtk_widget_show (label_sigsep);
	gtk_box_pack_start (GTK_BOX (hbox1), label_sigsep, FALSE, FALSE, 0);

	entry_sigsep = gtk_entry_new ();
	gtk_widget_show (entry_sigsep);
	gtk_box_pack_start (GTK_BOX (hbox1), entry_sigsep, FALSE, FALSE, 0);

	gtk_widget_set_size_request (entry_sigsep, 64, -1);

	sig_hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (sig_hbox);
	gtk_box_pack_start (GTK_BOX (vbox_sig), sig_hbox, FALSE, FALSE, 0);

	sigfile_radiobtn = gtk_radio_button_new_with_label (NULL, _("File"));
	gtk_widget_show (sigfile_radiobtn);
	gtk_box_pack_start (GTK_BOX (sig_hbox), sigfile_radiobtn,
			    FALSE, FALSE, 0);
	g_object_set_data (G_OBJECT (sigfile_radiobtn),
			   MENU_VAL_ID,
			   GINT_TO_POINTER (SIG_FILE));
	g_signal_connect(G_OBJECT(sigfile_radiobtn), "clicked",
			 G_CALLBACK(prefs_account_sigfile_radiobtn_cb), NULL);

	sigcmd_radiobtn = gtk_radio_button_new_with_label_from_widget
		(GTK_RADIO_BUTTON(sigfile_radiobtn), _("Command output"));
	gtk_widget_show (sigcmd_radiobtn);
	gtk_box_pack_start (GTK_BOX (sig_hbox), sigcmd_radiobtn,
			    FALSE, FALSE, 0);
	g_object_set_data (G_OBJECT (sigcmd_radiobtn),
			   MENU_VAL_ID,
			   GINT_TO_POINTER (SIG_COMMAND));
	g_signal_connect(G_OBJECT(sigcmd_radiobtn), "clicked",
			 G_CALLBACK(prefs_account_sigcmd_radiobtn_cb), NULL);

	hbox2 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox2);
	gtk_box_pack_start (GTK_BOX (vbox_sig), hbox2, TRUE, TRUE, 0);
	label_sigpath = gtk_label_new (_("Signature"));
	gtk_widget_show (label_sigpath);
	gtk_box_pack_start (GTK_BOX (hbox2), label_sigpath, FALSE, FALSE, 0);

	entry_sigpath = gtk_entry_new ();
	gtk_widget_show (entry_sigpath);
	gtk_box_pack_start (GTK_BOX (hbox2), entry_sigpath, TRUE, TRUE, 0);

	signature_browse_button = gtkut_get_browse_file_btn(_("_Browse"));
	gtk_widget_show (signature_browse_button);
	gtk_box_pack_start (GTK_BOX (hbox2), signature_browse_button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(signature_browse_button), "clicked",
			 G_CALLBACK(prefs_account_signature_browse_cb), NULL);

	PACK_FRAME (vbox1, frame, _("Automatically set the following addresses"));

	table =  gtk_table_new (3, 2, FALSE);
	gtk_widget_show (table);
	gtk_container_add (GTK_CONTAINER (frame), table);
	gtk_container_set_border_width (GTK_CONTAINER (table), 8);
	gtk_table_set_row_spacings (GTK_TABLE (table), VSPACING_NARROW_2);
	gtk_table_set_col_spacings (GTK_TABLE (table), 8);

	autocc_chkbtn = gtk_check_button_new_with_label (_("Cc"));
	gtk_widget_show (autocc_chkbtn);
	gtk_table_attach (GTK_TABLE (table), autocc_chkbtn, 0, 1, 0, 1,
			  GTK_FILL, 0, 0, 0);

	autocc_entry = gtk_entry_new ();
	gtk_widget_show (autocc_entry);
	gtk_table_attach (GTK_TABLE (table), autocc_entry, 1, 2, 0, 1,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	SET_TOGGLE_SENSITIVITY (autocc_chkbtn, autocc_entry);

	autobcc_chkbtn = gtk_check_button_new_with_label (_("Bcc"));
	gtk_widget_show (autobcc_chkbtn);
	gtk_table_attach (GTK_TABLE (table), autobcc_chkbtn, 0, 1, 1, 2,
			  GTK_FILL, 0, 0, 0);

	autobcc_entry = gtk_entry_new ();
	gtk_widget_show (autobcc_entry);
	gtk_table_attach (GTK_TABLE (table), autobcc_entry, 1, 2, 1, 2,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	SET_TOGGLE_SENSITIVITY (autobcc_chkbtn, autobcc_entry);

	autoreplyto_chkbtn = gtk_check_button_new_with_label (_("Reply-To"));
	gtk_widget_show (autoreplyto_chkbtn);
	gtk_table_attach (GTK_TABLE (table), autoreplyto_chkbtn, 0, 1, 2, 3,
			  GTK_FILL, 0, 0, 0);

	autoreplyto_entry = gtk_entry_new ();
	gtk_widget_show (autoreplyto_entry);
	gtk_table_attach (GTK_TABLE (table), autoreplyto_entry, 1, 2, 2, 3,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	SET_TOGGLE_SENSITIVITY (autoreplyto_chkbtn, autoreplyto_entry);

	compose.sigfile_radiobtn = sigfile_radiobtn;
	compose.entry_sigpath      = entry_sigpath;
	compose.checkbtn_autosig   = checkbtn_autosig;
	compose.entry_sigsep       = entry_sigsep;

	compose.autocc_chkbtn      = autocc_chkbtn;
	compose.autocc_entry       = autocc_entry;
	compose.autobcc_chkbtn     = autobcc_chkbtn;
	compose.autobcc_entry      = autobcc_entry;
	compose.autoreplyto_chkbtn = autoreplyto_chkbtn;
	compose.autoreplyto_entry  = autoreplyto_entry;
}

static void prefs_account_privacy_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *hbox1;
	GtkWidget *label;
	GtkWidget *default_privacy_system;
	GtkWidget *default_encrypt_chkbtn;
	GtkWidget *default_encrypt_reply_chkbtn;
	GtkWidget *default_sign_chkbtn;
	GtkWidget *save_clear_text_chkbtn;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	hbox1 = gtk_hbox_new(FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_container_add (GTK_CONTAINER(vbox2), hbox1);

	label = gtk_label_new(_("Default privacy system"));
	gtk_widget_show(label);
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);

	default_privacy_system = gtk_option_menu_new();
	gtk_widget_show (default_privacy_system);
	gtk_box_pack_start (GTK_BOX(hbox1), default_privacy_system, FALSE, TRUE, 0);

	PACK_CHECK_BUTTON (vbox2, default_encrypt_chkbtn,
			   _("Encrypt message by default"));
	PACK_CHECK_BUTTON (vbox2, default_encrypt_reply_chkbtn,
			   _("Encrypt message by default when replying to an "
			     "encrypted message"));
	PACK_CHECK_BUTTON (vbox2, default_sign_chkbtn,
			   _("Sign message by default"));
	PACK_CHECK_BUTTON (vbox2, save_clear_text_chkbtn,
			   _("Save sent encrypted messages as clear text"));

	privacy.default_privacy_system = default_privacy_system;
	privacy.default_encrypt_chkbtn = default_encrypt_chkbtn;
	privacy.default_encrypt_reply_chkbtn = default_encrypt_reply_chkbtn;
	privacy.default_sign_chkbtn    = default_sign_chkbtn;
	privacy.save_clear_text_chkbtn = save_clear_text_chkbtn;
}

#if USE_OPENSSL

#define CREATE_RADIO_BUTTON(box, btn, btn_p, label, data)		\
{									\
	btn = gtk_radio_button_new_with_label_from_widget		\
		(GTK_RADIO_BUTTON (btn_p), label);			\
	gtk_widget_show (btn);						\
	gtk_box_pack_start (GTK_BOX (box), btn, FALSE, FALSE, 0);	\
	g_object_set_data (G_OBJECT (btn),				\
			   MENU_VAL_ID,					\
			   GINT_TO_POINTER (data));			\
}

#define CREATE_RADIO_BUTTONS(box,					\
			     btn1, btn1_label, btn1_data,		\
			     btn2, btn2_label, btn2_data,		\
			     btn3, btn3_label, btn3_data)		\
{									\
	btn1 = gtk_radio_button_new_with_label(NULL, btn1_label);	\
	gtk_widget_show (btn1);						\
	gtk_box_pack_start (GTK_BOX (box), btn1, FALSE, FALSE, 0);	\
	g_object_set_data (G_OBJECT (btn1),				\
			   MENU_VAL_ID,					\
			   GINT_TO_POINTER (btn1_data));		\
									\
	CREATE_RADIO_BUTTON(box, btn2, btn1, btn2_label, btn2_data);	\
	CREATE_RADIO_BUTTON(box, btn3, btn1, btn3_label, btn3_data);	\
}

static void prefs_account_ssl_create(void)
{
	GtkWidget *vbox1;

	GtkWidget *pop_frame;
	GtkWidget *vbox2;
	GtkWidget *pop_nossl_radiobtn;
	GtkWidget *pop_ssltunnel_radiobtn;
	GtkWidget *pop_starttls_radiobtn;

	GtkWidget *imap_frame;
	GtkWidget *vbox3;
	GtkWidget *imap_nossl_radiobtn;
	GtkWidget *imap_ssltunnel_radiobtn;
	GtkWidget *imap_starttls_radiobtn;

	GtkWidget *nntp_frame;
	GtkWidget *vbox4;
	GtkWidget *nntp_nossl_radiobtn;
	GtkWidget *nntp_ssltunnel_radiobtn;

	GtkWidget *send_frame;
	GtkWidget *vbox5;
	GtkWidget *smtp_nossl_radiobtn;
	GtkWidget *smtp_ssltunnel_radiobtn;
	GtkWidget *smtp_starttls_radiobtn;

	GtkWidget *vbox6;
	GtkWidget *use_nonblocking_ssl_chkbtn;
	GtkWidget *hbox;
	GtkWidget *hbox_spc;
	GtkWidget *label;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	PACK_FRAME (vbox1, pop_frame, _("POP3"));
	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (pop_frame), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);

	CREATE_RADIO_BUTTONS(vbox2,
			     pop_nossl_radiobtn,
			     _("Don't use SSL"),
			     SSL_NONE,
			     pop_ssltunnel_radiobtn,
			     _("Use SSL for POP3 connection"),
			     SSL_TUNNEL,
			     pop_starttls_radiobtn,
			     _("Use STARTTLS command to start SSL session"),
			     SSL_STARTTLS);

	PACK_FRAME (vbox1, imap_frame, _("IMAP4"));
	vbox3 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox3);
	gtk_container_add (GTK_CONTAINER (imap_frame), vbox3);
	gtk_container_set_border_width (GTK_CONTAINER (vbox3), 8);

	CREATE_RADIO_BUTTONS(vbox3,
			     imap_nossl_radiobtn,
			     _("Don't use SSL"),
			     SSL_NONE,
			     imap_ssltunnel_radiobtn,
			     _("Use SSL for IMAP4 connection"),
			     SSL_TUNNEL,
			     imap_starttls_radiobtn,
			     _("Use STARTTLS command to start SSL session"),
			     SSL_STARTTLS);

	PACK_FRAME (vbox1, nntp_frame, _("NNTP"));
	vbox4 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox4);
	gtk_container_add (GTK_CONTAINER (nntp_frame), vbox4);
	gtk_container_set_border_width (GTK_CONTAINER (vbox4), 8);

	nntp_nossl_radiobtn =
		gtk_radio_button_new_with_label (NULL, _("Don't use SSL"));
	gtk_widget_show (nntp_nossl_radiobtn);
	gtk_box_pack_start (GTK_BOX (vbox4), nntp_nossl_radiobtn,
			    FALSE, FALSE, 0);
	g_object_set_data (G_OBJECT (nntp_nossl_radiobtn),
			   MENU_VAL_ID,
			   GINT_TO_POINTER (SSL_NONE));

	CREATE_RADIO_BUTTON(vbox4, nntp_ssltunnel_radiobtn, nntp_nossl_radiobtn,
			    _("Use SSL for NNTP connection"), SSL_TUNNEL);

	PACK_FRAME (vbox1, send_frame, _("Send (SMTP)"));
	vbox5 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox5);
	gtk_container_add (GTK_CONTAINER (send_frame), vbox5);
	gtk_container_set_border_width (GTK_CONTAINER (vbox5), 8);

	CREATE_RADIO_BUTTONS(vbox5,
			     smtp_nossl_radiobtn,
			     _("Don't use SSL (but, if necessary, use STARTTLS)"),
			     SSL_NONE,
			     smtp_ssltunnel_radiobtn,
			     _("Use SSL for SMTP connection"),
			     SSL_TUNNEL,
			     smtp_starttls_radiobtn,
			     _("Use STARTTLS command to start SSL session"),
			     SSL_STARTTLS);

	vbox6 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox6);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox6, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON(vbox6, use_nonblocking_ssl_chkbtn,
			  _("Use non-blocking SSL"));

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox6), hbox, FALSE, FALSE, 0);

	hbox_spc = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (hbox), hbox_spc, FALSE, FALSE, 0);
	gtk_widget_set_size_request (hbox_spc, 16, -1);

	label = gtk_label_new
		(_("Turn this off if you have SSL connection problems"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtkut_widget_set_small_font_size (label);

	ssl.pop_frame               = pop_frame;
	ssl.pop_nossl_radiobtn      = pop_nossl_radiobtn;
	ssl.pop_ssltunnel_radiobtn  = pop_ssltunnel_radiobtn;
	ssl.pop_starttls_radiobtn   = pop_starttls_radiobtn;

	ssl.imap_frame              = imap_frame;
	ssl.imap_nossl_radiobtn     = imap_nossl_radiobtn;
	ssl.imap_ssltunnel_radiobtn = imap_ssltunnel_radiobtn;
	ssl.imap_starttls_radiobtn  = imap_starttls_radiobtn;

	ssl.nntp_frame              = nntp_frame;
	ssl.nntp_nossl_radiobtn     = nntp_nossl_radiobtn;
	ssl.nntp_ssltunnel_radiobtn = nntp_ssltunnel_radiobtn;

	ssl.send_frame              = send_frame;
	ssl.smtp_nossl_radiobtn     = smtp_nossl_radiobtn;
	ssl.smtp_ssltunnel_radiobtn = smtp_ssltunnel_radiobtn;
	ssl.smtp_starttls_radiobtn  = smtp_starttls_radiobtn;

	ssl.use_nonblocking_ssl_chkbtn = use_nonblocking_ssl_chkbtn;
}

#undef CREATE_RADIO_BUTTONS
#undef CREATE_RADIO_BUTTON
#endif /* USE_OPENSSL */

static void crosspost_color_toggled(void)
{
	gboolean is_active;

	is_active = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(advanced.crosspost_chkbtn));
	gtk_widget_set_sensitive(advanced.crosspost_colormenu, is_active);
}

static void prefs_account_crosspost_set_data_from_colormenu(PrefParam *pparam)
{
	GtkWidget *menu;
	GtkWidget *menuitem;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(advanced.crosspost_colormenu));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	*((gint *)pparam->data) = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(menuitem), "color"));
}

static void prefs_account_crosspost_set_colormenu(PrefParam *pparam)
{
	gint colorlabel = *((gint *)pparam->data);
	GtkOptionMenu *colormenu = GTK_OPTION_MENU(*pparam->widget);
	GtkWidget *menu;
	GtkWidget *menuitem;

	gtk_option_menu_set_history(colormenu, colorlabel);
	menu = gtk_option_menu_get_menu(colormenu);
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	gtk_menu_item_activate(GTK_MENU_ITEM(menuitem));
}


static void prefs_account_advanced_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *hbox1;
	GtkWidget *checkbtn_smtpport;
	GtkWidget *entry_smtpport;
	GtkWidget *hbox_popport;
	GtkWidget *checkbtn_popport;
	GtkWidget *entry_popport;
	GtkWidget *hbox_imapport;
	GtkWidget *checkbtn_imapport;
	GtkWidget *entry_imapport;
	GtkWidget *hbox_nntpport;
	GtkWidget *checkbtn_nntpport;
	GtkWidget *entry_nntpport;
	GtkWidget *checkbtn_domain;
	GtkWidget *entry_domain;
	GtkWidget *checkbtn_crosspost;
 	GtkWidget *colormenu_crosspost;
 	GtkWidget *menu;
	GtkWidget *checkbtn_tunnelcmd;
	GtkWidget *entry_tunnelcmd;
	GtkWidget *folder_frame;
	GtkWidget *vbox3;
	GtkWidget *table;
	GtkWidget *sent_folder_chkbtn;
	GtkWidget *sent_folder_entry;
	GtkWidget *queue_folder_chkbtn;
	GtkWidget *queue_folder_entry;
	GtkWidget *draft_folder_chkbtn;
	GtkWidget *draft_folder_entry;
	GtkWidget *trash_folder_chkbtn;
	GtkWidget *trash_folder_entry;

#define PACK_HBOX(hbox) \
	{ \
	hbox = gtk_hbox_new (FALSE, 8); \
	gtk_widget_show (hbox); \
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0); \
	}

#define PACK_PORT_ENTRY(box, entry) \
	{ \
	entry = gtk_entry_new (); \
	gtk_entry_set_max_length  (GTK_ENTRY(entry), 5); \
	gtk_widget_show (entry); \
	gtk_box_pack_start (GTK_BOX (box), entry, FALSE, FALSE, 0); \
	gtk_widget_set_size_request (entry, 64, -1); \
	}

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW_2);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	PACK_HBOX (hbox1);
	PACK_CHECK_BUTTON (hbox1, checkbtn_smtpport, _("Specify SMTP port"));
	PACK_PORT_ENTRY (hbox1, entry_smtpport);
	SET_TOGGLE_SENSITIVITY (checkbtn_smtpport, entry_smtpport);

	PACK_HBOX (hbox_popport);
	PACK_CHECK_BUTTON (hbox_popport, checkbtn_popport,
			   _("Specify POP3 port"));
	PACK_PORT_ENTRY (hbox_popport, entry_popport);
	SET_TOGGLE_SENSITIVITY (checkbtn_popport, entry_popport);

	PACK_HBOX (hbox_imapport);
	PACK_CHECK_BUTTON (hbox_imapport, checkbtn_imapport,
			   _("Specify IMAP4 port"));
	PACK_PORT_ENTRY (hbox_imapport, entry_imapport);
	SET_TOGGLE_SENSITIVITY (checkbtn_imapport, entry_imapport);

	PACK_HBOX (hbox_nntpport);
	PACK_CHECK_BUTTON (hbox_nntpport, checkbtn_nntpport,
			   _("Specify NNTP port"));
	PACK_PORT_ENTRY (hbox_nntpport, entry_nntpport);
	SET_TOGGLE_SENSITIVITY (checkbtn_nntpport, entry_nntpport);

	PACK_HBOX (hbox1);
	PACK_CHECK_BUTTON (hbox1, checkbtn_domain, _("Specify domain name"));

	entry_domain = gtk_entry_new ();
	gtk_widget_show (entry_domain);
	gtk_box_pack_start (GTK_BOX (hbox1), entry_domain, TRUE, TRUE, 0);
	SET_TOGGLE_SENSITIVITY (checkbtn_domain, entry_domain);

	
	PACK_HBOX (hbox1);
	PACK_CHECK_BUTTON (hbox1, checkbtn_tunnelcmd,
			   _("Use command to communicate with server"));
	entry_tunnelcmd = gtk_entry_new ();
	gtk_widget_show (entry_tunnelcmd);
	gtk_box_pack_start (GTK_BOX (hbox1), entry_tunnelcmd, TRUE, TRUE, 0);
	SET_TOGGLE_SENSITIVITY (checkbtn_tunnelcmd, entry_tunnelcmd);

	PACK_HBOX (hbox1);
	PACK_CHECK_BUTTON (hbox1, checkbtn_crosspost, 
			   _("Mark cross-posted messages as read and color:"));
	g_signal_connect (G_OBJECT (checkbtn_crosspost), "toggled",
			  G_CALLBACK (crosspost_color_toggled),
			  NULL);

	colormenu_crosspost = gtk_option_menu_new();
	gtk_widget_show (colormenu_crosspost);
	gtk_box_pack_start (GTK_BOX (hbox1), colormenu_crosspost, FALSE, FALSE, 0);

	menu = colorlabel_create_color_menu();
	gtk_option_menu_set_menu (GTK_OPTION_MENU(colormenu_crosspost), menu);
	SET_TOGGLE_SENSITIVITY(checkbtn_crosspost, colormenu_crosspost);

	PACK_HBOX (hbox1);
#undef PACK_HBOX
#undef PACK_PORT_ENTRY

	/* special folder setting (maybe these options are redundant) */

	PACK_FRAME (vbox1, folder_frame, _("Folder"));

	vbox3 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox3);
	gtk_container_add (GTK_CONTAINER (folder_frame), vbox3);
	gtk_container_set_border_width (GTK_CONTAINER (vbox3), 8);

	table = gtk_table_new (4, 3, FALSE);
	gtk_widget_show (table);
	gtk_container_add (GTK_CONTAINER (vbox3), table);
	gtk_table_set_row_spacings (GTK_TABLE (table), VSPACING_NARROW_2);
	gtk_table_set_col_spacings (GTK_TABLE (table), 4);

#define SET_CHECK_BTN_AND_ENTRY(label, chkbtn, entry, n)		\
{									\
	GtkWidget *button;						\
									\
	chkbtn = gtk_check_button_new_with_label (label);		\
	gtk_widget_show (chkbtn);					\
	gtk_table_attach (GTK_TABLE (table), chkbtn,			\
			  0, 1, n, n + 1, GTK_FILL, 0, 0, 0);		\
									\
	entry = gtk_entry_new ();					\
	gtk_widget_show (entry);					\
	gtk_table_attach (GTK_TABLE (table), entry, 1, 2, n, n + 1,	\
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,		\
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);	\
									\
	button = gtkut_get_browse_file_btn(_("Browse"));		\
	gtk_widget_show (button);					\
	gtk_table_attach (GTK_TABLE (table), button,			\
			  2, 3, n, n + 1, GTK_FILL, 0, 0, 0);		\
	g_signal_connect						\
		(G_OBJECT (button), "clicked",			\
		 G_CALLBACK (prefs_account_select_folder_cb),		\
		 entry);						\
									\
	SET_TOGGLE_SENSITIVITY (chkbtn, entry);				\
	SET_TOGGLE_SENSITIVITY (chkbtn, button);			\
}

	SET_CHECK_BTN_AND_ENTRY(_("Put sent messages in"),
				sent_folder_chkbtn, sent_folder_entry, 0);
	SET_CHECK_BTN_AND_ENTRY(_("Put queued messages in"),
				queue_folder_chkbtn, queue_folder_entry, 1);
	SET_CHECK_BTN_AND_ENTRY(_("Put draft messages in"),
				draft_folder_chkbtn, draft_folder_entry, 2);
	SET_CHECK_BTN_AND_ENTRY(_("Put deleted messages in"),
				trash_folder_chkbtn, trash_folder_entry, 3);

	advanced.smtpport_chkbtn	= checkbtn_smtpport;
	advanced.smtpport_entry		= entry_smtpport;
	advanced.popport_hbox		= hbox_popport;
	advanced.popport_chkbtn		= checkbtn_popport;
	advanced.popport_entry		= entry_popport;
	advanced.imapport_hbox		= hbox_imapport;
	advanced.imapport_chkbtn	= checkbtn_imapport;
	advanced.imapport_entry		= entry_imapport;
	advanced.nntpport_hbox		= hbox_nntpport;
	advanced.nntpport_chkbtn	= checkbtn_nntpport;
	advanced.nntpport_entry		= entry_nntpport;
	advanced.domain_chkbtn		= checkbtn_domain;
	advanced.domain_entry		= entry_domain;
 	advanced.crosspost_chkbtn	= checkbtn_crosspost;
 	advanced.crosspost_colormenu	= colormenu_crosspost;

	advanced.tunnelcmd_chkbtn	= checkbtn_tunnelcmd;
	advanced.tunnelcmd_entry	= entry_tunnelcmd;

	advanced.sent_folder_chkbtn  = sent_folder_chkbtn;
	advanced.sent_folder_entry   = sent_folder_entry;
	advanced.queue_folder_chkbtn  = queue_folder_chkbtn;
	advanced.queue_folder_entry   = queue_folder_entry;
	advanced.draft_folder_chkbtn = draft_folder_chkbtn;
	advanced.draft_folder_entry  = draft_folder_entry;
	advanced.trash_folder_chkbtn = trash_folder_chkbtn;
	advanced.trash_folder_entry  = trash_folder_entry;
}

static gint prefs_account_apply(void)
{
	RecvProtocol protocol;
	GtkWidget *menu;
	GtkWidget *menuitem;
	gchar *old_id = NULL;
	gchar *new_id = NULL;
	
	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(basic.protocol_optmenu));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	protocol = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));

	if (*gtk_entry_get_text(GTK_ENTRY(basic.acname_entry)) == '\0') {
		alertpanel_error(_("Account name is not entered."));
		return -1;
	}
	if (*gtk_entry_get_text(GTK_ENTRY(basic.addr_entry)) == '\0') {
		alertpanel_error(_("Mail address is not entered."));
		return -1;
	}
	if (((protocol == A_POP3) || 
	     (protocol == A_LOCAL && !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(basic.mailcmd_chkbtn))) || 
	     (protocol == A_NONE)) &&
           *gtk_entry_get_text(GTK_ENTRY(basic.smtpserv_entry)) == '\0') {
		alertpanel_error(_("SMTP server is not entered."));
		return -1;
	}
	if ((protocol == A_POP3 || protocol == A_IMAP4) &&
	    *gtk_entry_get_text(GTK_ENTRY(basic.uid_entry)) == '\0') {
		alertpanel_error(_("User ID is not entered."));
		return -1;
	}
	if (protocol == A_POP3 &&
	    *gtk_entry_get_text(GTK_ENTRY(basic.recvserv_entry)) == '\0') {
		alertpanel_error(_("POP3 server is not entered."));
		return -1;
	}
	if (protocol == A_IMAP4 &&
	    *gtk_entry_get_text(GTK_ENTRY(basic.recvserv_entry)) == '\0') {
		alertpanel_error(_("IMAP4 server is not entered."));
		return -1;
	}
	if (protocol == A_NNTP &&
	    *gtk_entry_get_text(GTK_ENTRY(basic.nntpserv_entry)) == '\0') {
		alertpanel_error(_("NNTP server is not entered."));
		return -1;
	}

	if (protocol == A_LOCAL &&
	    *gtk_entry_get_text(GTK_ENTRY(basic.localmbox_entry)) == '\0') {
		alertpanel_error(_("local mailbox filename is not entered."));
		return -1;
	}

	if (protocol == A_LOCAL &&
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(basic.mailcmd_chkbtn)) && *gtk_entry_get_text(GTK_ENTRY(basic.mailcmd_entry)) == '\0') {
		alertpanel_error(_("mail command is not entered."));
		return -1;
	}
	
	if (protocol == A_IMAP4 || protocol == A_NNTP) 
		old_id = g_strdup_printf("#%s/%s",
				protocol == A_IMAP4 ? "imap":"nntp",
				tmp_ac_prefs.account_name ? tmp_ac_prefs.account_name : "(null)");
	
	prefs_set_data_from_dialog(param);
	
	if (protocol == A_IMAP4 || protocol == A_NNTP) {
		new_id = g_strdup_printf("#%s/%s",
				protocol == A_IMAP4 ? "imap":"nntp",
				tmp_ac_prefs.account_name);
		if (old_id != NULL && new_id != NULL)
			prefs_filtering_rename_path(old_id, new_id);
		g_free(old_id);
		g_free(new_id);
	}
	return 0;
}

static void pop_bfr_smtp_tm_set_sens(GtkWidget *widget, gpointer data)
{
	gtk_widget_set_sensitive(p_send.pop_bfr_smtp_tm_entry, 
				 gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(p_send.pop_bfr_smtp_chkbtn)));
}

static void prefs_account_select_folder_cb(GtkWidget *widget, gpointer data)
{
	FolderItem *item;
	gchar *id;

	item = foldersel_folder_sel(NULL, FOLDER_SEL_COPY, NULL);
	if (item && item->path) {
		id = folder_item_get_identifier(item);
		if (id) {
			gtk_entry_set_text(GTK_ENTRY(data), id);
			g_free(id);
		}
	}
}

static void prefs_account_sigfile_radiobtn_cb(GtkWidget *widget, gpointer data)
{
	gtk_widget_set_sensitive(GTK_WIDGET(signature_browse_button), TRUE);
}

static void prefs_account_sigcmd_radiobtn_cb(GtkWidget *widget, gpointer data)
{
	gtk_widget_set_sensitive(GTK_WIDGET(signature_browse_button), FALSE);
}

static void prefs_account_signature_browse_cb(GtkWidget *widget, gpointer data)
{
	gchar *filename;
	gchar *utf8_filename;

	filename = filesel_select_file_open(_("Select signature file"), NULL);
	if (!filename) return;

	utf8_filename = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
	if (!utf8_filename) {
		g_warning("prefs_account_signature_browse_cb(): failed to convert character set.");
		utf8_filename = g_strdup(filename);
	}
	gtk_entry_set_text(GTK_ENTRY(entry_sigpath), utf8_filename);
	g_free(utf8_filename);
}

static void prefs_account_edit_custom_header(void)
{
	prefs_custom_header_open(&tmp_ac_prefs);
}

static void prefs_account_enum_set_data_from_radiobtn(PrefParam *pparam)
{
	GtkRadioButton *radiobtn;
	GSList *group;

	radiobtn = GTK_RADIO_BUTTON (*pparam->widget);
	group = gtk_radio_button_get_group (radiobtn);
	while (group != NULL) {
		GtkToggleButton *btn = GTK_TOGGLE_BUTTON (group->data);
		if (gtk_toggle_button_get_active (btn)) {
			*((gint *)pparam->data) = GPOINTER_TO_INT
				(g_object_get_data (G_OBJECT (btn), MENU_VAL_ID));
			break;
		}
		group = group->next;
	}
}

static void prefs_account_enum_set_radiobtn(PrefParam *pparam)
{
	GtkRadioButton *radiobtn;
	GSList *group;
	gpointer data;

	data = GINT_TO_POINTER (*((gint *)pparam->data));
	radiobtn = GTK_RADIO_BUTTON (*pparam->widget);
	group = gtk_radio_button_get_group (radiobtn);
	while (group != NULL) {
		GtkToggleButton *btn = GTK_TOGGLE_BUTTON (group->data);
		gpointer data1 = g_object_get_data (G_OBJECT (btn), MENU_VAL_ID);
		if (data1 == data) {
			gtk_toggle_button_set_active (btn, TRUE);
			break;
		}
		group = group->next;
	}
}

static void prefs_account_protocol_set_data_from_optmenu(PrefParam *pparam)
{
	GtkWidget *menu;
	GtkWidget *menuitem;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(*pparam->widget));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	*((RecvProtocol *)pparam->data) = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));
}

static void prefs_account_protocol_set_optmenu(PrefParam *pparam)
{
	RecvProtocol protocol;
	GtkOptionMenu *optmenu = GTK_OPTION_MENU(*pparam->widget);
	GtkWidget *menu;
	GtkWidget *menuitem;
	gint index;

	protocol = *((RecvProtocol *)pparam->data);
	index = menu_find_option_menu_index
		(optmenu, GINT_TO_POINTER(protocol), NULL);
	if (index < 0) return;
	gtk_option_menu_set_history(optmenu, index);

	menu = gtk_option_menu_get_menu(optmenu);
	menu_set_insensitive_all(GTK_MENU_SHELL(menu));

	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	gtk_widget_set_sensitive(menuitem, TRUE);
	gtk_menu_item_activate(GTK_MENU_ITEM(menuitem));
}

static void prefs_account_imap_auth_type_set_data_from_optmenu(PrefParam *pparam)
{
	GtkWidget *menu;
	GtkWidget *menuitem;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(*pparam->widget));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	*((RecvProtocol *)pparam->data) = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));
}

static void prefs_account_imap_auth_type_set_optmenu(PrefParam *pparam)
{
	IMAPAuthType type = *((IMAPAuthType *)pparam->data);
	GtkOptionMenu *optmenu = GTK_OPTION_MENU(*pparam->widget);
	GtkWidget *menu;
	GtkWidget *menuitem;

	switch (type) {
	case IMAP_AUTH_LOGIN:
		gtk_option_menu_set_history(optmenu, 1);
		break;
	case IMAP_AUTH_CRAM_MD5:
		gtk_option_menu_set_history(optmenu, 2);
		break;
	case 0:
	default:
		gtk_option_menu_set_history(optmenu, 0);
	}

	menu = gtk_option_menu_get_menu(optmenu);
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	gtk_menu_item_activate(GTK_MENU_ITEM(menuitem));
}

static void prefs_account_smtp_auth_type_set_data_from_optmenu(PrefParam *pparam)
{
	GtkWidget *menu;
	GtkWidget *menuitem;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(*pparam->widget));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	*((RecvProtocol *)pparam->data) = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));
}

static void prefs_account_smtp_auth_type_set_optmenu(PrefParam *pparam)
{
	SMTPAuthType type = *((SMTPAuthType *)pparam->data);
	GtkOptionMenu *optmenu = GTK_OPTION_MENU(*pparam->widget);
	GtkWidget *menu;
	GtkWidget *menuitem;

	switch (type) {
	case SMTPAUTH_PLAIN:
		gtk_option_menu_set_history(optmenu, 1);
		break;
	case SMTPAUTH_LOGIN:
		gtk_option_menu_set_history(optmenu, 2);
		break;
	case SMTPAUTH_CRAM_MD5:
		gtk_option_menu_set_history(optmenu, 3);
		break;
	case SMTPAUTH_DIGEST_MD5:
		gtk_option_menu_set_history(optmenu, 4);
		break;
	case 0:
	default:
		gtk_option_menu_set_history(optmenu, 0);
	}

	menu = gtk_option_menu_get_menu(optmenu);
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	gtk_menu_item_activate(GTK_MENU_ITEM(menuitem));
}

static void prefs_account_set_string_from_optmenu(PrefParam *pparam)
{
	GtkWidget *menu;
	GtkWidget *menuitem;
	gchar **str;

	g_return_if_fail(*pparam->widget != NULL);

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(*pparam->widget));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	if (menuitem == NULL)
		return;

	str = (gchar **) pparam->data;
        g_free(*str);
	*str = g_strdup(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));
}

static void prefs_account_set_optmenu_from_string(PrefParam *pparam)
{
	GtkWidget *optionmenu;
	GtkWidget *menu;
	gboolean found = FALSE;
	GList *children, *cur;
	gchar *prefsid;
	guint i = 0;

	g_return_if_fail(*pparam->widget != NULL);

	prefsid = *((gchar **) pparam->data);
	if (prefsid == NULL)
		return;

	optionmenu = *pparam->widget;
	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(optionmenu));
	children = gtk_container_children(GTK_CONTAINER(menu));
	for (cur = children; cur != NULL; cur = g_list_next(cur)) {
		GtkWidget *item = (GtkWidget *) cur->data;
		gchar *id;

		id = g_object_get_data(G_OBJECT(item), MENU_VAL_ID);
		if (id != NULL && strcmp(id, prefsid) == 0) {
			found = TRUE;
			gtk_option_menu_set_history(GTK_OPTION_MENU(optionmenu), i);
		}
		i++;
	}

	if (!found) {
		gchar *name;
		GtkWidget *menuitem;

		name = g_strdup_printf(_("Unsupported (%s)"), prefsid);
		menuitem = gtk_menu_item_new_with_label(name);
		gtk_widget_show(menuitem);
		g_object_set_data_full(G_OBJECT(menuitem), MENU_VAL_ID, g_strdup(prefsid), g_free);
		gtk_menu_append(GTK_MENU(menu), menuitem);
		g_free(name);

		gtk_option_menu_set_history(GTK_OPTION_MENU(optionmenu), i);
	}

	g_list_free(children);
}

static void prefs_account_protocol_activated(GtkMenuItem *menuitem)
{
	RecvProtocol protocol;

	protocol = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));

	switch(protocol) {
	case A_NNTP:
		gtk_widget_show(basic.nntpserv_label);
		gtk_widget_show(basic.nntpserv_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   0, VSPACING_NARROW);

		gtk_widget_set_sensitive(basic.nntpauth_chkbtn, TRUE);
		gtk_widget_show(basic.nntpauth_chkbtn);

		gtk_widget_set_sensitive(basic.nntpauth_onconnect_chkbtn, TRUE);
		gtk_widget_show(basic.nntpauth_onconnect_chkbtn);

  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   1, VSPACING_NARROW);
		gtk_widget_hide(basic.recvserv_label);
		gtk_widget_hide(basic.recvserv_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   2, 0);
		gtk_widget_show(basic.smtpserv_label);
		gtk_widget_show(basic.smtpserv_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   4, VSPACING_NARROW);
		gtk_widget_hide(basic.localmbox_label);
		gtk_widget_hide(basic.localmbox_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   3, 0);
		gtk_widget_hide(basic.mailcmd_label);
		gtk_widget_hide(basic.mailcmd_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   6, 0);
		gtk_widget_hide(basic.mailcmd_chkbtn);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   5, 0);
		gtk_widget_show(basic.uid_label);
		gtk_widget_show(basic.pass_label);
		gtk_widget_show(basic.uid_entry);
		gtk_widget_show(basic.pass_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   7, VSPACING_NARROW);

		gtk_widget_set_sensitive(basic.uid_label,  TRUE);
		gtk_widget_set_sensitive(basic.pass_label, TRUE);
		gtk_widget_set_sensitive(basic.uid_entry,  TRUE);
		gtk_widget_set_sensitive(basic.pass_entry, TRUE);

		/* update userid/passwd sensitive state */

		prefs_account_nntpauth_toggled
			(GTK_TOGGLE_BUTTON(basic.nntpauth_chkbtn), NULL);
		gtk_widget_hide(receive.pop3_frame);
		gtk_widget_hide(receive.imap_frame);
		gtk_widget_hide(receive.local_frame);
		gtk_widget_show(receive.frame_maxarticle);
		gtk_widget_set_sensitive(receive.filter_on_recv_chkbtn, TRUE);
		gtk_widget_set_sensitive(receive.recvatgetall_chkbtn, TRUE);
		/* update pop_before_smtp sensitivity */
		gtk_toggle_button_set_active
			(GTK_TOGGLE_BUTTON(p_send.pop_bfr_smtp_chkbtn), FALSE);
		gtk_widget_set_sensitive(p_send.pop_bfr_smtp_chkbtn, FALSE);
		gtk_widget_set_sensitive(p_send.pop_bfr_smtp_tm_entry, FALSE);
		
		if (!tmp_ac_prefs.account_name) {
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(receive.filter_on_recv_chkbtn), 
				TRUE);
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(receive.recvatgetall_chkbtn),
				 FALSE);
		}

#if USE_OPENSSL
		gtk_widget_hide(ssl.pop_frame);
		gtk_widget_hide(ssl.imap_frame);
		gtk_widget_show(ssl.nntp_frame);
		gtk_widget_show(ssl.send_frame);
#endif
		gtk_widget_hide(advanced.popport_hbox);
		gtk_widget_hide(advanced.imapport_hbox);
		gtk_widget_show(advanced.nntpport_hbox);
		gtk_widget_show(advanced.crosspost_chkbtn);
		gtk_widget_show(advanced.crosspost_colormenu);
		gtk_widget_hide(advanced.tunnelcmd_chkbtn);
		gtk_widget_hide(advanced.tunnelcmd_entry);
		gtk_widget_hide(receive.imapdir_label);
		gtk_widget_hide(receive.imapdir_entry);
		break;
	case A_LOCAL:
		gtk_widget_hide(basic.nntpserv_label);
		gtk_widget_hide(basic.nntpserv_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   0, 0);
		gtk_widget_set_sensitive(basic.nntpauth_chkbtn, FALSE);
		gtk_widget_hide(basic.nntpauth_chkbtn);

		gtk_widget_set_sensitive(basic.nntpauth_onconnect_chkbtn, FALSE);
		gtk_widget_hide(basic.nntpauth_onconnect_chkbtn);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   1, 0);
		gtk_widget_hide(basic.recvserv_label);
		gtk_widget_hide(basic.recvserv_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   2, 0);
		gtk_widget_show(basic.smtpserv_label);
		gtk_widget_show(basic.smtpserv_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   4, VSPACING_NARROW);
		gtk_widget_show(basic.localmbox_label);
		gtk_widget_show(basic.localmbox_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   3, VSPACING_NARROW);
		gtk_widget_show(basic.mailcmd_label);
		gtk_widget_show(basic.mailcmd_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   6, VSPACING_NARROW);
		gtk_widget_show(basic.mailcmd_chkbtn);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   5, VSPACING_NARROW);
		gtk_widget_hide(basic.uid_label);
		gtk_widget_hide(basic.pass_label);
		gtk_widget_hide(basic.uid_entry);
		gtk_widget_hide(basic.pass_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   7, 0);

		gtk_widget_set_sensitive(basic.uid_label,  TRUE);
		gtk_widget_set_sensitive(basic.pass_label, TRUE);
		gtk_widget_set_sensitive(basic.uid_entry,  TRUE);
		gtk_widget_set_sensitive(basic.pass_entry, TRUE);
		gtk_widget_hide(receive.pop3_frame);
		gtk_widget_hide(receive.imap_frame);
		gtk_widget_show(receive.local_frame);
		gtk_widget_hide(receive.frame_maxarticle);
		gtk_widget_set_sensitive(receive.filter_on_recv_chkbtn, TRUE);
		gtk_widget_set_sensitive(receive.recvatgetall_chkbtn, TRUE);
		prefs_account_mailcmd_toggled
			(GTK_TOGGLE_BUTTON(basic.mailcmd_chkbtn), NULL);

		/* update pop_before_smtp sensitivity */
		gtk_toggle_button_set_active
			(GTK_TOGGLE_BUTTON(p_send.pop_bfr_smtp_chkbtn), FALSE);
		gtk_widget_set_sensitive(p_send.pop_bfr_smtp_chkbtn, FALSE);
		gtk_widget_set_sensitive(p_send.pop_bfr_smtp_tm_entry, FALSE);

		if (!tmp_ac_prefs.account_name) {
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(receive.filter_on_recv_chkbtn), 
				TRUE);
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(receive.recvatgetall_chkbtn),
				 TRUE);
		}

#if USE_OPENSSL
		gtk_widget_hide(ssl.pop_frame);
		gtk_widget_hide(ssl.imap_frame);
		gtk_widget_hide(ssl.nntp_frame);
		gtk_widget_show(ssl.send_frame);
#endif
		gtk_widget_hide(advanced.popport_hbox);
		gtk_widget_hide(advanced.imapport_hbox);
		gtk_widget_hide(advanced.nntpport_hbox);
		gtk_widget_hide(advanced.crosspost_chkbtn);
		gtk_widget_hide(advanced.crosspost_colormenu);
		gtk_widget_hide(advanced.tunnelcmd_chkbtn);
		gtk_widget_hide(advanced.tunnelcmd_entry);
		gtk_widget_hide(receive.imapdir_label);
		gtk_widget_hide(receive.imapdir_entry);
		break;
	case A_IMAP4:
		gtk_widget_hide(basic.nntpserv_label);
		gtk_widget_hide(basic.nntpserv_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   0, 0);
		gtk_widget_set_sensitive(basic.nntpauth_chkbtn, FALSE);
		gtk_widget_hide(basic.nntpauth_chkbtn);

		gtk_widget_set_sensitive(basic.nntpauth_onconnect_chkbtn, FALSE);
		gtk_widget_hide(basic.nntpauth_onconnect_chkbtn);

  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   1, 0);
		gtk_widget_set_sensitive(basic.recvserv_label, TRUE);
		gtk_widget_set_sensitive(basic.recvserv_entry, TRUE);
		gtk_widget_show(basic.recvserv_label);
		gtk_widget_show(basic.recvserv_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   2, VSPACING_NARROW);
		gtk_widget_show(basic.smtpserv_label);
		gtk_widget_show(basic.smtpserv_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   4, VSPACING_NARROW);
		gtk_widget_hide(basic.localmbox_label);
		gtk_widget_hide(basic.localmbox_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   3, 0);
		gtk_widget_hide(basic.mailcmd_label);
		gtk_widget_hide(basic.mailcmd_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   6, 0);
		gtk_widget_hide(basic.mailcmd_chkbtn);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   5, 0);
		gtk_widget_show(basic.uid_label);
		gtk_widget_show(basic.pass_label);
		gtk_widget_show(basic.uid_entry);
		gtk_widget_show(basic.pass_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   7, VSPACING_NARROW);

		gtk_widget_set_sensitive(basic.uid_label,  TRUE);
		gtk_widget_set_sensitive(basic.pass_label, TRUE);
		gtk_widget_set_sensitive(basic.uid_entry,  TRUE);
		gtk_widget_set_sensitive(basic.pass_entry, TRUE);
		gtk_widget_hide(receive.pop3_frame);
		gtk_widget_show(receive.imap_frame);
		gtk_widget_hide(receive.local_frame);
		gtk_widget_hide(receive.frame_maxarticle);
		gtk_widget_set_sensitive(receive.filter_on_recv_chkbtn, TRUE);
		gtk_widget_set_sensitive(receive.recvatgetall_chkbtn, TRUE);
		gtk_widget_set_sensitive(basic.smtpserv_entry, TRUE);
		gtk_widget_set_sensitive(basic.smtpserv_label, TRUE);

		/* update pop_before_smtp sensitivity */
		gtk_toggle_button_set_active
			(GTK_TOGGLE_BUTTON(p_send.pop_bfr_smtp_chkbtn), FALSE);
		gtk_widget_set_sensitive(p_send.pop_bfr_smtp_chkbtn, FALSE);
		gtk_widget_set_sensitive(p_send.pop_bfr_smtp_tm_entry, FALSE);

		if (!tmp_ac_prefs.account_name) {
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(receive.filter_on_recv_chkbtn), 
				TRUE);
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(receive.recvatgetall_chkbtn),
				 FALSE);
		}

#if USE_OPENSSL
		gtk_widget_hide(ssl.pop_frame);
		gtk_widget_show(ssl.imap_frame);
		gtk_widget_hide(ssl.nntp_frame);
		gtk_widget_show(ssl.send_frame);
#endif
		gtk_widget_hide(advanced.popport_hbox);
		gtk_widget_show(advanced.imapport_hbox);
		gtk_widget_hide(advanced.nntpport_hbox);
		gtk_widget_hide(advanced.crosspost_chkbtn);
		gtk_widget_hide(advanced.crosspost_colormenu);
		gtk_widget_show(advanced.tunnelcmd_chkbtn);
		gtk_widget_show(advanced.tunnelcmd_entry);
		gtk_widget_show(receive.imapdir_label);
		gtk_widget_show(receive.imapdir_entry);
		break;
	case A_NONE:
		gtk_widget_hide(basic.nntpserv_label);
		gtk_widget_hide(basic.nntpserv_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   0, 0);
		gtk_widget_set_sensitive(basic.nntpauth_chkbtn, FALSE);
		gtk_widget_hide(basic.nntpauth_chkbtn);

		gtk_widget_set_sensitive(basic.nntpauth_onconnect_chkbtn, FALSE);
		gtk_widget_hide(basic.nntpauth_onconnect_chkbtn);

  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   1, 0);
		gtk_widget_set_sensitive(basic.recvserv_label, FALSE);
		gtk_widget_set_sensitive(basic.recvserv_entry, FALSE);
		gtk_widget_hide(basic.recvserv_label);
		gtk_widget_hide(basic.recvserv_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   2, VSPACING_NARROW);
		gtk_widget_show(basic.smtpserv_label);
		gtk_widget_show(basic.smtpserv_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   4, VSPACING_NARROW);
		gtk_widget_hide(basic.localmbox_label);
		gtk_widget_hide(basic.localmbox_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   3, 0);
		gtk_widget_hide(basic.mailcmd_label);
		gtk_widget_hide(basic.mailcmd_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   6, 0);
		gtk_widget_hide(basic.mailcmd_chkbtn);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   5, 0);
		gtk_widget_hide(basic.uid_label);
		gtk_widget_hide(basic.pass_label);
		gtk_widget_hide(basic.uid_entry);
		gtk_widget_hide(basic.pass_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   7, VSPACING_NARROW);

		gtk_widget_set_sensitive(basic.uid_label,  FALSE);
		gtk_widget_set_sensitive(basic.pass_label, FALSE);
		gtk_widget_set_sensitive(basic.uid_entry,  FALSE);
		gtk_widget_set_sensitive(basic.pass_entry, FALSE);
		gtk_widget_set_sensitive(receive.pop3_frame, FALSE);
		gtk_widget_hide(receive.pop3_frame);
		gtk_widget_hide(receive.imap_frame);
		gtk_widget_hide(receive.local_frame);
		gtk_widget_hide(receive.frame_maxarticle);
		gtk_widget_set_sensitive(receive.filter_on_recv_chkbtn, FALSE);
		gtk_widget_set_sensitive(receive.recvatgetall_chkbtn, FALSE);

		gtk_widget_set_sensitive(basic.smtpserv_entry, TRUE);
		gtk_widget_set_sensitive(basic.smtpserv_label, TRUE);

		/* update pop_before_smtp sensitivity */
		gtk_widget_set_sensitive(p_send.pop_bfr_smtp_chkbtn, FALSE);
		pop_bfr_smtp_tm_set_sens(NULL, NULL);
	
		gtk_toggle_button_set_active
			(GTK_TOGGLE_BUTTON(receive.filter_on_recv_chkbtn), FALSE);
		gtk_toggle_button_set_active
			(GTK_TOGGLE_BUTTON(receive.recvatgetall_chkbtn), FALSE);

#if USE_OPENSSL
		gtk_widget_hide(ssl.pop_frame);
		gtk_widget_hide(ssl.imap_frame);
		gtk_widget_hide(ssl.nntp_frame);
		gtk_widget_show(ssl.send_frame);
#endif
		gtk_widget_hide(advanced.popport_hbox);
		gtk_widget_hide(advanced.imapport_hbox);
		gtk_widget_hide(advanced.nntpport_hbox);
		gtk_widget_hide(advanced.crosspost_chkbtn);
		gtk_widget_hide(advanced.crosspost_colormenu);
		gtk_widget_hide(advanced.tunnelcmd_chkbtn);
		gtk_widget_hide(advanced.tunnelcmd_entry);
		gtk_widget_hide(receive.imapdir_label);
		gtk_widget_hide(receive.imapdir_entry);
		break;
	case A_POP3:
	default:
		gtk_widget_hide(basic.nntpserv_label);
		gtk_widget_hide(basic.nntpserv_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   0, 0);
		gtk_widget_set_sensitive(basic.nntpauth_chkbtn, FALSE);
		gtk_widget_hide(basic.nntpauth_chkbtn);

		gtk_widget_set_sensitive(basic.nntpauth_onconnect_chkbtn, FALSE);
		gtk_widget_hide(basic.nntpauth_onconnect_chkbtn);

  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   1, 0);
		gtk_widget_set_sensitive(basic.recvserv_label, TRUE);
		gtk_widget_set_sensitive(basic.recvserv_entry, TRUE);
		gtk_widget_show(basic.recvserv_label);
		gtk_widget_show(basic.recvserv_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   2, VSPACING_NARROW);
		gtk_widget_show(basic.smtpserv_label);
		gtk_widget_show(basic.smtpserv_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   4, VSPACING_NARROW);
		gtk_widget_hide(basic.localmbox_label);
		gtk_widget_hide(basic.localmbox_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   3, 0);
		gtk_widget_hide(basic.mailcmd_label);
		gtk_widget_hide(basic.mailcmd_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   6, 0);
		gtk_widget_hide(basic.mailcmd_chkbtn);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   5, 0);
		gtk_widget_show(basic.uid_label);
		gtk_widget_show(basic.pass_label);
		gtk_widget_show(basic.uid_entry);
		gtk_widget_show(basic.pass_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   7, VSPACING_NARROW);

		gtk_widget_set_sensitive(basic.uid_label,  TRUE);
		gtk_widget_set_sensitive(basic.pass_label, TRUE);
		gtk_widget_set_sensitive(basic.uid_entry,  TRUE);
		gtk_widget_set_sensitive(basic.pass_entry, TRUE);
		gtk_widget_set_sensitive(receive.pop3_frame, TRUE);
		gtk_widget_show(receive.pop3_frame);
		gtk_widget_hide(receive.imap_frame);
		gtk_widget_hide(receive.local_frame);
		gtk_widget_hide(receive.frame_maxarticle);
		gtk_widget_set_sensitive(receive.filter_on_recv_chkbtn, TRUE);
		gtk_widget_set_sensitive(receive.recvatgetall_chkbtn, TRUE);

		gtk_widget_set_sensitive(basic.smtpserv_entry, TRUE);
		gtk_widget_set_sensitive(basic.smtpserv_label, TRUE);

		/* update pop_before_smtp sensitivity */
		gtk_widget_set_sensitive(p_send.pop_bfr_smtp_chkbtn, TRUE);
		pop_bfr_smtp_tm_set_sens(NULL, NULL);
		
		if (!tmp_ac_prefs.account_name) {
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(receive.filter_on_recv_chkbtn), 
				TRUE);
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(receive.recvatgetall_chkbtn),
				 TRUE);
		}

#if USE_OPENSSL
		gtk_widget_show(ssl.pop_frame);
		gtk_widget_hide(ssl.imap_frame);
		gtk_widget_hide(ssl.nntp_frame);
		gtk_widget_show(ssl.send_frame);
#endif
		gtk_widget_show(advanced.popport_hbox);
		gtk_widget_hide(advanced.imapport_hbox);
		gtk_widget_hide(advanced.nntpport_hbox);
		gtk_widget_hide(advanced.crosspost_chkbtn);
		gtk_widget_hide(advanced.crosspost_colormenu);
		gtk_widget_hide(advanced.tunnelcmd_chkbtn);
		gtk_widget_hide(advanced.tunnelcmd_entry);
		gtk_widget_hide(receive.imapdir_label);
		gtk_widget_hide(receive.imapdir_entry);
		break;
	}

	gtk_widget_queue_resize(basic.serv_frame);
}

static void prefs_account_nntpauth_toggled(GtkToggleButton *button,
					   gpointer user_data)
{
	gboolean auth;

	if (!GTK_WIDGET_SENSITIVE (GTK_WIDGET (button)))
		return;
	auth = gtk_toggle_button_get_active (button);
	gtk_widget_set_sensitive(basic.uid_label,  auth);
	gtk_widget_set_sensitive(basic.pass_label, auth);
	gtk_widget_set_sensitive(basic.uid_entry,  auth);
	gtk_widget_set_sensitive(basic.pass_entry, auth);
	gtk_widget_set_sensitive(basic.nntpauth_onconnect_chkbtn, auth);
}

static void prefs_account_mailcmd_toggled(GtkToggleButton *button,
					  gpointer user_data)
{
	gboolean use_mailcmd;

	use_mailcmd = gtk_toggle_button_get_active (button);

	gtk_widget_set_sensitive(basic.mailcmd_entry,  use_mailcmd);
	gtk_widget_set_sensitive(basic.mailcmd_label, use_mailcmd);
	gtk_widget_set_sensitive(basic.smtpserv_entry, !use_mailcmd);
	gtk_widget_set_sensitive(basic.smtpserv_label, !use_mailcmd);
	gtk_widget_set_sensitive(basic.uid_entry,  !use_mailcmd);
	gtk_widget_set_sensitive(basic.pass_entry, !use_mailcmd);
}

void prefs_account_register_page(PrefsPage *page)
{
	prefs_pages = g_slist_append(prefs_pages, page);
}

void prefs_account_unregister_page(PrefsPage *page)
{
	prefs_pages = g_slist_remove(prefs_pages, page);
}
