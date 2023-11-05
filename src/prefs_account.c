/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2023 the Claws Mail team and Hiroyuki Yamamoto
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include "defs.h"

#ifdef G_OS_WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#endif

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
#include "prefs_receive.h"
#include "prefs_common.h"
#include "prefs_customheader.h"
#include "account.h"
#include "mainwindow.h"
#include "manage_window.h"
#include "folder.h"
#include "foldersel.h"
#include "inc.h"
#include "menu.h"
#include "gtkutils.h"
#include "utils.h"
#include "alertpanel.h"
#include "colorlabel.h"
#include "smtp.h"
#include "imap.h"
#include "pop.h"
#ifdef USE_OAUTH2
#include "oauth2.h"
#endif
#include "remotefolder.h"
#include "combobox.h"
#include "setup.h"
#include "quote_fmt.h"
#include "hooks.h"
#include "privacy.h"
#include "inputdialog.h"
#include "ssl_certificate.h"
#include "passwordstore.h"
#include "file-utils.h"
#ifdef USE_GNUTLS
#include <gnutls/gnutls.h>
#endif

static gboolean cancelled;
static gboolean new_account;

static PrefsAccount tmp_ac_prefs;

static GtkWidget *sigfile_radiobtn;
static GtkWidget *sigcmd_radiobtn;
static GtkWidget *entry_sigpath;
static GtkWidget *signature_browse_button;
static GtkWidget *signature_edit_button;

#ifdef USE_GNUTLS
static GtkWidget *entry_in_cert_file;
static GtkWidget *entry_out_cert_file;
static GtkWidget *in_ssl_cert_browse_button;
static GtkWidget *out_ssl_cert_browse_button;
#endif

struct AutocheckWidgets {
	GtkWidget *autochk_hour_spinbtn;
	GtkWidget *autochk_min_spinbtn;
	GtkWidget *autochk_sec_spinbtn;
};

static GSList *prefs_pages = NULL;

#ifdef USE_OAUTH2
static GTask *oauth2_listener_task;
static int oauth2_listener_cancel = 0;
static int oauth2_listener_closed = 0;
#endif

typedef struct BasicPage
{
    PrefsPage page;

    GtkWidget *vbox;

	GtkWidget *acname_entry;
	GtkWidget *default_checkbtn;

	GtkWidget *name_entry;
	GtkWidget *addr_entry;
	GtkWidget *org_entry;

	GtkWidget *serv_frame;
	GtkWidget *serv_table;
	gpointer *protocol_optmenu;
	GtkWidget *recvserv_label;
	GtkWidget *smtpserv_label;
	GtkWidget *nntpserv_label;
	GtkWidget *localmbox_label;
	GtkWidget *mailcmd_label;
	GtkWidget *recvserv_entry;
	GtkWidget *smtpserv_entry;
	GtkWidget *nntpserv_entry;
	GtkWidget *nntpauth_checkbtn;
	GtkWidget *nntpauth_onconnect_checkbtn;
	GtkWidget *localmbox_entry;
	GtkWidget *mailcmd_checkbtn;
	GtkWidget *mailcmd_entry;
	GtkWidget *uid_label;
	GtkWidget *pass_label;
	GtkWidget *uid_entry;
	GtkWidget *pass_entry;
	GtkWidget *showpwd_checkbtn;
	GtkWidget *auto_configure_btn;
	GtkWidget *auto_configure_cancel_btn;
	GtkWidget *auto_configure_lbl;
} BasicPage;

typedef struct ReceivePage
{
    PrefsPage page;

    GtkWidget *vbox;

	GtkWidget *pop3_frame;
	GtkWidget *pop_auth_checkbtn;
	GtkWidget *pop_auth_type_optmenu;
	GtkWidget *rmmail_checkbtn;
	GtkWidget *leave_time_spinbtn;
	GtkWidget *leave_hour_spinbtn;
	GtkWidget *size_limit_checkbtn;
	GtkWidget *size_limit_spinbtn;
	GtkWidget *inbox_label;
	GtkWidget *inbox_entry;
	GtkWidget *inbox_btn;

	GtkWidget *autochk_frame;

	GtkWidget *local_frame;
	GtkWidget *local_inbox_label;
	GtkWidget *local_inbox_entry;
	GtkWidget *local_inbox_btn;

	GtkWidget *filter_on_recv_checkbtn;
	GtkWidget *filterhook_on_recv_checkbtn;
	GtkWidget *recvatgetall_checkbtn;
	
	GtkWidget *imap_frame;
	GtkWidget *imap_auth_type_optmenu;
	GtkWidget *imapdir_label;
	GtkWidget *imapdir_entry;
	GtkWidget *subsonly_checkbtn;
	GtkWidget *low_bandwidth_checkbtn;
	GtkWidget *imap_batch_size_spinbtn;

	GtkWidget *frame_maxarticle;
	GtkWidget *maxarticle_label;
	GtkWidget *maxarticle_spinbtn;
	GtkAdjustment *maxarticle_spinbtn_adj;

	GtkWidget *autochk_checkbtn;
	GtkWidget *autochk_use_default_checkbtn;
	struct AutocheckWidgets *autochk_widgets;
} ReceivePage;

typedef struct SendPage
{
    PrefsPage page;

    GtkWidget *vbox;

	GtkWidget *msgid_checkbtn;
	GtkWidget *xmailer_checkbtn;
	GtkWidget *customhdr_checkbtn;
	GtkWidget *msgid_with_addr_checkbtn;
	GtkWidget *smtp_auth_checkbtn;
	GtkWidget *smtp_auth_type_optmenu;
	GtkWidget *smtp_uid_entry;
	GtkWidget *smtp_pass_entry;
	GtkWidget *pop_bfr_smtp_checkbtn;
	GtkWidget *pop_bfr_smtp_tm_spinbtn;
	GtkWidget *pop_auth_timeout_lbl;
	GtkWidget *pop_auth_minutes_lbl;
} SendPage;

#ifdef USE_OAUTH2
typedef struct Oauth2Page
{
	PrefsPage page;

	GtkWidget *vbox;
	GtkWidget *oauth2_sensitive;

	GtkWidget *oauth2_authorise_btn;
	GtkWidget *oauth2_deauthorise_btn;
	GtkWidget *oauth2_authcode_entry;
	GtkWidget *oauth2_auth_optmenu;	
 	GtkWidget *oauth2_link_button;
 	GtkWidget *oauth2_link_copy_button;
	gpointer *protocol_optmenu;
	GtkWidget *oauth2_client_id_entry;
	GtkWidget *oauth2_client_secret_entry;
} Oauth2Page;
#endif

typedef struct
{
	gchar *auth_uri;
	GtkWidget *entry;
} AuthCodeQueryButtonData;

typedef struct ComposePage
{
    PrefsPage page;

    GtkWidget *vbox;

	GtkWidget *sigfile_radiobtn;
	GtkWidget *entry_sigpath;
	GtkWidget *checkbtn_autosig;
	GtkWidget *entry_sigsep;
	GtkWidget *autocc_checkbtn;
	GtkWidget *autocc_entry;
	GtkWidget *autobcc_checkbtn;
	GtkWidget *autobcc_entry;
	GtkWidget *autoreplyto_checkbtn;
	GtkWidget *autoreplyto_entry;
#if USE_ENCHANT
	GtkWidget *checkbtn_enable_default_dictionary;
	GtkWidget *combo_default_dictionary;
	GtkWidget *checkbtn_enable_default_alt_dictionary;
	GtkWidget *combo_default_alt_dictionary;
#endif
} ComposePage;

typedef struct TemplatesPage
{
    PrefsPage page;

    GtkWidget *vbox;

	GtkWidget *checkbtn_compose_with_format;
	GtkWidget *compose_subject_format;
	GtkWidget *compose_body_format;
	GtkWidget *checkbtn_reply_with_format;
	GtkWidget *reply_quotemark;
	GtkWidget *reply_body_format;
	GtkWidget *checkbtn_forward_with_format;
	GtkWidget *forward_quotemark;
	GtkWidget *forward_body_format;
} TemplatesPage;

typedef struct PrivacyPage
{
    PrefsPage page;

    GtkWidget *vbox;

	GtkWidget *default_privacy_system;
	GtkWidget *default_encrypt_checkbtn;
	GtkWidget *default_encrypt_reply_checkbtn;
	GtkWidget *default_sign_checkbtn;
	GtkWidget *default_sign_reply_checkbtn;
	GtkWidget *save_clear_text_checkbtn;
	GtkWidget *encrypt_to_self_checkbtn;
} PrivacyPage;

typedef struct SSLPage
{
    PrefsPage page;

    GtkWidget *vbox;

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

	GtkWidget *entry_in_cert_file;
	GtkWidget *entry_in_cert_pass;
	GtkWidget *entry_out_cert_file;
	GtkWidget *entry_out_cert_pass;

	GtkWidget *ssl_certs_auto_accept_checkbtn;
	GtkWidget *use_nonblocking_ssl_checkbtn;
} SSLPage;

typedef struct ProxyPage
{
	PrefsPage page;

	GtkWidget *vbox;

	GtkWidget *proxy_checkbtn;
	GtkWidget *default_proxy_checkbtn;
	GtkWidget *socks4_radiobtn;
	GtkWidget *socks5_radiobtn;
	GtkWidget *proxy_host_entry;
	GtkWidget *proxy_port_spinbtn;
	GtkWidget *proxy_auth_checkbtn;
	GtkWidget *proxy_name_entry;
	GtkWidget *proxy_pass_entry;
	GtkWidget *proxy_send_checkbtn;
} ProxyPage;

typedef struct AdvancedPage
{
    PrefsPage page;

    GtkWidget *vbox;

	GtkWidget *smtpport_checkbtn;
	GtkWidget *smtpport_spinbtn;
	GtkWidget *popport_hbox;
	GtkWidget *popport_checkbtn;
	GtkWidget *popport_spinbtn;
	GtkWidget *imapport_hbox;
	GtkWidget *imapport_checkbtn;
	GtkWidget *imapport_spinbtn;
	GtkWidget *nntpport_hbox;
	GtkWidget *nntpport_checkbtn;
	GtkWidget *nntpport_spinbtn;
	GtkWidget *domain_checkbtn;
	GtkWidget *domain_entry;
	GtkWidget *crosspost_checkbtn;
 	GtkWidget *crosspost_colormenu;

#ifndef G_OS_WIN32
	GtkWidget *tunnelcmd_checkbtn;
	GtkWidget *tunnelcmd_entry;
#endif

	GtkWidget *sent_folder_checkbtn;
	GtkWidget *sent_folder_entry;
	GtkWidget *queue_folder_checkbtn;
	GtkWidget *queue_folder_entry;
	GtkWidget *draft_folder_checkbtn;
	GtkWidget *draft_folder_entry;
	GtkWidget *trash_folder_checkbtn;
	GtkWidget *trash_folder_entry;
} AdvancedPage;

static BasicPage basic_page;
static ReceivePage receive_page;
static SendPage send_page;
#ifdef USE_OAUTH2
static Oauth2Page oauth2_page;
#endif
static ComposePage compose_page;
static TemplatesPage templates_page;
static PrivacyPage privacy_page;
#ifdef USE_GNUTLS
static SSLPage ssl_page;
#endif
static ProxyPage proxy_page;
static AdvancedPage advanced_page;

struct BasicProtocol {
	GtkWidget *combobox;
	GtkWidget *label;
	GtkWidget *descrlabel;
	GtkWidget *no_imap_warn_icon;
	GtkWidget *no_imap_warn_label;
};

static char *protocol_names[] = {
	N_("POP"),
	N_("IMAP"),
	N_("News (NNTP)"),
	N_("Local mbox file"),
	N_("None (SMTP only)")
};

#ifdef USE_OAUTH2
struct Oauth2Listener {
	int success;
  	Oauth2Service service;
	OAUTH2Data *OAUTH2Data;
	gchar *trim_text;
};
#endif

static void prefs_account_protocol_set_data_from_optmenu(PrefParam *pparam);
static void prefs_account_protocol_set_optmenu		(PrefParam *pparam);
static void prefs_account_protocol_changed		(GtkComboBox *combobox, gpointer data);

static void prefs_account_set_string_from_combobox (PrefParam *pparam);
static void prefs_account_set_privacy_combobox_from_string (PrefParam *pparam);

static void prefs_account_imap_auth_type_set_data_from_optmenu	(PrefParam *pparam);
static void prefs_account_imap_auth_type_set_optmenu	(PrefParam *pparam);
static void prefs_account_smtp_auth_type_set_data_from_optmenu (PrefParam *pparam);
static void prefs_account_smtp_auth_type_set_optmenu	(PrefParam *pparam);
static void prefs_account_pop_auth_type_set_data_from_optmenu (PrefParam *pparam);
static void prefs_account_pop_auth_type_set_optmenu	(PrefParam *pparam);
#ifdef USE_OAUTH2
static void prefs_account_oauth2_provider_set_data_from_optmenu	(PrefParam *pparam);
static void prefs_account_oauth2_provider_set_optmenu	(PrefParam *pparam);
static void prefs_account_oauth2_copy_url                       (GtkButton *button, gpointer data);
static void prefs_account_oauth2_listener(GTask *task, gpointer source, gpointer task_data, GCancellable *cancellable);
static void prefs_account_oauth2_callback(GObject *source, GAsyncResult *res, gpointer user_data);
static int  prefs_account_oauth2_get_line(int sock, char *buf, int size);
static void prefs_account_oauth2_set_sensitivity(void);
static void prefs_account_oauth2_set_auth_sensitivity(void);
static void prefs_account_oauth2_obtain_tokens(GtkButton *button, gpointer data);
#endif
static void prefs_account_set_autochk_interval_from_widgets(PrefParam *pparam);
static void prefs_account_set_autochk_interval_to_widgets(PrefParam *pparam);

static void prefs_account_enum_set_data_from_radiobtn	(PrefParam *pparam);
static void prefs_account_enum_set_radiobtn		(PrefParam *pparam);

static void crosspost_color_toggled(void);
static void prefs_account_crosspost_set_data_from_colormenu(PrefParam *pparam);
static void prefs_account_crosspost_set_colormenu(PrefParam *pparam);

static void prefs_account_nntpauth_toggled(GtkToggleButton *button, gpointer user_data);
static void prefs_account_mailcmd_toggled(GtkToggleButton *button,  gpointer user_data);
static void prefs_account_showpwd_checkbtn_toggled(GtkToggleButton *button, gpointer user_data);
static void prefs_account_entry_changed_newline_check_cb(GtkWidget *entry, gpointer user_data);
static void prefs_account_filter_on_recv_toggled(GtkToggleButton *button, gpointer user_data);

#if USE_ENCHANT
static void prefs_account_compose_default_dictionary_set_string_from_optmenu (PrefParam *pparam);
static void prefs_account_compose_default_dictionary_set_optmenu_from_string (PrefParam *pparam);
#endif

static gchar *privacy_prefs;

