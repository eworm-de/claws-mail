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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "intl.h"
#include "main.h"
#include "prefs.h"
#include "prefs_account.h"
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

static gboolean cancelled;

static PrefsAccount tmp_ac_prefs;

static PrefsDialog dialog;

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
	GtkWidget *rmmail_chkbtn;
	GtkWidget *leave_time_entry;
	GtkWidget *getall_chkbtn;
	GtkWidget *sd_filter_on_recv_chkbtn;
	GtkWidget *sd_rmmail_chkbtn;
	GtkWidget *size_limit_chkbtn;
	GtkWidget *size_limit_entry;
	GtkWidget *filter_on_recv_chkbtn;
	GtkWidget *inbox_label;
	GtkWidget *inbox_entry;
	GtkWidget *inbox_btn;

	GtkWidget *recvatgetall_chkbtn;
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
} send;

static struct Compose {
	GtkWidget *sigpath_entry;

	GtkWidget *autocc_chkbtn;
	GtkWidget *autocc_entry;
	GtkWidget *autobcc_chkbtn;
	GtkWidget *autobcc_entry;
	GtkWidget *autoreplyto_chkbtn;
	GtkWidget *autoreplyto_entry;
} compose;

#if USE_GPGME
static struct Privacy {
	GtkWidget *checkbtn_default_encrypt;
	GtkWidget *checkbtn_ascii_armored;
	GtkWidget *checkbtn_default_sign;
	GtkWidget *defaultkey_radiobtn;
	GtkWidget *emailkey_radiobtn;
	GtkWidget *customkey_radiobtn;
	GtkWidget *customkey_entry;
} privacy;
#endif /* USE_GPGME */

#if USE_SSL
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
} ssl;
#endif /* USE_SSL */

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
	GtkWidget *tunnelcmd_chkbtn;
	GtkWidget *tunnelcmd_entry;
	GtkWidget *crosspost_chkbtn;
 	GtkWidget *crosspost_colormenu;

	GtkWidget *imap_frame;
	GtkWidget *imapdir_entry;

	GtkWidget *sent_folder_chkbtn;
	GtkWidget *sent_folder_entry;
	GtkWidget *draft_folder_chkbtn;
	GtkWidget *draft_folder_entry;
	GtkWidget *trash_folder_chkbtn;
	GtkWidget *trash_folder_entry;
} advanced;

static void prefs_account_fix_size			(void);

static void prefs_account_protocol_set_data_from_optmenu(PrefParam *pparam);
static void prefs_account_protocol_set_optmenu		(PrefParam *pparam);
static void prefs_account_protocol_activated		(GtkMenuItem *menuitem);

static void prefs_account_smtp_auth_type_set_data_from_optmenu
							(PrefParam *pparam);
static void prefs_account_smtp_auth_type_set_optmenu	(PrefParam *pparam);

#if USE_GPGME || USE_SSL
static void prefs_account_enum_set_data_from_radiobtn	(PrefParam *pparam);
static void prefs_account_enum_set_radiobtn		(PrefParam *pparam);
static void prefs_account_ascii_armored_warning(GtkWidget* widget, 
				               gpointer unused);
#endif /* USE_GPGME || USE_SSL */
static void prefs_account_crosspost_set_data_from_colormenu(PrefParam *pparam);
static void prefs_account_crosspost_set_colormenu(PrefParam *pparam);

static void prefs_account_nntpauth_toggled(GtkToggleButton *button,
					   gpointer user_data);
static void prefs_account_mailcmd_toggled(GtkToggleButton *button,
					  gpointer user_data);