static PrefParam basic_param[] = {
	{"account_name", NULL, &tmp_ac_prefs.account_name, P_STRING,
	 &basic_page.acname_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"is_default", "FALSE", &tmp_ac_prefs.is_default, P_BOOL,
	 &basic_page.default_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"name", NULL, &tmp_ac_prefs.name, P_STRING,
	 &basic_page.name_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"address", NULL, &tmp_ac_prefs.address, P_STRING,
	 &basic_page.addr_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"organization", NULL, &tmp_ac_prefs.organization, P_STRING,
	 &basic_page.org_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"protocol", NULL, &tmp_ac_prefs.protocol, P_ENUM,
	 (GtkWidget **)&basic_page.protocol_optmenu,
	 prefs_account_protocol_set_data_from_optmenu,
	 prefs_account_protocol_set_optmenu},

	{"receive_server", NULL, &tmp_ac_prefs.recv_server, P_STRING,
	 &basic_page.recvserv_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"smtp_server", NULL, &tmp_ac_prefs.smtp_server, P_STRING,
	 &basic_page.smtpserv_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"nntp_server", NULL, &tmp_ac_prefs.nntp_server, P_STRING,
	 &basic_page.nntpserv_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"local_mbox", "/var/mail", &tmp_ac_prefs.local_mbox, P_STRING,
	 &basic_page.localmbox_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"use_mail_command", "FALSE", &tmp_ac_prefs.use_mail_command, P_BOOL,
	 &basic_page.mailcmd_checkbtn, prefs_set_data_from_toggle, prefs_set_toggle},

	{"mail_command", DEFAULT_SENDMAIL_CMD, &tmp_ac_prefs.mail_command, P_STRING,
	 &basic_page.mailcmd_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"use_nntp_auth", "FALSE", &tmp_ac_prefs.use_nntp_auth, P_BOOL,
	 &basic_page.nntpauth_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	
	{"use_nntp_auth_onconnect", "FALSE", &tmp_ac_prefs.use_nntp_auth_onconnect, P_BOOL,
	 &basic_page.nntpauth_onconnect_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"user_id", NULL, &tmp_ac_prefs.userid, P_STRING,
	 &basic_page.uid_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"password", NULL, &tmp_ac_prefs.passwd, P_PASSWORD,
	 NULL, NULL, NULL},

	{"config_version", "-1", &tmp_ac_prefs.config_version, P_INT,
		NULL, NULL, NULL},

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

static PrefParam receive_param[] = {
	{"use_apop_auth", "FALSE", &tmp_ac_prefs.use_apop_auth, P_BOOL,
	 NULL, NULL}, /* deprecated */

	{"use_pop_auth", "FALSE", &tmp_ac_prefs.use_pop_auth, P_BOOL,
	 &receive_page.pop_auth_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"pop_auth_method", "0", &tmp_ac_prefs.pop_auth_type, P_ENUM,
	 &receive_page.pop_auth_type_optmenu,
	 prefs_account_pop_auth_type_set_data_from_optmenu,
	 prefs_account_pop_auth_type_set_optmenu},

	{"remove_mail", "TRUE", &tmp_ac_prefs.rmmail, P_BOOL,
	 &receive_page.rmmail_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

#ifndef GENERIC_UMPC
	{"message_leave_time", "7", &tmp_ac_prefs.msg_leave_time, P_INT,
	 &receive_page.leave_time_spinbtn,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},
#else
	{"message_leave_time", "30", &tmp_ac_prefs.msg_leave_time, P_INT,
	 &receive_page.leave_time_spinbtn,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},
#endif
	{"message_leave_hour", "0", &tmp_ac_prefs.msg_leave_hour, P_INT,
	 &receive_page.leave_hour_spinbtn,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},

	{"enable_size_limit", "FALSE", &tmp_ac_prefs.enable_size_limit, P_BOOL,
	 &receive_page.size_limit_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"size_limit", "1024", &tmp_ac_prefs.size_limit, P_INT,
	 &receive_page.size_limit_spinbtn,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},

	{"filter_on_receive", "TRUE", &tmp_ac_prefs.filter_on_recv, P_BOOL,
	 &receive_page.filter_on_recv_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"filterhook_on_receive", "TRUE", &tmp_ac_prefs.filterhook_on_recv, P_BOOL,
	 &receive_page.filterhook_on_recv_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"imap_auth_method", "0", &tmp_ac_prefs.imap_auth_type, P_ENUM,
	 &receive_page.imap_auth_type_optmenu,
	 prefs_account_imap_auth_type_set_data_from_optmenu,
	 prefs_account_imap_auth_type_set_optmenu},

	{"receive_at_get_all", "TRUE", &tmp_ac_prefs.recv_at_getall, P_BOOL,
	 &receive_page.recvatgetall_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"max_news_articles", "300", &tmp_ac_prefs.max_articles, P_INT,
	 &receive_page.maxarticle_spinbtn,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},

	{"inbox", "#mh/Mailbox/inbox", &tmp_ac_prefs.inbox, P_STRING,
	 &receive_page.inbox_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"local_inbox", "#mh/Mailbox/inbox", &tmp_ac_prefs.local_inbox, P_STRING,
	 &receive_page.local_inbox_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"imap_directory", NULL, &tmp_ac_prefs.imap_dir, P_STRING,
	 &receive_page.imapdir_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"imap_subsonly", "TRUE", &tmp_ac_prefs.imap_subsonly, P_BOOL,
	 &receive_page.subsonly_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"low_bandwidth", "FALSE", &tmp_ac_prefs.low_bandwidth, P_BOOL,
	 &receive_page.low_bandwidth_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"imap_batch_size", "500", &tmp_ac_prefs.imap_batch_size, P_INT,
	 &receive_page.imap_batch_size_spinbtn,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},

	{"autochk_use_default", "TRUE", &tmp_ac_prefs.autochk_use_default, P_BOOL,
		&receive_page.autochk_use_default_checkbtn,
		prefs_set_data_from_toggle, prefs_set_toggle},

	{"autochk_use_custom", "FALSE", &tmp_ac_prefs.autochk_use_custom, P_BOOL,
		&receive_page.autochk_checkbtn,
		prefs_set_data_from_toggle, prefs_set_toggle},

	/* Here we lie a bit, passing a pointer to our custom struct,
	 * disguised as a GtkWidget pointer, to get around the
	 * inflexibility of PrefParam system. */
	{"autochk_interval", "600", &tmp_ac_prefs.autochk_itv, P_INT,
		(GtkWidget **)&receive_page.autochk_widgets,
		prefs_account_set_autochk_interval_from_widgets,
		prefs_account_set_autochk_interval_to_widgets},

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

static PrefParam send_param[] = {
	{"generate_msgid", "TRUE", &tmp_ac_prefs.gen_msgid, P_BOOL,
	 &send_page.msgid_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"generate_xmailer", "TRUE", &tmp_ac_prefs.gen_xmailer, P_BOOL,
	 &send_page.xmailer_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"add_custom_header", "FALSE", &tmp_ac_prefs.add_customhdr, P_BOOL,
	 &send_page.customhdr_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"msgid_with_addr", "FALSE", &tmp_ac_prefs.msgid_with_addr, P_BOOL,
	 &send_page.msgid_with_addr_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	 {"use_smtp_auth", "FALSE", &tmp_ac_prefs.use_smtp_auth, P_BOOL,
	 &send_page.smtp_auth_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"smtp_auth_method", "0", &tmp_ac_prefs.smtp_auth_type, P_ENUM,
	 &send_page.smtp_auth_type_optmenu,
	 prefs_account_smtp_auth_type_set_data_from_optmenu,
	 prefs_account_smtp_auth_type_set_optmenu},

	{"smtp_user_id", NULL, &tmp_ac_prefs.smtp_userid, P_STRING,
	 &send_page.smtp_uid_entry, prefs_set_data_from_entry, prefs_set_entry},
	{"smtp_password", NULL, &tmp_ac_prefs.smtp_passwd, P_PASSWORD,
	 NULL, NULL, NULL},

	{"pop_before_smtp", "FALSE", &tmp_ac_prefs.pop_before_smtp, P_BOOL,
	 &send_page.pop_bfr_smtp_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"pop_before_smtp_timeout", "5", &tmp_ac_prefs.pop_before_smtp_timeout, P_INT,
	 &send_page.pop_bfr_smtp_tm_spinbtn,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

static PrefParam oauth2_param[] = {
#ifdef USE_OAUTH2
	{"oauth2_auth_provider", "0", &tmp_ac_prefs.oauth2_provider, P_ENUM,
	 &oauth2_page.oauth2_auth_optmenu,
	 prefs_account_oauth2_provider_set_data_from_optmenu,
	 prefs_account_oauth2_provider_set_optmenu},

	{"oauth2_date", 0, &tmp_ac_prefs.oauth2_date, P_INT,
	 NULL, NULL, NULL},

	{"oauth2_authcode", NULL, &tmp_ac_prefs.oauth2_authcode, P_PASSWORD,
	 NULL, NULL, NULL},

	{"oauth2_client_id", NULL, &tmp_ac_prefs.oauth2_client_id, P_STRING,
	 &oauth2_page.oauth2_client_id_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"oauth2_client_secret", NULL, &tmp_ac_prefs.oauth2_client_secret, P_STRING,
	 &oauth2_page.oauth2_client_secret_entry, prefs_set_data_from_entry, prefs_set_entry},
#else
	{"oauth2_auth_provider", "0", &tmp_ac_prefs.oauth2_provider, P_ENUM,
	 NULL, NULL, NULL},

	{"oauth2_date", 0, &tmp_ac_prefs.oauth2_date, P_INT,
	 NULL, NULL, NULL},

	{"oauth2_authcode", NULL, &tmp_ac_prefs.oauth2_authcode, P_PASSWORD,
	 NULL, NULL, NULL},

	{"oauth2_client_id", NULL, &tmp_ac_prefs.oauth2_client_id, P_STRING,
	 NULL, NULL, NULL},

	{"oauth2_client_secret", NULL, &tmp_ac_prefs.oauth2_client_secret, P_STRING,
	 NULL, NULL, NULL},

#endif

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

static PrefParam compose_param[] = {
	{"signature_type", "0", &tmp_ac_prefs.sig_type, P_ENUM,
	 &compose_page.sigfile_radiobtn,
	 prefs_account_enum_set_data_from_radiobtn,
	 prefs_account_enum_set_radiobtn},
	{"signature_path", "~" G_DIR_SEPARATOR_S DEFAULT_SIGNATURE,
	 &tmp_ac_prefs.sig_path, P_STRING, &compose_page.entry_sigpath,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"auto_signature", "TRUE", &tmp_ac_prefs.auto_sig, P_BOOL,
	 &compose_page.checkbtn_autosig,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	 
	{"signature_separator", "-- ", &tmp_ac_prefs.sig_sep, P_STRING,
	 &compose_page.entry_sigsep, 
	 prefs_set_data_from_entry, prefs_set_entry},

	{"set_autocc", "FALSE", &tmp_ac_prefs.set_autocc, P_BOOL,
	 &compose_page.autocc_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"auto_cc", NULL, &tmp_ac_prefs.auto_cc, P_STRING,
	 &compose_page.autocc_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"set_autobcc", "FALSE", &tmp_ac_prefs.set_autobcc, P_BOOL,
	 &compose_page.autobcc_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"auto_bcc", NULL, &tmp_ac_prefs.auto_bcc, P_STRING,
	 &compose_page.autobcc_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"set_autoreplyto", "FALSE", &tmp_ac_prefs.set_autoreplyto, P_BOOL,
	 &compose_page.autoreplyto_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"auto_replyto", NULL, &tmp_ac_prefs.auto_replyto, P_STRING,
	 &compose_page.autoreplyto_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

#if USE_ENCHANT
	{"enable_default_dictionary", "", &tmp_ac_prefs.enable_default_dictionary, P_BOOL,
	 &compose_page.checkbtn_enable_default_dictionary,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"default_dictionary", NULL, &tmp_ac_prefs.default_dictionary, P_STRING,
	 &compose_page.combo_default_dictionary,
	 prefs_account_compose_default_dictionary_set_string_from_optmenu,
	 prefs_account_compose_default_dictionary_set_optmenu_from_string},

	{"enable_default_alt_dictionary", "", &tmp_ac_prefs.enable_default_alt_dictionary, P_BOOL,
	 &compose_page.checkbtn_enable_default_alt_dictionary,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"default_alt_dictionary", NULL, &tmp_ac_prefs.default_alt_dictionary, P_STRING,
	 &compose_page.combo_default_alt_dictionary,
	 prefs_account_compose_default_dictionary_set_string_from_optmenu,
	 prefs_account_compose_default_dictionary_set_optmenu_from_string},
#else
	{"enable_default_dictionary", "", &tmp_ac_prefs.enable_default_dictionary, P_BOOL,
	 NULL, NULL, NULL},

	{"default_dictionary", NULL, &tmp_ac_prefs.default_dictionary, P_STRING,
	 NULL, NULL, NULL},

	{"enable_default_alt_dictionary", "", &tmp_ac_prefs.enable_default_alt_dictionary, P_BOOL,
	 NULL, NULL, NULL},

	{"default_alt_dictionary", NULL, &tmp_ac_prefs.default_alt_dictionary, P_STRING,
	 NULL, NULL, NULL},
#endif	 

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

static PrefParam templates_param[] = {
	{"compose_with_format", "FALSE", &tmp_ac_prefs.compose_with_format, P_BOOL,
	 &templates_page.checkbtn_compose_with_format,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"compose_subject_format", NULL, &tmp_ac_prefs.compose_subject_format, P_STRING,
	 &templates_page.compose_subject_format,
	 prefs_set_escaped_data_from_entry, prefs_set_entry_from_escaped},

	{"compose_body_format", NULL, &tmp_ac_prefs.compose_body_format, P_STRING,
	 &templates_page.compose_body_format,
	 prefs_set_escaped_data_from_text, prefs_set_text_from_escaped},

	{"reply_with_format", "FALSE", &tmp_ac_prefs.reply_with_format, P_BOOL,
	 &templates_page.checkbtn_reply_with_format,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"reply_quotemark", NULL, &tmp_ac_prefs.reply_quotemark, P_STRING,
	 &templates_page.reply_quotemark,
	 prefs_set_data_from_entry, prefs_set_entry_from_escaped},

	{"reply_body_format", NULL, &tmp_ac_prefs.reply_body_format, P_STRING,
	 &templates_page.reply_body_format,
	 prefs_set_escaped_data_from_text, prefs_set_text_from_escaped},

	{"forward_with_format", "FALSE", &tmp_ac_prefs.forward_with_format, P_BOOL,
	 &templates_page.checkbtn_forward_with_format,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"forward_quotemark", NULL, &tmp_ac_prefs.forward_quotemark, P_STRING,
	 &templates_page.forward_quotemark,
	 prefs_set_data_from_entry, prefs_set_entry_from_escaped},

	{"forward_body_format", NULL, &tmp_ac_prefs.forward_body_format, P_STRING,
	 &templates_page.forward_body_format,
	 prefs_set_escaped_data_from_text, prefs_set_text_from_escaped},

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

static PrefParam privacy_param[] = {
	{"default_privacy_system", "", &tmp_ac_prefs.default_privacy_system, P_STRING,
	 &privacy_page.default_privacy_system,
	 prefs_account_set_string_from_combobox,
	 prefs_account_set_privacy_combobox_from_string},

	{"default_encrypt", "FALSE", &tmp_ac_prefs.default_encrypt, P_BOOL,
	 &privacy_page.default_encrypt_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"default_encrypt_reply", "TRUE", &tmp_ac_prefs.default_encrypt_reply, P_BOOL,
	 &privacy_page.default_encrypt_reply_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"default_sign", "FALSE", &tmp_ac_prefs.default_sign, P_BOOL,
	 &privacy_page.default_sign_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},
#ifdef G_OS_UNIX
	{"default_sign_reply", "TRUE", &tmp_ac_prefs.default_sign_reply, P_BOOL,
	 &privacy_page.default_sign_reply_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},
#else
	/* Bug 2367: disturbing for Win32 users with no keypair */
	{"default_sign_reply", "FALSE", &tmp_ac_prefs.default_sign_reply, P_BOOL,
	 &privacy_page.default_sign_reply_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},
#endif
	{"save_clear_text", "FALSE", &tmp_ac_prefs.save_encrypted_as_clear_text, P_BOOL,
	 &privacy_page.save_clear_text_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"encrypt_to_self", "FALSE", &tmp_ac_prefs.encrypt_to_self, P_BOOL,
	 &privacy_page.encrypt_to_self_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"privacy_prefs", "", &privacy_prefs, P_STRING,
	 NULL, NULL, NULL},

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

static PrefParam ssl_param[] = {
#ifdef USE_GNUTLS
	{"ssl_pop", "0", &tmp_ac_prefs.ssl_pop, P_ENUM,
	 &ssl_page.pop_nossl_radiobtn,
	 prefs_account_enum_set_data_from_radiobtn,
	 prefs_account_enum_set_radiobtn},

	{"ssl_imap", "0", &tmp_ac_prefs.ssl_imap, P_ENUM,
	 &ssl_page.imap_nossl_radiobtn,
	 prefs_account_enum_set_data_from_radiobtn,
	 prefs_account_enum_set_radiobtn},

	{"ssl_nntp", "0", &tmp_ac_prefs.ssl_nntp, P_ENUM,
	 &ssl_page.nntp_nossl_radiobtn,
	 prefs_account_enum_set_data_from_radiobtn,
	 prefs_account_enum_set_radiobtn},

	{"ssl_smtp", "0", &tmp_ac_prefs.ssl_smtp, P_ENUM,
	 &ssl_page.smtp_nossl_radiobtn,
	 prefs_account_enum_set_data_from_radiobtn,
	 prefs_account_enum_set_radiobtn},

	{"ssl_certs_auto_accept", "0", &tmp_ac_prefs.ssl_certs_auto_accept, P_BOOL,
	 &ssl_page.ssl_certs_auto_accept_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"use_nonblocking_ssl", "1", &tmp_ac_prefs.use_nonblocking_ssl, P_BOOL,
	 &ssl_page.use_nonblocking_ssl_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"use_tls_sni", "1", &tmp_ac_prefs.use_tls_sni, P_BOOL,
	 NULL, NULL, NULL},

	{"in_ssl_client_cert_file", "", &tmp_ac_prefs.in_ssl_client_cert_file, P_STRING,
	 &ssl_page.entry_in_cert_file, prefs_set_data_from_entry, prefs_set_entry},

	{"in_ssl_client_cert_pass", "", &tmp_ac_prefs.in_ssl_client_cert_pass, P_PASSWORD,
	 NULL, NULL, NULL},

	{"out_ssl_client_cert_file", "", &tmp_ac_prefs.out_ssl_client_cert_file, P_STRING,
	 &ssl_page.entry_out_cert_file, prefs_set_data_from_entry, prefs_set_entry},

	{"out_ssl_client_cert_pass", "", &tmp_ac_prefs.out_ssl_client_cert_pass, P_PASSWORD,
	 NULL, NULL, NULL},
#else
	{"ssl_pop", "0", &tmp_ac_prefs.ssl_pop, P_ENUM,
	 NULL, NULL, NULL},

	{"ssl_imap", "0", &tmp_ac_prefs.ssl_imap, P_ENUM,
	 NULL, NULL, NULL},

	{"ssl_nntp", "0", &tmp_ac_prefs.ssl_nntp, P_ENUM,
	 NULL, NULL, NULL},

	{"ssl_smtp", "0", &tmp_ac_prefs.ssl_smtp, P_ENUM,
	 NULL, NULL, NULL},

	{"in_ssl_client_cert_file", "", &tmp_ac_prefs.in_ssl_client_cert_file, P_STRING,
	 NULL, NULL, NULL},

	{"in_ssl_client_cert_pass", "", &tmp_ac_prefs.in_ssl_client_cert_pass, P_PASSWORD,
	 NULL, NULL, NULL},

	{"out_ssl_client_cert_file", "", &tmp_ac_prefs.out_ssl_client_cert_file, P_STRING,
	 NULL, NULL, NULL},

	{"out_ssl_client_cert_pass", "", &tmp_ac_prefs.out_ssl_client_cert_pass, P_PASSWORD,
	 NULL, NULL, NULL},

	{"ssl_certs_auto_accept", "0", &tmp_ac_prefs.ssl_certs_auto_accept, P_BOOL,
	 NULL, NULL, NULL},

	{"use_nonblocking_ssl", "1", &tmp_ac_prefs.use_nonblocking_ssl, P_BOOL,
	 NULL, NULL, NULL},
#endif /* USE_GNUTLS */

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

static PrefParam proxy_param[] = {
	/* SOCKS proxy */
	{"use_proxy", "FALSE", &tmp_ac_prefs.use_proxy, P_BOOL,
	&proxy_page.proxy_checkbtn,
	prefs_set_data_from_toggle, prefs_set_toggle},

	{"use_default_proxy", "TRUE", &tmp_ac_prefs.use_default_proxy, P_BOOL,
	&proxy_page.default_proxy_checkbtn,
	prefs_set_data_from_toggle, prefs_set_toggle},

	{"use_proxy_for_send", "TRUE", &tmp_ac_prefs.use_proxy_for_send, P_BOOL,
	&proxy_page.proxy_send_checkbtn,
	prefs_set_data_from_toggle, prefs_set_toggle},

	{"proxy_type", "1", &tmp_ac_prefs.proxy_info.proxy_type, P_ENUM,
	&proxy_page.socks4_radiobtn,
	prefs_account_enum_set_data_from_radiobtn,
	prefs_account_enum_set_radiobtn},

	{"proxy_host", "localhost", &tmp_ac_prefs.proxy_info.proxy_host, P_STRING,
	&proxy_page.proxy_host_entry,
	prefs_set_data_from_entry, prefs_set_entry},

	{"proxy_port", "1080", &tmp_ac_prefs.proxy_info.proxy_port, P_USHORT,
	&proxy_page.proxy_port_spinbtn,
	prefs_set_data_from_spinbtn, prefs_set_spinbtn},

	{"use_proxy_auth", "FALSE", &tmp_ac_prefs.proxy_info.use_proxy_auth, P_BOOL,
	&proxy_page.proxy_auth_checkbtn,
	prefs_set_data_from_toggle, prefs_set_toggle},

	{"proxy_name", "", &tmp_ac_prefs.proxy_info.proxy_name, P_STRING,
	&proxy_page.proxy_name_entry,
	prefs_set_data_from_entry, prefs_set_entry},

	{"proxy_pass", NULL, &tmp_ac_prefs.proxy_info.proxy_pass, P_PASSWORD,
	NULL, NULL, NULL},


	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

static PrefParam advanced_param[] = {
	{"set_smtpport", "FALSE", &tmp_ac_prefs.set_smtpport, P_BOOL,
	 &advanced_page.smtpport_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"smtp_port", "25", &tmp_ac_prefs.smtpport, P_USHORT,
	 &advanced_page.smtpport_spinbtn,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},

	{"set_popport", "FALSE", &tmp_ac_prefs.set_popport, P_BOOL,
	 &advanced_page.popport_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"pop_port", "110", &tmp_ac_prefs.popport, P_USHORT,
	 &advanced_page.popport_spinbtn,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},

	{"set_imapport", "FALSE", &tmp_ac_prefs.set_imapport, P_BOOL,
	 &advanced_page.imapport_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"imap_port", "143", &tmp_ac_prefs.imapport, P_USHORT,
	 &advanced_page.imapport_spinbtn,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},

	{"set_nntpport", "FALSE", &tmp_ac_prefs.set_nntpport, P_BOOL,
	 &advanced_page.nntpport_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"nntp_port", "119", &tmp_ac_prefs.nntpport, P_USHORT,
	 &advanced_page.nntpport_spinbtn,
	 prefs_set_data_from_spinbtn, prefs_set_spinbtn},

	{"set_domain", "FALSE", &tmp_ac_prefs.set_domain, P_BOOL,
	 &advanced_page.domain_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"domain", NULL, &tmp_ac_prefs.domain, P_STRING,
	 &advanced_page.domain_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

#ifdef USE_GNUTLS
	{"gnutls_set_priority", "FALSE", &tmp_ac_prefs.set_gnutls_priority, P_BOOL,
	 NULL, NULL, NULL},

	{"gnutls_priority", NULL, &tmp_ac_prefs.gnutls_priority, P_STRING,
	 NULL, NULL, NULL},
#endif

#ifndef G_OS_WIN32
	{"set_tunnelcmd", "FALSE", &tmp_ac_prefs.set_tunnelcmd, P_BOOL,
	 &advanced_page.tunnelcmd_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"tunnelcmd", NULL, &tmp_ac_prefs.tunnelcmd, P_STRING,
	 &advanced_page.tunnelcmd_entry,
	 prefs_set_data_from_entry, prefs_set_entry},
#endif
	{"mark_crosspost_read", "FALSE", &tmp_ac_prefs.mark_crosspost_read, P_BOOL,
	 &advanced_page.crosspost_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"crosspost_color", NULL, &tmp_ac_prefs.crosspost_col, P_ENUM,
	 &advanced_page.crosspost_colormenu,
	 prefs_account_crosspost_set_data_from_colormenu,
	 prefs_account_crosspost_set_colormenu},

	{"set_sent_folder", "FALSE", &tmp_ac_prefs.set_sent_folder, P_BOOL,
	 &advanced_page.sent_folder_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"sent_folder", NULL, &tmp_ac_prefs.sent_folder, P_STRING,
	 &advanced_page.sent_folder_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"set_queue_folder", "FALSE", &tmp_ac_prefs.set_queue_folder, P_BOOL,
	 &advanced_page.queue_folder_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"queue_folder", NULL, &tmp_ac_prefs.queue_folder, P_STRING,
	 &advanced_page.queue_folder_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"set_draft_folder", "FALSE", &tmp_ac_prefs.set_draft_folder, P_BOOL,
	 &advanced_page.draft_folder_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"draft_folder", NULL, &tmp_ac_prefs.draft_folder, P_STRING,
	 &advanced_page.draft_folder_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"set_trash_folder", "FALSE", &tmp_ac_prefs.set_trash_folder, P_BOOL,
	 &advanced_page.trash_folder_checkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},
	{"trash_folder", NULL, &tmp_ac_prefs.trash_folder, P_STRING,
	 &advanced_page.trash_folder_entry,
	 prefs_set_data_from_entry, prefs_set_entry},
	 

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

static gint prefs_account_get_new_id		(void);

static void prefs_account_select_folder_cb	(GtkWidget	*widget, gpointer	 data);

static void prefs_account_sigfile_radiobtn_cb	(GtkWidget	*widget, gpointer	 data);

static void prefs_account_sigcmd_radiobtn_cb	(GtkWidget	*widget, gpointer	 data);

static void prefs_account_signature_browse_cb	(GtkWidget	*widget, gpointer	 data);
#ifdef USE_GNUTLS
static void prefs_account_in_cert_browse_cb	(GtkWidget	*widget, gpointer	 data);

static void prefs_account_out_cert_browse_cb	(GtkWidget	*widget, gpointer	 data);
#endif
static void prefs_account_signature_edit_cb	(GtkWidget	*widget, gpointer	 data);

static void pop_bfr_smtp_tm_set_sens		(GtkWidget	*widget, gpointer	 data);

#if (defined USE_GNUTLS)
static void auto_configure_cb			(GtkWidget	*widget, gpointer	 data);

#endif
static void prefs_account_edit_custom_header	(void);

static void prefs_account_receive_itv_spinbutton_value_changed_cb(GtkWidget *w, gpointer data);

#define COMBOBOX_PRIVACY_PLUGIN_ID 3

/* Enable/disable necessary preference widgets based on current privacy
 * system choice. */
static void privacy_system_activated(GtkWidget *combobox)
{
	gtk_widget_set_sensitive (privacy_page.save_clear_text_checkbtn, 
		!gtk_toggle_button_get_active(
				GTK_TOGGLE_BUTTON(privacy_page.encrypt_to_self_checkbtn)));
}

/* Populate the privacy system choice combobox with valid choices */
static void update_privacy_system_menu() {
	GtkListStore *menu;
	GtkTreeIter iter;
	GSList *system_ids, *cur;

	menu = GTK_LIST_STORE(gtk_combo_box_get_model(
			GTK_COMBO_BOX(privacy_page.default_privacy_system)));

	/* First add "None", as that one is always available. :) */
	gtk_list_store_append(menu, &iter);
	gtk_list_store_set(menu, &iter,
			COMBOBOX_TEXT, _("None"),
			COMBOBOX_DATA, 0,
			COMBOBOX_SENS, TRUE,
			COMBOBOX_PRIVACY_PLUGIN_ID, "",
			-1);

	/* Now go through list of available privacy systems and add an entry
	 * for each. */
	system_ids = privacy_get_system_ids();
	for (cur = system_ids; cur != NULL; cur = g_slist_next(cur)) {
		gchar *id = (gchar *) cur->data;
		const gchar *name;
		
		name = privacy_system_get_name(id);
		gtk_list_store_append(menu, &iter);
		gtk_list_store_set(menu, &iter,
				COMBOBOX_TEXT, name,
				COMBOBOX_DATA, 1,
				COMBOBOX_SENS, TRUE,
				COMBOBOX_PRIVACY_PLUGIN_ID, id,
				-1);
		g_free(id);
	}
	g_slist_free(system_ids);

}

#define TABLE_YPAD 2

static void basic_create_widget_func(PrefsPage * _page,
                                           GtkWindow * window,
                                           gpointer data)
{
	BasicPage *page = (BasicPage *) _page;
	PrefsAccount *ac_prefs = (PrefsAccount *) data;

	GtkWidget *vbox1;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *acname_entry;
	GtkWidget *default_checkbtn;
	GtkWidget *frame1;
	GtkWidget *table1;
	GtkWidget *name_entry;
	GtkWidget *addr_entry;
	GtkWidget *org_entry;

	GtkWidget *serv_frame;
	GtkWidget *vbox2;
	GtkWidget *optmenubox;
	GtkWidget *optmenu;
	GtkWidget *optlabel;
	GtkWidget *no_imap_warn_icon;
	GtkWidget *no_imap_warn_label;
	GtkWidget *serv_table;
	GtkWidget *recvserv_label;
	GtkWidget *smtpserv_label;
	GtkWidget *nntpserv_label;
	GtkWidget *localmbox_label;
	GtkWidget *mailcmd_label;
	GtkWidget *recvserv_entry;
	GtkWidget *smtpserv_entry;
	GtkWidget *nntpserv_entry;
	GtkWidget *nntpauth_checkbtn;
	GtkWidget *nntpauth_onconnect_checkbtn;
	GtkWidget *localmbox_entry;
	GtkWidget *mailcmd_checkbtn;
	GtkWidget *mailcmd_entry;
	GtkWidget *uid_label;
	GtkWidget *pass_label;
	GtkWidget *uid_entry;
	GtkWidget *pass_entry;
	GtkWidget *auto_configure_btn;
	GtkWidget *auto_configure_cancel_btn;
	GtkWidget *auto_configure_lbl;
	GtkWidget *showpwd_checkbtn;
	GtkListStore *menu;
	GtkTreeIter iter;
	gchar *buf;

	struct BasicProtocol *protocol_optmenu;
	gint i;

	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox, FALSE, FALSE, 0);

	label = gtk_label_new (_("Name of account"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	acname_entry = gtk_entry_new ();
	gtk_widget_show (acname_entry);
	gtk_widget_set_size_request (acname_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_box_pack_start (GTK_BOX (hbox), acname_entry, TRUE, TRUE, 0);

	default_checkbtn = gtk_check_button_new_with_label (_("Set as default"));
	gtk_widget_show (default_checkbtn);
#ifndef GENERIC_UMPC
	gtk_box_pack_end (GTK_BOX (hbox), default_checkbtn, TRUE, FALSE, 0);
#else
	gtk_box_pack_start (GTK_BOX (vbox1), default_checkbtn, FALSE, FALSE, 0);
	
#endif
	PACK_FRAME (vbox1, frame1, _("Personal information"));

	table1 = gtk_grid_new();
	gtk_widget_show (table1);
	gtk_container_add (GTK_CONTAINER (frame1), table1);
	gtk_container_set_border_width (GTK_CONTAINER (table1), 8);
	gtk_grid_set_row_spacing(GTK_GRID(table1), VSPACING_NARROW);
	gtk_grid_set_column_spacing(GTK_GRID(table1), 8);

	label = gtk_label_new (_("Full name"));
	gtk_widget_show (label);
	gtk_label_set_xalign(GTK_LABEL (label), 1.0);
	gtk_grid_attach(GTK_GRID(table1), label, 0, 0, 1, 1);

	label = gtk_label_new (_("Mail address"));
	gtk_widget_show (label);
	gtk_label_set_xalign(GTK_LABEL (label), 1.0);
	gtk_grid_attach(GTK_GRID(table1), label, 0, 1, 1, 1);

	label = gtk_label_new (_("Organization"));
	gtk_widget_show (label);
	gtk_label_set_xalign(GTK_LABEL (label), 1.0);
	gtk_grid_attach(GTK_GRID(table1), label, 0, 2, 1, 1);

	name_entry = gtk_entry_new ();
	gtk_widget_show (name_entry);
	gtk_grid_attach(GTK_GRID(table1), name_entry, 1, 0, 1, 1);
	gtk_widget_set_hexpand(name_entry, TRUE);
	gtk_widget_set_halign(name_entry, GTK_ALIGN_FILL);

	addr_entry = gtk_entry_new ();
	gtk_widget_show (addr_entry);
	gtk_grid_attach(GTK_GRID(table1), addr_entry, 1, 1, 1, 1);
	gtk_widget_set_hexpand(addr_entry, TRUE);
	gtk_widget_set_halign(addr_entry, GTK_ALIGN_FILL);

	org_entry = gtk_entry_new ();
	gtk_widget_show (org_entry);
	gtk_grid_attach(GTK_GRID(table1), org_entry, 1, 2, 1, 1);
	gtk_widget_set_hexpand(org_entry, TRUE);
	gtk_widget_set_halign(org_entry, GTK_ALIGN_FILL);

	vbox2 = gtkut_get_options_frame(vbox1, &serv_frame, _("Server information"));

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);

	label = gtk_label_new (_("Protocol"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	/* Create GtkHBox for protocol combobox and label */
	optmenubox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
	gtk_widget_show(optmenubox);
	gtk_box_pack_start (GTK_BOX (hbox), optmenubox, FALSE, FALSE, 0);

	/* Create and populate the combobox */
	optmenu = gtkut_sc_combobox_create(NULL, FALSE);
	gtk_box_pack_start(GTK_BOX (optmenubox), optmenu, FALSE, FALSE, 0);

	menu = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(optmenu)));
	for( i = 0; i < NUM_RECV_PROTOCOLS; i++ )
		if( protocol_names[i] != NULL )
			COMBOBOX_ADD (menu, _(protocol_names[i]), i);

	g_signal_connect(G_OBJECT(optmenu), "changed",
			G_CALLBACK(prefs_account_protocol_changed), NULL);

	/* Create protocol label, empty for now */
	optlabel = gtk_label_new("");
	gtk_label_set_use_markup(GTK_LABEL(optlabel), TRUE);
	gtk_label_set_justify(GTK_LABEL(optlabel), GTK_JUSTIFY_CENTER);
	gtk_box_pack_start(GTK_BOX (optmenubox), optlabel, FALSE, FALSE, 0);

	auto_configure_btn = gtk_button_new_with_label(_("Auto-configure"));
	gtk_box_pack_start(GTK_BOX (optmenubox), auto_configure_btn, FALSE, FALSE, 0);
	auto_configure_cancel_btn = gtk_button_new_with_label(_("Cancel"));
	gtk_box_pack_start(GTK_BOX (optmenubox), auto_configure_cancel_btn, FALSE, FALSE, 0);
	auto_configure_lbl = gtk_label_new("");
	gtk_label_set_justify(GTK_LABEL(auto_configure_lbl), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start(GTK_BOX (optmenubox), auto_configure_lbl, FALSE, FALSE, 0);
#if (defined USE_GNUTLS)
	gtk_widget_show(auto_configure_btn);
	gtk_widget_show(auto_configure_lbl);
	g_signal_connect (G_OBJECT (auto_configure_btn), "clicked",
			  G_CALLBACK (auto_configure_cb), NULL);
	g_signal_connect (G_OBJECT (auto_configure_cancel_btn), "clicked",
			  G_CALLBACK (auto_configure_cb), NULL);
#endif

	no_imap_warn_icon = gtk_image_new_from_icon_name
                        ("dialog-warning", GTK_ICON_SIZE_SMALL_TOOLBAR);
	buf = g_strconcat("<span weight=\"bold\">",
			_("Warning: this version of Claws Mail\n"
			  "has been built without IMAP and News support."), "</span>", NULL);
	no_imap_warn_label = gtk_label_new(buf);
	g_free(buf);
	gtk_label_set_use_markup(GTK_LABEL(no_imap_warn_label), TRUE);

	gtk_box_pack_start(GTK_BOX (optmenubox), no_imap_warn_icon, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX (optmenubox), no_imap_warn_label, FALSE, FALSE, 0);
	/* Set up a struct to store pointers to necessary widgets */
	protocol_optmenu = g_new(struct BasicProtocol, 1);
	protocol_optmenu->combobox = optmenu;
	protocol_optmenu->label = optlabel;
	protocol_optmenu->descrlabel = label;
	protocol_optmenu->no_imap_warn_icon = no_imap_warn_icon;
	protocol_optmenu->no_imap_warn_label = no_imap_warn_label;

	serv_table = gtk_grid_new ();
	gtk_widget_show (serv_table);
	gtk_grid_set_row_spacing(GTK_GRID(serv_table), VSPACING_NARROW);
	gtk_grid_set_column_spacing(GTK_GRID(serv_table), 8);
	gtk_box_pack_start (GTK_BOX (vbox2), serv_table, FALSE, FALSE, 0);

	nntpserv_entry = gtk_entry_new ();
	gtk_widget_show (nntpserv_entry);
	gtk_grid_attach(GTK_GRID(serv_table), nntpserv_entry, 1, 0, 2, 1);
	gtk_widget_set_hexpand(nntpserv_entry, TRUE);
	gtk_widget_set_halign(nntpserv_entry, GTK_ALIGN_FILL);

	nntpauth_checkbtn = gtk_check_button_new_with_label
		(_("This server requires authentication"));
	gtk_widget_show (nntpauth_checkbtn);
	
	gtk_grid_attach(GTK_GRID(serv_table), nntpauth_checkbtn, 0, 6, 2, 1);

	nntpauth_onconnect_checkbtn = gtk_check_button_new_with_label
		(_("Authenticate on connect"));
	gtk_widget_show (nntpauth_onconnect_checkbtn);

	gtk_grid_attach(GTK_GRID(serv_table), nntpauth_onconnect_checkbtn, 2, 6, 1, 1);

	recvserv_entry = gtk_entry_new ();
	gtk_widget_show (recvserv_entry);
	gtk_grid_attach(GTK_GRID(serv_table), recvserv_entry, 1, 2, 2, 1);
	gtk_widget_set_hexpand(recvserv_entry, TRUE);
	gtk_widget_set_halign(recvserv_entry, GTK_ALIGN_FILL);

	localmbox_entry = gtk_entry_new ();
	gtk_widget_show (localmbox_entry);
	gtk_grid_attach(GTK_GRID(serv_table), localmbox_entry, 1, 3, 2, 1);
	gtk_widget_set_hexpand(localmbox_entry, TRUE);
	gtk_widget_set_halign(localmbox_entry, GTK_ALIGN_FILL);

	smtpserv_entry = gtk_entry_new ();
	gtk_widget_show (smtpserv_entry);
	gtk_grid_attach(GTK_GRID(serv_table), smtpserv_entry, 1, 4, 2, 1);
	gtk_widget_set_hexpand(smtpserv_entry, TRUE);
	gtk_widget_set_halign(smtpserv_entry, GTK_ALIGN_FILL);

	mailcmd_entry = gtk_entry_new ();
	gtk_widget_show (mailcmd_entry);
	gtk_grid_attach(GTK_GRID(serv_table), mailcmd_entry, 1, 6, 2, 1);
	gtk_widget_set_hexpand(mailcmd_entry, TRUE);
	gtk_widget_set_halign(mailcmd_entry, GTK_ALIGN_FILL);

	uid_entry = gtk_entry_new ();
	gtk_widget_show (uid_entry);
	gtk_widget_set_size_request (uid_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_grid_attach(GTK_GRID(serv_table), uid_entry, 1, 7, 1, 1);
	gtk_widget_set_hexpand(uid_entry, TRUE);
	gtk_widget_set_halign(uid_entry, GTK_ALIGN_FILL);
	g_signal_connect(G_OBJECT(uid_entry), "changed",
			G_CALLBACK(prefs_account_entry_changed_newline_check_cb),
			GINT_TO_POINTER(ac_prefs->protocol));

	pass_entry = gtk_entry_new ();
	gtk_widget_show (pass_entry);
	gtk_widget_set_size_request (pass_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_grid_attach(GTK_GRID(serv_table), pass_entry, 1, 8, 1, 1);
	gtk_widget_set_hexpand(pass_entry, TRUE);
	gtk_widget_set_halign(pass_entry, GTK_ALIGN_FILL);
	gtk_entry_set_visibility (GTK_ENTRY (pass_entry), FALSE);
	g_signal_connect(G_OBJECT(pass_entry), "changed",
			G_CALLBACK(prefs_account_entry_changed_newline_check_cb),
			GINT_TO_POINTER(ac_prefs->protocol));

	nntpserv_label = gtk_label_new (_("News server"));
	gtk_widget_show (nntpserv_label);
	gtk_label_set_xalign(GTK_LABEL (nntpserv_label), 1.0);
	gtk_grid_attach(GTK_GRID(serv_table), nntpserv_label, 0, 0, 1, 1);

	recvserv_label = gtk_label_new (_("Server for receiving"));
	gtk_widget_show (recvserv_label);
	gtk_label_set_xalign(GTK_LABEL (recvserv_label), 1.0);
	gtk_grid_attach(GTK_GRID(serv_table), recvserv_label, 0, 2, 1, 1);

	localmbox_label = gtk_label_new (_("Local mailbox"));
	gtk_widget_show (localmbox_label);
	gtk_label_set_xalign(GTK_LABEL (localmbox_label), 1.0);
	gtk_grid_attach(GTK_GRID(serv_table), localmbox_label, 0, 3, 1, 1);

	smtpserv_label = gtk_label_new (_("SMTP server (send)"));
	gtk_widget_show (smtpserv_label);
	gtk_label_set_xalign(GTK_LABEL (smtpserv_label), 1.0);
	gtk_grid_attach(GTK_GRID(serv_table), smtpserv_label, 0, 4, 1, 1);

	mailcmd_checkbtn = gtk_check_button_new_with_label
		(_("Use mail command rather than SMTP server"));
	gtk_widget_show (mailcmd_checkbtn);
	gtk_grid_attach(GTK_GRID(serv_table), mailcmd_checkbtn, 0, 5, 2, 1);

	g_signal_connect(G_OBJECT(mailcmd_checkbtn), "toggled",
			 G_CALLBACK(prefs_account_mailcmd_toggled),
			 NULL);

	mailcmd_label = gtk_label_new (_("command to send mails"));
	gtk_widget_show (mailcmd_label);
	gtk_label_set_xalign(GTK_LABEL (mailcmd_label), 1.0);
	gtk_grid_attach(GTK_GRID(serv_table), mailcmd_label, 0, 6, 1, 1);

	uid_label = gtk_label_new (_("User ID"));
	gtk_widget_show (uid_label);
	gtk_label_set_xalign(GTK_LABEL (uid_label), 1.0);
	gtk_grid_attach(GTK_GRID(serv_table), uid_label, 0, 7, 1, 1);

	pass_label = gtk_label_new (_("Password"));
	gtk_widget_show (pass_label);
#ifdef GENERIC_UMPC
	gtk_label_set_xalign(GTK_LABEL (pass_label), 1.0);
#endif
	gtk_grid_attach(GTK_GRID(serv_table), pass_label, 0, 8, 1, 1);

	showpwd_checkbtn = gtk_check_button_new_with_label (_("Show password"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(showpwd_checkbtn), FALSE);
	gtk_widget_show(showpwd_checkbtn);
	gtk_grid_attach(GTK_GRID(serv_table), showpwd_checkbtn, 2, 8, 1, 1);
	g_signal_connect(G_OBJECT(showpwd_checkbtn), "toggled",
			G_CALLBACK(prefs_account_showpwd_checkbtn_toggled), pass_entry);

	SET_TOGGLE_SENSITIVITY (nntpauth_checkbtn, uid_label);
	SET_TOGGLE_SENSITIVITY (nntpauth_checkbtn, pass_label);
	SET_TOGGLE_SENSITIVITY (nntpauth_checkbtn, uid_entry);
	SET_TOGGLE_SENSITIVITY (nntpauth_checkbtn, pass_entry);
	SET_TOGGLE_SENSITIVITY (nntpauth_checkbtn, showpwd_checkbtn);
	SET_TOGGLE_SENSITIVITY (nntpauth_checkbtn, nntpauth_onconnect_checkbtn);

	page->acname_entry   = acname_entry;
	page->default_checkbtn = default_checkbtn;

	page->name_entry = name_entry;
	page->addr_entry = addr_entry;
	page->org_entry  = org_entry;

	page->serv_frame       = serv_frame;
	page->serv_table       = serv_table;
	page->protocol_optmenu = (gpointer)protocol_optmenu;
	page->recvserv_label   = recvserv_label;
	page->recvserv_entry   = recvserv_entry;
	page->smtpserv_label   = smtpserv_label;
	page->smtpserv_entry   = smtpserv_entry;
	page->nntpserv_label   = nntpserv_label;
	page->nntpserv_entry   = nntpserv_entry;
	page->nntpauth_checkbtn  = nntpauth_checkbtn;
	page->nntpauth_onconnect_checkbtn  = nntpauth_onconnect_checkbtn;
	page->localmbox_label   = localmbox_label;
	page->localmbox_entry   = localmbox_entry;
	page->mailcmd_checkbtn   = mailcmd_checkbtn;
	page->mailcmd_label   = mailcmd_label;
	page->mailcmd_entry   = mailcmd_entry;
	page->uid_label        = uid_label;
	page->pass_label       = pass_label;
	page->uid_entry        = uid_entry;
	page->pass_entry       = pass_entry;
	page->showpwd_checkbtn = showpwd_checkbtn;
	page->auto_configure_btn = auto_configure_btn;
	page->auto_configure_cancel_btn = auto_configure_cancel_btn;
	page->auto_configure_lbl = auto_configure_lbl;

	if (new_account) {
		prefs_set_dialog_to_default(basic_param);
		buf = g_strdup_printf(_("Account%d"), ac_prefs->account_id);
		gtk_entry_set_text(GTK_ENTRY(basic_page.acname_entry), buf);
		g_free(buf);
	} else {
		prefs_set_dialog(basic_param);

		/* Passwords are handled outside of PrefParams. */
		buf = passwd_store_get_account(ac_prefs->account_id,
				PWS_ACCOUNT_RECV);
		gtk_entry_set_text(GTK_ENTRY(page->pass_entry), buf != NULL ? buf : "");
		if (buf != NULL) {
			memset(buf, 0, strlen(buf));
			g_free(buf);
		}
	}

	page->vbox = vbox1;

	page->page.widget = vbox1;
}

static void receive_create_widget_func(PrefsPage * _page,
                                           GtkWindow * window,
                                           gpointer data)
{
	ReceivePage *page = (ReceivePage *) _page;
	PrefsAccount *ac_prefs = (PrefsAccount *) data;

	GtkWidget *vbox1, *vbox2, *vbox3, *vbox4, *vbox5;
	GtkWidget *hbox1, *hbox2, *hbox3;
	GtkWidget *frame1;
	GtkWidget *pop_auth_checkbtn;
	GtkWidget *rmmail_checkbtn;
	GtkWidget *hbox_spc;
	GtkWidget *leave_time_label;
	GtkWidget *leave_time_spinbtn;
	GtkWidget *leave_hour_label;
	GtkWidget *leave_hour_spinbtn;
	GtkWidget *size_limit_checkbtn;
	GtkWidget *size_limit_spinbtn;
	GtkWidget *label;
	GtkWidget *filter_on_recv_checkbtn;
	GtkWidget *filterhook_on_recv_checkbtn;
	GtkWidget *inbox_label;
	GtkWidget *inbox_entry;
	GtkWidget *inbox_btn;
	GtkWidget *imap_frame;
 	GtkWidget *imapdir_label;
	GtkWidget *imapdir_entry;
	GtkWidget *subsonly_checkbtn;
	GtkWidget *low_bandwidth_checkbtn;
	GtkWidget *imap_batch_size_spinbtn;
	GtkWidget *local_frame;
	GtkWidget *local_vbox;
	GtkWidget *local_hbox;
	GtkWidget *local_inbox_label;
	GtkWidget *local_inbox_entry;
	GtkWidget *local_inbox_btn;
	GtkWidget *autochk_checkbtn;
	GtkWidget *autochk_hour_spinbtn, *autochk_hour_label;
	GtkWidget *autochk_min_spinbtn, *autochk_min_label;
	GtkWidget *autochk_sec_spinbtn, *autochk_sec_label;
	GtkWidget *autochk_use_default_checkbtn;
	GtkAdjustment *adj;
	struct AutocheckWidgets *autochk_widgets;

	GtkWidget *optmenu, *optmenu2;
	GtkListStore *menu, *menu2;
	GtkTreeIter iter;
	GtkWidget *recvatgetall_checkbtn;

	GtkWidget *frame, *frame2;
	GtkWidget *maxarticle_label;
	GtkWidget *maxarticle_spinbtn;
	GtkAdjustment *maxarticle_spinbtn_adj;

	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	local_vbox = gtkut_get_options_frame(vbox1, &local_frame, _("Local"));

	local_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (local_hbox);
	gtk_box_pack_start (GTK_BOX (local_vbox), local_hbox, TRUE, TRUE, 0);

	local_inbox_label = gtk_label_new (_("Default Inbox"));
	gtk_widget_show (local_inbox_label);
	gtk_box_pack_start (GTK_BOX (local_hbox), local_inbox_label, FALSE, FALSE, 0);

	local_inbox_entry = gtk_entry_new ();
	gtk_widget_show (local_inbox_entry);
	CLAWS_SET_TIP(local_inbox_entry,
			     _("Unfiltered messages will be stored in this folder"));
	gtk_box_pack_start (GTK_BOX (local_hbox), local_inbox_entry, TRUE, TRUE, 0);

	local_inbox_btn = gtkut_get_browse_file_btn(_("Bro_wse"));
	gtk_widget_show (local_inbox_btn);
	CLAWS_SET_TIP(local_inbox_btn,
			     _("Unfiltered messages will be stored in this folder"));
	gtk_box_pack_start (GTK_BOX (local_hbox), local_inbox_btn, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (local_inbox_btn), "clicked",
			  G_CALLBACK (prefs_account_select_folder_cb),
			  local_inbox_entry);

	vbox2 = gtkut_get_options_frame(vbox1, &frame1, _("POP"));

	PACK_CHECK_BUTTON (vbox2, pop_auth_checkbtn,
			   _("Authenticate before POP connection"));

	//vbox5 = gtk_vbox_new (FALSE, 0);
	vbox5 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_widget_show (vbox5);
	gtk_box_pack_start (GTK_BOX (vbox2), vbox5, FALSE, FALSE, 0);

	hbox3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox3);
	gtk_box_pack_start (GTK_BOX (vbox5), hbox3, FALSE, FALSE, 0);

	hbox_spc = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (hbox3), hbox_spc, FALSE, FALSE, 0);
	gtk_widget_set_size_request (hbox_spc, 12, -1);

	label = gtk_label_new (_("Authentication method"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox3), label, FALSE, FALSE, 0);

	optmenu2 = gtkut_sc_combobox_create(NULL, FALSE);
	menu2 = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(optmenu2)));
	gtk_widget_show (optmenu2);
	gtk_box_pack_start (GTK_BOX (hbox3), optmenu2, FALSE, FALSE, 0);

	COMBOBOX_ADD (menu2, _("Select"), 0);
	COMBOBOX_ADD (menu2, NULL, 0);
	COMBOBOX_ADD (menu2, "APOP", POPAUTH_APOP);
	COMBOBOX_ADD (menu2, "OAuth2", POPAUTH_OAUTH2);
#ifndef USE_OAUTH2
	gtk_list_store_set(menu2, &iter, COMBOBOX_SENS, FALSE, -1);
#endif

	SET_TOGGLE_SENSITIVITY (pop_auth_checkbtn, vbox5);

	PACK_CHECK_BUTTON (vbox2, rmmail_checkbtn,
			   _("Remove messages on server when received"));

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

	hbox_spc = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (hbox1), hbox_spc, FALSE, FALSE, 0);
	gtk_widget_set_size_request (hbox_spc, 12, -1);

	leave_time_label = gtk_label_new (_("Remove after"));
	gtk_widget_show (leave_time_label);
	gtk_box_pack_start (GTK_BOX (hbox1), leave_time_label, FALSE, FALSE, 0);

	leave_time_spinbtn = gtk_spin_button_new_with_range(0, 365, 1);
	gtk_widget_show (leave_time_spinbtn);
	CLAWS_SET_TIP(leave_time_spinbtn,
			     _("0 days and 0 hours : remove immediately"));
	gtk_box_pack_start (GTK_BOX (hbox1), leave_time_spinbtn, FALSE, FALSE, 0);

	leave_time_label = gtk_label_new (_("days"));
	gtk_widget_show (leave_time_label);
	gtk_box_pack_start (GTK_BOX (hbox1), leave_time_label, FALSE, FALSE, 0);

	leave_hour_spinbtn = gtk_spin_button_new_with_range(0, 23, 1);
	gtk_widget_show (leave_hour_spinbtn);
	CLAWS_SET_TIP(leave_hour_spinbtn,
			     _("0 days and 0 hours : remove immediately"));
	gtk_box_pack_start (GTK_BOX (hbox1), leave_hour_spinbtn, FALSE, FALSE, 0);

	leave_hour_label = gtk_label_new (_("hours"));
	gtk_widget_show (leave_hour_label);
	gtk_box_pack_start (GTK_BOX (hbox1), leave_hour_label, FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY (rmmail_checkbtn, hbox1);

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox1, size_limit_checkbtn, _("Receive size limit"));

	CLAWS_SET_TIP(size_limit_checkbtn,
			     _("Messages over this limit will be partially retrieved. "
		   	       "When selecting them you will be able to download them fully "
			       "or delete them."));

	size_limit_spinbtn = gtk_spin_button_new_with_range(0, 100000, 1);
	gtk_widget_show (size_limit_spinbtn);
	gtk_box_pack_start (GTK_BOX (hbox1), size_limit_spinbtn, FALSE, FALSE, 0);

	label = gtk_label_new (_("KiB"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY (size_limit_checkbtn, size_limit_spinbtn);

	PACK_SPACER(vbox2, vbox3, VSPACING_NARROW_2);

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, TRUE, TRUE, 0);

	inbox_label = gtk_label_new (_("Default Inbox"));
	gtk_widget_show (inbox_label);
	gtk_box_pack_start (GTK_BOX (hbox1), inbox_label, FALSE, FALSE, 0);

	inbox_entry = gtk_entry_new ();
	gtk_widget_show (inbox_entry);
	CLAWS_SET_TIP(inbox_entry,
			     _("Unfiltered messages will be stored in this folder"));
	gtk_box_pack_start (GTK_BOX (hbox1), inbox_entry, TRUE, TRUE, 0);

	inbox_btn = gtkut_get_browse_file_btn(_("Bro_wse"));
	gtk_widget_show (inbox_btn);
	CLAWS_SET_TIP(inbox_btn,
			     _("Unfiltered messages will be stored in this folder"));
	gtk_box_pack_start (GTK_BOX (hbox1), inbox_btn, FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (inbox_btn), "clicked",
			  G_CALLBACK (prefs_account_select_folder_cb),
			  inbox_entry);

	vbox2 = gtkut_get_options_frame(vbox1, &frame2, _("NNTP"));

	hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox2);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox2, FALSE, FALSE, 0);

	maxarticle_label = gtk_label_new
		(_("Maximum number of articles to download"));
	gtk_widget_show (maxarticle_label);
	gtk_box_pack_start (GTK_BOX (hbox2), maxarticle_label, FALSE, FALSE, 0);

	maxarticle_spinbtn_adj =
		GTK_ADJUSTMENT(gtk_adjustment_new (300, 0, 10000, 10, 100, 0));
	maxarticle_spinbtn = gtk_spin_button_new
		(GTK_ADJUSTMENT (maxarticle_spinbtn_adj), 10, 0);
	gtk_widget_show (maxarticle_spinbtn);
	CLAWS_SET_TIP(maxarticle_spinbtn,
			     _("unlimited if 0 is specified"));
	gtk_box_pack_start (GTK_BOX (hbox2), maxarticle_spinbtn,
			    FALSE, FALSE, 0);
	gtk_widget_set_size_request (maxarticle_spinbtn, 64, -1);
	gtk_spin_button_set_numeric
		(GTK_SPIN_BUTTON (maxarticle_spinbtn), TRUE);

	vbox2 = gtkut_get_options_frame(vbox1, &imap_frame, _("IMAP"));

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

	label = gtk_label_new (_("Authentication method"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);


	optmenu = gtkut_sc_combobox_create(NULL, FALSE);
	menu = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(optmenu)));
	gtk_widget_show (optmenu);
	gtk_box_pack_start (GTK_BOX (hbox1), optmenu, FALSE, FALSE, 0);

	COMBOBOX_ADD (menu, _("Automatic"), 0);
	COMBOBOX_ADD (menu, NULL, 0);
	COMBOBOX_ADD (menu, _("Plain text"), IMAP_AUTH_PLAINTEXT);
	COMBOBOX_ADD (menu, "CRAM-MD5", IMAP_AUTH_CRAM_MD5);
	COMBOBOX_ADD (menu, "ANONYMOUS", IMAP_AUTH_ANON);
	COMBOBOX_ADD (menu, "GSSAPI", IMAP_AUTH_GSSAPI);
	COMBOBOX_ADD (menu, "DIGEST-MD5", IMAP_AUTH_DIGEST_MD5);
	COMBOBOX_ADD (menu, "SCRAM-SHA-1", IMAP_AUTH_SCRAM_SHA1);
	COMBOBOX_ADD (menu, "SCRAM-SHA-224", IMAP_AUTH_SCRAM_SHA224);
	COMBOBOX_ADD (menu, "SCRAM-SHA-256", IMAP_AUTH_SCRAM_SHA256);
	COMBOBOX_ADD (menu, "SCRAM-SHA-384", IMAP_AUTH_SCRAM_SHA384);
	COMBOBOX_ADD (menu, "SCRAM-SHA-512", IMAP_AUTH_SCRAM_SHA512);
	COMBOBOX_ADD (menu, "PLAIN", IMAP_AUTH_PLAIN);
	COMBOBOX_ADD (menu, "LOGIN", IMAP_AUTH_LOGIN);
	COMBOBOX_ADD (menu, "OAUTH2", IMAP_AUTH_OAUTH2);
#ifndef USE_OAUTH2
	gtk_list_store_set(menu, &iter, COMBOBOX_SENS, FALSE, -1);
#endif

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
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

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 4);

	PACK_CHECK_BUTTON (hbox1, subsonly_checkbtn,
			   _("Show subscribed folders only"));

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 4);

	PACK_CHECK_BUTTON (hbox1, low_bandwidth_checkbtn,
			   _("Bandwidth-efficient mode (prevents retrieving remote tags)"));
	CLAWS_SET_TIP(low_bandwidth_checkbtn,
			     _("This mode uses less bandwidth, but can be slower with some servers."));

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 4);

	label = gtk_label_new(_("Batch size"));
	gtk_widget_show (label);
	gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 0);

	imap_batch_size_spinbtn = gtk_spin_button_new_with_range(0, 1000000, 1);
	gtk_widget_set_size_request(imap_batch_size_spinbtn, 64, -1);
	gtk_widget_show (imap_batch_size_spinbtn);
	gtk_box_pack_start(GTK_BOX(hbox1), imap_batch_size_spinbtn, FALSE, FALSE, 0);

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 4);

	/* Auto-checking */
	vbox4 = gtkut_get_options_frame(vbox1, &frame, _("Automatic checking"));

	PACK_CHECK_BUTTON(vbox4, autochk_use_default_checkbtn,
			_("Use global settings"));

	hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start(GTK_BOX(vbox4), hbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON(hbox2, autochk_checkbtn,
			_("Check for new mail every"));

	adj = gtk_adjustment_new(5, 0, 99, 1, 10, 0);
	autochk_hour_spinbtn = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(autochk_hour_spinbtn), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox2), autochk_hour_spinbtn, FALSE, FALSE, 0);
	autochk_hour_label = gtk_label_new(_("hours"));
	gtk_box_pack_start(GTK_BOX(hbox2), autochk_hour_label, FALSE, FALSE, 0);

	adj = gtk_adjustment_new(5, 0, 99, 1, 10, 0);
	autochk_min_spinbtn = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(autochk_min_spinbtn), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox2), autochk_min_spinbtn, FALSE, FALSE, 0);
	autochk_min_label = gtk_label_new(_("minutes"));
	gtk_box_pack_start(GTK_BOX(hbox2), autochk_min_label, FALSE, FALSE, 0);

	adj = gtk_adjustment_new(5, 0, 99, 1, 10, 0);
	autochk_sec_spinbtn = gtk_spin_button_new(GTK_ADJUSTMENT(adj), 1, 0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(autochk_sec_spinbtn), TRUE);
	gtk_box_pack_start(GTK_BOX(hbox2), autochk_sec_spinbtn, FALSE, FALSE, 0);
	autochk_sec_label = gtk_label_new(_("seconds"));
	gtk_box_pack_start(GTK_BOX(hbox2), autochk_sec_label, FALSE, FALSE, 0);

	autochk_widgets = g_new0(struct AutocheckWidgets, 1);
	autochk_widgets->autochk_hour_spinbtn = autochk_hour_spinbtn;
	autochk_widgets->autochk_min_spinbtn = autochk_min_spinbtn;
	autochk_widgets->autochk_sec_spinbtn = autochk_sec_spinbtn;

	gtk_widget_show_all(vbox4);

	SET_TOGGLE_SENSITIVITY_REVERSE(autochk_use_default_checkbtn, hbox2);
	SET_TOGGLE_SENSITIVITY(autochk_checkbtn, autochk_hour_spinbtn);
	SET_TOGGLE_SENSITIVITY(autochk_checkbtn, autochk_min_spinbtn);
	SET_TOGGLE_SENSITIVITY(autochk_checkbtn, autochk_sec_spinbtn);
	SET_TOGGLE_SENSITIVITY(autochk_checkbtn, autochk_hour_label);
	SET_TOGGLE_SENSITIVITY(autochk_checkbtn, autochk_min_label);
	SET_TOGGLE_SENSITIVITY(autochk_checkbtn, autochk_sec_label);

	PACK_CHECK_BUTTON (vbox1, filter_on_recv_checkbtn,
			   _("Filter messages on receiving"));

	g_signal_connect(G_OBJECT(filter_on_recv_checkbtn), "toggled",
			 G_CALLBACK(prefs_account_filter_on_recv_toggled),
			 NULL);
	g_signal_connect(G_OBJECT(autochk_hour_spinbtn), "value-changed",
		G_CALLBACK(prefs_account_receive_itv_spinbutton_value_changed_cb),
		(gpointer) page);
	g_signal_connect(G_OBJECT(autochk_min_spinbtn), "value-changed",
		G_CALLBACK(prefs_account_receive_itv_spinbutton_value_changed_cb),
		(gpointer) page);
	g_signal_connect(G_OBJECT(autochk_sec_spinbtn), "value-changed",
		G_CALLBACK(prefs_account_receive_itv_spinbutton_value_changed_cb),
		(gpointer) page);

	PACK_CHECK_BUTTON (vbox1, filterhook_on_recv_checkbtn,
			   _("Allow filtering using plugins on receiving"));

	PACK_CHECK_BUTTON
		(vbox1, recvatgetall_checkbtn,
		 _("'Get Mail' checks for new messages on this account"));

	page->pop3_frame               = frame1;
	page->pop_auth_checkbtn          = pop_auth_checkbtn;
	page->pop_auth_type_optmenu      = optmenu2;
	page->rmmail_checkbtn            = rmmail_checkbtn;
	page->leave_time_spinbtn         = leave_time_spinbtn;
	page->leave_hour_spinbtn         = leave_hour_spinbtn;
	page->size_limit_checkbtn        = size_limit_checkbtn;
	page->size_limit_spinbtn         = size_limit_spinbtn;
	page->filter_on_recv_checkbtn    = filter_on_recv_checkbtn;
	page->filterhook_on_recv_checkbtn = filterhook_on_recv_checkbtn;
	page->inbox_label              = inbox_label;
	page->inbox_entry              = inbox_entry;
	page->inbox_btn                = inbox_btn;

	page->autochk_frame            = frame;

	page->imap_frame               = imap_frame;
	page->imap_auth_type_optmenu   = optmenu;

	page->imapdir_label		= imapdir_label;
	page->imapdir_entry		= imapdir_entry;
	page->subsonly_checkbtn		= subsonly_checkbtn;
	page->low_bandwidth_checkbtn	= low_bandwidth_checkbtn;
	page->imap_batch_size_spinbtn	= imap_batch_size_spinbtn;
	page->local_frame		= local_frame;
	page->local_inbox_label	= local_inbox_label;
	page->local_inbox_entry	= local_inbox_entry;
	page->local_inbox_btn		= local_inbox_btn;

	page->recvatgetall_checkbtn      = recvatgetall_checkbtn;

	page->frame_maxarticle	= frame2;
	page->maxarticle_spinbtn     	= maxarticle_spinbtn;
	page->maxarticle_spinbtn_adj 	= maxarticle_spinbtn_adj;

	page->autochk_checkbtn = autochk_checkbtn;
	page->autochk_widgets = autochk_widgets;
	page->autochk_use_default_checkbtn = autochk_use_default_checkbtn;

	tmp_ac_prefs = *ac_prefs;

	if (new_account) {
		prefs_set_dialog_to_default(receive_param);
	} else
		prefs_set_dialog(receive_param);

	page->vbox = vbox1;

	page->page.widget = vbox1;
}

static void send_create_widget_func(PrefsPage * _page,
                                           GtkWindow * window,
                                           gpointer data)
{
	SendPage *page = (SendPage *) _page;
	PrefsAccount *ac_prefs = (PrefsAccount *) data;

	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *frame;
	GtkWidget *msgid_checkbtn;
	GtkWidget *xmailer_checkbtn;
	GtkWidget *hbox;
	GtkWidget *customhdr_checkbtn;
	GtkWidget *customhdr_edit_btn;
	GtkWidget *checkbtn_msgid_with_addr;
	GtkWidget *vbox3;
	GtkWidget *smtp_auth_checkbtn;
	GtkWidget *showpwd_checkbtn;
	GtkWidget *optmenu;
	GtkListStore *menu;
	GtkTreeIter iter;
	GtkWidget *vbox4;
	GtkWidget *hbox_spc;
	GtkWidget *label;
	GtkWidget *smtp_uid_entry;
	GtkWidget *smtp_pass_entry;
	GtkWidget *vbox_spc;
	GtkWidget *pop_bfr_smtp_checkbtn;
	GtkWidget *pop_bfr_smtp_tm_spinbtn;
	GtkWidget *pop_auth_timeout_lbl;
	GtkWidget *pop_auth_minutes_lbl;
	gchar *buf;

	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtkut_get_options_frame(vbox1, &frame, _("Header"));

	PACK_CHECK_BUTTON (vbox2, msgid_checkbtn, _("Generate Message-ID"));

	PACK_CHECK_BUTTON (vbox2, checkbtn_msgid_with_addr,
			   _("Send account mail address in Message-ID"));

	PACK_CHECK_BUTTON (vbox2, xmailer_checkbtn,
			   _("Add user agent header"));

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox, customhdr_checkbtn,
			   _("Add user-defined header"));

	customhdr_edit_btn = gtk_button_new_with_mnemonic("_Edit");
	gtk_widget_show (customhdr_edit_btn);
	gtk_box_pack_start (GTK_BOX (hbox), customhdr_edit_btn,
			    FALSE, FALSE, 0);
	g_signal_connect (G_OBJECT (customhdr_edit_btn), "clicked",
			  G_CALLBACK (prefs_account_edit_custom_header),
			  NULL);

	SET_TOGGLE_SENSITIVITY (customhdr_checkbtn, customhdr_edit_btn);

	vbox3 = gtkut_get_options_frame(vbox1, &frame, _("Authentication"));

	PACK_CHECK_BUTTON (vbox3, smtp_auth_checkbtn,
		_("SMTP Authentication (SMTP AUTH)"));

	vbox4 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (vbox4);
	gtk_box_pack_start (GTK_BOX (vbox3), vbox4, FALSE, FALSE, 0);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox4), hbox, FALSE, FALSE, 0);

	hbox_spc = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (hbox), hbox_spc, FALSE, FALSE, 0);
	gtk_widget_set_size_request (hbox_spc, 12, -1);

	label = gtk_label_new (_("Authentication method"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	optmenu = gtkut_sc_combobox_create(NULL, FALSE);
	menu = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(optmenu)));
	gtk_widget_show (optmenu);
	gtk_box_pack_start (GTK_BOX (hbox), optmenu, FALSE, FALSE, 0);

	COMBOBOX_ADD (menu, _("Automatic"), 0);
	COMBOBOX_ADD (menu, NULL, 0);
	COMBOBOX_ADD (menu, "PLAIN", SMTPAUTH_PLAIN);
	COMBOBOX_ADD (menu, "LOGIN", SMTPAUTH_LOGIN);
	COMBOBOX_ADD (menu, "CRAM-MD5", SMTPAUTH_CRAM_MD5);
	COMBOBOX_ADD (menu, "OAUTH2", SMTPAUTH_OAUTH2);
#ifndef USE_OAUTH2
	gtk_list_store_set(menu, &iter, COMBOBOX_SENS, FALSE, -1);
#endif
	COMBOBOX_ADD (menu, "DIGEST-MD5", SMTPAUTH_DIGEST_MD5);
	gtk_list_store_set(menu, &iter, COMBOBOX_SENS, FALSE, -1);

	PACK_SPACER(vbox4, vbox_spc, VSPACING_NARROW_2);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox4), hbox, FALSE, FALSE, 0);

	hbox_spc = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
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
	g_signal_connect(G_OBJECT(smtp_uid_entry), "changed",
			G_CALLBACK(prefs_account_entry_changed_newline_check_cb),
			GINT_TO_POINTER(ac_prefs->protocol));

#ifdef GENERIC_UMPC
	PACK_SPACER(vbox4, vbox_spc, VSPACING_NARROW_2);
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox4), hbox, FALSE, FALSE, 0);

	hbox_spc = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (hbox), hbox_spc, FALSE, FALSE, 0);
	gtk_widget_set_size_request (hbox_spc, 12, -1);
#endif
	label = gtk_label_new (_("Password"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	smtp_pass_entry = gtk_entry_new ();
	gtk_widget_show (smtp_pass_entry);
	gtk_widget_set_size_request (smtp_pass_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_box_pack_start (GTK_BOX (hbox), smtp_pass_entry, TRUE, TRUE, 0);
	gtk_entry_set_visibility (GTK_ENTRY (smtp_pass_entry), FALSE);
	g_signal_connect(G_OBJECT(smtp_pass_entry), "changed",
			G_CALLBACK(prefs_account_entry_changed_newline_check_cb),
			GINT_TO_POINTER(ac_prefs->protocol));

	showpwd_checkbtn = gtk_check_button_new_with_label (_("Show password"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(showpwd_checkbtn), FALSE);
	gtk_widget_show(showpwd_checkbtn);
	gtk_box_pack_start(GTK_BOX (hbox), showpwd_checkbtn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(showpwd_checkbtn), "toggled",
			G_CALLBACK(prefs_account_showpwd_checkbtn_toggled), smtp_pass_entry);

	PACK_SPACER(vbox4, vbox_spc, VSPACING_NARROW_2);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox4), hbox, FALSE, FALSE, 0);

	hbox_spc = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
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

	SET_TOGGLE_SENSITIVITY (smtp_auth_checkbtn, vbox4);

	PACK_CHECK_BUTTON (vbox3, pop_bfr_smtp_checkbtn,
		_("Authenticate with POP before sending"));
	
	g_signal_connect (G_OBJECT (pop_bfr_smtp_checkbtn), "clicked",
			  G_CALLBACK (pop_bfr_smtp_tm_set_sens),
			  NULL);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox3), hbox, FALSE, FALSE, 0);

	hbox_spc = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (hbox), hbox_spc, FALSE, FALSE, 0);
	gtk_widget_set_size_request (hbox_spc, 12, -1);

	pop_auth_timeout_lbl = gtk_label_new(_("POP authentication timeout"));
	gtk_widget_show (pop_auth_timeout_lbl);
	gtk_box_pack_start (GTK_BOX (hbox), pop_auth_timeout_lbl, FALSE, FALSE, 0);

	pop_bfr_smtp_tm_spinbtn = gtk_spin_button_new_with_range(0, 1440, 1);
	gtk_widget_show (pop_bfr_smtp_tm_spinbtn);
	gtk_box_pack_start (GTK_BOX (hbox), pop_bfr_smtp_tm_spinbtn, FALSE, FALSE, 0);

	pop_auth_minutes_lbl = gtk_label_new(_("minutes"));
	gtk_widget_show (pop_auth_minutes_lbl);
	gtk_box_pack_start (GTK_BOX (hbox), pop_auth_minutes_lbl, FALSE, FALSE, 0);
	
	page->msgid_checkbtn     = msgid_checkbtn;
	page->xmailer_checkbtn   = xmailer_checkbtn;
	page->customhdr_checkbtn = customhdr_checkbtn;
	page->msgid_with_addr_checkbtn	= checkbtn_msgid_with_addr;

	page->smtp_auth_checkbtn       = smtp_auth_checkbtn;
	page->smtp_auth_type_optmenu = optmenu;
	page->smtp_uid_entry         = smtp_uid_entry;
	page->smtp_pass_entry        = smtp_pass_entry;
	page->pop_bfr_smtp_checkbtn    = pop_bfr_smtp_checkbtn;
	page->pop_bfr_smtp_tm_spinbtn  = pop_bfr_smtp_tm_spinbtn;
	page->pop_auth_timeout_lbl   = pop_auth_timeout_lbl;
	page->pop_auth_minutes_lbl   = pop_auth_minutes_lbl;

	tmp_ac_prefs = *ac_prefs;

	if (new_account) {
		prefs_set_dialog_to_default(send_param);
	} else {
		prefs_set_dialog(send_param);

		/* Passwords are handled outside of PrefParams. */
		buf = passwd_store_get_account(ac_prefs->account_id,
				PWS_ACCOUNT_SEND);
		gtk_entry_set_text(GTK_ENTRY(page->smtp_pass_entry), buf != NULL ? buf : "");
		if (buf != NULL) {
			memset(buf, 0, strlen(buf));
			g_free(buf);
		}
	}

	pop_bfr_smtp_tm_set_sens (NULL, NULL);

	page->vbox = vbox1;

	page->page.widget = vbox1;
}