static void prefs_account_smtp_userid_cb(GtkEditable *editable,
					 gpointer smtp_passwd);

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

	{"local_mbox", NULL, &tmp_ac_prefs.local_mbox, P_STRING,
	 &basic.localmbox_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"use_mail_command", "FALSE", &tmp_ac_prefs.use_mail_command, P_BOOL,
	 &basic.mailcmd_chkbtn, prefs_set_data_from_toggle, prefs_set_toggle},

	{"mail_command", "/usr/sbin/sendmail -t", &tmp_ac_prefs.mail_command, P_STRING,
	 &basic.mailcmd_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"use_nntp_auth", "FALSE", &tmp_ac_prefs.use_nntp_auth, P_BOOL,
	 &basic.nntpauth_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"user_id", "ENV_USER", &tmp_ac_prefs.userid, P_STRING,
	 &basic.uid_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"password", NULL, &tmp_ac_prefs.passwd, P_STRING,
	 &basic.pass_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"inbox", "inbox", &tmp_ac_prefs.inbox, P_STRING,
	 &receive.inbox_entry, prefs_set_data_from_entry, prefs_set_entry},

	/* Receive */
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

	/* selective download */	
	{"sd_filter_on_receive", "TRUE", &tmp_ac_prefs.sd_filter_on_recv, P_BOOL,
	 &receive.sd_filter_on_recv_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"sd_remove_mail_on_download", "TRUE", &tmp_ac_prefs.sd_rmmail_on_download, P_BOOL,
	 &receive.sd_rmmail_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"receive_at_get_all", "TRUE", &tmp_ac_prefs.recv_at_getall, P_BOOL,
	 &receive.recvatgetall_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	/* Send */
	{"add_date", "TRUE", &tmp_ac_prefs.add_date, P_BOOL,
	 &send.date_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"generate_msgid", "TRUE", &tmp_ac_prefs.gen_msgid, P_BOOL,
	 &send.msgid_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"add_custom_header", "FALSE", &tmp_ac_prefs.add_customhdr, P_BOOL,
	 &send.customhdr_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"use_smtp_auth", "FALSE", &tmp_ac_prefs.use_smtp_auth, P_BOOL,
	 &send.smtp_auth_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"smtp_auth_method", "0", &tmp_ac_prefs.smtp_auth_type, P_ENUM,
	 &send.smtp_auth_type_optmenu,
	 prefs_account_smtp_auth_type_set_data_from_optmenu,
	 prefs_account_smtp_auth_type_set_optmenu},

	{"smtp_user_id", NULL, &tmp_ac_prefs.smtp_userid, P_STRING,
	 &send.smtp_uid_entry, prefs_set_data_from_entry, prefs_set_entry},
	{"smtp_password", NULL, &tmp_ac_prefs.smtp_passwd, P_STRING,
	 &send.smtp_pass_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"pop_before_smtp", "FALSE", &tmp_ac_prefs.pop_before_smtp, P_BOOL,
	 &send.pop_bfr_smtp_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	/* Compose */
	{"signature_path", "~/"DEFAULT_SIGNATURE, &tmp_ac_prefs.sig_path, P_STRING,
	 &compose.sigpath_entry,
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

#if USE_GPGME
	/* Privacy */
	{"default_encrypt", "FALSE", &tmp_ac_prefs.default_encrypt, P_BOOL,
	 &privacy.checkbtn_default_encrypt,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"ascii_armored", "FALSE", &tmp_ac_prefs.ascii_armored, P_BOOL,
	 &privacy.checkbtn_ascii_armored,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"default_sign", "FALSE", &tmp_ac_prefs.default_sign, P_BOOL,
	 &privacy.checkbtn_default_sign,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"sign_key", NULL, &tmp_ac_prefs.sign_key, P_ENUM,
	 &privacy.defaultkey_radiobtn,
	 prefs_account_enum_set_data_from_radiobtn,
	 prefs_account_enum_set_radiobtn},
	{"sign_key_id", NULL, &tmp_ac_prefs.sign_key_id, P_STRING,
	 &privacy.customkey_entry,
	 prefs_set_data_from_entry, prefs_set_entry},
#endif /* USE_GPGME */

#if USE_SSL
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
#endif /* USE_SSL */

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
	 &advanced.imapdir_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"set_sent_folder", "FALSE", &tmp_ac_prefs.set_sent_folder, P_BOOL,
	 &advanced.sent_folder_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"sent_folder", NULL, &tmp_ac_prefs.sent_folder, P_STRING,
	 &advanced.sent_folder_entry,
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

static void prefs_account_create		(void);
static void prefs_account_basic_create		(void);
static void prefs_account_receive_create	(void);
static void prefs_account_send_create		(void);
static void prefs_account_compose_create	(void);
#if USE_GPGME
static void prefs_account_privacy_create	(void);
#endif /* USE_GPGME */
#if USE_SSL
static void prefs_account_ssl_create		(void);
#endif /* USE_SSL */
static void prefs_account_advanced_create	(void);

static void prefs_account_select_folder_cb	(GtkWidget	*widget,
						 gpointer	 data);
static void prefs_account_edit_custom_header	(void);

static gint prefs_account_deleted		(GtkWidget	*widget,
						 GdkEventAny	*event,
						 gpointer	 data);
static void prefs_account_key_pressed		(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gpointer	 data);
static void prefs_account_ok			(void);
static gint prefs_account_apply			(void);
static void prefs_account_cancel		(void);

void prefs_account_read_config(PrefsAccount *ac_prefs, const gchar *label)
{
	const gchar *p = label;
	gint id;

	g_return_if_fail(ac_prefs != NULL);
	g_return_if_fail(label != NULL);

	memset(&tmp_ac_prefs, 0, sizeof(PrefsAccount));
	prefs_read_config(param, label, ACCOUNT_RC);
	*ac_prefs = tmp_ac_prefs;
	while (*p && !isdigit(*p)) p++;
	id = atoi(p);
	if (id < 0) g_warning("wrong account id: %d\n", id);
	ac_prefs->account_id = id;

	prefs_custom_header_read_config(ac_prefs);
}

void prefs_account_save_config(PrefsAccount *ac_prefs)
{
	gchar *buf;

	g_return_if_fail(ac_prefs != NULL);

	tmp_ac_prefs = *ac_prefs;
	buf = g_strdup_printf("Account: %d", ac_prefs->account_id);
	prefs_save_config(param, buf, ACCOUNT_RC);
}

void prefs_account_save_config_all(GList *account_list)
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
		tmp_ac_prefs = *(PrefsAccount *)cur->data;
		if (fprintf(pfile->fp, "[Account: %d]\n",
			    tmp_ac_prefs.account_id) <= 0 ||
		    prefs_write_param(param, pfile->fp) < 0) {
			g_warning(_("failed to write configuration to file\n"));
			prefs_write_close_revert(pfile);
			return;
		}
		if (cur->next) {
			if (fputc('\n', pfile->fp) == EOF) {
				FILE_OP_ERROR(rcpath, "fputc");
				prefs_write_close_revert(pfile);
				return;
			}
		}
	}

	if (prefs_write_close(pfile) < 0)
		g_warning(_("failed to write configuration to file\n"));
}

void prefs_account_free(PrefsAccount *ac_prefs)
{
	if (!ac_prefs) return;

	tmp_ac_prefs = *ac_prefs;
	prefs_free(param);
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

PrefsAccount *prefs_account_open(PrefsAccount *ac_prefs)
{
	gboolean new_account = FALSE;

	if (prefs_rc_is_readonly(ACCOUNT_RC))
		return ac_prefs;

	debug_print(_("Opening account preferences window...\n"));

	inc_lock();

	cancelled = FALSE;

	if (!ac_prefs) {
		ac_prefs = g_new0(PrefsAccount, 1);
		memset(&tmp_ac_prefs, 0, sizeof(PrefsAccount));
		prefs_set_default(param);
		*ac_prefs = tmp_ac_prefs;
		ac_prefs->account_id = prefs_account_get_new_id();
		new_account = TRUE;
	}

	if (!dialog.window) {
		prefs_account_create();
	}

	manage_window_set_transient(GTK_WINDOW(dialog.window));
	gtk_notebook_set_page(GTK_NOTEBOOK(dialog.notebook), 0);
	gtk_widget_grab_focus(dialog.ok_btn);

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
		gtk_window_set_title(GTK_WINDOW(dialog.window),
				     _("Preferences for new account"));
		gtk_widget_hide(dialog.apply_btn);
	} else {
		prefs_set_dialog(param);
		gtk_window_set_title(GTK_WINDOW(dialog.window),
				     _("Account preferences"));
		gtk_widget_show(dialog.apply_btn);
	}

	if (ac_prefs->protocol != A_LOCAL) {
		gtk_widget_set_sensitive(basic.smtpserv_entry, TRUE);
		gtk_widget_set_sensitive(basic.smtpserv_label, TRUE);
	}

	gtk_widget_show(dialog.window);
	gtk_main();
	gtk_widget_hide(dialog.window);

	inc_unlock();

	if (cancelled && new_account) {
		g_free(ac_prefs);
		return NULL;
	} else {
		*ac_prefs = tmp_ac_prefs;
		return ac_prefs;
	}
}

static void prefs_account_create(void)
{
	gint page = 0;

	debug_print(_("Creating account preferences window...\n"));

	/* create dialog */
	prefs_dialog_create(&dialog);
	gtk_signal_connect(GTK_OBJECT(dialog.window), "delete_event",
			   GTK_SIGNAL_FUNC(prefs_account_deleted), NULL);
	gtk_signal_connect(GTK_OBJECT(dialog.window), "key_press_event",
			   GTK_SIGNAL_FUNC(prefs_account_key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(dialog.window);

	gtk_signal_connect(GTK_OBJECT(dialog.ok_btn), "clicked",
	 		   GTK_SIGNAL_FUNC(prefs_account_ok), NULL);
	gtk_signal_connect(GTK_OBJECT(dialog.apply_btn), "clicked",
			   GTK_SIGNAL_FUNC(prefs_account_apply), NULL);
	gtk_signal_connect(GTK_OBJECT(dialog.cancel_btn), "clicked",
			   GTK_SIGNAL_FUNC(prefs_account_cancel), NULL);

	prefs_account_basic_create();
	SET_NOTEBOOK_LABEL(dialog.notebook, _("Basic"), page++);
	prefs_account_receive_create();
	SET_NOTEBOOK_LABEL(dialog.notebook, _("Receive"), page++);
	prefs_account_send_create();
	SET_NOTEBOOK_LABEL(dialog.notebook, _("Send"), page++);
	prefs_account_compose_create();
	SET_NOTEBOOK_LABEL(dialog.notebook, _("Compose"), page++);
#if USE_GPGME
	prefs_account_privacy_create();
	SET_NOTEBOOK_LABEL(dialog.notebook, _("Privacy"), page++);
#endif /* USE_GPGME */
#if USE_SSL
	prefs_account_ssl_create();
	SET_NOTEBOOK_LABEL(dialog.notebook, _("SSL"), page++);
#endif /* USE_SSL */
	prefs_account_advanced_create();
	SET_NOTEBOOK_LABEL(dialog.notebook, _("Advanced"), page++);

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
	gtk_signal_connect(GTK_OBJECT(menuitem), "activate", \
			   GTK_SIGNAL_FUNC(prefs_account_protocol_activated), \
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
	GtkWidget *localmbox_entry;
	GtkWidget *mailcmd_chkbtn;
	GtkWidget *mailcmd_entry;
	GtkWidget *uid_label;
	GtkWidget *pass_label;
	GtkWidget *uid_entry;
	GtkWidget *pass_entry;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox, FALSE, FALSE, 0);

	label = gtk_label_new (_("Name of this account"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	acname_entry = gtk_entry_new ();
	gtk_widget_show (acname_entry);
	gtk_widget_set_usize (acname_entry, DEFAULT_ENTRY_WIDTH, -1);
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

	MENUITEM_ADD (optmenu_menu, menuitem, _("POP3 (normal)"),  A_POP3);
	SET_ACTIVATE (menuitem);
	MENUITEM_ADD (optmenu_menu, menuitem, _("POP3 (APOP auth)"),  A_APOP);
	SET_ACTIVATE (menuitem);
	MENUITEM_ADD (optmenu_menu, menuitem, _("IMAP4"), A_IMAP4);
	SET_ACTIVATE (menuitem);
	MENUITEM_ADD (optmenu_menu, menuitem, _("News (NNTP)"), A_NNTP);
	SET_ACTIVATE (menuitem);
	MENUITEM_ADD (optmenu_menu, menuitem, _("None (local)"), A_LOCAL);
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
	gtk_table_attach (GTK_TABLE (serv_table), nntpauth_chkbtn, 0, 4, 1, 2,
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
	gtk_widget_set_usize (uid_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_table_attach (GTK_TABLE (serv_table), uid_entry, 1, 2, 7, 8,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	pass_entry = gtk_entry_new ();
	gtk_widget_show (pass_entry);
	gtk_widget_set_usize (pass_entry, DEFAULT_ENTRY_WIDTH, -1);
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

	localmbox_label = gtk_label_new (_("Local mailbox file"));
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
	gtk_signal_connect(GTK_OBJECT(mailcmd_chkbtn), "toggled",
			   GTK_SIGNAL_FUNC(prefs_account_mailcmd_toggled),
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
	GtkWidget *rmmail_chkbtn;
	GtkWidget *hbox_spc;
	GtkWidget *leave_time_label;
	GtkWidget *leave_time_entry;
	GtkWidget *getall_chkbtn;
	GtkWidget *hbox1;
	GtkWidget *size_limit_chkbtn;
	GtkWidget *size_limit_entry;
	GtkWidget *label;
	GtkWidget *filter_on_recv_chkbtn;
	GtkWidget *sd_filter_on_recv_chkbtn;
	GtkWidget *sd_rmmail_chkbtn;
	GtkWidget *vbox3;
	GtkWidget *hbox2;
	GtkWidget *inbox_label;
	GtkWidget *inbox_entry;
	GtkWidget *inbox_btn;
	GtkWidget *recvatgetall_chkbtn;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	PACK_FRAME (vbox1, frame1, _("POP3"));

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (frame1), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);

	PACK_CHECK_BUTTON (vbox2, rmmail_chkbtn,
			   _("Remove messages on server when received"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

	hbox_spc = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (hbox1), hbox_spc, FALSE, FALSE, 0);
	gtk_widget_set_usize (hbox_spc, 12, -1);

	leave_time_label = gtk_label_new (_("Remove after"));
	gtk_widget_show (leave_time_label);
	gtk_box_pack_start (GTK_BOX (hbox1), leave_time_label, FALSE, FALSE, 0);

	leave_time_entry = gtk_entry_new ();
	gtk_widget_show (leave_time_entry);
	gtk_widget_set_usize (leave_time_entry, 64, -1);
	gtk_box_pack_start (GTK_BOX (hbox1), leave_time_entry, FALSE, FALSE, 0);

	leave_time_label = gtk_label_new (_("days"));
	gtk_widget_show (leave_time_label);
	gtk_box_pack_start (GTK_BOX (hbox1), leave_time_label, FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY (rmmail_chkbtn, hbox1);

	PACK_VSPACER(vbox2, vbox3, VSPACING_NARROW_2);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

	hbox_spc = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (hbox1), hbox_spc, FALSE, FALSE, 0);
	gtk_widget_set_usize (hbox_spc, 12, -1);

	leave_time_label = gtk_label_new (_("(0 days: remove immediately)"));
	gtk_widget_show (leave_time_label);
	gtk_box_pack_start (GTK_BOX (hbox1), leave_time_label, FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY (rmmail_chkbtn, hbox1);

	PACK_CHECK_BUTTON (vbox2, getall_chkbtn,
			   _("Download all messages on server"));
	PACK_CHECK_BUTTON (vbox2, sd_filter_on_recv_chkbtn,
			   _("Use filtering rules with Selective Download"));
	PACK_CHECK_BUTTON (vbox2, sd_rmmail_chkbtn,
			   _("Remove mail after downloading with Selective Download"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox1, size_limit_chkbtn, _("Receive size limit"));

	size_limit_entry = gtk_entry_new ();
	gtk_widget_show (size_limit_entry);
	gtk_widget_set_usize (size_limit_entry, 64, -1);
	gtk_box_pack_start (GTK_BOX (hbox1), size_limit_entry, FALSE, FALSE, 0);

	label = gtk_label_new ("KB");
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY (size_limit_chkbtn, size_limit_entry);

	PACK_CHECK_BUTTON (vbox2, filter_on_recv_chkbtn,
			   _("Filter messages on receiving"));

	PACK_VSPACER(vbox2, vbox3, VSPACING_NARROW_2);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

	inbox_label = gtk_label_new (_("Default inbox"));
	gtk_widget_show (inbox_label);
	gtk_box_pack_start (GTK_BOX (hbox1), inbox_label, FALSE, FALSE, 0);

	inbox_entry = gtk_entry_new ();
	gtk_widget_show (inbox_entry);
	gtk_widget_set_usize (inbox_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_box_pack_start (GTK_BOX (hbox1), inbox_entry, TRUE, TRUE, 0);

	inbox_btn = gtk_button_new_with_label (_(" Select... "));
	gtk_widget_show (inbox_btn);
	gtk_box_pack_start (GTK_BOX (hbox1), inbox_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (inbox_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_account_select_folder_cb),
			    inbox_entry);

	PACK_VSPACER(vbox2, vbox3, VSPACING_NARROW_2);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

	label = gtk_label_new
		(_("(Unfiltered messages will be stored in this folder)"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

	PACK_CHECK_BUTTON
		(vbox1, recvatgetall_chkbtn,
		 _("`Get all' checks for new messages on this account"));

	receive.pop3_frame               = frame1;
	receive.rmmail_chkbtn            = rmmail_chkbtn;
	receive.leave_time_entry         = leave_time_entry;
	receive.getall_chkbtn            = getall_chkbtn;
	receive.size_limit_chkbtn        = size_limit_chkbtn;
	receive.size_limit_entry         = size_limit_entry;
	receive.filter_on_recv_chkbtn    = filter_on_recv_chkbtn;
	receive.sd_filter_on_recv_chkbtn = sd_filter_on_recv_chkbtn;
	receive.sd_rmmail_chkbtn         = sd_rmmail_chkbtn;
	receive.inbox_label              = inbox_label;
	receive.inbox_entry              = inbox_entry;
	receive.inbox_btn                = inbox_btn;

	receive.recvatgetall_chkbtn      = recvatgetall_chkbtn;
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

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	PACK_FRAME (vbox1, frame, _("Header"));

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (frame), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);

	PACK_CHECK_BUTTON (vbox2, date_chkbtn, _("Add Date header field"));
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
	gtk_signal_connect (GTK_OBJECT (customhdr_edit_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_account_edit_custom_header),
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
	gtk_widget_set_usize (hbox_spc, 12, -1);

	label = gtk_label_new (_("Authentication method"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	optmenu = gtk_option_menu_new ();
	gtk_widget_show (optmenu);
	gtk_box_pack_start (GTK_BOX (hbox), optmenu, FALSE, FALSE, 0);

	optmenu_menu = gtk_menu_new ();

	MENUITEM_ADD (optmenu_menu, menuitem, _("Automatic"), 0);
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
	gtk_widget_set_usize (hbox_spc, 12, -1);

	label = gtk_label_new (_("User ID"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	smtp_uid_entry = gtk_entry_new ();
	gtk_widget_show (smtp_uid_entry);
	gtk_widget_set_usize (smtp_uid_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_box_pack_start (GTK_BOX (hbox), smtp_uid_entry, TRUE, TRUE, 0);

	label = gtk_label_new (_("Password"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	smtp_pass_entry = gtk_entry_new ();
	gtk_widget_show (smtp_pass_entry);
	gtk_widget_set_usize (smtp_pass_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_box_pack_start (GTK_BOX (hbox), smtp_pass_entry, TRUE, TRUE, 0);
	gtk_entry_set_visibility (GTK_ENTRY (smtp_pass_entry), FALSE);

	PACK_VSPACER(vbox4, vbox_spc, VSPACING_NARROW_2);

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox4), hbox, FALSE, FALSE, 0);

	hbox_spc = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (hbox), hbox_spc, FALSE, FALSE, 0);
	gtk_widget_set_usize (hbox_spc, 12, -1);

	label = gtk_label_new
		(_("If you leave these entries empty, the same\n"
		   "user ID and password as receiving will be used."));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);

	SET_TOGGLE_SENSITIVITY (smtp_auth_chkbtn, vbox4);

	PACK_CHECK_BUTTON (vbox3, pop_bfr_smtp_chkbtn,
		_("Authenticate with POP3 before sending"));
	gtk_widget_set_sensitive(pop_bfr_smtp_chkbtn, FALSE);

	send.date_chkbtn      = date_chkbtn;
	send.msgid_chkbtn     = msgid_chkbtn;
	send.customhdr_chkbtn = customhdr_chkbtn;

	send.smtp_auth_chkbtn       = smtp_auth_chkbtn;
	send.smtp_auth_type_optmenu = optmenu;
	send.smtp_uid_entry         = smtp_uid_entry;
	send.smtp_pass_entry        = smtp_pass_entry;
	send.pop_bfr_smtp_chkbtn    = pop_bfr_smtp_chkbtn;
}

static void prefs_account_compose_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *sigpath_entry;
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
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox, FALSE, FALSE, 0);

	label = gtk_label_new (_("Signature file"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	sigpath_entry = gtk_entry_new ();
	gtk_widget_show (sigpath_entry);
	gtk_box_pack_start (GTK_BOX (hbox), sigpath_entry, TRUE, TRUE, 0);

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

	compose.sigpath_entry = sigpath_entry;

	compose.autocc_chkbtn      = autocc_chkbtn;
	compose.autocc_entry       = autocc_entry;
	compose.autobcc_chkbtn     = autobcc_chkbtn;
	compose.autobcc_entry      = autobcc_entry;
	compose.autoreplyto_chkbtn = autoreplyto_chkbtn;
	compose.autoreplyto_entry  = autoreplyto_entry;
}

#if USE_GPGME
static void prefs_account_privacy_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *frame1;
	GtkWidget *vbox2;
	GtkWidget *frame2;
	GtkWidget *vbox3;
	GtkWidget *hbox1;
	GtkWidget *label;
	GtkWidget *checkbtn_default_encrypt;
	GtkWidget *checkbtn_ascii_armored;
	GtkWidget *checkbtn_default_sign;
	GtkWidget *defaultkey_radiobtn;
	GtkWidget *emailkey_radiobtn;
	GtkWidget *customkey_radiobtn;
	GtkWidget *customkey_entry;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	PACK_FRAME (vbox1, frame1, _("Default Actions"));

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (frame1), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);

	PACK_CHECK_BUTTON (vbox2, checkbtn_default_encrypt,
			   _("Encrypt message by default"));

	PACK_CHECK_BUTTON (vbox2, checkbtn_ascii_armored,
			   _("Plain ASCII-armored"));
	gtk_signal_connect(GTK_OBJECT(checkbtn_ascii_armored), "toggled",
				prefs_account_ascii_armored_warning, (gpointer)0);

	PACK_CHECK_BUTTON (vbox2, checkbtn_default_sign,
			   _("Sign message by default"));

	PACK_FRAME (vbox1, frame2, _("Sign key"));

	vbox3 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox3);
	gtk_container_add (GTK_CONTAINER (frame2), vbox3);
	gtk_container_set_border_width (GTK_CONTAINER (vbox3), 8);

	defaultkey_radiobtn = gtk_radio_button_new_with_label
		(NULL, _("Use default GnuPG key"));
	gtk_widget_show (defaultkey_radiobtn);
	gtk_box_pack_start (GTK_BOX (vbox3), defaultkey_radiobtn,
			    FALSE, FALSE, 0);
	gtk_object_set_user_data (GTK_OBJECT (defaultkey_radiobtn),
				  GINT_TO_POINTER (SIGN_KEY_DEFAULT));

	emailkey_radiobtn = gtk_radio_button_new_with_label_from_widget
		(GTK_RADIO_BUTTON (defaultkey_radiobtn),
		 _("Select key by your email address"));
	gtk_widget_show (emailkey_radiobtn);
	gtk_box_pack_start (GTK_BOX (vbox3), emailkey_radiobtn,
			    FALSE, FALSE, 0);
	gtk_object_set_user_data (GTK_OBJECT (emailkey_radiobtn),
				  GINT_TO_POINTER (SIGN_KEY_BY_FROM));

	customkey_radiobtn = gtk_radio_button_new_with_label_from_widget
		(GTK_RADIO_BUTTON (defaultkey_radiobtn),
		 _("Specify key manually"));
	gtk_widget_show (customkey_radiobtn);
	gtk_box_pack_start (GTK_BOX (vbox3), customkey_radiobtn,
			    FALSE, FALSE, 0);
	gtk_object_set_user_data (GTK_OBJECT (customkey_radiobtn),
				  GINT_TO_POINTER (SIGN_KEY_CUSTOM));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox3), hbox1, FALSE, FALSE, 0);

	label = gtk_label_new ("");
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);
	gtk_widget_set_usize (label, 16, -1);

	label = gtk_label_new (_("User or key ID:"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);

	customkey_entry = gtk_entry_new ();
	gtk_widget_show (customkey_entry);
	gtk_box_pack_start (GTK_BOX (hbox1), customkey_entry,
			    TRUE, TRUE, 0);

	SET_TOGGLE_SENSITIVITY (customkey_radiobtn, customkey_entry);

	privacy.checkbtn_default_encrypt = checkbtn_default_encrypt;
	privacy.checkbtn_ascii_armored   = checkbtn_ascii_armored;
	privacy.checkbtn_default_sign    = checkbtn_default_sign;
	privacy.defaultkey_radiobtn = defaultkey_radiobtn;
	privacy.emailkey_radiobtn = emailkey_radiobtn;
	privacy.customkey_radiobtn = customkey_radiobtn;
	privacy.customkey_entry = customkey_entry;
}

static void prefs_account_ascii_armored_warning(GtkWidget* widget,
					       gpointer unused)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))
		&& gtk_notebook_get_current_page(GTK_NOTEBOOK(dialog.notebook))) {
		alertpanel_message(_("Warning - Privacy/Plain ASCII-armored"),
			_("Its not recommend to use the old style plain ASCII-\n"
			"armored mode for encrypted messages. It doesn't comply\n"
			"with the RFC 3156 - MIME security with OpenPGP."));
	}
}
#endif /* USE_GPGME */

#if USE_SSL

#define CREATE_RADIO_BUTTON(box, btn, btn_p, label, data)		\
{									\
	btn = gtk_radio_button_new_with_label_from_widget		\
		(GTK_RADIO_BUTTON (btn_p), label);			\
	gtk_widget_show (btn);						\
	gtk_box_pack_start (GTK_BOX (box), btn, FALSE, FALSE, 0);	\
	gtk_object_set_user_data (GTK_OBJECT (btn),			\
				  GINT_TO_POINTER (data));		\
}

#define CREATE_RADIO_BUTTONS(box,					\
			     btn1, btn1_label, btn1_data,		\
			     btn2, btn2_label, btn2_data,		\
			     btn3, btn3_label, btn3_data)		\
{									\
	btn1 = gtk_radio_button_new_with_label(NULL, btn1_label);	\
	gtk_widget_show (btn1);						\
	gtk_box_pack_start (GTK_BOX (box), btn1, FALSE, FALSE, 0);	\
	gtk_object_set_user_data (GTK_OBJECT (btn1),			\
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

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
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
	gtk_object_set_user_data (GTK_OBJECT (nntp_nossl_radiobtn),
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
			     _("Don't use SSL"),
			     SSL_NONE,
			     smtp_ssltunnel_radiobtn,
			     _("Use SSL for SMTP connection"),
			     SSL_TUNNEL,
			     smtp_starttls_radiobtn,
			     _("Use STARTTLS command to start SSL session"),
			     SSL_STARTTLS);

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
}

#undef CREATE_RADIO_BUTTONS
#undef CREATE_RADIO_BUTTON
#endif /* USE_SSL */

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
		(gtk_object_get_data(GTK_OBJECT(menuitem), "color"));
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
	GtkWidget *checkbtn_tunnelcmd;
	GtkWidget *entry_tunnelcmd;
 	GtkWidget *checkbtn_crosspost;
 	GtkWidget *colormenu_crosspost;
 	GtkWidget *menu;
 	GtkWidget *menuitem;
 	GtkWidget *item;
 	gint i;
	GtkWidget *imap_frame;
	GtkWidget *imapdir_label;
	GtkWidget *imapdir_entry;
	GtkWidget *folder_frame;
	GtkWidget *vbox3;
	GtkWidget *table;
	GtkWidget *sent_folder_chkbtn;
	GtkWidget *sent_folder_entry;
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
	entry = gtk_entry_new_with_max_length (5); \
	gtk_widget_show (entry); \
	gtk_box_pack_start (GTK_BOX (box), entry, FALSE, FALSE, 0); \
	gtk_widget_set_usize (entry, 64, -1); \
	}

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
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
			   _("Tunnel command to open connection"));
	entry_tunnelcmd = gtk_entry_new ();
	gtk_widget_show (entry_tunnelcmd);
	gtk_box_pack_start (GTK_BOX (hbox1), entry_tunnelcmd, TRUE, TRUE, 0);
	SET_TOGGLE_SENSITIVITY (checkbtn_tunnelcmd, entry_tunnelcmd);

	PACK_HBOX (hbox1);
	PACK_CHECK_BUTTON (hbox1, checkbtn_crosspost, 
			   _("Mark cross-posted messages as read and color:"));
	gtk_signal_connect (GTK_OBJECT (checkbtn_crosspost), "toggled",
					GTK_SIGNAL_FUNC (crosspost_color_toggled),
					NULL);

	colormenu_crosspost = gtk_option_menu_new();
	gtk_widget_show (colormenu_crosspost);
	gtk_box_pack_start (GTK_BOX (hbox1), colormenu_crosspost, FALSE, FALSE, 0);

	menu = colorlabel_create_color_menu();
	gtk_option_menu_set_menu (GTK_OPTION_MENU(colormenu_crosspost), menu);
	SET_TOGGLE_SENSITIVITY(checkbtn_crosspost, colormenu_crosspost);

	PACK_FRAME (vbox1, imap_frame, _("IMAP4"));

	vbox3 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox3);
	gtk_container_add (GTK_CONTAINER (imap_frame), vbox3);
	gtk_container_set_border_width (GTK_CONTAINER (vbox3), 8);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox3), hbox1, FALSE, FALSE, 0);

	imapdir_label = gtk_label_new (_("IMAP server directory"));
	gtk_widget_show (imapdir_label);
	gtk_box_pack_start (GTK_BOX (hbox1), imapdir_label, FALSE, FALSE, 0);

	imapdir_entry = gtk_entry_new();
	gtk_widget_show (imapdir_entry);
	gtk_box_pack_start (GTK_BOX (hbox1), imapdir_entry, TRUE, TRUE, 0);

#undef PACK_HBOX
#undef PACK_PORT_ENTRY

	/* special folder setting (maybe these options are redundant) */

	PACK_FRAME (vbox1, folder_frame, _("Folder"));

	vbox3 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox3);
	gtk_container_add (GTK_CONTAINER (folder_frame), vbox3);
	gtk_container_set_border_width (GTK_CONTAINER (vbox3), 8);

	table = gtk_table_new (3, 3, FALSE);
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
	button = gtk_button_new_with_label (_(" ... "));		\
	gtk_widget_show (button);					\
	gtk_table_attach (GTK_TABLE (table), button,			\
			  2, 3, n, n + 1, GTK_FILL, 0, 0, 0);		\
	gtk_signal_connect						\
		(GTK_OBJECT (button), "clicked",			\
		 GTK_SIGNAL_FUNC (prefs_account_select_folder_cb),	\
		 entry);						\
									\
	SET_TOGGLE_SENSITIVITY (chkbtn, entry);				\
	SET_TOGGLE_SENSITIVITY (chkbtn, button);			\
}

	SET_CHECK_BTN_AND_ENTRY(_("Put sent messages in"),
				sent_folder_chkbtn, sent_folder_entry, 0);
	SET_CHECK_BTN_AND_ENTRY(_("Put draft messages in"),
				draft_folder_chkbtn, draft_folder_entry, 1);
	SET_CHECK_BTN_AND_ENTRY(_("Put deleted messages in"),
				trash_folder_chkbtn, trash_folder_entry, 2);

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
	advanced.tunnelcmd_chkbtn	= checkbtn_tunnelcmd;
	advanced.tunnelcmd_entry	= entry_tunnelcmd;
 	advanced.crosspost_chkbtn	= checkbtn_crosspost;
 	advanced.crosspost_colormenu	= colormenu_crosspost;

	advanced.imap_frame             = imap_frame;
	advanced.imapdir_entry          = imapdir_entry;

	advanced.sent_folder_chkbtn  = sent_folder_chkbtn;
	advanced.sent_folder_entry   = sent_folder_entry;
	advanced.draft_folder_chkbtn = draft_folder_chkbtn;
	advanced.draft_folder_entry  = draft_folder_entry;
	advanced.trash_folder_chkbtn = trash_folder_chkbtn;
	advanced.trash_folder_entry  = trash_folder_entry;
}

static gint prefs_account_deleted(GtkWidget *widget, GdkEventAny *event,
				  gpointer data)
{
	prefs_account_cancel();
	return TRUE;
}

static void prefs_account_key_pressed(GtkWidget *widget, GdkEventKey *event,
				      gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		prefs_account_cancel();
}

static void prefs_account_ok(void)
{
	if (prefs_account_apply() == 0)
		gtk_main_quit();
}

static gint prefs_account_apply(void)
{
	RecvProtocol protocol;
	GtkWidget *menu;
	GtkWidget *menuitem;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(basic.protocol_optmenu));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	protocol = GPOINTER_TO_INT
		(gtk_object_get_user_data(GTK_OBJECT(menuitem)));

	if (*gtk_entry_get_text(GTK_ENTRY(basic.acname_entry)) == '\0') {
		alertpanel_error(_("Account name is not entered."));
		return -1;
	}
	if (*gtk_entry_get_text(GTK_ENTRY(basic.addr_entry)) == '\0') {
		alertpanel_error(_("Mail address is not entered."));
		return -1;
	}
	if ((protocol == A_POP3 || protocol == A_APOP || (protocol == A_LOCAL && !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(basic.mailcmd_chkbtn)))) &&
           *gtk_entry_get_text(GTK_ENTRY(basic.smtpserv_entry)) == '\0') {
		alertpanel_error(_("SMTP server is not entered."));
		return -1;
	}
	if ((protocol == A_POP3 || protocol == A_APOP || protocol == A_IMAP4) &&
	    *gtk_entry_get_text(GTK_ENTRY(basic.uid_entry)) == '\0') {
		alertpanel_error(_("User ID is not entered."));
		return -1;
	}
	if ((protocol == A_POP3 || protocol == A_APOP) &&
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

	prefs_set_data_from_dialog(param);
	return 0;
}