#ifdef USE_OAUTH2
static void oauth2_create_widget_func(PrefsPage * _page,
                                           GtkWindow * window,
                                           gpointer data)
{
	Oauth2Page *page = (Oauth2Page *) _page;
	PrefsAccount *ac_prefs = (PrefsAccount *) data;

	GtkWidget *vbox1, *vbox2, *vbox3;
	GtkWidget *hbox;
	GtkWidget *hbox_spc;
	GtkWidget *oauth2_authorise_btn;
	GtkWidget *auth_vbox, *auth_frame;
	GtkWidget *label;
	GtkWidget *oauth2_authcode_entry;
	GtkWidget *oauth2_auth_optmenu;
        GtkWidget *oauth2_link_button;
        GtkWidget *oauth2_link_copy_button;
	GtkWidget *oauth2_client_id_entry;
	GtkWidget *oauth2_client_secret_entry;
	GtkWidget *table1;
	GtkListStore *menu;
	GtkTreeIter iter;
	char *buf;
	struct BasicProtocol *protocol_optmenu;

	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	auth_vbox = gtkut_get_options_frame(vbox1, &auth_frame,
			_("Authorization"));
	
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (auth_vbox), hbox, FALSE, FALSE, 0);

	hbox_spc = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (hbox_spc);
	gtk_widget_set_size_request (hbox_spc, 12, -1);
	gtk_box_pack_start (GTK_BOX (hbox), hbox_spc, FALSE, FALSE, 0);

	/* Email service provider */

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (auth_vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new (_("Select OAuth2 Email Service Provider"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	oauth2_auth_optmenu = gtkut_sc_combobox_create(NULL, FALSE);
	menu = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(oauth2_auth_optmenu)));
	gtk_widget_show (oauth2_auth_optmenu);
	gtk_box_pack_start (GTK_BOX (hbox), oauth2_auth_optmenu, FALSE, FALSE, 0);

	COMBOBOX_ADD (menu, _("Select"), NULL);
	COMBOBOX_ADD (menu, NULL, 0);
	COMBOBOX_ADD (menu, "Google/Gmail", OAUTH2AUTH_GOOGLE);
	COMBOBOX_ADD (menu, "MS Outlook", OAUTH2AUTH_OUTLOOK);
	COMBOBOX_ADD (menu, "MS Exchange", OAUTH2AUTH_EXCHANGE);
	COMBOBOX_ADD (menu, "Yahoo", OAUTH2AUTH_YAHOO);

	protocol_optmenu = g_new(struct BasicProtocol, 1);
	protocol_optmenu->combobox = oauth2_auth_optmenu;
	protocol_optmenu->label = label;
	protocol_optmenu->descrlabel = label;

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (auth_vbox), hbox, FALSE, FALSE, 0);

	vbox3 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (vbox3);
	gtk_box_pack_start (GTK_BOX (auth_vbox), vbox3, FALSE, FALSE, 0);
 
	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox3), vbox2, FALSE, FALSE, 0);

	table1 = gtk_grid_new();
	gtk_widget_show (table1);
	gtk_container_add (GTK_CONTAINER (vbox2), table1);
	gtk_container_set_border_width (GTK_CONTAINER (table1), 8);
	gtk_grid_set_row_spacing(GTK_GRID(table1), VSPACING_NARROW);
	gtk_grid_set_column_spacing(GTK_GRID(table1), 8);

	label = gtk_label_new (_("Client ID"));
	gtk_widget_show (label);
	gtk_label_set_xalign(GTK_LABEL (label), 1.0);
	gtk_grid_attach(GTK_GRID(table1), label, 0, 0, 1, 1);

	oauth2_client_id_entry = gtk_entry_new();
	gtk_widget_show (oauth2_client_id_entry);
	gtk_grid_attach(GTK_GRID(table1), oauth2_client_id_entry, 1, 0, 1, 1);
	gtk_widget_set_hexpand(oauth2_client_id_entry, TRUE);
	gtk_widget_set_halign(oauth2_client_id_entry, GTK_ALIGN_FILL);

	label = gtk_label_new (_("Client secret"));
	gtk_widget_show (label);
	gtk_label_set_xalign(GTK_LABEL (label), 1.0);
	gtk_grid_attach(GTK_GRID(table1), label, 0, 1, 1, 1);

	oauth2_client_secret_entry = gtk_entry_new ();
	gtk_widget_show (oauth2_client_secret_entry);
	gtk_grid_attach(GTK_GRID(table1), oauth2_client_secret_entry, 1, 1, 1, 1);
	gtk_widget_set_hexpand(oauth2_client_secret_entry, TRUE);
	gtk_widget_set_halign(oauth2_client_secret_entry, GTK_ALIGN_FILL);

	hbox_spc = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (hbox), hbox_spc, FALSE, FALSE, 0);
	//gtk_widget_set_size_request (hbox_spc, 12, -1);

	hbox_spc = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_spc, FALSE, FALSE, 0);
	//gtk_widget_set_size_request (hbox_spc, 12, 10);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox3), hbox, FALSE, FALSE, 0);

	label = gtk_label_new (_("Obtain authorization code"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	oauth2_link_button = gtk_button_new_with_label(_("Open default browser with request"));
	g_signal_connect(G_OBJECT(oauth2_link_button), "clicked", G_CALLBACK(prefs_account_oauth2_copy_url), NULL);
	gtk_widget_set_sensitive(oauth2_link_button, TRUE);
	gtk_widget_set_margin_bottom(oauth2_link_button, 8);
	gtk_widget_show (oauth2_link_button);
	gtk_box_pack_start (GTK_BOX (hbox), oauth2_link_button, FALSE, FALSE, 0);

	oauth2_link_copy_button = gtk_button_new_with_label(_("Copy link"));
	g_signal_connect(G_OBJECT(oauth2_link_copy_button), "clicked", G_CALLBACK(prefs_account_oauth2_copy_url), NULL);
	gtk_widget_set_sensitive(oauth2_link_copy_button, TRUE);
	gtk_widget_set_margin_bottom(oauth2_link_copy_button, 8);
	gtk_widget_show (oauth2_link_copy_button);
	gtk_box_pack_start (GTK_BOX (hbox), oauth2_link_copy_button, FALSE, FALSE, 0);

	/* Authorisation code */
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox3), hbox, FALSE, FALSE, 0);
	//gtk_widget_set_size_request (hbox, -1, 50);

	label = gtk_label_new (_("Authorization code"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	oauth2_authcode_entry = gtk_entry_new ();
	gtk_widget_show (oauth2_authcode_entry);
	gtk_widget_set_margin_bottom(oauth2_authcode_entry, 8);
	//gtk_widget_set_size_request (oauth2_authcode_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_widget_set_tooltip_text(oauth2_authcode_entry,
		_("Paste complete URL from browser or the provided authorization token"));
	gtk_box_pack_start (GTK_BOX (hbox), oauth2_authcode_entry, TRUE, TRUE, 0);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox3), hbox, FALSE, FALSE, 0);

	label = gtk_label_new (_("Complete authorization"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	oauth2_authorise_btn = gtk_button_new_with_label(_("Authorize"));
	gtk_box_pack_start(GTK_BOX(hbox), oauth2_authorise_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(oauth2_authorise_btn), "clicked",
			 G_CALLBACK(prefs_account_oauth2_obtain_tokens), NULL);
	gtk_widget_show(oauth2_authorise_btn);

	/* Add items to page struct */

	page->oauth2_link_button = oauth2_link_button;
	page->oauth2_link_copy_button = oauth2_link_copy_button;
	page->oauth2_authcode_entry = oauth2_authcode_entry;
	page->oauth2_auth_optmenu = oauth2_auth_optmenu;
	page->protocol_optmenu = (gpointer)protocol_optmenu;
	page->oauth2_authorise_btn = oauth2_authorise_btn;
	page->oauth2_client_id_entry = oauth2_client_id_entry;
	page->oauth2_client_secret_entry = oauth2_client_secret_entry;
	page->vbox = vbox1;
	page->page.widget = vbox1;
	page->oauth2_sensitive = vbox3;

	tmp_ac_prefs = *ac_prefs;

	g_signal_connect(G_OBJECT(oauth2_auth_optmenu), "changed", 
			 G_CALLBACK(prefs_account_oauth2_set_sensitivity), NULL);
	g_signal_connect(G_OBJECT(oauth2_authcode_entry), "changed", 
			 G_CALLBACK(prefs_account_oauth2_set_auth_sensitivity), NULL);
	prefs_account_oauth2_set_auth_sensitivity();

	if (new_account) {
		prefs_set_dialog_to_default(oauth2_param);
	} else {
		prefs_set_dialog(oauth2_param);

		/* Passwords are handled outside of PrefParams. */
		buf = passwd_store_get_account(ac_prefs->account_id,
				PWS_ACCOUNT_OAUTH2_AUTH);
		gtk_entry_set_text(GTK_ENTRY(page->oauth2_authcode_entry), buf != NULL ? buf : "");
		if (buf != NULL) {
			memset(buf, 0, strlen(buf));
			g_free(buf);
		}
	}

	/* For testing */
	/* 	oauth2_encode(OAUTH2info[0][OA2_CLIENT_ID]); */

}
#endif

static void compose_create_widget_func(PrefsPage * _page,
                                           GtkWindow * window,
                                           gpointer data)
{
	ComposePage *page = (ComposePage *) _page;
	PrefsAccount *ac_prefs = (PrefsAccount *) data;

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
	GtkWidget *autocc_checkbtn;
	GtkWidget *autocc_entry;
	GtkWidget *autobcc_checkbtn;
	GtkWidget *autobcc_entry;
	GtkWidget *autoreplyto_checkbtn;
	GtkWidget *autoreplyto_entry;
#if USE_ENCHANT
	GtkWidget *frame_dict;
	GtkWidget *table_dict;
	GtkWidget *checkbtn_enable_default_dictionary = NULL;
	GtkWidget *combo_default_dictionary = NULL;
	GtkWidget *checkbtn_enable_default_alt_dictionary = NULL;
	GtkWidget *combo_default_alt_dictionary = NULL;
#endif

	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox_sig = gtkut_get_options_frame(vbox1, &frame_sig, _("Signature"));

	PACK_CHECK_BUTTON (vbox_sig, checkbtn_autosig,
			   _("Automatically insert signature"));

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_sig), hbox1, TRUE, TRUE, 0);
	label_sigsep = gtk_label_new (_("Signature separator"));
	gtk_widget_show (label_sigsep);
	gtk_box_pack_start (GTK_BOX (hbox1), label_sigsep, FALSE, FALSE, 0);

	entry_sigsep = gtk_entry_new ();
	gtk_widget_show (entry_sigsep);
	gtk_box_pack_start (GTK_BOX (hbox1), entry_sigsep, FALSE, FALSE, 0);

	gtk_widget_set_size_request (entry_sigsep, 64, -1);

	sig_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
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

	hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox2);
	gtk_box_pack_start (GTK_BOX (vbox_sig), hbox2, TRUE, TRUE, 0);
	label_sigpath = gtk_label_new (_("Signature"));
	gtk_widget_show (label_sigpath);
	gtk_box_pack_start (GTK_BOX (hbox2), label_sigpath, FALSE, FALSE, 0);

	entry_sigpath = gtk_entry_new ();
	gtk_widget_show (entry_sigpath);
	gtk_box_pack_start (GTK_BOX (hbox2), entry_sigpath, TRUE, TRUE, 0);

	signature_browse_button = gtkut_get_browse_file_btn(_("Bro_wse"));
	gtk_widget_show (signature_browse_button);
	gtk_box_pack_start (GTK_BOX (hbox2), signature_browse_button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(signature_browse_button), "clicked",
			 G_CALLBACK(prefs_account_signature_browse_cb), NULL);

	signature_edit_button = gtk_button_new_with_mnemonic("_Edit");
	gtk_widget_show (signature_edit_button);
	gtk_box_pack_start (GTK_BOX (hbox2), signature_edit_button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(signature_edit_button), "clicked",
			 G_CALLBACK(prefs_account_signature_edit_cb), entry_sigpath);

	PACK_FRAME (vbox1, frame, _("Automatically set the following addresses"));

	table =  gtk_grid_new();
	gtk_widget_show (table);
	gtk_container_add (GTK_CONTAINER (frame), table);
	gtk_container_set_border_width (GTK_CONTAINER (table), 8);
	gtk_grid_set_row_spacing(GTK_GRID(table), VSPACING_NARROW_2);
	gtk_grid_set_column_spacing(GTK_GRID(table), 8);

	autocc_checkbtn = gtk_check_button_new_with_label (
				prefs_common_translated_header_name("Cc:"));
	gtk_widget_show (autocc_checkbtn);
	gtk_grid_attach(GTK_GRID(table), autocc_checkbtn, 0, 0, 1, 1);

	autocc_entry = gtk_entry_new ();
	gtk_widget_show (autocc_entry);
	gtk_grid_attach(GTK_GRID(table), autocc_entry, 1, 0, 1, 1);
	gtk_widget_set_hexpand(autocc_entry, TRUE);
	gtk_widget_set_halign(autocc_entry, GTK_ALIGN_FILL);

	SET_TOGGLE_SENSITIVITY (autocc_checkbtn, autocc_entry);

	autobcc_checkbtn = gtk_check_button_new_with_label (
				prefs_common_translated_header_name("Bcc:"));
	gtk_widget_show (autobcc_checkbtn);
	gtk_grid_attach(GTK_GRID(table), autobcc_checkbtn, 0, 1, 1, 1);

	autobcc_entry = gtk_entry_new ();
	gtk_widget_show (autobcc_entry);
	gtk_grid_attach(GTK_GRID(table), autobcc_entry, 1, 1, 1, 1);
	gtk_widget_set_hexpand(autobcc_entry, TRUE);
	gtk_widget_set_halign(autobcc_entry, GTK_ALIGN_FILL);

	SET_TOGGLE_SENSITIVITY (autobcc_checkbtn, autobcc_entry);

	autoreplyto_checkbtn = gtk_check_button_new_with_label (
				prefs_common_translated_header_name("Reply-To:"));
	gtk_widget_show (autoreplyto_checkbtn);
	gtk_grid_attach(GTK_GRID(table), autoreplyto_checkbtn, 0, 2, 1, 1);

	autoreplyto_entry = gtk_entry_new ();
	gtk_widget_show (autoreplyto_entry);
	gtk_grid_attach(GTK_GRID(table), autoreplyto_entry, 1, 2, 1, 1);
	gtk_widget_set_hexpand(autoreplyto_entry, TRUE);
	gtk_widget_set_halign(autoreplyto_entry, GTK_ALIGN_FILL);

	SET_TOGGLE_SENSITIVITY (autoreplyto_checkbtn, autoreplyto_entry);

#if USE_ENCHANT
	PACK_FRAME (vbox1, frame_dict, _("Spell check dictionaries"));

	table_dict =  gtk_grid_new();
	gtk_widget_show (table_dict);
	gtk_container_add (GTK_CONTAINER (frame_dict), table_dict);
	gtk_container_set_border_width (GTK_CONTAINER (table_dict), 8);
	gtk_grid_set_row_spacing(GTK_GRID(table_dict), VSPACING_NARROW_2);
	gtk_grid_set_column_spacing(GTK_GRID(table_dict), 8);

	/* Default dictionary */
	checkbtn_enable_default_dictionary = gtk_check_button_new_with_label(_("Default dictionary"));
	gtk_grid_attach(GTK_GRID(table_dict), checkbtn_enable_default_dictionary, 0, 0, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_enable_default_dictionary),
			tmp_ac_prefs.enable_default_dictionary);

	combo_default_dictionary = gtkaspell_dictionary_combo_new(TRUE);
	gtk_grid_attach(GTK_GRID(table_dict), combo_default_dictionary, 1, 0, 1, 1);

	SET_TOGGLE_SENSITIVITY(checkbtn_enable_default_dictionary, combo_default_dictionary);

	/* Default dictionary */
	checkbtn_enable_default_alt_dictionary = gtk_check_button_new_with_label(_("Default alternate dictionary"));
	gtk_grid_attach(GTK_GRID(table_dict), checkbtn_enable_default_alt_dictionary, 0, 1, 1, 1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_enable_default_alt_dictionary),
			tmp_ac_prefs.enable_default_alt_dictionary);

	combo_default_alt_dictionary = gtkaspell_dictionary_combo_new(FALSE);
	gtk_grid_attach(GTK_GRID(table_dict), combo_default_alt_dictionary, 1, 1, 1, 1);

	SET_TOGGLE_SENSITIVITY(checkbtn_enable_default_alt_dictionary, combo_default_alt_dictionary);

	gtk_widget_show_all(table_dict);
#endif

	page->sigfile_radiobtn = sigfile_radiobtn;
	page->entry_sigpath      = entry_sigpath;
	page->checkbtn_autosig   = checkbtn_autosig;
	page->entry_sigsep       = entry_sigsep;

	page->autocc_checkbtn      = autocc_checkbtn;
	page->autocc_entry       = autocc_entry;
	page->autobcc_checkbtn     = autobcc_checkbtn;
	page->autobcc_entry      = autobcc_entry;
	page->autoreplyto_checkbtn = autoreplyto_checkbtn;
	page->autoreplyto_entry  = autoreplyto_entry;
#ifdef USE_ENCHANT
	page->checkbtn_enable_default_dictionary = checkbtn_enable_default_dictionary;
	page->combo_default_dictionary = combo_default_dictionary;
	page->checkbtn_enable_default_alt_dictionary = checkbtn_enable_default_alt_dictionary;
	page->combo_default_alt_dictionary = combo_default_alt_dictionary;
#endif

#ifdef USE_ENCHANT
	/* reset gtkaspell menus */
	if (compose_page.combo_default_dictionary != NULL) {
		gtk_combo_box_set_model(GTK_COMBO_BOX(compose_page.combo_default_dictionary),
					gtkaspell_dictionary_store_new());
		gtk_combo_box_set_model(GTK_COMBO_BOX(compose_page.combo_default_alt_dictionary),
					gtkaspell_dictionary_store_new_with_refresh(
						FALSE));
	}
#endif

	tmp_ac_prefs = *ac_prefs;

	if (new_account) {
		prefs_set_dialog_to_default(compose_param);
	} else
		prefs_set_dialog(compose_param);

	page->vbox = vbox1;

	page->page.widget = vbox1;
}
	
static void templates_create_widget_func(PrefsPage * _page,
                                           GtkWindow * window,
                                           gpointer data)
{
	TemplatesPage *page = (TemplatesPage *) _page;
	PrefsAccount *ac_prefs = (PrefsAccount *) data;
	GtkWidget *vbox;
	GtkWidget *vbox2;
	GtkWidget *notebook;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show(vbox);
	
	notebook = gtk_notebook_new();
	gtk_widget_show(notebook);
	gtk_box_pack_start(GTK_BOX(vbox), notebook, TRUE, TRUE, 0);

	/* compose format */
	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING);
	gtk_widget_show (vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), VBOX_BORDER);

	quotefmt_create_new_msg_fmt_widgets(
				window,
				vbox2,
				&page->checkbtn_compose_with_format,
				NULL,
				&page->compose_subject_format,
				&page->compose_body_format,
				TRUE, NULL);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox2, gtk_label_new(_("Compose")));

	/* reply format */	
	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING);
	gtk_widget_show (vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), VBOX_BORDER);
	
	quotefmt_create_reply_fmt_widgets(
				window,
				vbox2,
				&page->checkbtn_reply_with_format,
				NULL,
				&page->reply_quotemark,
				&page->reply_body_format,
				TRUE, NULL);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox2, gtk_label_new(_("Reply")));

	/* forward format */	
	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING);
	gtk_widget_show (vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), VBOX_BORDER);

	quotefmt_create_forward_fmt_widgets(
				window,
				vbox2,
				&page->checkbtn_forward_with_format,
				NULL,
				&page->forward_quotemark,
				&page->forward_body_format,
				TRUE, NULL);
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox2, gtk_label_new(_("Forward")));

	tmp_ac_prefs = *ac_prefs;

	if (new_account) {
		prefs_set_dialog_to_default(templates_param);
	} else
		prefs_set_dialog(templates_param);

	page->vbox = vbox;

	page->page.widget = vbox;
}

static void privacy_create_widget_func(PrefsPage * _page,
                                           GtkWindow * window,
                                           gpointer data)
{
	PrivacyPage *page = (PrivacyPage *) _page;
	PrefsAccount *ac_prefs = (PrefsAccount *) data;

	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *hbox1;
	GtkWidget *label;
	GtkWidget *default_privacy_system;
	GtkListStore *menu;
	GtkCellRenderer *rend;
	GtkWidget *default_encrypt_checkbtn;
	GtkWidget *default_encrypt_reply_checkbtn;
	GtkWidget *default_sign_checkbtn;
	GtkWidget *default_sign_reply_checkbtn;
	GtkWidget *save_clear_text_checkbtn;
	GtkWidget *encrypt_to_self_checkbtn;

	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox1);
	gtk_container_add (GTK_CONTAINER(vbox2), hbox1);

	label = gtk_label_new(_("Default privacy system"));
	gtk_widget_show(label);
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);

	/* Can't use gtkut_sc_combobox_create() here, because model for this
	 * combobox needs an extra string column to store privacy plugin id. */
	menu = gtk_list_store_new(4,
			G_TYPE_STRING,
			G_TYPE_INT,
			G_TYPE_BOOLEAN,
			G_TYPE_STRING);	/* This is where we store the privacy plugin id. */
	default_privacy_system = gtk_combo_box_new_with_model(GTK_TREE_MODEL(menu));

	rend = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(default_privacy_system), rend, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(default_privacy_system), rend,
			"text", COMBOBOX_TEXT,
			"sensitive", COMBOBOX_SENS,
			NULL);
	gtk_widget_set_focus_on_click(GTK_WIDGET(default_privacy_system), FALSE);

	gtk_widget_show (default_privacy_system);
	gtk_box_pack_start (GTK_BOX(hbox1), default_privacy_system, FALSE, TRUE, 0);

	g_signal_connect(G_OBJECT(default_privacy_system), "changed",
			 G_CALLBACK(privacy_system_activated),
			 NULL);

	PACK_CHECK_BUTTON (vbox2, default_sign_checkbtn,
			   _("Always sign messages"));
	PACK_CHECK_BUTTON (vbox2, default_encrypt_checkbtn,
			   _("Always encrypt messages"));
	PACK_CHECK_BUTTON (vbox2, default_sign_reply_checkbtn,
			   _("Always sign messages when replying to a "
			     "signed message"));
	PACK_CHECK_BUTTON (vbox2, default_encrypt_reply_checkbtn,
			   _("Always encrypt messages when replying to an "
			     "encrypted message"));
	PACK_CHECK_BUTTON (vbox2, encrypt_to_self_checkbtn,
			   _("Encrypt sent messages with your own key in addition to recipient's"));
	PACK_CHECK_BUTTON (vbox2, save_clear_text_checkbtn,
			   _("Save sent encrypted messages as clear text"));

	SET_TOGGLE_SENSITIVITY_REVERSE(encrypt_to_self_checkbtn, save_clear_text_checkbtn);
	SET_TOGGLE_SENSITIVITY_REVERSE(save_clear_text_checkbtn, encrypt_to_self_checkbtn);

	page->default_privacy_system = default_privacy_system;
	page->default_encrypt_checkbtn = default_encrypt_checkbtn;
	page->default_encrypt_reply_checkbtn = default_encrypt_reply_checkbtn;
	page->default_sign_reply_checkbtn = default_sign_reply_checkbtn;
	page->default_sign_checkbtn    = default_sign_checkbtn;
	page->save_clear_text_checkbtn = save_clear_text_checkbtn;
	page->encrypt_to_self_checkbtn = encrypt_to_self_checkbtn;

	update_privacy_system_menu();

	tmp_ac_prefs = *ac_prefs;

	if (new_account) {
		prefs_set_dialog_to_default(privacy_param);
	} else
		prefs_set_dialog(privacy_param);

	page->vbox = vbox1;

	page->page.widget = vbox1;
}

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

#ifdef USE_GNUTLS

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

static void pop_ssltunnel_toggled(GtkToggleButton *button,
					gpointer data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			advanced_page.popport_checkbtn)) == TRUE)
		return;
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(advanced_page.popport_spinbtn),
				  active ? 995 : 110);
}

static void imap_ssltunnel_toggled(GtkToggleButton *button,
					gpointer data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			advanced_page.imapport_checkbtn)) == TRUE)
		return;
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(advanced_page.imapport_spinbtn),
				  active ? 993 : 143);
}

static void nntp_ssltunnel_toggled(GtkToggleButton *button,
					gpointer data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			advanced_page.nntpport_checkbtn)) == TRUE)
		return;
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(advanced_page.nntpport_spinbtn),
				  active ? 563 : 119);
}	

static void smtp_ssltunnel_toggled(GtkToggleButton *button,
					gpointer data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
			advanced_page.smtpport_checkbtn)) == TRUE)
		return;
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(advanced_page.smtpport_spinbtn),
				  active ? 465 : 25);	
}

static void ssl_create_widget_func(PrefsPage * _page,
                                           GtkWindow * window,
                                           gpointer data)
{
	SSLPage *page = (SSLPage *) _page;
	PrefsAccount *ac_prefs = (PrefsAccount *) data;

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

	GtkWidget *cert_frame;
	GtkWidget *cert_table;
	GtkWidget *entry_in_cert_pass;
	GtkWidget *entry_out_cert_pass;
	GtkWidget *showpwd_checkbtn;

	GtkWidget *vbox7;
	GtkWidget *ssl_certs_auto_accept_checkbtn;
	GtkWidget *use_nonblocking_ssl_checkbtn;
	GtkWidget *hbox;
	GtkWidget *hbox_spc;
	GtkWidget *label;
	gchar *buf;

	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtkut_get_options_frame(vbox1, &pop_frame, _("POP"));

	CREATE_RADIO_BUTTONS(vbox2,
			     pop_nossl_radiobtn,
			     _("Don't use TLS"),
			     SSL_NONE,
			     pop_ssltunnel_radiobtn,
			     _("Use TLS"),
			     SSL_TUNNEL,
			     pop_starttls_radiobtn,
			     _("Use STARTTLS command to start encrypted session"),
			     SSL_STARTTLS);
	g_signal_connect(G_OBJECT(pop_ssltunnel_radiobtn), "toggled",
			 G_CALLBACK(pop_ssltunnel_toggled), NULL);
	
	vbox3 = gtkut_get_options_frame(vbox1, &imap_frame, _("IMAP"));

	CREATE_RADIO_BUTTONS(vbox3,
			     imap_nossl_radiobtn,
			     _("Don't use TLS"),
			     SSL_NONE,
			     imap_ssltunnel_radiobtn,
			     _("Use TLS"),
			     SSL_TUNNEL,
			     imap_starttls_radiobtn,
			     _("Use STARTTLS command to start encrypted session"),
			     SSL_STARTTLS);
	g_signal_connect(G_OBJECT(imap_ssltunnel_radiobtn), "toggled",
			 G_CALLBACK(imap_ssltunnel_toggled), NULL);

	vbox4 = gtkut_get_options_frame(vbox1, &nntp_frame, _("NNTP"));

	nntp_nossl_radiobtn =
		gtk_radio_button_new_with_label (NULL, _("Don't use TLS"));
	gtk_widget_show (nntp_nossl_radiobtn);
	gtk_box_pack_start (GTK_BOX (vbox4), nntp_nossl_radiobtn,
			    FALSE, FALSE, 0);
	g_object_set_data (G_OBJECT (nntp_nossl_radiobtn),
			   MENU_VAL_ID,
			   GINT_TO_POINTER (SSL_NONE));

	CREATE_RADIO_BUTTON(vbox4, nntp_ssltunnel_radiobtn, nntp_nossl_radiobtn,
			    _("Use TLS"), SSL_TUNNEL);
	g_signal_connect(G_OBJECT(nntp_ssltunnel_radiobtn), "toggled",
			 G_CALLBACK(nntp_ssltunnel_toggled), NULL);

	vbox5 = gtkut_get_options_frame(vbox1, &send_frame, _("Send (SMTP)"));

	CREATE_RADIO_BUTTONS(vbox5,
			     smtp_nossl_radiobtn,
			     _("Don't use TLS (but, if necessary, use STARTTLS)"),
			     SSL_NONE,
			     smtp_ssltunnel_radiobtn,
			     _("Use TLS"),
			     SSL_TUNNEL,
			     smtp_starttls_radiobtn,
			     _("Use STARTTLS command to start encrypted session"),
			     SSL_STARTTLS);
	g_signal_connect(G_OBJECT(smtp_ssltunnel_radiobtn), "toggled",
			 G_CALLBACK(smtp_ssltunnel_toggled), NULL);

	PACK_FRAME(vbox1, cert_frame, _("Client certificates"));

	cert_table = gtk_grid_new();
	gtk_container_add(GTK_CONTAINER(cert_frame), cert_table);
	gtk_container_set_border_width(GTK_CONTAINER(cert_table), 8);
	gtk_grid_set_row_spacing(GTK_GRID(cert_table), VSPACING_NARROW_2);
	gtk_grid_set_column_spacing(GTK_GRID(cert_table), 8);

	label = gtk_label_new(_("Certificate for receiving"));
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	entry_in_cert_file = gtk_entry_new();
	in_ssl_cert_browse_button = gtkut_get_browse_file_btn(_("Browse"));
	CLAWS_SET_TIP(label,
			     _("Client certificate file as a PKCS12 or PEM file"));
	CLAWS_SET_TIP(entry_in_cert_file,
			     _("Client certificate file as a PKCS12 or PEM file"));
	gtk_grid_attach(GTK_GRID(cert_table), label, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(cert_table), entry_in_cert_file, 1, 0, 1, 1);
	gtk_widget_set_hexpand(entry_in_cert_file, TRUE);
	gtk_widget_set_halign(entry_in_cert_file, GTK_ALIGN_FILL);
	gtk_grid_attach(GTK_GRID(cert_table), in_ssl_cert_browse_button, 2, 0, 1, 1);

	label = gtk_label_new(_("Password"));
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	entry_in_cert_pass = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(entry_in_cert_pass), FALSE);
	showpwd_checkbtn = gtk_check_button_new_with_label (_("Show password"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(showpwd_checkbtn), FALSE);
	g_signal_connect(G_OBJECT(showpwd_checkbtn), "toggled",
			G_CALLBACK(prefs_account_showpwd_checkbtn_toggled), entry_in_cert_pass);
	gtk_grid_attach(GTK_GRID(cert_table), label, 0, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(cert_table), entry_in_cert_pass, 1, 1, 1, 1);
	gtk_widget_set_hexpand(entry_in_cert_pass, TRUE);
	gtk_widget_set_halign(entry_in_cert_pass, GTK_ALIGN_FILL);
	gtk_grid_attach(GTK_GRID(cert_table), showpwd_checkbtn, 2, 1, 1, 1);

	label = gtk_label_new(_("Certificate for sending"));
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	entry_out_cert_file = gtk_entry_new();
	out_ssl_cert_browse_button = gtkut_get_browse_file_btn(_("Browse"));
	CLAWS_SET_TIP(label,
			     _("Client certificate file as a PKCS12 or PEM file"));
	CLAWS_SET_TIP(entry_out_cert_file,
			     _("Client certificate file as a PKCS12 or PEM file"));
	gtk_grid_attach(GTK_GRID(cert_table), label, 0, 2, 1, 1);
	gtk_grid_attach(GTK_GRID(cert_table), entry_out_cert_file, 1, 2, 1, 1);
	gtk_widget_set_hexpand(entry_out_cert_file, TRUE);
	gtk_widget_set_halign(entry_out_cert_file, GTK_ALIGN_FILL);
	gtk_grid_attach(GTK_GRID(cert_table), out_ssl_cert_browse_button, 2, 2, 1, 1);

	label = gtk_label_new(_("Password"));
	gtk_label_set_xalign(GTK_LABEL(label), 1.0);
	entry_out_cert_pass = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(entry_out_cert_pass), FALSE);
	showpwd_checkbtn = gtk_check_button_new_with_label (_("Show password"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(showpwd_checkbtn), FALSE);
	g_signal_connect(G_OBJECT(showpwd_checkbtn), "toggled",
			G_CALLBACK(prefs_account_showpwd_checkbtn_toggled), entry_out_cert_pass);
	gtk_grid_attach(GTK_GRID(cert_table), label, 0, 3, 1, 1);
	gtk_grid_attach(GTK_GRID(cert_table), entry_out_cert_pass, 1, 3, 1, 1);
	gtk_widget_set_hexpand(entry_out_cert_pass, TRUE);
	gtk_widget_set_halign(entry_out_cert_pass, GTK_ALIGN_FILL);
	gtk_grid_attach(GTK_GRID(cert_table), showpwd_checkbtn, 2, 3, 1, 1);

	gtk_widget_show_all(cert_table);

	g_signal_connect(G_OBJECT(in_ssl_cert_browse_button), "clicked",
			 G_CALLBACK(prefs_account_in_cert_browse_cb), NULL);
	g_signal_connect(G_OBJECT(out_ssl_cert_browse_button), "clicked",
			 G_CALLBACK(prefs_account_out_cert_browse_cb), NULL);
	
	vbox7 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (vbox7);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox7, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON(vbox7, ssl_certs_auto_accept_checkbtn,
			  _("Automatically accept valid TLS certificates"));

	PACK_CHECK_BUTTON(vbox7, use_nonblocking_ssl_checkbtn,
			  _("Use non-blocking TLS"));

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox7), hbox, FALSE, FALSE, 0);

	hbox_spc = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (hbox), hbox_spc, FALSE, FALSE, 0);
	gtk_widget_set_size_request (hbox_spc, 16, -1);

	label = gtk_label_new
		(_("Turn this off if you have TLS connection problems"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtkut_widget_set_small_font_size (label);

	page->pop_frame               = pop_frame;
	page->pop_nossl_radiobtn      = pop_nossl_radiobtn;
	page->pop_ssltunnel_radiobtn  = pop_ssltunnel_radiobtn;
	page->pop_starttls_radiobtn   = pop_starttls_radiobtn;

	page->imap_frame              = imap_frame;
	page->imap_nossl_radiobtn     = imap_nossl_radiobtn;
	page->imap_ssltunnel_radiobtn = imap_ssltunnel_radiobtn;
	page->imap_starttls_radiobtn  = imap_starttls_radiobtn;

	page->nntp_frame              = nntp_frame;
	page->nntp_nossl_radiobtn     = nntp_nossl_radiobtn;
	page->nntp_ssltunnel_radiobtn = nntp_ssltunnel_radiobtn;

	page->send_frame              = send_frame;
	page->smtp_nossl_radiobtn     = smtp_nossl_radiobtn;
	page->smtp_ssltunnel_radiobtn = smtp_ssltunnel_radiobtn;
	page->smtp_starttls_radiobtn  = smtp_starttls_radiobtn;

	page->entry_in_cert_file      = entry_in_cert_file;
	page->entry_in_cert_pass      = entry_in_cert_pass;
	page->entry_out_cert_file     = entry_out_cert_file;
	page->entry_out_cert_pass     = entry_out_cert_pass;

	page->ssl_certs_auto_accept_checkbtn = ssl_certs_auto_accept_checkbtn;
	page->use_nonblocking_ssl_checkbtn = use_nonblocking_ssl_checkbtn;

	tmp_ac_prefs = *ac_prefs;

	if (new_account) {
		prefs_set_dialog_to_default(ssl_param);
	} else {
		prefs_set_dialog(ssl_param);

		/* Passwords are handled outside of PrefParams. */
		buf = passwd_store_get_account(ac_prefs->account_id,
				PWS_ACCOUNT_RECV_CERT);
		gtk_entry_set_text(GTK_ENTRY(page->entry_in_cert_pass), buf != NULL ? buf : "");
		if (buf != NULL) {
			memset(buf, 0, strlen(buf));
			g_free(buf);
		}

		buf = passwd_store_get_account(ac_prefs->account_id,
				PWS_ACCOUNT_SEND_CERT);
		gtk_entry_set_text(GTK_ENTRY(page->entry_out_cert_pass), buf != NULL ? buf : "");
		if (buf != NULL) {
			memset(buf, 0, strlen(buf));
			g_free(buf);
		}
	}

	page->vbox = vbox1;

	page->page.widget = vbox1;
}

#undef CREATE_RADIO_BUTTONS
#endif /* USE_GNUTLS */

static void proxy_create_widget_func(PrefsPage * _page,
                                           GtkWindow * window,
                                           gpointer data)
{
	ProxyPage *page = (ProxyPage *) _page;
	PrefsAccount *ac_prefs = (PrefsAccount *) data;
	GtkWidget *vbox1, *vbox2, *vbox3, *vbox4;
	GtkWidget *proxy_frame;
	GtkWidget *proxy_checkbtn;
	GtkWidget *default_proxy_checkbtn;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *socks4_radiobtn;
	GtkWidget *socks5_radiobtn;
	GtkWidget *proxy_host_entry;
	GtkWidget *proxy_port_spinbtn;
	GtkWidget *proxy_auth_checkbtn;
	GtkWidget *proxy_name_entry;
	GtkWidget *proxy_pass_entry;
	GtkWidget *proxy_send_checkbtn;
	GtkWidget *table;
	GtkWidget *button;
	gchar *buf;

	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	proxy_checkbtn = gtk_check_button_new_with_label (_("Use proxy server"));
	PACK_FRAME (vbox1, proxy_frame, NULL);
	gtk_frame_set_label_widget (GTK_FRAME(proxy_frame), proxy_checkbtn);

	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING_NARROW);
	gtk_container_add (GTK_CONTAINER (proxy_frame), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);

	default_proxy_checkbtn =
		gtk_check_button_new_with_label (C_("In account preferences, referring to whether or not use proxy settings from common preferences", "Use default settings"));
	CLAWS_SET_TIP(default_proxy_checkbtn,
			_("Use global proxy server settings"));
	gtk_box_pack_start (GTK_BOX (vbox2), default_proxy_checkbtn, FALSE, FALSE, 0);

	vbox3 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING_NARROW);
	gtk_box_pack_start (GTK_BOX (vbox2), vbox3, FALSE, FALSE, 0);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start (GTK_BOX (vbox3), hbox, FALSE, FALSE, 0);

	socks4_radiobtn = gtk_radio_button_new_with_label(NULL, "SOCKS4");
	gtk_box_pack_start (GTK_BOX (hbox), socks4_radiobtn, FALSE, FALSE, 0);
	g_object_set_data(G_OBJECT(socks4_radiobtn), MENU_VAL_ID,
			  GINT_TO_POINTER(PROXY_SOCKS4));

	CREATE_RADIO_BUTTON(hbox, socks5_radiobtn, socks4_radiobtn, "SOCKS5",
			    PROXY_SOCKS5);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start (GTK_BOX (vbox3), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Hostname"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	proxy_host_entry = gtk_entry_new();
	gtk_widget_set_size_request(proxy_host_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_box_pack_start(GTK_BOX(hbox), proxy_host_entry, TRUE, TRUE, 0);

	label = gtk_label_new(_("Port"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	proxy_port_spinbtn = gtk_spin_button_new_with_range(0, 65535, 1080);
	gtk_widget_set_size_request(proxy_port_spinbtn, 64, -1);
	gtk_box_pack_start(GTK_BOX(hbox), proxy_port_spinbtn, FALSE, FALSE, 0);

	vbox4 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING_NARROW);
	gtk_box_pack_start(GTK_BOX(vbox3), vbox4, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (vbox4, proxy_auth_checkbtn, _("Use authentication"));

	table = gtk_grid_new();
	gtk_box_pack_start (GTK_BOX (vbox4), table, FALSE, FALSE, 0);

	label = gtk_label_new(_("Username"));
	gtk_grid_attach(GTK_GRID(table), label, 0, 0, 1, 1);

	proxy_name_entry = gtk_entry_new();
	gtk_widget_set_size_request(proxy_name_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_grid_attach(GTK_GRID(table), proxy_name_entry, 1, 0, 1, 1);
	gtk_widget_set_hexpand(proxy_name_entry, TRUE);
	gtk_widget_set_halign(proxy_name_entry, GTK_ALIGN_FILL);

	label = gtk_label_new(_("Password"));
	gtk_grid_attach(GTK_GRID(table), label, 2, 0, 1, 1);

	proxy_pass_entry = gtk_entry_new();
	gtk_widget_set_size_request(proxy_pass_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_entry_set_visibility(GTK_ENTRY(proxy_pass_entry), FALSE);
	gtk_grid_attach(GTK_GRID(table), proxy_pass_entry, 3, 0, 1, 1);
	gtk_widget_set_hexpand(proxy_pass_entry, TRUE);
	gtk_widget_set_halign(proxy_pass_entry, GTK_ALIGN_FILL);

	button = gtk_check_button_new_with_label(_("Show password"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
	gtk_grid_attach(GTK_GRID(table), button, 3, 1, 1, 1);

	g_signal_connect(G_OBJECT(button), "toggled",
			G_CALLBACK(prefs_account_showpwd_checkbtn_toggled), proxy_pass_entry);

	gtk_box_pack_start(GTK_BOX(vbox2), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);

	PACK_CHECK_BUTTON(vbox2, proxy_send_checkbtn,
			  _("Use proxy server for sending"));
	CLAWS_SET_TIP(proxy_send_checkbtn,
			_("If disabled, messages will be sent using direct connection to configured outgoing server, bypassing any configured proxy server."));

	SET_TOGGLE_SENSITIVITY(proxy_auth_checkbtn, table);
	SET_TOGGLE_SENSITIVITY(socks5_radiobtn, vbox4);
	SET_TOGGLE_SENSITIVITY(proxy_checkbtn, vbox2);
	SET_TOGGLE_SENSITIVITY_REVERSE(default_proxy_checkbtn, vbox3);

	gtk_widget_show_all(vbox1);

	page->proxy_checkbtn = proxy_checkbtn;
	page->default_proxy_checkbtn = default_proxy_checkbtn;
	page->socks4_radiobtn = socks4_radiobtn;
	page->socks5_radiobtn = socks5_radiobtn;
	page->proxy_host_entry = proxy_host_entry;
	page->proxy_port_spinbtn = proxy_port_spinbtn;
	page->proxy_auth_checkbtn = proxy_auth_checkbtn;
	page->proxy_name_entry = proxy_name_entry;
	page->proxy_pass_entry = proxy_pass_entry;
	page->proxy_send_checkbtn = proxy_send_checkbtn;
	page->vbox = vbox1;
	page->page.widget = vbox1;

	tmp_ac_prefs = *ac_prefs;

	if (new_account) {
		prefs_set_dialog_to_default(proxy_param);
	} else {
		prefs_set_dialog(proxy_param);

		/* Passwords are handled outside of PrefParams. */
		buf = passwd_store_get_account(ac_prefs->account_id,
				PWS_ACCOUNT_PROXY_PASS);
		gtk_entry_set_text(GTK_ENTRY(page->proxy_pass_entry),
				buf != NULL ? buf : "");
		if (buf != NULL) {
			memset(buf, 0, strlen(buf));
			g_free(buf);
		}
	}

	page->vbox = vbox1;

	page->page.widget = vbox1;
}

static void advanced_create_widget_func(PrefsPage * _page,
                                           GtkWindow * window,
                                           gpointer data)
{
	AdvancedPage *page = (AdvancedPage *) _page;
	PrefsAccount *ac_prefs = (PrefsAccount *) data;

	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *hbox1;
	GtkWidget *checkbtn_smtpport;
	GtkWidget *spinbtn_smtpport;
	GtkWidget *hbox_popport;
	GtkWidget *checkbtn_popport;
	GtkWidget *spinbtn_popport;
	GtkWidget *hbox_imapport;
	GtkWidget *checkbtn_imapport;
	GtkWidget *spinbtn_imapport;
	GtkWidget *hbox_nntpport;
	GtkWidget *checkbtn_nntpport;
	GtkWidget *spinbtn_nntpport;
	GtkWidget *checkbtn_domain;
	GtkWidget *entry_domain;
	gchar *tip_domain;
	GtkWidget *checkbtn_crosspost;
 	GtkWidget *colormenu_crosspost;
#ifndef G_OS_WIN32
	GtkWidget *checkbtn_tunnelcmd;
	GtkWidget *entry_tunnelcmd;
#endif
	GtkWidget *folder_frame;
	GtkWidget *vbox3;
	GtkWidget *table;
	GtkWidget *sent_folder_checkbtn;
	GtkWidget *sent_folder_entry;
	GtkWidget *queue_folder_checkbtn;
	GtkWidget *queue_folder_entry;
	GtkWidget *draft_folder_checkbtn;
	GtkWidget *draft_folder_entry;
	GtkWidget *trash_folder_checkbtn;
	GtkWidget *trash_folder_entry;
	GtkSizeGroup *size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

#define PACK_HBOX(hbox) \
	{ \
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8); \
	gtk_widget_show (hbox); \
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0); \
	}

#define PACK_PORT_SPINBTN(box, spinbtn) \
	{ \
	spinbtn = gtk_spin_button_new_with_range(0, 65535, 1); \
	gtk_widget_show (spinbtn); \
	gtk_box_pack_start (GTK_BOX (box), spinbtn, FALSE, FALSE, 0); \
	}

	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING_NARROW_2);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	PACK_HBOX (hbox1);
	PACK_CHECK_BUTTON (hbox1, checkbtn_smtpport, _("SMTP port"));
	PACK_PORT_SPINBTN (hbox1, spinbtn_smtpport);
	SET_TOGGLE_SENSITIVITY (checkbtn_smtpport, spinbtn_smtpport);
	gtk_size_group_add_widget(size_group, checkbtn_smtpport);
	
	PACK_HBOX (hbox_popport);
	PACK_CHECK_BUTTON (hbox_popport, checkbtn_popport,
			   _("POP port"));
	PACK_PORT_SPINBTN (hbox_popport, spinbtn_popport);
	SET_TOGGLE_SENSITIVITY (checkbtn_popport, spinbtn_popport);
	gtk_size_group_add_widget(size_group, checkbtn_popport);

	PACK_HBOX (hbox_imapport);
	PACK_CHECK_BUTTON (hbox_imapport, checkbtn_imapport,
			   _("IMAP port"));
	PACK_PORT_SPINBTN (hbox_imapport, spinbtn_imapport);
	SET_TOGGLE_SENSITIVITY (checkbtn_imapport, spinbtn_imapport);
	gtk_size_group_add_widget(size_group, checkbtn_imapport);

	PACK_HBOX (hbox_nntpport);
	PACK_CHECK_BUTTON (hbox_nntpport, checkbtn_nntpport,
			   _("NNTP port"));
	PACK_PORT_SPINBTN (hbox_nntpport, spinbtn_nntpport);
	SET_TOGGLE_SENSITIVITY (checkbtn_nntpport, spinbtn_nntpport);
	gtk_size_group_add_widget(size_group, checkbtn_nntpport);

	PACK_HBOX (hbox1);
	PACK_CHECK_BUTTON (hbox1, checkbtn_domain, _("Domain name"));
	gtk_size_group_add_widget(size_group, checkbtn_domain);	

	tip_domain = _("The domain name will be used in the generated "
			"Message-ID, and when connecting to SMTP servers.");

	CLAWS_SET_TIP(checkbtn_domain, tip_domain);

	entry_domain = gtk_entry_new ();
	gtk_widget_show (entry_domain);
	gtk_box_pack_start (GTK_BOX (hbox1), entry_domain, TRUE, TRUE, 0);
	SET_TOGGLE_SENSITIVITY (checkbtn_domain, entry_domain);
	CLAWS_SET_TIP(entry_domain, tip_domain);

#ifndef G_OS_WIN32	
	PACK_HBOX (hbox1);
	PACK_CHECK_BUTTON (hbox1, checkbtn_tunnelcmd,
			   _("Use command to communicate with server"));
	entry_tunnelcmd = gtk_entry_new ();
	gtk_widget_show (entry_tunnelcmd);
	gtk_box_pack_start (GTK_BOX (hbox1), entry_tunnelcmd, TRUE, TRUE, 0);
	SET_TOGGLE_SENSITIVITY (checkbtn_tunnelcmd, entry_tunnelcmd);
#endif

	PACK_HBOX (hbox1);
	PACK_CHECK_BUTTON (hbox1, checkbtn_crosspost, 
			   _("Mark cross-posted messages as read and color:"));
	g_signal_connect (G_OBJECT (checkbtn_crosspost), "toggled",
			  G_CALLBACK (crosspost_color_toggled),
			  NULL);

	colormenu_crosspost = colorlabel_create_combobox_colormenu();
	gtk_widget_show (colormenu_crosspost);
	gtk_box_pack_start (GTK_BOX (hbox1), colormenu_crosspost, FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY(checkbtn_crosspost, colormenu_crosspost);
#undef PACK_HBOX
#undef PACK_PORT_SPINBTN

	/* special folder setting (maybe these options are redundant) */

	vbox3 = gtkut_get_options_frame(vbox1, &folder_frame, _("Folder"));

	table = gtk_grid_new();
	gtk_widget_show (table);
	gtk_container_add (GTK_CONTAINER (vbox3), table);
	gtk_grid_set_row_spacing(GTK_GRID(table), VSPACING_NARROW_2);
	gtk_grid_set_column_spacing(GTK_GRID(table), 4);

#define SET_CHECK_BTN_AND_ENTRY(label, checkbtn, entry, n)		\
{									\
	GtkWidget *button;						\
									\
	checkbtn = gtk_check_button_new_with_label (label);		\
	gtk_widget_show (checkbtn);					\
	gtk_grid_attach(GTK_GRID(table), checkbtn, 0, n, 1, 1);		\
									\
	entry = gtk_entry_new ();					\
	gtk_widget_show (entry);					\
	gtk_grid_attach(GTK_GRID(table), entry, 1, n, 1, 1);		\
	gtk_widget_set_hexpand(entry, TRUE);				\
	gtk_widget_set_halign(entry, GTK_ALIGN_FILL);			\
									\
	button = gtkut_get_browse_file_btn(_("Browse"));		\
	gtk_widget_show (button);					\
	gtk_grid_attach(GTK_GRID(table), button, 2, n, 1, 1);		\
	g_signal_connect						\
		(G_OBJECT (button), "clicked",				\
		 G_CALLBACK (prefs_account_select_folder_cb),		\
		 entry);						\
									\
	SET_TOGGLE_SENSITIVITY (checkbtn, entry);			\
	SET_TOGGLE_SENSITIVITY (checkbtn, button);			\
}

	SET_CHECK_BTN_AND_ENTRY(_("Put sent messages in"),
				sent_folder_checkbtn, sent_folder_entry, 0);
	SET_CHECK_BTN_AND_ENTRY(_("Put queued messages in"),
				queue_folder_checkbtn, queue_folder_entry, 1);
	SET_CHECK_BTN_AND_ENTRY(_("Put draft messages in"),
				draft_folder_checkbtn, draft_folder_entry, 2);
	SET_CHECK_BTN_AND_ENTRY(_("Put deleted messages in"),
				trash_folder_checkbtn, trash_folder_entry, 3);

	page->smtpport_checkbtn	= checkbtn_smtpport;
	page->smtpport_spinbtn		= spinbtn_smtpport;
	page->popport_hbox		= hbox_popport;
	page->popport_checkbtn		= checkbtn_popport;
	page->popport_spinbtn		= spinbtn_popport;
	page->imapport_hbox		= hbox_imapport;
	page->imapport_checkbtn	= checkbtn_imapport;
	page->imapport_spinbtn		= spinbtn_imapport;
	page->nntpport_hbox		= hbox_nntpport;
	page->nntpport_checkbtn	= checkbtn_nntpport;
	page->nntpport_spinbtn		= spinbtn_nntpport;
	page->domain_checkbtn		= checkbtn_domain;
	page->domain_entry		= entry_domain;
 	page->crosspost_checkbtn	= checkbtn_crosspost;
 	page->crosspost_colormenu	= colormenu_crosspost;

#ifndef G_OS_WIN32
	page->tunnelcmd_checkbtn	= checkbtn_tunnelcmd;
	page->tunnelcmd_entry	= entry_tunnelcmd;
#endif
	page->sent_folder_checkbtn  = sent_folder_checkbtn;
	page->sent_folder_entry   = sent_folder_entry;
	page->queue_folder_checkbtn  = queue_folder_checkbtn;
	page->queue_folder_entry   = queue_folder_entry;
	page->draft_folder_checkbtn = draft_folder_checkbtn;
	page->draft_folder_entry  = draft_folder_entry;
	page->trash_folder_checkbtn = trash_folder_checkbtn;
	page->trash_folder_entry  = trash_folder_entry;

	tmp_ac_prefs = *ac_prefs;

	if (new_account) {
		prefs_set_dialog_to_default(advanced_param);
	} else
		prefs_set_dialog(advanced_param);

	page->vbox = vbox1;

	page->page.widget = vbox1;
	
	g_object_unref(G_OBJECT(size_group));
}
	
static gint prefs_basic_apply(void)
{
	RecvProtocol protocol;
	gchar *old_id = NULL;
	gchar *new_id = NULL;
	struct BasicProtocol *protocol_optmenu = (struct BasicProtocol *) basic_page.protocol_optmenu;
	GtkWidget *optmenu = protocol_optmenu->combobox;

	protocol = combobox_get_active_data(GTK_COMBO_BOX(optmenu));

	if (*gtk_entry_get_text(GTK_ENTRY(basic_page.acname_entry)) == '\0') {
		alertpanel_error(_("Account name is not entered."));
		return -1;
	}
	if (*gtk_entry_get_text(GTK_ENTRY(basic_page.addr_entry)) == '\0') {
		alertpanel_error(_("Mail address is not entered."));
		return -1;
	}

	if (((protocol == A_POP3) || 
	     (protocol == A_LOCAL && !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(basic_page.mailcmd_checkbtn))) || 
	     (protocol == A_NONE)) &&
           *gtk_entry_get_text(GTK_ENTRY(basic_page.smtpserv_entry)) == '\0') {
		alertpanel_error(_("SMTP server is not entered."));
		return -1;
	}
	if ((protocol == A_POP3 || protocol == A_IMAP4) &&
	    *gtk_entry_get_text(GTK_ENTRY(basic_page.uid_entry)) == '\0') {
		alertpanel_error(_("User ID is not entered."));
		return -1;
	}
	if (protocol == A_POP3 &&
	    *gtk_entry_get_text(GTK_ENTRY(basic_page.recvserv_entry)) == '\0') {
		alertpanel_error(_("POP server is not entered."));
		return -1;
	}
	if (protocol == A_POP3 || protocol == A_LOCAL) {
		GtkWidget *inbox_entry = (protocol == A_POP3 ? receive_page.inbox_entry : receive_page.local_inbox_entry );
		const gchar *mailbox = gtk_entry_get_text(GTK_ENTRY(inbox_entry));
		FolderItem *inbox =  folder_find_item_from_identifier(mailbox);
		if (!inbox) {
			gchar *id = NULL;
			setup_write_mailbox_path(mainwindow_get_mainwindow(), "Mail");
			id = folder_item_get_identifier(folder_get_default_inbox_for_class(F_MH));
			gtk_entry_set_text(GTK_ENTRY(receive_page.inbox_entry),
				id);
			gtk_entry_set_text(GTK_ENTRY(receive_page.local_inbox_entry),
				id);
			g_free(id);
			mailbox = gtk_entry_get_text(GTK_ENTRY(inbox_entry));
			inbox =  folder_find_item_from_identifier(mailbox);
		}
	    	if (inbox == NULL) {
			alertpanel_error(_("The default Inbox folder doesn't exist."));
			return -1;
		}
	}
	if (protocol == A_IMAP4 &&
	    *gtk_entry_get_text(GTK_ENTRY(basic_page.recvserv_entry)) == '\0') {
		alertpanel_error(_("IMAP server is not entered."));
		return -1;
	}
	if (protocol == A_NNTP &&
	    *gtk_entry_get_text(GTK_ENTRY(basic_page.nntpserv_entry)) == '\0') {
		alertpanel_error(_("NNTP server is not entered."));
		return -1;
	}

	if (protocol == A_LOCAL &&
	    *gtk_entry_get_text(GTK_ENTRY(basic_page.localmbox_entry)) == '\0') {
		alertpanel_error(_("local mailbox filename is not entered."));
		return -1;
	}

	if (protocol == A_LOCAL &&
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(basic_page.mailcmd_checkbtn)) && *gtk_entry_get_text(GTK_ENTRY(basic_page.mailcmd_entry)) == '\0') {
		alertpanel_error(_("mail command is not entered."));
		return -1;
	}
	
	if (protocol == A_IMAP4 || protocol == A_NNTP) 
		old_id = g_strdup_printf("#%s/%s",
				protocol == A_IMAP4 ? "imap":"news",
				tmp_ac_prefs.account_name ? tmp_ac_prefs.account_name : "(null)");
	
	if (strchr(gtk_entry_get_text(GTK_ENTRY(basic_page.uid_entry)), '\n') != NULL) {
		alertpanel_error(_("User ID cannot contain a newline character."));
		if (old_id)
			g_free(old_id);
		return -1;
	}

	if (strchr(gtk_entry_get_text(GTK_ENTRY(basic_page.pass_entry)), '\n') != NULL) {
		alertpanel_error(_("Password cannot contain a newline character."));
		if (old_id)
			g_free(old_id);
		return -1;
	}

	prefs_set_data_from_dialog(basic_param);

	/* Passwords are stored outside of PrefParams. */
	passwd_store_set_account(tmp_ac_prefs.account_id,
			PWS_ACCOUNT_RECV,
			gtk_entry_get_text(GTK_ENTRY(basic_page.pass_entry)),
			FALSE);

#ifdef USE_OAUTH2
	/* Manual password change - reset expiry on OAUTH2 tokens*/
	passwd_store_set_account(tmp_ac_prefs.account_id, PWS_ACCOUNT_OAUTH2_EXPIRY, "0", FALSE);
#endif

	if (protocol == A_IMAP4 || protocol == A_NNTP) {
		new_id = g_strdup_printf("#%s/%s",
				protocol == A_IMAP4 ? "imap":"news",
				tmp_ac_prefs.account_name);
		if (old_id != NULL && new_id != NULL)
			prefs_filtering_rename_path(old_id, new_id);
		if (old_id)
			g_free(old_id);
		if (new_id)
			g_free(new_id);
	}
	
	return 0;
}

static gint prefs_receive_apply(void)
{
	if (strchr(gtk_entry_get_text(GTK_ENTRY(send_page.smtp_uid_entry)), '\n') != NULL) {
		alertpanel_error(_("SMTP user ID cannot contain a newline character."));
		return -1;
	}

	if (strchr(gtk_entry_get_text(GTK_ENTRY(send_page.smtp_pass_entry)), '\n') != NULL) {
		alertpanel_error(_("SMTP password cannot contain a newline character."));
		return -1;
	}

	prefs_set_data_from_dialog(receive_param);

	return 0;
}

static gint prefs_send_apply(void)
{
	prefs_set_data_from_dialog(send_param);

	/* Passwords are stored outside of PrefParams. */
	passwd_store_set_account(tmp_ac_prefs.account_id,
			PWS_ACCOUNT_SEND,
			gtk_entry_get_text(GTK_ENTRY(send_page.smtp_pass_entry)),
			FALSE);
#ifdef USE_OAUTH2
	/* Manual password change - reset expiry on OAUTH2 tokens*/
	passwd_store_set_account(tmp_ac_prefs.account_id, PWS_ACCOUNT_OAUTH2_EXPIRY, "0", FALSE);
#endif

	return 0;
}

#ifdef USE_OAUTH2
static gint prefs_oauth2_apply(void)
{
	prefs_set_data_from_dialog(oauth2_param);

	/* Passwords are stored outside of PrefParams. */
	passwd_store_set_account(tmp_ac_prefs.account_id,
			PWS_ACCOUNT_OAUTH2_AUTH,
			gtk_entry_get_text(GTK_ENTRY(oauth2_page.oauth2_authcode_entry)),
			FALSE);

 	return 0;
}
#endif

static gint prefs_compose_apply(void)
{
	prefs_set_data_from_dialog(compose_param);
	return 0;
}

static gint prefs_templates_apply(void)
{
	prefs_set_data_from_dialog(templates_param);
	return 0;
}

static gint prefs_privacy_apply(void)
{
	prefs_set_data_from_dialog(privacy_param);
	return 0;
}

#ifdef USE_GNUTLS
static gint prefs_ssl_apply(void)
{
	prefs_set_data_from_dialog(ssl_param);

	/* Passwords are stored outside of PrefParams. */
	passwd_store_set_account(tmp_ac_prefs.account_id,
			PWS_ACCOUNT_RECV_CERT,
			gtk_entry_get_text(GTK_ENTRY(ssl_page.entry_in_cert_pass)),
			FALSE);
	passwd_store_set_account(tmp_ac_prefs.account_id,
			PWS_ACCOUNT_SEND_CERT,
			gtk_entry_get_text(GTK_ENTRY(ssl_page.entry_out_cert_pass)),
			FALSE);

	return 0;
}
#endif

static gint prefs_proxy_apply(void)
{
	prefs_set_data_from_dialog(proxy_param);

	/* Passwords are stored outside of PrefParams. */
	passwd_store_set_account(tmp_ac_prefs.account_id,
			PWS_ACCOUNT_PROXY_PASS,
			gtk_entry_get_text(GTK_ENTRY(proxy_page.proxy_pass_entry)),
			FALSE);

	return 0;
}

static gint prefs_advanced_apply(void)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(advanced_page.domain_checkbtn)) &&
	    *gtk_entry_get_text(GTK_ENTRY(advanced_page.domain_entry)) == '\0') {
		alertpanel_error(_("domain is not specified."));
		return -1;
	}
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(advanced_page.sent_folder_checkbtn)) &&
	    *gtk_entry_get_text(GTK_ENTRY(advanced_page.sent_folder_entry)) == '\0') {
		alertpanel_error(_("sent folder is not selected."));
		return -1;
	}
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(advanced_page.queue_folder_checkbtn)) &&
	    *gtk_entry_get_text(GTK_ENTRY(advanced_page.queue_folder_entry)) == '\0') {
		alertpanel_error(_("queue folder is not selected."));
		return -1;
	}
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(advanced_page.draft_folder_checkbtn)) &&
	    *gtk_entry_get_text(GTK_ENTRY(advanced_page.draft_folder_entry)) == '\0') {
		alertpanel_error(_("draft folder is not selected."));
		return -1;
	}
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(advanced_page.trash_folder_checkbtn)) &&
	    *gtk_entry_get_text(GTK_ENTRY(advanced_page.trash_folder_entry)) == '\0') {
		alertpanel_error(_("trash folder is not selected."));
		return -1;
	}

	prefs_set_data_from_dialog(advanced_param);
	return 0;
}