static void prefs_account_cancel(void)
{
	cancelled = TRUE;
	gtk_main_quit();
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

static void prefs_account_edit_custom_header(void)
{
	prefs_custom_header_open(&tmp_ac_prefs);
}

#if USE_GPGME || USE_SSL
static void prefs_account_enum_set_data_from_radiobtn(PrefParam *pparam)
{
	GtkRadioButton *radiobtn;
	GSList *group;

	radiobtn = GTK_RADIO_BUTTON (*pparam->widget);
	group = gtk_radio_button_group (radiobtn);
	while (group != NULL) {
		GtkToggleButton *btn = GTK_TOGGLE_BUTTON (group->data);
		if (gtk_toggle_button_get_active (btn)) {
			*((gint *)pparam->data) = GPOINTER_TO_INT
				(gtk_object_get_user_data (GTK_OBJECT (btn)));
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
	group = gtk_radio_button_group (radiobtn);
	while (group != NULL) {
		GtkToggleButton *btn = GTK_TOGGLE_BUTTON (group->data);
		gpointer data1 = gtk_object_get_user_data (GTK_OBJECT (btn));
		if (data1 == data) {
			gtk_toggle_button_set_active (btn, TRUE);
			break;
		}
		group = group->next;
	}
}

#endif /* USE_GPGME */

static void prefs_account_protocol_set_data_from_optmenu(PrefParam *pparam)
{
	GtkWidget *menu;
	GtkWidget *menuitem;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(*pparam->widget));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	*((RecvProtocol *)pparam->data) = GPOINTER_TO_INT
		(gtk_object_get_user_data(GTK_OBJECT(menuitem)));
}

static void prefs_account_protocol_set_optmenu(PrefParam *pparam)
{
	RecvProtocol protocol;
	GtkOptionMenu *optmenu = GTK_OPTION_MENU(*pparam->widget);
	GtkWidget *menu;
	GtkWidget *menuitem;
	GList *children;
	gint list_order[] = {
		0,  /* A_POP3  */
		1,  /* A_APOP  */
		-1, /* A_RPOP  */
		2,  /* A_IMAP4 */
		3,  /* A_NNTP  */
		4   /* A_LOCAL */
	};

	protocol = *((RecvProtocol *)pparam->data);
	if (protocol < 0 || protocol > A_LOCAL) return;
	if (list_order[protocol] < 0) return;
	gtk_option_menu_set_history(optmenu, list_order[protocol]);

	menu = gtk_option_menu_get_menu(optmenu);
	menu_set_insensitive_all(GTK_MENU_SHELL(menu));

#define SET_NTH_SENSITIVE(proto) \
{ \
	menuitem = g_list_nth_data(children, list_order[proto]); \
	if (menuitem) \
		gtk_widget_set_sensitive(menuitem, TRUE); \
}

	children = GTK_MENU_SHELL(menu)->children;
	SET_NTH_SENSITIVE(protocol);
	if (protocol == A_POP3) {
		SET_NTH_SENSITIVE(A_APOP);
	} else if (protocol == A_APOP) {
		SET_NTH_SENSITIVE(A_POP3);
	}

#undef SET_NTH_SENSITIVE

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
		(gtk_object_get_user_data(GTK_OBJECT(menuitem)));
}

static void prefs_account_smtp_auth_type_set_optmenu(PrefParam *pparam)
{
	SMTPAuthType type = *((SMTPAuthType *)pparam->data);
	GtkOptionMenu *optmenu = GTK_OPTION_MENU(*pparam->widget);
	GtkWidget *menu;
	GtkWidget *menuitem;

	switch (type) {
	case SMTPAUTH_LOGIN:
		gtk_option_menu_set_history(optmenu, 1);
		break;
	case SMTPAUTH_CRAM_MD5:
		gtk_option_menu_set_history(optmenu, 2);
		break;
	case SMTPAUTH_DIGEST_MD5:
		gtk_option_menu_set_history(optmenu, 3);
		break;
	case 0:
	default:
		gtk_option_menu_set_history(optmenu, 0);
	}

	menu = gtk_option_menu_get_menu(optmenu);
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	gtk_menu_item_activate(GTK_MENU_ITEM(menuitem));
}

static void prefs_account_protocol_activated(GtkMenuItem *menuitem)
{
	RecvProtocol protocol;
	gboolean active;
	gint auth;

	protocol = GPOINTER_TO_INT
		(gtk_object_get_user_data(GTK_OBJECT(menuitem)));

	switch(protocol) {
	case A_NNTP:
		gtk_widget_show(basic.nntpserv_label);
		gtk_widget_show(basic.nntpserv_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   0, VSPACING_NARROW);
		gtk_widget_set_sensitive(basic.nntpauth_chkbtn, TRUE);
		gtk_widget_show(basic.nntpauth_chkbtn);
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
		gtk_widget_set_sensitive(receive.pop3_frame, FALSE);
		gtk_widget_set_sensitive(receive.recvatgetall_chkbtn, TRUE);

		if (!tmp_ac_prefs.account_name) {
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(receive.recvatgetall_chkbtn),
				 FALSE);
		}

#if USE_SSL
		gtk_widget_hide(ssl.pop_frame);
		gtk_widget_hide(ssl.imap_frame);
		gtk_widget_show(ssl.nntp_frame);
		gtk_widget_hide(ssl.send_frame);
#endif
		gtk_widget_hide(advanced.popport_hbox);
		gtk_widget_hide(advanced.imapport_hbox);
		gtk_widget_show(advanced.nntpport_hbox);
		gtk_widget_show(advanced.crosspost_chkbtn);
		gtk_widget_show(advanced.crosspost_colormenu);
		gtk_widget_hide(advanced.imap_frame);
		break;
	case A_LOCAL:
		gtk_widget_hide(basic.nntpserv_label);
		gtk_widget_hide(basic.nntpserv_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   0, 0);
		gtk_widget_set_sensitive(basic.nntpauth_chkbtn, FALSE);
		gtk_widget_hide(basic.nntpauth_chkbtn);
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
		gtk_widget_set_sensitive(receive.pop3_frame, FALSE);
		gtk_widget_set_sensitive(receive.recvatgetall_chkbtn, FALSE);
		prefs_account_mailcmd_toggled
			(GTK_TOGGLE_BUTTON(basic.mailcmd_chkbtn), NULL);

		if (!tmp_ac_prefs.account_name) {
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(receive.recvatgetall_chkbtn),
				 TRUE);
		}

#if USE_SSL
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
		gtk_widget_hide(advanced.imap_frame);
		break;
	case A_IMAP4:
		gtk_widget_hide(basic.nntpserv_label);
		gtk_widget_hide(basic.nntpserv_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   0, 0);
		gtk_widget_set_sensitive(basic.nntpauth_chkbtn, FALSE);
		gtk_widget_hide(basic.nntpauth_chkbtn);
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
		gtk_widget_set_sensitive(receive.pop3_frame, FALSE);
		gtk_widget_set_sensitive(receive.recvatgetall_chkbtn, TRUE);
		gtk_widget_set_sensitive(basic.smtpserv_entry, TRUE);
		gtk_widget_set_sensitive(basic.smtpserv_label, TRUE);

		if (!tmp_ac_prefs.account_name) {
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(receive.recvatgetall_chkbtn),
				 FALSE);
		}

#if USE_SSL
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
		gtk_widget_show(advanced.imap_frame);
		break;
	case A_POP3:
	default:
		gtk_widget_hide(basic.nntpserv_label);
		gtk_widget_hide(basic.nntpserv_entry);
  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table),
					   0, 0);
		gtk_widget_set_sensitive(basic.nntpauth_chkbtn, FALSE);
		gtk_widget_hide(basic.nntpauth_chkbtn);
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
		gtk_widget_set_sensitive(receive.recvatgetall_chkbtn, TRUE);
		gtk_widget_set_sensitive(basic.smtpserv_entry, TRUE);
		gtk_widget_set_sensitive(basic.smtpserv_label, TRUE);

		if (!tmp_ac_prefs.account_name) {
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(receive.recvatgetall_chkbtn),
				 TRUE);
		}

#if USE_SSL
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
		gtk_widget_hide(advanced.imap_frame);
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

static void prefs_account_smtp_userid_cb(GtkEditable *editable,
					 gpointer smtp_passwd)
{
	gchar *buf;
	gboolean use_smtp_userid;
	
	buf = gtk_editable_get_chars(editable, 0, -1);
	if(buf[0] == '\0') {
		use_smtp_userid = FALSE;
	} else {
		use_smtp_userid = TRUE;
	}
	gtk_widget_set_sensitive(GTK_WIDGET(smtp_passwd), use_smtp_userid);
	g_free(buf);
}