static void basic_destroy_widget_func(PrefsPage *_page)
{
	/* BasicPage *page = (BasicPage *) _page; */
}

static void receive_destroy_widget_func(PrefsPage *_page)
{
	/* ReceivePage *page = (ReceivePage *) _page; */
}

static void send_destroy_widget_func(PrefsPage *_page)
{
	/* SendPage *page = (SendPage *) _page; */
}

#ifdef USE_OAUTH2
static void oauth2_destroy_widget_func(PrefsPage *_page)
{
	/* Oauth2Page *page = (Oauth2Page *) _page; */

	if(oauth2_listener_task){
		debug_print("Closing oauth2 listener task\n");
		oauth2_listener_cancel = 1;
	}
}
#endif

static void compose_destroy_widget_func(PrefsPage *_page)
{
	/* ComposePage *page = (ComposePage *) _page; */
}

static void templates_destroy_widget_func(PrefsPage *_page)
{
	/* TemplatesPage *page = (TemplatesPage *) _page; */
}

static void privacy_destroy_widget_func(PrefsPage *_page)
{
	/* PrivacyPage *page = (PrivacyPage *) _page; */
}

#ifdef USE_GNUTLS
static void ssl_destroy_widget_func(PrefsPage *_page)
{
	/* SSLPage *page = (SSLPage *) _page; */
}
#endif

static void proxy_destroy_widget_func(PrefsPage *_page)
{
	/* ProxyPage *page = (ProxyPage *) _page; */
}

static void advanced_destroy_widget_func(PrefsPage *_page)
{
	/* AdvancedPage *page = (AdvancedPage *) _page; */
}

static gboolean basic_can_close_func(PrefsPage *_page)
{	
	BasicPage *page = (BasicPage *) _page;

	if (!page->page.page_open)
		return TRUE;

	return prefs_basic_apply() >= 0;
}

static gboolean receive_can_close_func(PrefsPage *_page)
{	
	ReceivePage *page = (ReceivePage *) _page;

	if (!page->page.page_open)
		return TRUE;

	return prefs_receive_apply() >= 0;
}

static gboolean send_can_close_func(PrefsPage *_page)
{	
	SendPage *page = (SendPage *) _page;

	if (!page->page.page_open)
		return TRUE;

	return prefs_send_apply() >= 0;
}

#ifdef USE_OAUTH2
static gboolean oauth2_can_close_func(PrefsPage *_page)
{	
	Oauth2Page *page = (Oauth2Page *) _page;

	if (!page->page.page_open)
		return TRUE;

	return prefs_oauth2_apply() >= 0;
}
#endif

static gboolean compose_can_close_func(PrefsPage *_page)
{	
	ComposePage *page = (ComposePage *) _page;

	if (!page->page.page_open)
		return TRUE;

	return prefs_compose_apply() >= 0;
}

static gboolean templates_can_close_func(PrefsPage *_page)
{
	TemplatesPage *page = (TemplatesPage *) _page;

	if (!page->page.page_open)
		return TRUE;

	return prefs_templates_apply() >= 0;
}

static gboolean privacy_can_close_func(PrefsPage *_page)
{
	PrivacyPage *page = (PrivacyPage *) _page;

	if (!page->page.page_open)
		return TRUE;

	return prefs_privacy_apply() >= 0;
}

#ifdef USE_GNUTLS
static gboolean ssl_can_close_func(PrefsPage *_page)
{
	SSLPage *page = (SSLPage *) _page;

	if (!page->page.page_open)
		return TRUE;

	return prefs_ssl_apply() >= 0;
}
#endif

static gboolean proxy_can_close_func(PrefsPage *_page)
{
	ProxyPage *page = (ProxyPage *) _page;

	if (!page->page.page_open)
		return TRUE;

	return prefs_proxy_apply() >= 0;
}

static gboolean advanced_can_close_func(PrefsPage *_page)
{
	AdvancedPage *page = (AdvancedPage *) _page;

	if (!page->page.page_open)
		return TRUE;

	return prefs_advanced_apply() >= 0;
}

static void basic_save_func(PrefsPage *_page)
{
	BasicPage *page = (BasicPage *) _page;

	if (!page->page.page_open)
		return;

	if (prefs_basic_apply() >= 0)
		cancelled = FALSE;
}

static void receive_save_func(PrefsPage *_page)
{
	ReceivePage *page = (ReceivePage *) _page;

	if (!page->page.page_open)
		return;

	if (prefs_receive_apply() >= 0)
		cancelled = FALSE;
}

static void send_save_func(PrefsPage *_page)
{
	SendPage *page = (SendPage *) _page;

	if (!page->page.page_open)
		return;

	if (prefs_send_apply() >= 0)
		cancelled = FALSE;
}

#ifdef USE_OAUTH2
static void oauth2_save_func(PrefsPage *_page)
{
	Oauth2Page *page = (Oauth2Page *) _page;

	if (!page->page.page_open)
		return;

	if (prefs_oauth2_apply() >= 0)
		cancelled = FALSE;
}
#endif

static void compose_save_func(PrefsPage *_page)
{
	ComposePage *page = (ComposePage *) _page;

	if (!page->page.page_open)
		return;

	if (prefs_compose_apply() >= 0)
		cancelled = FALSE;
}

static void templates_save_func(PrefsPage *_page)
{
	TemplatesPage *page = (TemplatesPage *) _page;

	if (!page->page.page_open)
		return;

	quotefmt_check_new_msg_formats(tmp_ac_prefs.compose_with_format,
									NULL,
									tmp_ac_prefs.compose_subject_format,
									tmp_ac_prefs.compose_body_format);
	quotefmt_check_reply_formats(tmp_ac_prefs.reply_with_format,
									NULL,
									tmp_ac_prefs.reply_quotemark,
									tmp_ac_prefs.reply_body_format);
	quotefmt_check_forward_formats(tmp_ac_prefs.forward_with_format,
									NULL,
									tmp_ac_prefs.forward_quotemark,
									tmp_ac_prefs.forward_body_format);
	if (prefs_templates_apply() >= 0)
		cancelled = FALSE;
}

static void privacy_save_func(PrefsPage *_page)
{
	PrivacyPage *page = (PrivacyPage *) _page;

	if (!page->page.page_open)
		return;

	if (prefs_privacy_apply() >= 0)
		cancelled = FALSE;
}

#ifdef USE_GNUTLS
static void ssl_save_func(PrefsPage *_page)
{
	SSLPage *page = (SSLPage *) _page;

	if (!page->page.page_open)
		return;

	if (prefs_ssl_apply() >= 0)
		cancelled = FALSE;
}
#endif

static void proxy_save_func(PrefsPage *_page)
{
	ProxyPage *page = (ProxyPage *) _page;

	if (!page->page.page_open)
		return;

	if (prefs_proxy_apply() >= 0)
		cancelled = FALSE;
}

static void advanced_save_func(PrefsPage *_page)
{
	AdvancedPage *page = (AdvancedPage *) _page;

	if (!page->page.page_open)
		return;

	if (prefs_advanced_apply() >= 0)
		cancelled = FALSE;
}

static void register_basic_page(void)
{
	static gchar *path[3];

	path[0] = _("Account");
	path[1] = _("Basic");
	path[2] = NULL;
        
	basic_page.page.path = path;
	basic_page.page.weight = 1000.0;
	basic_page.page.create_widget = basic_create_widget_func;
	basic_page.page.destroy_widget = basic_destroy_widget_func;
	basic_page.page.save_page = basic_save_func;
	basic_page.page.can_close = basic_can_close_func;

	prefs_account_register_page((PrefsPage *) &basic_page);
}

static void register_receive_page(void)
{
	static gchar *path[3];

	path[0] = _("Account");
	path[1] = _("Receive");
	path[2] = NULL;
        
	receive_page.page.path = path;
	receive_page.page.weight = 1000.0;
	receive_page.page.create_widget = receive_create_widget_func;
	receive_page.page.destroy_widget = receive_destroy_widget_func;
	receive_page.page.save_page = receive_save_func;
	receive_page.page.can_close = receive_can_close_func;

	prefs_account_register_page((PrefsPage *) &receive_page);
}

static void register_send_page(void)
{
	static gchar *path[3];

	path[0] = _("Account");
	path[1] = (gchar *) C_("Preferences menu item", "Send");
	path[2] = NULL;
        
	send_page.page.path = path;
	send_page.page.weight = 1000.0;
	send_page.page.create_widget = send_create_widget_func;
	send_page.page.destroy_widget = send_destroy_widget_func;
	send_page.page.save_page = send_save_func;
	send_page.page.can_close = send_can_close_func;

	prefs_account_register_page((PrefsPage *) &send_page);
}

#ifdef USE_OAUTH2
static void register_oauth2_page(void)
{
	static gchar *path[3];

	path[0] = _("Account");
	path[1] = _("OAuth2");
	path[2] = NULL;
        
	oauth2_page.page.path = path;
	oauth2_page.page.weight = 1000.0;
	oauth2_page.page.create_widget = oauth2_create_widget_func;
	oauth2_page.page.destroy_widget = oauth2_destroy_widget_func;
	oauth2_page.page.save_page = oauth2_save_func;
	oauth2_page.page.can_close = oauth2_can_close_func;
	  
	prefs_account_register_page((PrefsPage *) &oauth2_page);
}
#endif

static void register_compose_page(void)
{
	static gchar *path[3];

	path[0] = _("Account");
	path[1] = _("Compose");
	path[2] = NULL;
        
	compose_page.page.path = path;
	compose_page.page.weight = 1000.0;
	compose_page.page.create_widget = compose_create_widget_func;
	compose_page.page.destroy_widget = compose_destroy_widget_func;
	compose_page.page.save_page = compose_save_func;
	compose_page.page.can_close = compose_can_close_func;

	prefs_account_register_page((PrefsPage *) &compose_page);
}

static void register_templates_page(void)
{
	static gchar *path[3];

	path[0] = _("Account");
	path[1] = _("Templates");
	path[2] = NULL;
        
	templates_page.page.path = path;
	templates_page.page.weight = 1000.0;
	templates_page.page.create_widget = templates_create_widget_func;
	templates_page.page.destroy_widget = templates_destroy_widget_func;
	templates_page.page.save_page = templates_save_func;
	templates_page.page.can_close = templates_can_close_func;

	prefs_account_register_page((PrefsPage *) &templates_page);
}

static void register_privacy_page(void)
{
	static gchar *path[3];

	path[0] = _("Account");
	path[1] = _("Privacy");
	path[2] = NULL;
        
	privacy_page.page.path = path;
	privacy_page.page.weight = 1000.0;
	privacy_page.page.create_widget = privacy_create_widget_func;
	privacy_page.page.destroy_widget = privacy_destroy_widget_func;
	privacy_page.page.save_page = privacy_save_func;
	privacy_page.page.can_close = privacy_can_close_func;

	prefs_account_register_page((PrefsPage *) &privacy_page);
}

#ifdef USE_GNUTLS
static void register_ssl_page(void)
{
	static gchar *path[3];

	path[0] = _("Account");
	path[1] = _("TLS");
	path[2] = NULL;
        
	ssl_page.page.path = path;
	ssl_page.page.weight = 1000.0;
	ssl_page.page.create_widget = ssl_create_widget_func;
	ssl_page.page.destroy_widget = ssl_destroy_widget_func;
	ssl_page.page.save_page = ssl_save_func;
	ssl_page.page.can_close = ssl_can_close_func;

	prefs_account_register_page((PrefsPage *) &ssl_page);
}

static gboolean sslcert_get_client_cert_hook(gpointer source, gpointer data)
{
	SSLClientCertHookData *hookdata = (SSLClientCertHookData *)source;
	PrefsAccount *account = (PrefsAccount *)hookdata->account;
	gchar *pwd_id;

	hookdata->cert_path = NULL;
	hookdata->password = NULL;

	if (!g_list_find(account_get_list(), account)) {
		g_warning("can't find account");
		return TRUE;
	}
	
	if (hookdata->is_smtp) {
		if (account->out_ssl_client_cert_file && *account->out_ssl_client_cert_file)
			hookdata->cert_path = account->out_ssl_client_cert_file;
		pwd_id = PWS_ACCOUNT_SEND_CERT;
	} else {
		if (account->in_ssl_client_cert_file && *account->in_ssl_client_cert_file)
			hookdata->cert_path = account->in_ssl_client_cert_file;
		pwd_id = PWS_ACCOUNT_RECV_CERT;
	}

	hookdata->password = passwd_store_get_account(account->account_id, pwd_id);
	return TRUE;
}

struct GetPassData {
	GCond cond;
	GMutex mutex;
	gchar **pass;
};


static gboolean do_get_pass(gpointer data)
{
	struct GetPassData *pass_data = (struct GetPassData *)data;
	g_mutex_lock(&pass_data->mutex);
	*(pass_data->pass) = input_dialog_query_password("the PKCS12 client certificate", NULL);
	g_cond_signal(&pass_data->cond);
	g_mutex_unlock(&pass_data->mutex);
	return FALSE;
}
static gboolean sslcert_get_password(gpointer source, gpointer data)
{ 
	struct GetPassData pass_data;
	/* do complicated stuff to be able to call GTK from the mainloop */
	g_cond_init(&pass_data.cond);
	g_mutex_init(&pass_data.mutex);
	pass_data.pass = (gchar **)source;

	g_mutex_lock(&pass_data.mutex);

	g_idle_add(do_get_pass, &pass_data);

	g_cond_wait(&pass_data.cond, &pass_data.mutex);
	g_cond_clear(&pass_data.cond);
	g_mutex_unlock(&pass_data.mutex);
	g_mutex_clear(&pass_data.mutex);

	return TRUE;
}
#endif

static void register_proxy_page(void)
{
	static gchar *path[3];

	path[0] = _("Account");
	path[1] = _("Proxy");
	path[2] = NULL;

	proxy_page.page.path = path;
	proxy_page.page.weight = 1000.0;
	proxy_page.page.create_widget = proxy_create_widget_func;
	proxy_page.page.destroy_widget = proxy_destroy_widget_func;
	proxy_page.page.save_page = proxy_save_func;
	proxy_page.page.can_close = proxy_can_close_func;

	prefs_account_register_page((PrefsPage *) &proxy_page);
}

static void register_advanced_page(void)
{
	static gchar *path[3];

	path[0] = _("Account");
	path[1] = _("Advanced");
	path[2] = NULL;
        
	advanced_page.page.path = path;
	advanced_page.page.weight = 1000.0;
	advanced_page.page.create_widget = advanced_create_widget_func;
	advanced_page.page.destroy_widget = advanced_destroy_widget_func;
	advanced_page.page.save_page = advanced_save_func;
	advanced_page.page.can_close = advanced_can_close_func;

	prefs_account_register_page((PrefsPage *) &advanced_page);
}

void prefs_account_init()
{
	register_basic_page();
	register_receive_page();
	register_send_page();
	register_compose_page();
	register_templates_page();
	register_privacy_page();
#ifdef USE_GNUTLS
	register_ssl_page();
	hooks_register_hook(SSLCERT_GET_CLIENT_CERT_HOOKLIST, sslcert_get_client_cert_hook, NULL);
	hooks_register_hook(SSL_CERT_GET_PASSWORD, sslcert_get_password, NULL);
#endif
	register_proxy_page();
#ifdef USE_OAUTH2
	register_oauth2_page();
#endif
	register_advanced_page();
}

PrefsAccount *prefs_account_new(void)
{
	PrefsAccount *ac_prefs;

	ac_prefs = g_new0(PrefsAccount, 1);
	memset(&tmp_ac_prefs, 0, sizeof(PrefsAccount));
	prefs_set_default(basic_param);
	prefs_set_default(receive_param);
	prefs_set_default(send_param);
	prefs_set_default(oauth2_param);
	prefs_set_default(compose_param);
	prefs_set_default(templates_param);
	prefs_set_default(privacy_param);
	prefs_set_default(ssl_param);
	prefs_set_default(proxy_param);
	prefs_set_default(advanced_param);
	*ac_prefs = tmp_ac_prefs;
	ac_prefs->account_id = prefs_account_get_new_id();

	ac_prefs->privacy_prefs = g_hash_table_new(g_str_hash, g_str_equal);

	return ac_prefs;
}

PrefsAccount *prefs_account_new_from_config(const gchar *label)
{
	const gchar *p = label;
	gchar *rcpath;
	gint id;
	gchar **strv, **cur;
	gsize len;
	PrefsAccount *ac_prefs;

	cm_return_val_if_fail(label != NULL, NULL);

	ac_prefs = g_new0(PrefsAccount, 1);

	/* Load default values to tmp_ac_prefs first, ... */
	memset(&tmp_ac_prefs, 0, sizeof(PrefsAccount));
	prefs_set_default(basic_param);
	prefs_set_default(receive_param);
	prefs_set_default(send_param);
	prefs_set_default(oauth2_param);
	prefs_set_default(compose_param);
	prefs_set_default(templates_param);
	prefs_set_default(privacy_param);
	prefs_set_default(ssl_param);
	prefs_set_default(advanced_param);

	/* ... overriding them with values from stored config file. */
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, ACCOUNT_RC, NULL);
	prefs_read_config(basic_param, label, rcpath, NULL);
	prefs_read_config(receive_param, label, rcpath, NULL);
	prefs_read_config(send_param, label, rcpath, NULL);
	prefs_read_config(oauth2_param, label, rcpath, NULL);
	prefs_read_config(compose_param, label, rcpath, NULL);
	prefs_read_config(templates_param, label, rcpath, NULL);
	prefs_read_config(privacy_param, label, rcpath, NULL);
	prefs_read_config(ssl_param, label, rcpath, NULL);
	prefs_read_config(proxy_param, label, rcpath, NULL);
	prefs_read_config(advanced_param, label, rcpath, NULL);
	g_free(rcpath);

	*ac_prefs = tmp_ac_prefs;

	while (*p && !g_ascii_isdigit(*p)) p++;
	id = atoi(p);
	if (id < 0) g_warning("wrong account id: %d", id);
	ac_prefs->account_id = id;

	/* Now parse privacy_prefs. */
	ac_prefs->privacy_prefs = g_hash_table_new(g_str_hash, g_str_equal);
	if (privacy_prefs != NULL) {
		strv = g_strsplit(privacy_prefs, ",", 0);
		for (cur = strv; *cur != NULL; cur++) {
			gchar *encvalue, *tmp;

			encvalue = strchr(*cur, '=');
			if (encvalue == NULL)
				continue;
			encvalue[0] = '\0';
			encvalue++;

			tmp = g_base64_decode_zero(encvalue, &len);
			if (len > 0)
				g_hash_table_insert(ac_prefs->privacy_prefs, g_strdup(*cur), tmp);
			else
				g_free(tmp);
		}
		g_strfreev(strv);
		g_free(privacy_prefs);
		privacy_prefs = NULL;
	}

	/* For older configurations, move stored passwords into the
	 * password store. */
	gboolean passwords_migrated = FALSE;

	if (ac_prefs->passwd != NULL && strlen(ac_prefs->passwd) > 1) {
		passwd_store_set_account(ac_prefs->account_id,
				PWS_ACCOUNT_RECV, ac_prefs->passwd, TRUE);
		passwords_migrated = TRUE;
	}
	if (ac_prefs->smtp_passwd != NULL && strlen(ac_prefs->smtp_passwd) > 1) {
		passwd_store_set_account(ac_prefs->account_id,
				PWS_ACCOUNT_SEND, ac_prefs->smtp_passwd, TRUE);
		passwords_migrated = TRUE;
	}
	if (ac_prefs->in_ssl_client_cert_pass != NULL
			&& strlen(ac_prefs->in_ssl_client_cert_pass) > 1) {
		passwd_store_set_account(ac_prefs->account_id,
				PWS_ACCOUNT_RECV_CERT, ac_prefs->in_ssl_client_cert_pass, TRUE);
		passwords_migrated = TRUE;
	}
	if (ac_prefs->out_ssl_client_cert_pass != NULL
			&& strlen(ac_prefs->out_ssl_client_cert_pass) > 1) {
		passwd_store_set_account(ac_prefs->account_id,
				PWS_ACCOUNT_SEND_CERT, ac_prefs->out_ssl_client_cert_pass, TRUE);
		passwords_migrated = TRUE;
	}

	/* Write out password store to file immediately, to prevent
	 * their loss. */
	if (passwords_migrated)
		passwd_store_write_config();

	ac_prefs->receive_in_progress = FALSE;

	prefs_custom_header_read_config(ac_prefs);

	/* Start the auto-check interval, if needed. */
	if (!ac_prefs->autochk_use_default && ac_prefs->autochk_use_custom
			&& ac_prefs->autochk_itv > PREFS_RECV_AUTOCHECK_MIN_INTERVAL) {
		inc_account_autocheck_timer_set_interval(ac_prefs);
	}

	return ac_prefs;
}

static void create_privacy_prefs(gpointer key, gpointer _value, gpointer user_data)
{
	GString *str = (GString *) user_data;
	gchar *encvalue;
	gchar *value = (gchar *) _value;

	if (str->len > 0)
		g_string_append_c(str, ',');

	encvalue = g_base64_encode(value, strlen(value));
	g_string_append_printf(str, "%s=%s", (gchar *) key, encvalue);
	g_free(encvalue);
}

#define WRITE_PARAM(PARAM_TABLE) \
		if (prefs_write_param(PARAM_TABLE, pfile->fp) < 0) { \
			g_warning("failed to write configuration to file"); \
			prefs_file_close_revert(pfile); \
			g_free(privacy_prefs); \
			privacy_prefs = NULL; \
			g_free(rcpath); \
			return; \
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

	for (cur = account_list; cur != NULL; cur = cur->next) {
		GString *str;

		tmp_ac_prefs = *(PrefsAccount *)cur->data;
		if (fprintf(pfile->fp, "[Account: %d]\n",
			    tmp_ac_prefs.account_id) <= 0) {
			g_free(pfile);
			g_free(rcpath);
			return;
        }
        
		str = g_string_sized_new(32);
		g_hash_table_foreach(tmp_ac_prefs.privacy_prefs, create_privacy_prefs, str);
		privacy_prefs = g_string_free(str, FALSE);

		WRITE_PARAM(basic_param)
		WRITE_PARAM(receive_param)
		WRITE_PARAM(send_param)
		WRITE_PARAM(oauth2_param)
		WRITE_PARAM(compose_param)
		WRITE_PARAM(templates_param)
		WRITE_PARAM(privacy_param)
		WRITE_PARAM(ssl_param)
		WRITE_PARAM(proxy_param)
		WRITE_PARAM(advanced_param)

		g_free(privacy_prefs);
		privacy_prefs = NULL;

		if (cur->next) {
			if (claws_fputc('\n', pfile->fp) == EOF) {
				FILE_OP_ERROR(rcpath, "claws_fputc");
				prefs_file_close_revert(pfile);
				g_free(rcpath);
				return;
			}
		}
	}
	g_free(rcpath);

	if (prefs_file_close(pfile) < 0)
		g_warning("failed to write configuration to file");

	passwd_store_write_config();
}
#undef WRITE_PARAM

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
	prefs_free(basic_param);
	prefs_free(receive_param);
	prefs_free(send_param);
	prefs_free(oauth2_param);
	prefs_free(compose_param);
	prefs_free(templates_param);
	prefs_free(privacy_param);
	prefs_free(ssl_param);
	prefs_free(proxy_param);
	prefs_free(advanced_param);
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

static void destroy_dialog(gpointer data)
{
	PrefsAccount *ac_prefs = (PrefsAccount *) data;
	if (!cancelled) {
		gboolean update_fld_list = FALSE;
		if (ac_prefs->protocol == A_IMAP4 && !new_account) {
			if ((&tmp_ac_prefs)->imap_subsonly != ac_prefs->imap_subsonly) {
				update_fld_list = TRUE;
			} 
		}
		*ac_prefs = tmp_ac_prefs;
		if (update_fld_list)
			folderview_rescan_tree(ac_prefs->folder, FALSE);

		inc_account_autocheck_timer_set_interval(ac_prefs);

	} else /* the customhdr_list may have changed, update it anyway */
		ac_prefs->customhdr_list = (&tmp_ac_prefs)->customhdr_list;

	
	gtk_main_quit();
}

PrefsAccount *prefs_account_open(PrefsAccount *ac_prefs, gboolean *dirty)
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
			&prefs_common.editaccountwin_width, &prefs_common.editaccountwin_height,
			TRUE, NULL, NULL, NULL);
	g_free(title);
	gtk_main();

	inc_unlock();

	if (!cancelled && dirty != NULL)
		*dirty = TRUE;
	if (cancelled && new_account)
		return NULL;
	else {
		if (ac_prefs->recv_server)
			g_strstrip(ac_prefs->recv_server);
		if (ac_prefs->smtp_server)
			g_strstrip(ac_prefs->smtp_server);
		if (ac_prefs->nntp_server)
			g_strstrip(ac_prefs->nntp_server);

		return ac_prefs;
	}
}

static void crosspost_color_toggled(void)
{
	gboolean is_active;

	is_active = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(advanced_page.crosspost_checkbtn));
	gtk_widget_set_sensitive(advanced_page.crosspost_colormenu, is_active);
}

static void prefs_account_crosspost_set_data_from_colormenu(PrefParam *pparam)
{
	gint color;
	GtkWidget *combobox = *pparam->widget;

	color = colorlabel_get_combobox_colormenu_active(GTK_COMBO_BOX(combobox));
	*((gint *)pparam->data) = color;
}

static void prefs_account_crosspost_set_colormenu(PrefParam *pparam)
{
	gint color = *((gint *)pparam->data);
	GtkWidget *combobox = *pparam->widget;

	colorlabel_set_combobox_colormenu_active(GTK_COMBO_BOX(combobox), color);
}

static void pop_bfr_smtp_tm_set_sens(GtkWidget *widget, gpointer data)
{
	gtk_widget_set_sensitive(send_page.pop_bfr_smtp_tm_spinbtn, 
				 gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(send_page.pop_bfr_smtp_checkbtn)));
	gtk_widget_set_sensitive(send_page.pop_auth_timeout_lbl, 
				 gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(send_page.pop_bfr_smtp_checkbtn)));
	gtk_widget_set_sensitive(send_page.pop_auth_minutes_lbl, 
				 gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(send_page.pop_bfr_smtp_checkbtn)));
}

static void prefs_account_select_folder_cb(GtkWidget *widget, gpointer data)
{
	FolderItem *item;
	gchar *id;

	item = foldersel_folder_sel(NULL, FOLDER_SEL_COPY, NULL, FALSE, NULL);
	if (item && item->path) {
		id = folder_item_get_identifier(item);
		if (id) {
			gtk_entry_set_text(GTK_ENTRY(data), id);
			g_free(id);
		}
	}
}

#if (defined USE_GNUTLS)
static void auto_configure_cb (GtkWidget *widget, gpointer data)
{
	gchar *address = NULL;
	AutoConfigureData *recv_data;
	AutoConfigureData *send_data;
	static GCancellable *recv_cancel = NULL;
	static GCancellable *send_cancel = NULL;
	RecvProtocol protocol;
	struct BasicProtocol *protocol_optmenu = (struct BasicProtocol *) basic_page.protocol_optmenu;
	GtkWidget *optmenu = protocol_optmenu->combobox;

	if (!recv_cancel) {
		recv_cancel = g_cancellable_new();
		send_cancel = g_cancellable_new();
	}

	if (widget == basic_page.auto_configure_cancel_btn) {
		g_cancellable_cancel(recv_cancel);
		g_cancellable_cancel(send_cancel);
		g_object_unref(recv_cancel);
		g_object_unref(send_cancel);
		recv_cancel = NULL;
		send_cancel = NULL;
		return;
	}

	protocol = combobox_get_active_data(GTK_COMBO_BOX(optmenu));

	address = gtk_editable_get_chars(GTK_EDITABLE(basic_page.addr_entry), 0, -1);

	if (strchr(address, '@') == NULL) {
		g_free(address);
		gtk_label_set_text(GTK_LABEL(basic_page.auto_configure_lbl),
			   _("Failed (wrong address)"));
		return;
	}

	if (protocol == A_POP3 || protocol == A_IMAP4) {
		recv_data = g_new0(AutoConfigureData, 1);
		recv_data->configure_button = GTK_BUTTON(basic_page.auto_configure_btn);
		recv_data->cancel_button = GTK_BUTTON(basic_page.auto_configure_cancel_btn);
		recv_data->info_label = GTK_LABEL(basic_page.auto_configure_lbl);
		recv_data->cancel = recv_cancel;
		switch(protocol) {
		case A_POP3:
			recv_data->ssl_service = "pop3s";
			recv_data->tls_service = "pop3";
			recv_data->address = g_strdup(address);
			recv_data->hostname_entry = GTK_ENTRY(basic_page.recvserv_entry);
			recv_data->set_port = GTK_TOGGLE_BUTTON(advanced_page.popport_checkbtn);
			recv_data->port = GTK_SPIN_BUTTON(advanced_page.popport_spinbtn);
			recv_data->tls_checkbtn = GTK_TOGGLE_BUTTON(ssl_page.pop_starttls_radiobtn);
			recv_data->ssl_checkbtn = GTK_TOGGLE_BUTTON(ssl_page.pop_ssltunnel_radiobtn);
			recv_data->default_port = 110;
			recv_data->default_ssl_port = 995;
			recv_data->uid_entry = GTK_ENTRY(basic_page.uid_entry);
			break;
		case A_IMAP4:
			recv_data->ssl_service = "imaps";
			recv_data->tls_service = "imap";
			recv_data->address = g_strdup(address);
			recv_data->hostname_entry = GTK_ENTRY(basic_page.recvserv_entry);
			recv_data->set_port = GTK_TOGGLE_BUTTON(advanced_page.imapport_checkbtn);
			recv_data->port = GTK_SPIN_BUTTON(advanced_page.imapport_spinbtn);
			recv_data->tls_checkbtn = GTK_TOGGLE_BUTTON(ssl_page.imap_starttls_radiobtn);
			recv_data->ssl_checkbtn = GTK_TOGGLE_BUTTON(ssl_page.imap_ssltunnel_radiobtn);
			recv_data->default_port = 143;
			recv_data->default_ssl_port = 993;
			recv_data->uid_entry = GTK_ENTRY(basic_page.uid_entry);
			break;
		default:
			cm_return_if_fail(FALSE);
		}
		auto_configure_service(recv_data);
	}

	send_data = g_new0(AutoConfigureData, 1);
	send_data->configure_button = GTK_BUTTON(basic_page.auto_configure_btn);
	send_data->cancel_button = GTK_BUTTON(basic_page.auto_configure_cancel_btn);
	send_data->info_label = GTK_LABEL(basic_page.auto_configure_lbl);
	send_data->cancel = send_cancel;

	send_data->ssl_service = NULL;
	send_data->tls_service = "submission";
	send_data->address = g_strdup(address);
	send_data->hostname_entry = GTK_ENTRY(basic_page.smtpserv_entry);
	send_data->set_port = GTK_TOGGLE_BUTTON(advanced_page.smtpport_checkbtn);
	send_data->port = GTK_SPIN_BUTTON(advanced_page.smtpport_spinbtn);
	send_data->tls_checkbtn = GTK_TOGGLE_BUTTON(ssl_page.smtp_starttls_radiobtn);
	send_data->ssl_checkbtn = GTK_TOGGLE_BUTTON(ssl_page.smtp_ssltunnel_radiobtn);
	send_data->default_port = 25;
	send_data->default_ssl_port = -1;
	send_data->uid_entry = NULL;
	send_data->auth_checkbtn = GTK_TOGGLE_BUTTON(send_page.smtp_auth_checkbtn);

	auto_configure_service(send_data);

	g_free(address);
}
#endif

static void prefs_account_sigfile_radiobtn_cb(GtkWidget *widget, gpointer data)
{
	gtk_widget_set_sensitive(GTK_WIDGET(signature_browse_button), TRUE);
	gtk_widget_set_sensitive(GTK_WIDGET(signature_edit_button), TRUE);
}

static void prefs_account_sigcmd_radiobtn_cb(GtkWidget *widget, gpointer data)
{
	gtk_widget_set_sensitive(GTK_WIDGET(signature_browse_button), FALSE);
	gtk_widget_set_sensitive(GTK_WIDGET(signature_edit_button), FALSE);
}

static void prefs_account_signature_browse_cb(GtkWidget *widget, gpointer data)
{
	gchar *filename;
	gchar *utf8_filename;

	filename = filesel_select_file_open(_("Select signature file"), NULL);
	if (!filename) return;

	utf8_filename = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
	if (!utf8_filename) {
		g_warning("prefs_account_signature_browse_cb(): failed to convert character set");
		utf8_filename = g_strdup(filename);
	}
	gtk_entry_set_text(GTK_ENTRY(entry_sigpath), utf8_filename);
	g_free(utf8_filename);
}

#ifdef USE_GNUTLS
static void prefs_account_in_cert_browse_cb(GtkWidget *widget, gpointer data)
{
	gchar *filename;
	gchar *utf8_filename;

	filename = filesel_select_file_open(_("Select certificate file"), NULL);
	if (!filename) return;

	utf8_filename = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
	if (!utf8_filename) {
		g_warning("prefs_account_cert_browse_cb(): failed to convert character set");
		utf8_filename = g_strdup(filename);
	}
	gtk_entry_set_text(GTK_ENTRY(entry_in_cert_file), utf8_filename);
	g_free(utf8_filename);
}

static void prefs_account_out_cert_browse_cb(GtkWidget *widget, gpointer data)
{
	gchar *filename;
	gchar *utf8_filename;

	filename = filesel_select_file_open(_("Select certificate file"), NULL);
	if (!filename) return;

	utf8_filename = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
	if (!utf8_filename) {
		g_warning("prefs_account_cert_browse_cb(): failed to convert character set");
		utf8_filename = g_strdup(filename);
	}
	gtk_entry_set_text(GTK_ENTRY(entry_out_cert_file), utf8_filename);
	g_free(utf8_filename);
}
#endif

static void prefs_account_signature_edit_cb(GtkWidget *widget, gpointer data)
{
	const gchar *sigpath = gtk_entry_get_text(GTK_ENTRY(data));
	if (!is_file_exist(sigpath))
		str_write_to_file(sigpath, "", TRUE);
	open_txt_editor(sigpath, prefs_common_get_ext_editor_cmd());
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
	struct BasicProtocol *protocol_optmenu =
		(struct BasicProtocol *)*pparam->widget;
	GtkWidget *optmenu = protocol_optmenu->combobox;

	*((RecvProtocol *)pparam->data) =
		combobox_get_active_data(GTK_COMBO_BOX(optmenu));
}

static void prefs_account_protocol_set_optmenu(PrefParam *pparam)
{
	RecvProtocol protocol;
	struct BasicProtocol *protocol_optmenu =
		(struct BasicProtocol *)*pparam->widget;
	GtkWidget *optmenu = protocol_optmenu->combobox;
	GtkWidget *optlabel = protocol_optmenu->label;
	GtkWidget *descrlabel = protocol_optmenu->descrlabel;
	gchar *label = NULL;

	protocol = *((RecvProtocol *)pparam->data);

	/* Set combobox to correct value even if it will be hidden, so
	 * we won't break existing accounts when saving. */
	combobox_select_by_data(GTK_COMBO_BOX(optmenu), protocol);

	/* Set up widgets accordingly */
	if( new_account ) {
		gtk_label_set_text(GTK_LABEL(descrlabel), _("Protocol"));
		gtk_widget_hide(optlabel);
		gtk_widget_show(optmenu);
	} else {
		gtk_label_set_text(GTK_LABEL(descrlabel), _("Protocol:"));
		label = g_markup_printf_escaped("<b>%s</b>", protocol_names[protocol]);
		gtk_label_set_markup(GTK_LABEL(optlabel), label);
		g_free(label);
		gtk_widget_hide(optmenu);
		gtk_widget_show(optlabel);
#ifndef HAVE_LIBETPAN
		if (protocol == A_IMAP4 || protocol == A_NNTP) {
			gtk_widget_show(protocol_optmenu->no_imap_warn_icon);
			gtk_widget_show(protocol_optmenu->no_imap_warn_label);
		} else {
			gtk_widget_hide(protocol_optmenu->no_imap_warn_icon);
			gtk_widget_hide(protocol_optmenu->no_imap_warn_label);
		}
#endif
		if (protocol == A_IMAP4) {
			if (new_account)
				gtk_toggle_button_set_active(
					GTK_TOGGLE_BUTTON(send_page.msgid_checkbtn), 
					TRUE);
			gtk_widget_hide(send_page.msgid_checkbtn);
		} else
			gtk_widget_show(send_page.msgid_checkbtn);

		gtk_widget_show(send_page.xmailer_checkbtn);
	}
}

static void prefs_account_imap_auth_type_set_data_from_optmenu(PrefParam *pparam)
{
	*((RecvProtocol *)pparam->data) =
			combobox_get_active_data(GTK_COMBO_BOX(*pparam->widget));
}

static void prefs_account_imap_auth_type_set_optmenu(PrefParam *pparam)
{
	IMAPAuthType type = *((IMAPAuthType *)pparam->data);
	GtkComboBox *optmenu = GTK_COMBO_BOX(*pparam->widget);

	combobox_select_by_data(optmenu, type);
}

static void prefs_account_smtp_auth_type_set_data_from_optmenu(PrefParam *pparam)
{
	*((RecvProtocol *)pparam->data) =
		combobox_get_active_data(GTK_COMBO_BOX(*pparam->widget));
}

static void prefs_account_smtp_auth_type_set_optmenu(PrefParam *pparam)
{
	SMTPAuthType type = *((SMTPAuthType *)pparam->data);
	GtkComboBox *optmenu = GTK_COMBO_BOX(*pparam->widget);

	combobox_select_by_data(optmenu, type);
}

static void prefs_account_set_autochk_interval_from_widgets(PrefParam *pparam)
{
	struct AutocheckWidgets *autochk_widgets =
		(struct AutocheckWidgets *)*pparam->widget;

	*(gint *)pparam->data =
		(3600 * gtk_spin_button_get_value_as_int(
			GTK_SPIN_BUTTON(autochk_widgets->autochk_hour_spinbtn)))
		+ (60 * gtk_spin_button_get_value_as_int(
			GTK_SPIN_BUTTON(autochk_widgets->autochk_min_spinbtn)))
		+ gtk_spin_button_get_value_as_int(
			GTK_SPIN_BUTTON(autochk_widgets->autochk_sec_spinbtn));
}

static void prefs_account_pop_auth_type_set_data_from_optmenu(PrefParam *pparam)
{
	*((RecvProtocol *)pparam->data) =
		combobox_get_active_data(GTK_COMBO_BOX(*pparam->widget));
}

static void prefs_account_pop_auth_type_set_optmenu(PrefParam *pparam)
{
	POPAuthType type = *((POPAuthType *)pparam->data);
	GtkComboBox *optmenu = GTK_COMBO_BOX(*pparam->widget);

	combobox_select_by_data(optmenu, type);
}

#ifdef USE_OAUTH2
static void prefs_account_oauth2_provider_set_data_from_optmenu(PrefParam *pparam)
{
	*((Oauth2Service *)pparam->data) =
		combobox_get_active_data(GTK_COMBO_BOX(*pparam->widget));
}

static void prefs_account_oauth2_provider_set_optmenu(PrefParam *pparam)
{
	Oauth2Service type = *((Oauth2Service *)pparam->data);
	GtkComboBox *optmenu = GTK_COMBO_BOX(*pparam->widget);

	combobox_select_by_data(optmenu, type);
}

static void prefs_account_oauth2_set_auth_sensitivity(void)
{
	const gchar *authcode = gtk_entry_get_text((GtkEntry *)oauth2_page.oauth2_authcode_entry);
	gchar *trim_text = g_strdup(authcode);
	g_strstrip(trim_text);
	gtk_widget_set_sensitive(oauth2_page.oauth2_authorise_btn, (*trim_text != 0));
	g_free(trim_text);
}

static void prefs_account_oauth2_set_sensitivity(void)
{
	struct BasicProtocol *protocol_optmenu = (struct BasicProtocol *)oauth2_page.protocol_optmenu;
	GtkWidget *optmenu = protocol_optmenu->combobox;
	Oauth2Service service;

	service = combobox_get_active_data(GTK_COMBO_BOX(optmenu));

	if(service == OAUTH2AUTH_NONE)
		gtk_widget_set_sensitive(oauth2_page.oauth2_sensitive, FALSE);
	else
		gtk_widget_set_sensitive(oauth2_page.oauth2_sensitive, TRUE);
}

static void prefs_account_oauth2_copy_url(GtkButton *button, gpointer data)
{
	struct BasicProtocol *protocol_optmenu = (struct BasicProtocol *)oauth2_page.protocol_optmenu;

	GtkWidget *optmenu = protocol_optmenu->combobox;

	GtkWidget *win;
	GtkClipboard *clip, *clip2;
	gint len;
	gchar *url;
	url = g_malloc(OAUTH2BUFSIZE+1);
	Oauth2Service service;
	const gchar * custom_client_id = NULL;
	struct Oauth2Listener *oauth2_listener_data;
		
	service = combobox_get_active_data(GTK_COMBO_BOX(optmenu));
	custom_client_id = gtk_entry_get_text((GtkEntry *)oauth2_page.oauth2_client_id_entry);
	
	oauth2_authorisation_url(service, &url, custom_client_id);
	
	win = gtk_widget_get_toplevel (optmenu);
	len = strlen(url);
       
	clip = gtk_widget_get_clipboard (win, GDK_SELECTION_PRIMARY);
	clip2 = gtk_widget_get_clipboard (win, GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text (clip, url, len);
	gtk_clipboard_set_text (clip2, url, len);

	if (strcmp(gtk_button_get_label(button), "Copy link") != 0)
		open_uri(url, prefs_common_get_uri_cmd());

	g_free(url);

	//Start listener task for authorisation reply using a separate task
	//Avoids hanging while we wait, and to allow cancellation of the process
	//If task already exists gracefully close it
	if(oauth2_listener_task){
		debug_print("Closing oauth2 listener task\n");
		oauth2_listener_cancel = 1;
		while (oauth2_listener_closed == 0)
			gtk_main_iteration();
	}
		
	debug_print("Starting oauth2 listener task\n");

	oauth2_listener_data = g_new(struct Oauth2Listener, 1);
	oauth2_listener_data->success = FALSE;
	oauth2_listener_data->trim_text = NULL;
	oauth2_listener_data->service = combobox_get_active_data(GTK_COMBO_BOX(optmenu));
	oauth2_listener_data->OAUTH2Data = g_malloc(sizeof(* oauth2_listener_data->OAUTH2Data));
	oauth2_init (oauth2_listener_data->OAUTH2Data);
	oauth2_listener_data->OAUTH2Data->custom_client_secret = 
		g_strdup(gtk_entry_get_text((GtkEntry *)oauth2_page.oauth2_client_secret_entry));
	oauth2_listener_data->OAUTH2Data->custom_client_id = 
		g_strdup(gtk_entry_get_text((GtkEntry *)oauth2_page.oauth2_client_id_entry));

	oauth2_listener_cancel = 0;
	oauth2_listener_closed = 0;
	
	oauth2_listener_task = g_task_new(NULL, NULL, prefs_account_oauth2_callback, oauth2_listener_data);
	g_task_set_task_data(oauth2_listener_task, oauth2_listener_data, NULL);
	g_task_run_in_thread(oauth2_listener_task, prefs_account_oauth2_listener);
}

static void prefs_account_oauth2_obtain_tokens(GtkButton *button, gpointer data)
{
	struct BasicProtocol *protocol_optmenu = (struct BasicProtocol *)oauth2_page.protocol_optmenu;

	GtkWidget *optmenu = protocol_optmenu->combobox;
	Oauth2Service service;
	OAUTH2Data *OAUTH2Data = g_malloc(sizeof(* OAUTH2Data));
	const gchar *authcode = gtk_entry_get_text ((GtkEntry *)oauth2_page.oauth2_authcode_entry);
	gchar *trim_text = g_strdup(authcode);
	g_strstrip(trim_text);
	gint ret;

	oauth2_init (OAUTH2Data);


	OAUTH2Data->custom_client_secret = 
		g_strdup(gtk_entry_get_text((GtkEntry *)oauth2_page.oauth2_client_secret_entry));
	OAUTH2Data->custom_client_id = 
		g_strdup(gtk_entry_get_text((GtkEntry *)oauth2_page.oauth2_client_id_entry));

		
	service = combobox_get_active_data(GTK_COMBO_BOX(optmenu));
	ret = oauth2_obtain_tokens (service, OAUTH2Data, trim_text);
	
	if(!ret){
		if(OAUTH2Data->refresh_token != NULL){
			passwd_store_set_account(tmp_ac_prefs.account_id,
				PWS_ACCOUNT_OAUTH2_REFRESH, OAUTH2Data->refresh_token, FALSE);
			log_message(LOG_PROTOCOL, "OAuth2 refresh token stored\n");
	    }

	    if(OAUTH2Data->access_token != NULL){
			passwd_store_set_account(tmp_ac_prefs.account_id,
				PWS_ACCOUNT_RECV, OAUTH2Data->access_token, FALSE);

			passwd_store_set_account(tmp_ac_prefs.account_id,
				PWS_ACCOUNT_SEND, OAUTH2Data->access_token, FALSE);
			log_message(LOG_PROTOCOL, "OAuth2 access token stored\n");

			gtk_entry_set_text(GTK_ENTRY(basic_page.pass_entry), OAUTH2Data->access_token);
			gtk_entry_set_text(GTK_ENTRY(send_page.smtp_pass_entry), OAUTH2Data->access_token);
	    }

	    if(OAUTH2Data->expiry_str != NULL){
			passwd_store_set_account(tmp_ac_prefs.account_id,
				PWS_ACCOUNT_OAUTH2_EXPIRY, OAUTH2Data->expiry_str, FALSE);
			log_message(LOG_PROTOCOL, "OAuth2 access token expiry stored\n");
	    }

	    tmp_ac_prefs.oauth2_date = g_get_real_time () / G_USEC_PER_SEC;
	}
	g_free(trim_text);
	g_free(OAUTH2Data);
}
#endif

static void prefs_account_set_autochk_interval_to_widgets(PrefParam *pparam)
{
	gint val = *((gint *)pparam->data);
	struct AutocheckWidgets *autochk_widgets =
		(struct AutocheckWidgets *)*pparam->widget;

	gtk_spin_button_set_value(
			GTK_SPIN_BUTTON(autochk_widgets->autochk_hour_spinbtn),
			val / 3600);
	gtk_spin_button_set_value(
			GTK_SPIN_BUTTON(autochk_widgets->autochk_min_spinbtn),
			(val % 3600) / 60);
	gtk_spin_button_set_value(
			GTK_SPIN_BUTTON(autochk_widgets->autochk_sec_spinbtn),
			(val % 3600) % 60);
}

static void prefs_account_set_string_from_combobox(PrefParam *pparam)
{
	GtkWidget *combobox;
	GtkListStore *menu;
	GtkTreeIter iter;
	gchar **str;

	cm_return_if_fail(*pparam->widget != NULL);

	combobox = *pparam->widget;
	cm_return_if_fail(gtk_combo_box_get_active_iter(
				GTK_COMBO_BOX(combobox), &iter));

	str = (gchar **)pparam->data;
	g_free(*str);
	menu = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(combobox)));
	gtk_tree_model_get(GTK_TREE_MODEL(menu), &iter,
			COMBOBOX_PRIVACY_PLUGIN_ID, &(*str),
			-1);
}

/* Context struct and internal function called by gtk_tree_model_foreach().
 * This is used in prefs_account_set_privacy_combobox_from_string() to find
 * correct combobox entry to activate when account preferences are displayed
 * and their values are set according to preferences. */
typedef struct _privacy_system_set_ctx {
	GtkWidget *combobox;
	gchar *prefsid;
	gboolean found;
} PrivacySystemSetCtx;

static gboolean _privacy_system_set_func(GtkTreeModel *model, GtkTreePath *path,
		GtkTreeIter *iter, PrivacySystemSetCtx *ctx)
{
	GtkWidget *combobox = ctx->combobox;
	gchar *prefsid = ctx->prefsid;
	gchar *curid;

	/* We're searching for correct privacy plugin ID. */
	gtk_tree_model_get(model, iter, COMBOBOX_PRIVACY_PLUGIN_ID, &curid, -1);
	if( strcmp(prefsid, curid) == 0 ) {
		gtk_combo_box_set_active_iter(GTK_COMBO_BOX(combobox), iter);
		g_free(curid);
		ctx->found = TRUE;
		return TRUE;
	}

	g_free(curid);
	return FALSE;
}

static void prefs_account_set_privacy_combobox_from_string(PrefParam *pparam)
{
	GtkWidget *optionmenu;
	GtkListStore *menu;
	GtkTreeIter iter;
	gboolean found = FALSE;
	gchar *prefsid;
	PrivacySystemSetCtx *ctx = NULL;

	cm_return_if_fail(*pparam->widget != NULL);

	prefsid = *((gchar **) pparam->data);
	if (prefsid == NULL)
		return;

	optionmenu = *pparam->widget;
	menu = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(optionmenu)));

	ctx = g_new(PrivacySystemSetCtx, sizeof(PrivacySystemSetCtx));
	ctx->combobox = optionmenu;
	ctx->prefsid = prefsid;
	ctx->found = FALSE;

	gtk_tree_model_foreach(GTK_TREE_MODEL(menu),
			(GtkTreeModelForeachFunc)_privacy_system_set_func, ctx);
	found = ctx->found;
	g_free(ctx);

	/* If chosen privacy system is not available, add a dummy entry with
	 * "not loaded" note and make it active. */
	if (!found) {
		gchar *name;

		name = g_strdup_printf(_("%s (plugin not loaded)"), prefsid);
		gtk_list_store_append(menu, &iter);
		gtk_list_store_set(menu, &iter,
				COMBOBOX_TEXT, name,
				COMBOBOX_DATA, 0,
				COMBOBOX_SENS, TRUE,
				COMBOBOX_PRIVACY_PLUGIN_ID, prefsid,
				-1);
		g_free(name);

		gtk_combo_box_set_active_iter(GTK_COMBO_BOX(optionmenu), &iter);
	}
}

static void prefs_account_protocol_changed(GtkComboBox *combobox, gpointer data)
{
	RecvProtocol protocol;
	struct BasicProtocol *protocol_optmenu = (struct BasicProtocol *)basic_page.protocol_optmenu;

	protocol = combobox_get_active_data(combobox);

	gtk_widget_hide(protocol_optmenu->no_imap_warn_icon);
	gtk_widget_hide(protocol_optmenu->no_imap_warn_label);

	switch(protocol) {
	case A_NNTP:
#ifndef HAVE_LIBETPAN
		gtk_widget_show(protocol_optmenu->no_imap_warn_icon);
		gtk_widget_show(protocol_optmenu->no_imap_warn_label);
#else
		gtk_widget_hide(protocol_optmenu->no_imap_warn_icon);
		gtk_widget_hide(protocol_optmenu->no_imap_warn_label);
#endif
		gtk_widget_show(send_page.msgid_checkbtn);
		gtk_widget_show(send_page.xmailer_checkbtn);
		gtk_widget_show(basic_page.nntpserv_label);
		gtk_widget_show(basic_page.nntpserv_entry);
		gtk_grid_set_row_spacing(GTK_GRID(basic_page.serv_table), VSPACING_NARROW);

		gtk_widget_set_sensitive(basic_page.nntpauth_checkbtn, TRUE);
		gtk_widget_show(basic_page.nntpauth_checkbtn);

		gtk_widget_set_sensitive(basic_page.nntpauth_onconnect_checkbtn, TRUE);
		gtk_widget_show(basic_page.nntpauth_onconnect_checkbtn);

		gtk_widget_hide(basic_page.recvserv_label);
		gtk_widget_hide(basic_page.recvserv_entry);
		gtk_widget_show(basic_page.smtpserv_label);
		gtk_widget_show(basic_page.smtpserv_entry);
 		gtk_widget_hide(basic_page.localmbox_label);
		gtk_widget_hide(basic_page.localmbox_entry);
		gtk_widget_hide(basic_page.mailcmd_label);
		gtk_widget_hide(basic_page.mailcmd_entry);
		gtk_widget_hide(basic_page.mailcmd_checkbtn);
		gtk_widget_show(basic_page.uid_label);
		gtk_widget_show(basic_page.pass_label);
		gtk_widget_show(basic_page.uid_entry);
		gtk_widget_show(basic_page.pass_entry);
		gtk_widget_show(basic_page.showpwd_checkbtn);
 
		gtk_widget_set_sensitive(basic_page.uid_label,  TRUE);
		gtk_widget_set_sensitive(basic_page.pass_label, TRUE);
		gtk_widget_set_sensitive(basic_page.uid_entry,  TRUE);
		gtk_widget_set_sensitive(basic_page.pass_entry, TRUE);
		gtk_widget_set_sensitive(basic_page.showpwd_checkbtn, TRUE);

		/* update userid/passwd sensitive state */

		prefs_account_nntpauth_toggled
			(GTK_TOGGLE_BUTTON(basic_page.nntpauth_checkbtn), NULL);
		gtk_widget_hide(receive_page.pop3_frame);
		gtk_widget_hide(receive_page.imap_frame);
		gtk_widget_hide(receive_page.local_frame);
		gtk_widget_show(receive_page.autochk_frame);
		gtk_widget_show(receive_page.frame_maxarticle);
		gtk_widget_set_sensitive(receive_page.filter_on_recv_checkbtn, TRUE);
		prefs_account_filter_on_recv_toggled
			(GTK_TOGGLE_BUTTON(receive_page.filter_on_recv_checkbtn), NULL);
		gtk_widget_set_sensitive(receive_page.recvatgetall_checkbtn, TRUE);
		/* update pop_before_smtp sensitivity */
		gtk_toggle_button_set_active
			(GTK_TOGGLE_BUTTON(send_page.pop_bfr_smtp_checkbtn), FALSE);
		gtk_widget_set_sensitive(send_page.pop_bfr_smtp_checkbtn, FALSE);
		gtk_widget_set_sensitive(send_page.pop_bfr_smtp_tm_spinbtn, FALSE);
		
		if (!tmp_ac_prefs.account_name) {
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(receive_page.filter_on_recv_checkbtn), 
				TRUE);
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(receive_page.filterhook_on_recv_checkbtn), 
				TRUE);
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(receive_page.recvatgetall_checkbtn),
				 FALSE);
		}

#ifdef USE_GNUTLS
		gtk_widget_hide(ssl_page.pop_frame);
		gtk_widget_hide(ssl_page.imap_frame);
		gtk_widget_show(ssl_page.nntp_frame);
		gtk_widget_show(ssl_page.send_frame);
#endif
		gtk_widget_hide(advanced_page.popport_hbox);
		gtk_widget_hide(advanced_page.imapport_hbox);
		gtk_widget_show(advanced_page.nntpport_hbox);
		gtk_widget_show(advanced_page.crosspost_checkbtn);
		gtk_widget_show(advanced_page.crosspost_colormenu);
#ifndef G_OS_WIN32
		gtk_widget_hide(advanced_page.tunnelcmd_checkbtn);
		gtk_widget_hide(advanced_page.tunnelcmd_entry);
#endif
		gtk_widget_hide(receive_page.imapdir_label);
		gtk_widget_hide(receive_page.imapdir_entry);
		gtk_widget_hide(receive_page.subsonly_checkbtn);
		gtk_widget_hide(receive_page.low_bandwidth_checkbtn);
		gtk_widget_hide(receive_page.imap_batch_size_spinbtn);
		break;
	case A_LOCAL:
		gtk_widget_show(send_page.msgid_checkbtn);
		gtk_widget_show(send_page.xmailer_checkbtn);
		gtk_widget_hide(protocol_optmenu->no_imap_warn_icon);
		gtk_widget_hide(protocol_optmenu->no_imap_warn_label);
		gtk_widget_hide(basic_page.nntpserv_label);
		gtk_widget_hide(basic_page.nntpserv_entry);
 		gtk_widget_set_sensitive(basic_page.nntpauth_checkbtn, FALSE);
		gtk_widget_hide(basic_page.nntpauth_checkbtn);
		gtk_grid_set_row_spacing(GTK_GRID(basic_page.serv_table), VSPACING_NARROW);

		gtk_widget_set_sensitive(basic_page.nntpauth_onconnect_checkbtn, FALSE);
		gtk_widget_hide(basic_page.nntpauth_onconnect_checkbtn);
		gtk_widget_hide(basic_page.recvserv_label);
		gtk_widget_hide(basic_page.recvserv_entry);
		gtk_widget_show(basic_page.smtpserv_label);
		gtk_widget_show(basic_page.smtpserv_entry);
		gtk_widget_show(basic_page.localmbox_label);
		gtk_widget_show(basic_page.localmbox_entry);
		gtk_widget_show(basic_page.mailcmd_label);
		gtk_widget_show(basic_page.mailcmd_entry);
		gtk_widget_show(basic_page.mailcmd_checkbtn);
 		gtk_widget_hide(basic_page.uid_label);
		gtk_widget_hide(basic_page.pass_label);
		gtk_widget_hide(basic_page.uid_entry);
		gtk_widget_hide(basic_page.pass_entry);
		gtk_widget_hide(basic_page.showpwd_checkbtn);

		gtk_widget_set_sensitive(basic_page.uid_label,  TRUE);
		gtk_widget_set_sensitive(basic_page.pass_label, TRUE);
		gtk_widget_set_sensitive(basic_page.uid_entry,  TRUE);
		gtk_widget_set_sensitive(basic_page.pass_entry, TRUE);
		gtk_widget_set_sensitive(basic_page.showpwd_checkbtn, TRUE);
		gtk_widget_hide(receive_page.pop3_frame);
		gtk_widget_hide(receive_page.imap_frame);
		gtk_widget_show(receive_page.local_frame);
		gtk_widget_show(receive_page.autochk_frame);
		gtk_widget_hide(receive_page.frame_maxarticle);
		gtk_widget_set_sensitive(receive_page.filter_on_recv_checkbtn, TRUE);
		prefs_account_filter_on_recv_toggled
			(GTK_TOGGLE_BUTTON(receive_page.filter_on_recv_checkbtn), NULL);
		gtk_widget_set_sensitive(receive_page.recvatgetall_checkbtn, TRUE);
		prefs_account_mailcmd_toggled
			(GTK_TOGGLE_BUTTON(basic_page.mailcmd_checkbtn), NULL);

		/* update pop_before_smtp sensitivity */
		gtk_toggle_button_set_active
			(GTK_TOGGLE_BUTTON(send_page.pop_bfr_smtp_checkbtn), FALSE);
		gtk_widget_set_sensitive(send_page.pop_bfr_smtp_checkbtn, FALSE);
		gtk_widget_set_sensitive(send_page.pop_bfr_smtp_tm_spinbtn, FALSE);

		if (!tmp_ac_prefs.account_name) {
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(receive_page.filter_on_recv_checkbtn), 
				TRUE);
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(receive_page.filterhook_on_recv_checkbtn), 
				TRUE);
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(receive_page.recvatgetall_checkbtn),
				 TRUE);
		}

#ifdef USE_GNUTLS
		gtk_widget_hide(ssl_page.pop_frame);
		gtk_widget_hide(ssl_page.imap_frame);
		gtk_widget_hide(ssl_page.nntp_frame);
		gtk_widget_show(ssl_page.send_frame);
#endif
		gtk_widget_hide(advanced_page.popport_hbox);
		gtk_widget_hide(advanced_page.imapport_hbox);
		gtk_widget_hide(advanced_page.nntpport_hbox);
		gtk_widget_hide(advanced_page.crosspost_checkbtn);
		gtk_widget_hide(advanced_page.crosspost_colormenu);
#ifndef G_OS_WIN32
		gtk_widget_hide(advanced_page.tunnelcmd_checkbtn);
		gtk_widget_hide(advanced_page.tunnelcmd_entry);
#endif
		gtk_widget_hide(receive_page.imapdir_label);
		gtk_widget_hide(receive_page.imapdir_entry);
		gtk_widget_hide(receive_page.subsonly_checkbtn);
		gtk_widget_hide(receive_page.low_bandwidth_checkbtn);
		gtk_widget_hide(receive_page.imap_batch_size_spinbtn);
		break;
	case A_IMAP4:
#ifndef HAVE_LIBETPAN
		gtk_widget_show(protocol_optmenu->no_imap_warn_icon);
		gtk_widget_show(protocol_optmenu->no_imap_warn_label);
#endif
		if (new_account)
			gtk_toggle_button_set_active(
				GTK_TOGGLE_BUTTON(send_page.msgid_checkbtn), 
				TRUE);
		gtk_widget_hide(send_page.msgid_checkbtn);
		gtk_widget_show(send_page.xmailer_checkbtn);
		gtk_widget_hide(basic_page.nntpserv_label);
		gtk_widget_hide(basic_page.nntpserv_entry);
		gtk_grid_set_row_spacing(GTK_GRID(basic_page.serv_table), VSPACING_NARROW);
		gtk_widget_set_sensitive(basic_page.nntpauth_checkbtn, FALSE);
		gtk_widget_hide(basic_page.nntpauth_checkbtn);

		gtk_widget_set_sensitive(basic_page.nntpauth_onconnect_checkbtn, FALSE);
		gtk_widget_hide(basic_page.nntpauth_onconnect_checkbtn);

		gtk_widget_set_sensitive(basic_page.recvserv_label, TRUE);
		gtk_widget_set_sensitive(basic_page.recvserv_entry, TRUE);
		gtk_widget_show(basic_page.recvserv_label);
		gtk_widget_show(basic_page.recvserv_entry);
		gtk_widget_show(basic_page.smtpserv_label);
		gtk_widget_show(basic_page.smtpserv_entry);
 		gtk_widget_hide(basic_page.localmbox_label);
		gtk_widget_hide(basic_page.localmbox_entry);
		gtk_widget_hide(basic_page.mailcmd_label);
		gtk_widget_hide(basic_page.mailcmd_entry);
 		gtk_widget_hide(basic_page.mailcmd_checkbtn);
 		gtk_widget_show(basic_page.uid_label);
		gtk_widget_show(basic_page.pass_label);
		gtk_widget_show(basic_page.uid_entry);
		gtk_widget_show(basic_page.pass_entry);
		gtk_widget_show(basic_page.showpwd_checkbtn);
 
		gtk_widget_set_sensitive(basic_page.uid_label,  TRUE);
		gtk_widget_set_sensitive(basic_page.pass_label, TRUE);
		gtk_widget_set_sensitive(basic_page.uid_entry,  TRUE);
		gtk_widget_set_sensitive(basic_page.pass_entry, TRUE);
		gtk_widget_set_sensitive(basic_page.showpwd_checkbtn, TRUE);
		gtk_widget_hide(receive_page.pop3_frame);
		gtk_widget_show(receive_page.imap_frame);
		gtk_widget_hide(receive_page.local_frame);
		gtk_widget_show(receive_page.autochk_frame);
		gtk_widget_hide(receive_page.frame_maxarticle);
		gtk_widget_set_sensitive(receive_page.filter_on_recv_checkbtn, TRUE);
		prefs_account_filter_on_recv_toggled
			(GTK_TOGGLE_BUTTON(receive_page.filter_on_recv_checkbtn), NULL);
		gtk_widget_set_sensitive(receive_page.recvatgetall_checkbtn, TRUE);
		gtk_widget_set_sensitive(basic_page.smtpserv_entry, TRUE);
		gtk_widget_set_sensitive(basic_page.smtpserv_label, TRUE);

		/* update pop_before_smtp sensitivity */
		gtk_toggle_button_set_active
			(GTK_TOGGLE_BUTTON(send_page.pop_bfr_smtp_checkbtn), FALSE);
		gtk_widget_set_sensitive(send_page.pop_bfr_smtp_checkbtn, FALSE);
		gtk_widget_set_sensitive(send_page.pop_bfr_smtp_tm_spinbtn, FALSE);

		if (!tmp_ac_prefs.account_name) {
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(receive_page.filter_on_recv_checkbtn), 
				TRUE);
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(receive_page.filterhook_on_recv_checkbtn), 
				TRUE);
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(receive_page.recvatgetall_checkbtn),
				 FALSE);
		}

#ifdef USE_GNUTLS
		gtk_widget_hide(ssl_page.pop_frame);
		gtk_widget_show(ssl_page.imap_frame);
		gtk_widget_hide(ssl_page.nntp_frame);
		gtk_widget_show(ssl_page.send_frame);
#endif
		gtk_widget_hide(advanced_page.popport_hbox);
		gtk_widget_show(advanced_page.imapport_hbox);
		gtk_widget_hide(advanced_page.nntpport_hbox);
		gtk_widget_hide(advanced_page.crosspost_checkbtn);
		gtk_widget_hide(advanced_page.crosspost_colormenu);
#ifndef G_OS_WIN32
		gtk_widget_show(advanced_page.tunnelcmd_checkbtn);
		gtk_widget_show(advanced_page.tunnelcmd_entry);
#endif
		gtk_widget_show(receive_page.imapdir_label);
		gtk_widget_show(receive_page.imapdir_entry);
		gtk_widget_show(receive_page.subsonly_checkbtn);
		gtk_widget_show(receive_page.low_bandwidth_checkbtn);
		gtk_widget_show(receive_page.imap_batch_size_spinbtn);
		break;
	case A_NONE:
		gtk_widget_show(send_page.msgid_checkbtn);
		gtk_widget_show(send_page.xmailer_checkbtn);
		gtk_widget_hide(protocol_optmenu->no_imap_warn_icon);
		gtk_widget_hide(protocol_optmenu->no_imap_warn_label);
		gtk_widget_hide(basic_page.nntpserv_label);
		gtk_widget_hide(basic_page.nntpserv_entry);
		gtk_grid_set_row_spacing(GTK_GRID(basic_page.serv_table), VSPACING_NARROW);
		gtk_widget_set_sensitive(basic_page.nntpauth_checkbtn, FALSE);
		gtk_widget_hide(basic_page.nntpauth_checkbtn);

		gtk_widget_set_sensitive(basic_page.nntpauth_onconnect_checkbtn, FALSE);
		gtk_widget_hide(basic_page.nntpauth_onconnect_checkbtn);

		gtk_widget_set_sensitive(basic_page.recvserv_label, FALSE);
		gtk_widget_set_sensitive(basic_page.recvserv_entry, FALSE);
		gtk_widget_hide(basic_page.recvserv_label);
		gtk_widget_hide(basic_page.recvserv_entry);
 		gtk_widget_show(basic_page.smtpserv_label);
		gtk_widget_show(basic_page.smtpserv_entry);
		gtk_widget_hide(basic_page.localmbox_label);
		gtk_widget_hide(basic_page.localmbox_entry);
		gtk_widget_hide(basic_page.mailcmd_label);
		gtk_widget_hide(basic_page.mailcmd_entry);
 		gtk_widget_hide(basic_page.mailcmd_checkbtn);
		gtk_widget_hide(basic_page.uid_label);
		gtk_widget_hide(basic_page.pass_label);
		gtk_widget_hide(basic_page.uid_entry);
		gtk_widget_hide(basic_page.pass_entry);
		gtk_widget_hide(basic_page.showpwd_checkbtn);

		gtk_widget_set_sensitive(basic_page.uid_label,  FALSE);
		gtk_widget_set_sensitive(basic_page.pass_label, FALSE);
		gtk_widget_set_sensitive(basic_page.uid_entry,  FALSE);
		gtk_widget_set_sensitive(basic_page.pass_entry, FALSE);
		gtk_widget_set_sensitive(basic_page.showpwd_checkbtn, FALSE);
		gtk_widget_set_sensitive(receive_page.pop3_frame, FALSE);
		gtk_widget_hide(receive_page.pop3_frame);
		gtk_widget_hide(receive_page.imap_frame);
		gtk_widget_hide(receive_page.local_frame);
		gtk_widget_hide(receive_page.autochk_frame);
		gtk_widget_hide(receive_page.frame_maxarticle);
		gtk_widget_set_sensitive(receive_page.filter_on_recv_checkbtn, FALSE);
		prefs_account_filter_on_recv_toggled
			(GTK_TOGGLE_BUTTON(receive_page.filter_on_recv_checkbtn), NULL);
		gtk_widget_set_sensitive(receive_page.recvatgetall_checkbtn, FALSE);

		gtk_widget_set_sensitive(basic_page.smtpserv_entry, TRUE);
		gtk_widget_set_sensitive(basic_page.smtpserv_label, TRUE);

		/* update pop_before_smtp sensitivity */
		gtk_widget_set_sensitive(send_page.pop_bfr_smtp_checkbtn, FALSE);
		pop_bfr_smtp_tm_set_sens(NULL, NULL);
	
		gtk_toggle_button_set_active
			(GTK_TOGGLE_BUTTON(receive_page.filter_on_recv_checkbtn), FALSE);
		gtk_toggle_button_set_active
			(GTK_TOGGLE_BUTTON(receive_page.filterhook_on_recv_checkbtn), FALSE);
		gtk_toggle_button_set_active
			(GTK_TOGGLE_BUTTON(receive_page.recvatgetall_checkbtn), FALSE);

#ifdef USE_GNUTLS
		gtk_widget_hide(ssl_page.pop_frame);
		gtk_widget_hide(ssl_page.imap_frame);
		gtk_widget_hide(ssl_page.nntp_frame);
		gtk_widget_show(ssl_page.send_frame);
#endif
		gtk_widget_hide(advanced_page.popport_hbox);
		gtk_widget_hide(advanced_page.imapport_hbox);
		gtk_widget_hide(advanced_page.nntpport_hbox);
		gtk_widget_hide(advanced_page.crosspost_checkbtn);
		gtk_widget_hide(advanced_page.crosspost_colormenu);
#ifndef G_OS_WIN32
		gtk_widget_hide(advanced_page.tunnelcmd_checkbtn);
		gtk_widget_hide(advanced_page.tunnelcmd_entry);
#endif
		gtk_widget_hide(receive_page.imapdir_label);
		gtk_widget_hide(receive_page.imapdir_entry);
		gtk_widget_hide(receive_page.subsonly_checkbtn);
		gtk_widget_hide(receive_page.low_bandwidth_checkbtn);
		gtk_widget_hide(receive_page.imap_batch_size_spinbtn);
		break;
	case A_POP3:
		/* continue to default: */
	default:
		gtk_widget_show(send_page.msgid_checkbtn);
		gtk_widget_show(send_page.xmailer_checkbtn);
		gtk_widget_hide(protocol_optmenu->no_imap_warn_icon);
		gtk_widget_hide(protocol_optmenu->no_imap_warn_label);
		gtk_widget_hide(basic_page.nntpserv_label);
		gtk_widget_hide(basic_page.nntpserv_entry);
		gtk_grid_set_row_spacing(GTK_GRID(basic_page.serv_table), VSPACING_NARROW);
		gtk_widget_set_sensitive(basic_page.nntpauth_checkbtn, FALSE);
		gtk_widget_hide(basic_page.nntpauth_checkbtn);

		gtk_widget_set_sensitive(basic_page.nntpauth_onconnect_checkbtn, FALSE);
		gtk_widget_hide(basic_page.nntpauth_onconnect_checkbtn);

 		gtk_widget_set_sensitive(basic_page.recvserv_label, TRUE);
		gtk_widget_set_sensitive(basic_page.recvserv_entry, TRUE);
		gtk_widget_show(basic_page.recvserv_label);
		gtk_widget_show(basic_page.recvserv_entry);
		gtk_widget_show(basic_page.smtpserv_label);
		gtk_widget_show(basic_page.smtpserv_entry);
 		gtk_widget_hide(basic_page.localmbox_label);
		gtk_widget_hide(basic_page.localmbox_entry);
 		gtk_widget_hide(basic_page.mailcmd_label);
		gtk_widget_hide(basic_page.mailcmd_entry);
 		gtk_widget_hide(basic_page.mailcmd_checkbtn);
 		gtk_widget_show(basic_page.uid_label);
		gtk_widget_show(basic_page.pass_label);
		gtk_widget_show(basic_page.uid_entry);
		gtk_widget_show(basic_page.pass_entry);
		gtk_widget_show(basic_page.showpwd_checkbtn);

		gtk_widget_set_sensitive(basic_page.uid_label,  TRUE);
		gtk_widget_set_sensitive(basic_page.pass_label, TRUE);
		gtk_widget_set_sensitive(basic_page.uid_entry,  TRUE);
		gtk_widget_set_sensitive(basic_page.pass_entry, TRUE);
		gtk_widget_set_sensitive(basic_page.showpwd_checkbtn, TRUE);
		gtk_widget_set_sensitive(receive_page.pop3_frame, TRUE);
		gtk_widget_show(receive_page.pop3_frame);
		gtk_widget_hide(receive_page.imap_frame);
		gtk_widget_hide(receive_page.local_frame);
		gtk_widget_show(receive_page.autochk_frame);
		gtk_widget_hide(receive_page.frame_maxarticle);
		gtk_widget_set_sensitive(receive_page.filter_on_recv_checkbtn, TRUE);
		prefs_account_filter_on_recv_toggled
			(GTK_TOGGLE_BUTTON(receive_page.filter_on_recv_checkbtn), NULL);
		gtk_widget_set_sensitive(receive_page.recvatgetall_checkbtn, TRUE);

		gtk_widget_set_sensitive(basic_page.smtpserv_entry, TRUE);
		gtk_widget_set_sensitive(basic_page.smtpserv_label, TRUE);

		/* update pop_before_smtp sensitivity */
		gtk_widget_set_sensitive(send_page.pop_bfr_smtp_checkbtn, TRUE);
		pop_bfr_smtp_tm_set_sens(NULL, NULL);
		
		if (!tmp_ac_prefs.account_name) {
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(receive_page.filter_on_recv_checkbtn), 
				TRUE);
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(receive_page.filterhook_on_recv_checkbtn), 
				TRUE);
			gtk_toggle_button_set_active
				(GTK_TOGGLE_BUTTON(receive_page.recvatgetall_checkbtn),
				 TRUE);
		}

#ifdef USE_GNUTLS
		gtk_widget_show(ssl_page.pop_frame);
		gtk_widget_hide(ssl_page.imap_frame);
		gtk_widget_hide(ssl_page.nntp_frame);
		gtk_widget_show(ssl_page.send_frame);
#endif
		gtk_widget_show(advanced_page.popport_hbox);
		gtk_widget_hide(advanced_page.imapport_hbox);
		gtk_widget_hide(advanced_page.nntpport_hbox);
		gtk_widget_hide(advanced_page.crosspost_checkbtn);
		gtk_widget_hide(advanced_page.crosspost_colormenu);
#ifndef G_OS_WIN32
		gtk_widget_hide(advanced_page.tunnelcmd_checkbtn);
		gtk_widget_hide(advanced_page.tunnelcmd_entry);
#endif
		gtk_widget_hide(receive_page.imapdir_label);
		gtk_widget_hide(receive_page.imapdir_entry);
		gtk_widget_hide(receive_page.subsonly_checkbtn);
		gtk_widget_hide(receive_page.low_bandwidth_checkbtn);
		gtk_widget_hide(receive_page.imap_batch_size_spinbtn);
		break;
	}

	gtk_widget_queue_resize(basic_page.serv_frame);
}

static void prefs_account_nntpauth_toggled(GtkToggleButton *button,
					   gpointer user_data)
{
	gboolean auth;

	if (!gtk_widget_get_sensitive (GTK_WIDGET (button)))
		return;
	auth = gtk_toggle_button_get_active (button);
	gtk_widget_set_sensitive(basic_page.uid_label,  auth);
	gtk_widget_set_sensitive(basic_page.pass_label, auth);
	gtk_widget_set_sensitive(basic_page.uid_entry,  auth);
	gtk_widget_set_sensitive(basic_page.pass_entry, auth);
	gtk_widget_set_sensitive(basic_page.showpwd_checkbtn, auth);
	gtk_widget_set_sensitive(basic_page.nntpauth_onconnect_checkbtn, auth);
}

static void prefs_account_mailcmd_toggled(GtkToggleButton *button,
					  gpointer user_data)
{
	gboolean use_mailcmd;

	use_mailcmd = gtk_toggle_button_get_active (button);

	gtk_widget_set_sensitive(basic_page.mailcmd_entry,  use_mailcmd);
	gtk_widget_set_sensitive(basic_page.mailcmd_label, use_mailcmd);
	gtk_widget_set_sensitive(basic_page.smtpserv_entry, !use_mailcmd);
	gtk_widget_set_sensitive(basic_page.smtpserv_label, !use_mailcmd);
	gtk_widget_set_sensitive(basic_page.uid_entry,  !use_mailcmd);
	gtk_widget_set_sensitive(basic_page.pass_entry, !use_mailcmd);
	gtk_widget_set_sensitive(basic_page.showpwd_checkbtn, !use_mailcmd);
}

static void prefs_account_showpwd_checkbtn_toggled(GtkToggleButton *button,
		gpointer user_data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	GtkWidget *entry = GTK_WIDGET(user_data);

	gtk_entry_set_visibility(GTK_ENTRY(entry), active);
}

static void prefs_account_entry_changed_newline_check_cb(GtkWidget *entry,
		gpointer user_data)
{
	static GdkColor red = { (guint32)0, (guint16)0xff, (guint16)0x70, (guint16)0x70 };

	if (strchr(gtk_entry_get_text(GTK_ENTRY(entry)), '\n') != NULL) {
		/* Entry contains a newline, light it up. */
		debug_print("found newline in string, painting entry red\n");
		gtk_widget_modify_base(entry, GTK_STATE_NORMAL, &red);
	} else {
		gtk_widget_modify_base(entry, GTK_STATE_NORMAL, NULL);
	}
}

static void prefs_account_filter_on_recv_toggled(GtkToggleButton *button,
					  gpointer user_data)
{
	gboolean do_filter;

	do_filter = gtk_toggle_button_get_active (button);
	gtk_widget_set_sensitive(receive_page.filterhook_on_recv_checkbtn, do_filter);
}

#if USE_ENCHANT
static void prefs_account_compose_default_dictionary_set_string_from_optmenu
							(PrefParam *pparam)
{
	GtkWidget *combo;
	gchar **str;

	cm_return_if_fail(*pparam->widget != NULL);

	combo = *pparam->widget;
	str = (gchar **) pparam->data;

	g_free(*str);
	*str = gtkaspell_get_dictionary_menu_active_item(GTK_COMBO_BOX(combo));
}

static void prefs_account_compose_default_dictionary_set_optmenu_from_string
							(PrefParam *pparam)
{
	GtkWidget *combo;
	gchar *dictionary;

	cm_return_if_fail(*pparam->widget != NULL);

	dictionary = *((gchar **) pparam->data);
	if (dictionary != NULL) {
		if (strrchr(dictionary, '/')) {
			dictionary = g_strdup(strrchr(dictionary, '/')+1);
		}

		if (strchr(dictionary, '-')) {
			*(strchr(dictionary, '-')) = '\0';
		}
	}
	combo = *pparam->widget;
	if (dictionary && *dictionary)
		gtkaspell_set_dictionary_menu_active_item(GTK_COMBO_BOX(combo), 
							  dictionary);
	else {
		GtkTreeModel *model;
		GtkTreeIter iter;
		if((model = gtk_combo_box_get_model(GTK_COMBO_BOX(combo))) == NULL)
			return;
		if((gtk_tree_model_get_iter_first(model, &iter)) == FALSE)
			return;
		gtk_combo_box_set_active_iter(GTK_COMBO_BOX(combo), &iter);
	}
}
#endif

gchar *prefs_account_generate_msgid(PrefsAccount *account)
{
	gchar *addr, *tmbuf, *buf = NULL;
	GDateTime *now;
	gchar *user_addr = account->msgid_with_addr ? g_strdup(account->address) : NULL;

	if (account->set_domain && account->domain) {
		buf = g_strdup(account->domain);
	} else if (!strncmp(get_domain_name(), "localhost", strlen("localhost"))) {
		buf = g_strdup(
				strchr(account->address, '@') ?
				strchr(account->address, '@')+1 :
				account->address);
	}

	if (user_addr != NULL) {
		addr = g_strdup_printf(".%s", user_addr);
	} else {
		addr = g_strdup_printf("@%s",
				buf != NULL && strlen(buf) > 0 ?
				buf : get_domain_name());
	}

	if (buf != NULL)
		g_free(buf);
	if (user_addr != NULL)
		g_free(user_addr);

	/* Replace all @ but the last one in addr, with underscores.
	 * RFC 2822 States that msg-id syntax only allows one @.
	 */
	while (strchr(addr, '@') != NULL && strchr(addr, '@') != strrchr(addr, '@'))
		*(strchr(addr, '@')) = '_';

	now = (prefs_common.hide_timezone)? g_date_time_new_now_utc() :
					    g_date_time_new_now_local();
	tmbuf = g_date_time_format(now, "%Y%m%d%H%M%S");
	buf = g_strdup_printf("%s.%08x%s",
			tmbuf, (guint)rand(), addr);
	g_date_time_unref(now);
	g_free(tmbuf);
	g_free(addr);

	debug_print("Generated Message-ID string '%s'\n", buf);
	return buf;
}

void prefs_account_register_page(PrefsPage *page)
{
	prefs_pages = g_slist_append(prefs_pages, page);
	
}

void prefs_account_unregister_page(PrefsPage *page)
{
	prefs_pages = g_slist_remove(prefs_pages, page);
}

gchar *prefs_account_cache_dir(PrefsAccount *ac_prefs, gboolean for_server)
{
	gchar *dir = NULL;
#ifdef G_OS_WIN32
	gchar *sanitized_server;
#endif

	if (ac_prefs->protocol == A_IMAP4) {
#ifdef G_OS_WIN32
		sanitized_server = g_strdup(ac_prefs->recv_server);
		g_strdelimit(sanitized_server, ":", ',');
#endif
		if (for_server) {
			dir = g_strconcat(get_imap_cache_dir(),
					  G_DIR_SEPARATOR_S,
#ifdef G_OS_WIN32
					  sanitized_server,
#else
					  ac_prefs->recv_server,
#endif
					  NULL);
		} else {
			dir = g_strconcat(get_imap_cache_dir(),
					  G_DIR_SEPARATOR_S,
#ifdef G_OS_WIN32
					  sanitized_server,
#else
					  ac_prefs->recv_server,
#endif
					  G_DIR_SEPARATOR_S,
					  ac_prefs->userid,
					  NULL);
		}
#ifdef G_OS_WIN32
		g_free(sanitized_server);
#endif
	}

	return dir;
}

static void prefs_account_receive_itv_spinbutton_value_changed_cb(GtkWidget *w, gpointer data)
{
	ReceivePage *page = (ReceivePage *)data;
	gint seconds = gtk_spin_button_get_value_as_int (
		GTK_SPIN_BUTTON (page->autochk_widgets->autochk_sec_spinbtn));
	gint minutes = gtk_spin_button_get_value_as_int (
		GTK_SPIN_BUTTON (page->autochk_widgets->autochk_min_spinbtn));
	gint hours = gtk_spin_button_get_value_as_int (
		GTK_SPIN_BUTTON(page->autochk_widgets->autochk_hour_spinbtn));
	if (seconds < PREFS_RECV_AUTOCHECK_MIN_INTERVAL && minutes == 0 && hours == 0) {
		gtk_spin_button_set_value (
			GTK_SPIN_BUTTON (page->autochk_widgets->autochk_sec_spinbtn),
				PREFS_RECV_AUTOCHECK_MIN_INTERVAL);
	}
}

#ifdef USE_OAUTH2
//Automation of the oauth2 authorisation process to receive loopback callback generated by redirect in browser
static void prefs_account_oauth2_listener(GTask *task, gpointer source, gpointer task_data, GCancellable *cancellable)
{
	struct Oauth2Listener *oauth2_listener_data = (struct Oauth2Listener *)task_data;
	unsigned int socket_desc;
	int client_sock, c;
	struct sockaddr_in server , client;
	char client_message[2000];
	char *reply;
	char *reply_message;
	char *title;
	char *body;
	fd_set rfds;
	gint ret = 1;
	struct timeval timeout;

	debug_print("oauth2 listener task running\n");
     
	//Create socket
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
	if (socket_desc == -1)
	{
		debug_print("oauth2 listener could not create socket\n");
		g_task_return_boolean (task, TRUE);
		g_object_unref (task);
		return;
	}
	debug_print("oauth2 listener socket created\n");
	
	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons( 8888 );
	
	//Bind
	if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
	{
		close(socket_desc);
		debug_print("oauth2 listener bind failed\n");
		g_task_return_boolean (task, TRUE);
		g_object_unref (task);
		return;
	}
	debug_print("oauth2 listener bind done\n");

	listen(socket_desc , 1);
	
	//Accept and incoming connection
	debug_print("oauth2 listener waiting for incoming connections...\n");
	c = sizeof(struct sockaddr_in);
	
	do{
		FD_ZERO(&rfds);
		FD_SET(socket_desc, &rfds);
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		select(socket_desc+1, &rfds, NULL, NULL, &timeout);

		//select woke up, maybe accept connection from an incoming client
		if(FD_ISSET(socket_desc, &rfds)){
			client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
			if (client_sock < 0){
				debug_print("oauth2 listener accept failed\n");
				g_task_return_boolean (task, TRUE);
				g_object_unref (task);
				return;
			}
			debug_print("oauth2 listener connection accepted\n");

			//Receive message sent to the loopback address by the authorisation page
			prefs_account_oauth2_get_line(client_sock, client_message, sizeof(client_message));
			oauth2_listener_data->trim_text = g_strdup(client_message);
			g_strstrip(oauth2_listener_data->trim_text);

			ret = oauth2_obtain_tokens (oauth2_listener_data->service, oauth2_listener_data->OAUTH2Data, oauth2_listener_data->trim_text);

			if(!ret){
				oauth2_listener_data->success = TRUE;
				title = _("Authorisation complete");
				body = _("Your OAuth2 authorisation code has been received by Claws Mail");
			}else{
				//Something went wrong
				title = _("Authorisation NOT completed");
				body = _("Your OAuth2 authorisation code was not received by Claws Mail");
				log_message(LOG_PROTOCOL, "OAuth2 authorisation code not received\n");
			}
			reply_message = g_strconcat("<html><head><title>", title,
					"</title><meta charset=\"utf-8\"></head><body><h1>", title,
					"</h1><p>", body, "</p></body></html>", NULL);
			reply = g_strdup_printf(
					"HTTP/1.0 200 OK\r\nContent-Type: text/html\r\nContent-Length: %" G_GSIZE_FORMAT "\r\n\r\n%s",
					strlen(reply_message), reply_message);
			g_free(reply_message);
			write(client_sock, reply, strlen(reply));
			g_free(reply);
			close(client_sock);
		}
	}while(ret && !oauth2_listener_cancel);
	
	close(socket_desc);
	debug_print("oauth2 closing task\n");
	g_task_return_boolean (task, TRUE);
	g_object_unref (task);
}

static void prefs_account_oauth2_callback(GObject *source, GAsyncResult *res, gpointer user_data)
{
	struct Oauth2Listener *oauth2_listener_data = (struct Oauth2Listener *)user_data;

	if(oauth2_listener_data->success){
		debug_print("oauth2 listener callback storing data and updating GUI\n");

		if(oauth2_listener_data->OAUTH2Data->refresh_token != NULL){
			passwd_store_set_account(tmp_ac_prefs.account_id,
				PWS_ACCOUNT_OAUTH2_REFRESH,
				oauth2_listener_data->OAUTH2Data->refresh_token,
				FALSE);
			log_message(LOG_PROTOCOL, "OAuth2 refresh token stored\n");
		}

		if(oauth2_listener_data->OAUTH2Data->access_token != NULL){
			passwd_store_set_account(tmp_ac_prefs.account_id,
				    PWS_ACCOUNT_RECV,
					oauth2_listener_data->OAUTH2Data->access_token,
				    FALSE);
			passwd_store_set_account(tmp_ac_prefs.account_id,
					PWS_ACCOUNT_SEND,
				    oauth2_listener_data->OAUTH2Data->access_token,
				    FALSE);
			log_message(LOG_PROTOCOL, "OAuth2 access token stored\n");
		}

		if(oauth2_listener_data->OAUTH2Data->expiry_str != NULL){
			passwd_store_set_account(tmp_ac_prefs.account_id,
					PWS_ACCOUNT_OAUTH2_EXPIRY,
					oauth2_listener_data->OAUTH2Data->expiry_str,
				    FALSE);
			log_message(LOG_PROTOCOL, "OAuth2 access token expiry stored\n");
		}

		tmp_ac_prefs.oauth2_date = g_get_real_time () / G_USEC_PER_SEC;

		gtk_entry_set_text(GTK_ENTRY(oauth2_page.oauth2_authcode_entry), oauth2_listener_data->trim_text != NULL ? oauth2_listener_data->trim_text : "");
		gtk_widget_set_sensitive(oauth2_page.oauth2_authcode_entry, FALSE);
		gtk_widget_set_sensitive(oauth2_page.oauth2_authorise_btn, FALSE);
		gtk_entry_set_text(GTK_ENTRY(basic_page.pass_entry), oauth2_listener_data->OAUTH2Data->access_token);
		gtk_entry_set_text(GTK_ENTRY(send_page.smtp_pass_entry), oauth2_listener_data->OAUTH2Data->access_token);
	}

	debug_print("oauth2 listener callback freeing resources\n");
	g_free(oauth2_listener_data->trim_text);
	g_free(oauth2_listener_data->OAUTH2Data);
	g_free(oauth2_listener_data);
	oauth2_listener_cancel = 0;
	oauth2_listener_closed = 1;
}

static int prefs_account_oauth2_get_line(int sock, char *buf, int size)
{
	int i = 0;
	char c = '\0';
	int n;
	
	while ((i < size - 1) && (c != '\n')) {
                n = recv(sock, &c, 1, 0);
                //printf("%02X\n", c);
                if (n > 0) {
                        if (c == '\r') {
                                n = recv(sock, &c, 1, MSG_PEEK);
                                //printf("%02X\n", c);
                                if ((n > 0) && (c == '\n')) {
                                        n = recv(sock, &c, 1, 0);
                                        if (n < 0)
                                               log_message(LOG_PROTOCOL, "Receiving from pipe failed\n"); 
                                }
                                else
                                        c = '\n';
                        }
                        buf[i] = c;
                        i++;
                }
                else
                        c = '\n';
        }
        buf[i] = '\0';

        return (i);
}
#endif
