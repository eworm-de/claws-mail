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
#include "inc.h"
#include "menu.h"
#include "gtkutils.h"
#include "utils.h"
#include "alertpanel.h"

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
	GtkWidget *inbox_label;
	GtkWidget *inbox_entry;
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
	GtkWidget *getall_chkbtn;
	GtkWidget *recvatgetall_chkbtn;
	GtkWidget *filter_on_recv_chkbtn;
} receive;

static struct Send {
	GtkWidget *date_chkbtn;
	GtkWidget *msgid_chkbtn;

	GtkWidget *customhdr_chkbtn;

	GtkWidget *autocc_chkbtn;
	GtkWidget *autocc_entry;
	GtkWidget *autobcc_chkbtn;
	GtkWidget *autobcc_entry;
	GtkWidget *autoreplyto_chkbtn;
	GtkWidget *autoreplyto_entry;

	GtkWidget *smtp_auth_chkbtn;
	GtkWidget *pop_bfr_smtp_chkbtn;
} send;

static struct Compose {
	GtkWidget *sigpath_entry;
} compose;

#if USE_GPGME
static struct Privacy {
	GtkWidget *defaultkey_radiobtn;
	GtkWidget *emailkey_radiobtn;
	GtkWidget *customkey_radiobtn;
	GtkWidget *customkey_entry;
} privacy;
#endif /* USE_GPGME */

static struct Advanced {
	GtkWidget *smtpport_chkbtn;
	GtkWidget *smtpport_entry;
	GtkWidget *popport_chkbtn;
	GtkWidget *popport_entry;
	GtkWidget *domain_chkbtn;
	GtkWidget *domain_entry;
} advanced;

static void prefs_account_fix_size			(void);

static void prefs_account_protocol_set_data_from_optmenu(PrefParam *pparam);
static void prefs_account_protocol_set_optmenu		(PrefParam *pparam);
static void prefs_account_protocol_activated		(GtkMenuItem *menuitem);
#if USE_GPGME
static void prefs_account_sign_key_set_data_from_radiobtn (PrefParam *pparam);
static void prefs_account_sign_key_set_radiobtn		  (PrefParam *pparam);
#endif /* USE_GPGME */

static void prefs_account_nntpauth_toggled(GtkToggleButton *button,
					   gpointer user_data);
static void prefs_account_mailcmd_toggled(GtkToggleButton *button,
					  gpointer user_data);

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

	{"mail_command", "/usr/sbin/sendmail", &tmp_ac_prefs.mail_command, P_STRING,
	 &basic.mailcmd_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"use_nntp_auth", "FALSE", &tmp_ac_prefs.use_nntp_auth, P_BOOL,
	 &basic.nntpauth_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"user_id", "ENV_USER", &tmp_ac_prefs.userid, P_STRING,
	 &basic.uid_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"password", NULL, &tmp_ac_prefs.passwd, P_STRING,
	 &basic.pass_entry, prefs_set_data_from_entry, prefs_set_entry},

	{"inbox", "inbox", &tmp_ac_prefs.inbox, P_STRING,
	 &basic.inbox_entry, prefs_set_data_from_entry, prefs_set_entry},

	/* Receive */
	{"remove_mail", "TRUE", &tmp_ac_prefs.rmmail, P_BOOL,
	 &receive.rmmail_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"get_all_mail", "FALSE", &tmp_ac_prefs.getall, P_BOOL,
	 &receive.getall_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"receive_at_get_all", "TRUE", &tmp_ac_prefs.recv_at_getall, P_BOOL,
	 &receive.recvatgetall_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"filter_on_receive", "TRUE", &tmp_ac_prefs.filter_on_recv, P_BOOL,
	 &receive.filter_on_recv_chkbtn,
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

	{"set_autocc", "FALSE", &tmp_ac_prefs.set_autocc, P_BOOL,
	 &send.autocc_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"auto_cc", NULL, &tmp_ac_prefs.auto_cc, P_STRING,
	 &send.autocc_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"set_autobcc", "FALSE", &tmp_ac_prefs.set_autobcc, P_BOOL,
	 &send.autobcc_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"auto_bcc", NULL, &tmp_ac_prefs.auto_bcc, P_STRING,
	 &send.autobcc_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"set_autoreplyto", "FALSE", &tmp_ac_prefs.set_autoreplyto, P_BOOL,
	 &send.autoreplyto_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"auto_replyto", NULL, &tmp_ac_prefs.auto_replyto, P_STRING,
	 &send.autoreplyto_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

	{"use_smtp_auth", "FALSE", &tmp_ac_prefs.use_smtp_auth, P_BOOL,
	 &send.smtp_auth_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"pop_before_smtp", "FALSE", &tmp_ac_prefs.pop_before_smtp, P_BOOL,
	 &send.pop_bfr_smtp_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	/* Compose */
	{"signature_path", "~/"DEFAULT_SIGNATURE, &tmp_ac_prefs.sig_path, P_STRING,
	 &compose.sigpath_entry,
	 prefs_set_data_from_entry, prefs_set_entry},

#if USE_GPGME
	/* Privacy */
	{"sign_key", NULL, &tmp_ac_prefs.sign_key, P_ENUM,
	 &privacy.defaultkey_radiobtn,
	 prefs_account_sign_key_set_data_from_radiobtn,
	 prefs_account_sign_key_set_radiobtn},
	{"sign_key_id", NULL, &tmp_ac_prefs.sign_key_id, P_STRING,
	 &privacy.customkey_entry,
	 prefs_set_data_from_entry, prefs_set_entry},
#endif /* USE_GPGME */

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

	{"set_domain", "FALSE", &tmp_ac_prefs.set_domain, P_BOOL,
	 &advanced.domain_chkbtn,
	 prefs_set_data_from_toggle, prefs_set_toggle},

	{"domain", NULL, &tmp_ac_prefs.domain, P_STRING,
	 &advanced.domain_entry,
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
static void prefs_account_advanced_create	(void);

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

#define VSPACING		12
#define VSPACING_NARROW		4
#define BOX_BORDER		16
#define DEFAULT_ENTRY_WIDTH	80

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

	if (!account_list) return;

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

	inc_autocheck_timer_remove();

	cancelled = FALSE;

	if (!ac_prefs) {
		ac_prefs = g_new0(PrefsAccount, 1);
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
		gtk_window_set_title(GTK_WINDOW(dialog.window),
				     _("Preferences for new account"));
		gtk_widget_hide(dialog.apply_btn);
	} else {
		prefs_set_dialog(param);
		gtk_window_set_title(GTK_WINDOW(dialog.window),
				     _("Preferences for each account"));
		gtk_widget_show(dialog.apply_btn);
	}

	if (ac_prefs->protocol != A_LOCAL) {
		gtk_widget_set_sensitive(basic.smtpserv_entry, TRUE);
		gtk_widget_set_sensitive(basic.smtpserv_label, TRUE);
	}

	gtk_widget_show(dialog.window);
	gtk_main();
	gtk_widget_hide(dialog.window);

	inc_autocheck_timer_set();

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
	gtk_signal_connect(GTK_OBJECT(dialog.window), "focus_in_event",
			   GTK_SIGNAL_FUNC(manage_window_focus_in), NULL);
	gtk_signal_connect(GTK_OBJECT(dialog.window), "focus_out_event",
			   GTK_SIGNAL_FUNC(manage_window_focus_out), NULL);
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
	prefs_account_advanced_create();
	SET_NOTEBOOK_LABEL(dialog.notebook, _("Advanced"), page++);

	prefs_account_fix_size();
	gtk_widget_show(dialog.window);
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
	GtkWidget *inbox_label;
	GtkWidget *inbox_entry;
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
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), BOX_BORDER);

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

	default_chkbtn = gtk_check_button_new_with_label (_("Usually used"));
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

	inbox_label = gtk_label_new (_("Inbox"));
	gtk_widget_show (inbox_label);
	gtk_box_pack_start (GTK_BOX (hbox), inbox_label, FALSE, FALSE, 0);

	inbox_entry = gtk_entry_new ();
	gtk_widget_show (inbox_entry);
	gtk_widget_set_usize (inbox_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_box_pack_start (GTK_BOX (hbox), inbox_entry, TRUE, TRUE, 0);

	serv_table = gtk_table_new (6, 4, FALSE);
	gtk_widget_show (serv_table);
	gtk_box_pack_start (GTK_BOX (vbox2), serv_table, FALSE, FALSE, 0);
	gtk_table_set_row_spacings (GTK_TABLE (serv_table), VSPACING_NARROW);
	gtk_table_set_row_spacing (GTK_TABLE (serv_table), 3, 0);
	gtk_table_set_col_spacings (GTK_TABLE (serv_table), 8);

	nntpauth_chkbtn = gtk_check_button_new_with_label
		(_("This server requires authentication"));
	gtk_widget_show (nntpauth_chkbtn);
	gtk_table_attach (GTK_TABLE (serv_table), nntpauth_chkbtn, 0, 4, 4, 5,
			  GTK_FILL, 0, 0, 0);

	nntpserv_entry = gtk_entry_new ();
	gtk_widget_show (nntpserv_entry);
	gtk_table_attach (GTK_TABLE (serv_table), nntpserv_entry, 1, 4, 0, 1,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);
/*  	gtk_table_set_row_spacing (GTK_TABLE (serv_table), 0, 0); */

	recvserv_entry = gtk_entry_new ();
	gtk_widget_show (recvserv_entry);
	gtk_table_attach (GTK_TABLE (serv_table), recvserv_entry, 1, 4, 1, 2,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	localmbox_entry = gtk_entry_new ();
	gtk_widget_show (localmbox_entry);
	gtk_table_attach (GTK_TABLE (serv_table), localmbox_entry, 1, 4, 0, 1,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	smtpserv_entry = gtk_entry_new ();
	gtk_widget_show (smtpserv_entry);
	gtk_table_attach (GTK_TABLE (serv_table), smtpserv_entry, 1, 4, 2, 3,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	mailcmd_entry = gtk_entry_new ();
	gtk_widget_show (mailcmd_entry);
	gtk_table_attach (GTK_TABLE (serv_table), mailcmd_entry, 1, 4, 7, 8,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	uid_entry = gtk_entry_new ();
	gtk_widget_show (uid_entry);
	gtk_widget_set_usize (uid_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_table_attach (GTK_TABLE (serv_table), uid_entry, 1, 2, 5, 6,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL, 0, 0);

	pass_entry = gtk_entry_new ();
	gtk_widget_show (pass_entry);
	gtk_widget_set_usize (pass_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_table_attach (GTK_TABLE (serv_table), pass_entry, 3, 4, 5, 6,
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
	gtk_table_attach (GTK_TABLE (serv_table), recvserv_label, 0, 1, 1, 2,
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (recvserv_label), 1, 0.5);

	localmbox_label = gtk_label_new (_("Local mailbox file"));
	gtk_widget_show (localmbox_label);
	gtk_table_attach (GTK_TABLE (serv_table), localmbox_label, 0, 1, 0, 1,
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (localmbox_label), 1, 0.5);
/*  	gtk_table_set_row_spacing (GTK_TABLE (serv_table), 2, 0); */

	smtpserv_label = gtk_label_new (_("SMTP server (send)"));
	gtk_widget_show (smtpserv_label);
	gtk_table_attach (GTK_TABLE (serv_table), smtpserv_label, 0, 1, 2, 3,
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (smtpserv_label), 1, 0.5);
/*  	gtk_table_set_row_spacing (GTK_TABLE (serv_table), 2, 0); */

	mailcmd_chkbtn = gtk_check_button_new_with_label
		(_("Use mail command rather than SMTP server"));
	gtk_widget_show (mailcmd_chkbtn);
	gtk_table_attach (GTK_TABLE (serv_table), mailcmd_chkbtn, 0, 4, 6, 7,
			  GTK_EXPAND | GTK_FILL,
			  0, 0, TABLE_YPAD);
	gtk_signal_connect(GTK_OBJECT(mailcmd_chkbtn), "toggled",
			   GTK_SIGNAL_FUNC(prefs_account_mailcmd_toggled),
			   NULL);

	mailcmd_label = gtk_label_new (_("command to send mails"));
	gtk_widget_show (mailcmd_label);
	gtk_table_attach (GTK_TABLE (serv_table), mailcmd_label, 0, 1, 7, 8,
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (mailcmd_label), 1, 0.5);
/*  	gtk_table_set_row_spacing (GTK_TABLE (serv_table), 2, 0); */

	uid_label = gtk_label_new (_("User ID"));
	gtk_widget_show (uid_label);
	gtk_table_attach (GTK_TABLE (serv_table), uid_label, 0, 1, 5, 6,
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (uid_label), 1, 0.5);

	pass_label = gtk_label_new (_("Password"));
	gtk_widget_show (pass_label);
	gtk_table_attach (GTK_TABLE (serv_table), pass_label, 2, 3, 5, 6,
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
	basic.inbox_label      = inbox_label;
	basic.inbox_entry      = inbox_entry;
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
	GtkWidget *getall_chkbtn;
	GtkWidget *recvatgetall_chkbtn;
	GtkWidget *filter_on_recv_chkbtn;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), BOX_BORDER);

	PACK_FRAME (vbox1, frame1, _("POP3"));

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (frame1), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);

	PACK_CHECK_BUTTON (vbox2, rmmail_chkbtn,
			   _("Remove messages on server when received"));
	PACK_CHECK_BUTTON (vbox2, getall_chkbtn,
			   _("Receive all messages on server"));
	PACK_CHECK_BUTTON
		(vbox2, recvatgetall_chkbtn,
		 _("`Receive all' checks for new mail on this account"));
	PACK_CHECK_BUTTON (vbox2, filter_on_recv_chkbtn,
			   _("Filter messages on receiving"));

	receive.pop3_frame            = frame1;
	receive.rmmail_chkbtn         = rmmail_chkbtn;
	receive.getall_chkbtn         = getall_chkbtn;
	receive.recvatgetall_chkbtn   = recvatgetall_chkbtn;
	receive.filter_on_recv_chkbtn = filter_on_recv_chkbtn;
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
	GtkWidget *frame2;
	GtkWidget *table;
	GtkWidget *autocc_chkbtn;
	GtkWidget *autocc_entry;
	GtkWidget *autobcc_chkbtn;
	GtkWidget *autobcc_entry;
	GtkWidget *autoreplyto_chkbtn;
	GtkWidget *autoreplyto_entry;
	GtkWidget *frame3;
	GtkWidget *vbox3;
	GtkWidget *smtp_auth_chkbtn;
	GtkWidget *pop_bfr_smtp_chkbtn;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), BOX_BORDER);

	PACK_FRAME (vbox1, frame, _("Header"));

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
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

	PACK_FRAME (vbox1, frame2, _("Automatically set following addresses"));

	table =  gtk_table_new (3, 2, FALSE);
	gtk_widget_show (table);
	gtk_container_add (GTK_CONTAINER (frame2), table);
	gtk_container_set_border_width (GTK_CONTAINER (table), 8);
	gtk_table_set_row_spacings (GTK_TABLE (table), VSPACING_NARROW);
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

	PACK_FRAME (vbox1, frame3, _("Authentication"));

	vbox3 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox3);
	gtk_container_add (GTK_CONTAINER (frame3), vbox3);
	gtk_container_set_border_width (GTK_CONTAINER (vbox3), 8);

	PACK_CHECK_BUTTON (vbox3, smtp_auth_chkbtn,
		_("SMTP Authentication (SMTP AUTH)"));
	PACK_CHECK_BUTTON (vbox3, pop_bfr_smtp_chkbtn,
		_("Authenticate with POP3 before sending"));
	gtk_widget_set_sensitive(pop_bfr_smtp_chkbtn, FALSE);

	send.date_chkbtn      = date_chkbtn;
	send.msgid_chkbtn     = msgid_chkbtn;
	send.customhdr_chkbtn = customhdr_chkbtn;

	send.autocc_chkbtn      = autocc_chkbtn;
	send.autocc_entry       = autocc_entry;
	send.autobcc_chkbtn     = autobcc_chkbtn;
	send.autobcc_entry      = autobcc_entry;
	send.autoreplyto_chkbtn = autoreplyto_chkbtn;
	send.autoreplyto_entry  = autoreplyto_entry;

	send.smtp_auth_chkbtn    = smtp_auth_chkbtn;
	send.pop_bfr_smtp_chkbtn = pop_bfr_smtp_chkbtn;
}

static void prefs_account_compose_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *sigpath_entry;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), BOX_BORDER);

	hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox, FALSE, FALSE, 0);

	label = gtk_label_new (_("Signature file"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	sigpath_entry = gtk_entry_new ();
	gtk_widget_show (sigpath_entry);
	gtk_box_pack_start (GTK_BOX (hbox), sigpath_entry, TRUE, TRUE, 0);

	compose.sigpath_entry = sigpath_entry;
}

#if USE_GPGME
static void prefs_account_privacy_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *frame1;
	GtkWidget *vbox2;
	GtkWidget *hbox1;
	GtkWidget *label;
	GtkWidget *defaultkey_radiobtn;
	GtkWidget *emailkey_radiobtn;
	GtkWidget *customkey_radiobtn;
	GtkWidget *customkey_entry;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), BOX_BORDER);

	PACK_FRAME (vbox1, frame1, _("Sign key"));

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (frame1), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);

	defaultkey_radiobtn = gtk_radio_button_new_with_label
		(NULL, _("Use default GnuPG key"));
	gtk_widget_show (defaultkey_radiobtn);
	gtk_box_pack_start (GTK_BOX (vbox2), defaultkey_radiobtn,
			    FALSE, FALSE, 0);
	gtk_object_set_user_data (GTK_OBJECT (defaultkey_radiobtn),
				  GINT_TO_POINTER (SIGN_KEY_DEFAULT));

	emailkey_radiobtn = gtk_radio_button_new_with_label_from_widget
		(GTK_RADIO_BUTTON (defaultkey_radiobtn),
		 _("Select key by your email address"));
	gtk_widget_show (emailkey_radiobtn);
	gtk_box_pack_start (GTK_BOX (vbox2), emailkey_radiobtn,
			    FALSE, FALSE, 0);
	gtk_object_set_user_data (GTK_OBJECT (emailkey_radiobtn),
				  GINT_TO_POINTER (SIGN_KEY_BY_FROM));

	customkey_radiobtn = gtk_radio_button_new_with_label_from_widget
		(GTK_RADIO_BUTTON (defaultkey_radiobtn),
		 _("Specify key manually"));
	gtk_widget_show (customkey_radiobtn);
	gtk_box_pack_start (GTK_BOX (vbox2), customkey_radiobtn,
			    FALSE, FALSE, 0);
	gtk_object_set_user_data (GTK_OBJECT (customkey_radiobtn),
				  GINT_TO_POINTER (SIGN_KEY_CUSTOM));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

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

	privacy.defaultkey_radiobtn = defaultkey_radiobtn;
	privacy.emailkey_radiobtn = emailkey_radiobtn;
	privacy.customkey_radiobtn = customkey_radiobtn;
	privacy.customkey_entry = customkey_entry;
}
#endif /* USE_GPGME */

static void prefs_account_advanced_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *hbox1;
	GtkWidget *checkbtn_smtpport;
	GtkWidget *entry_smtpport;
	GtkWidget *hbox2;
	GtkWidget *checkbtn_popport;
	GtkWidget *entry_popport;
	GtkWidget *hbox3;
	GtkWidget *checkbtn_domain;
	GtkWidget *entry_domain;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (dialog.notebook), vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), BOX_BORDER);

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox1, checkbtn_smtpport, _("Specify SMTP port"));

	entry_smtpport = gtk_entry_new_with_max_length (5);
	gtk_widget_show (entry_smtpport);
	gtk_box_pack_start (GTK_BOX (hbox1), entry_smtpport, FALSE, FALSE, 0);
	gtk_widget_set_usize (entry_smtpport, 64, -1);
	SET_TOGGLE_SENSITIVITY(checkbtn_smtpport, entry_smtpport);

	hbox2 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox2);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox2, checkbtn_popport, _("Specify POP3 port"));

	entry_popport = gtk_entry_new_with_max_length (5);
	gtk_widget_show (entry_popport);
	gtk_box_pack_start (GTK_BOX (hbox2), entry_popport, FALSE, FALSE, 0);
	gtk_widget_set_usize (entry_popport, 64, -1);
	SET_TOGGLE_SENSITIVITY(checkbtn_popport, entry_popport);

	hbox3 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox3);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox3, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox3, checkbtn_domain, _("Specify domain name"));

	entry_domain = gtk_entry_new ();
	gtk_widget_show (entry_domain);
	gtk_box_pack_start (GTK_BOX (hbox3), entry_domain, TRUE, TRUE, 0);
	SET_TOGGLE_SENSITIVITY(checkbtn_domain, entry_domain);

	advanced.smtpport_chkbtn	= checkbtn_smtpport;
	advanced.smtpport_entry		= entry_smtpport;
	advanced.popport_chkbtn		= checkbtn_popport;
	advanced.popport_entry		= entry_popport;
	advanced.domain_chkbtn		= checkbtn_domain;
	advanced.domain_entry		= entry_domain;
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

	if (*gtk_entry_get_text(GTK_ENTRY(basic.addr_entry)) == '\0') {
		alertpanel_error(_("Mail address is not entered."));
		return -1;
	}
	if ((protocol == A_POP3 || protocol == A_APOP || (protocol == A_LOCAL && !gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(basic.mailcmd_chkbtn)))) &&
           *gtk_entry_get_text(GTK_ENTRY(basic.smtpserv_entry)) == '\0') {
		alertpanel_error(_("SMTP server is not entered."));
		return -1;
	}
	if ((protocol == A_POP3 || protocol == A_APOP || protocol == A_LOCAL) &&
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

static void prefs_account_edit_custom_header(void)
{
	prefs_custom_header_open(&tmp_ac_prefs);
}

#if USE_GPGME
static void prefs_account_sign_key_set_data_from_radiobtn(PrefParam *pparam)
{
	GtkRadioButton *radiobtn;
	GSList *group;

	radiobtn = GTK_RADIO_BUTTON (*pparam->widget);
	group = gtk_radio_button_group (radiobtn);
	while (group != NULL) {
		GtkToggleButton *btn = GTK_TOGGLE_BUTTON (group->data);
		if (gtk_toggle_button_get_active (btn)) {
			*((SignKeyType *)pparam->data) = GPOINTER_TO_INT
				(gtk_object_get_user_data (GTK_OBJECT (btn)));
			break;
		}
		group = group->next;
	}
}

static void prefs_account_sign_key_set_radiobtn(PrefParam *pparam)
{
	GtkRadioButton *radiobtn;
	GSList *group;
	gpointer data;

	data = GINT_TO_POINTER (*((RecvProtocol *)pparam->data));
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

	protocol = *((RecvProtocol *)pparam->data);

	switch (protocol) {
	case A_POP3:
		gtk_option_menu_set_history(optmenu, 0);
		break;
	case A_APOP:
		gtk_option_menu_set_history(optmenu, 1);
		break;
	case A_IMAP4:
		gtk_option_menu_set_history(optmenu, 2);
		break;
	case A_NNTP:
		gtk_option_menu_set_history(optmenu, 3);
		break;
	case A_LOCAL:
		gtk_option_menu_set_history(optmenu, 4);
		break;
		/*
	case A_LOCAL_CMD:
		gtk_option_menu_set_history(optmenu, 5);
		break;
		*/
	default:
	}

	menu = gtk_option_menu_get_menu(optmenu);
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	gtk_menu_item_activate(GTK_MENU_ITEM(menuitem));
}

static void prefs_account_protocol_activated(GtkMenuItem *menuitem)
{
	RecvProtocol protocol;
	gboolean active;

	protocol = GPOINTER_TO_INT
		(gtk_object_get_user_data(GTK_OBJECT(menuitem)));

	switch(protocol) {
	case A_NNTP:
		gtk_widget_set_sensitive(basic.inbox_label, FALSE);
		gtk_widget_set_sensitive(basic.inbox_entry, FALSE);
		gtk_widget_show(basic.nntpserv_label);
		gtk_widget_show(basic.nntpserv_entry);
		gtk_widget_set_sensitive(basic.nntpauth_chkbtn, TRUE);
		gtk_widget_show(basic.nntpauth_chkbtn);
		gtk_widget_hide(basic.recvserv_label);
		gtk_widget_hide(basic.recvserv_entry);
		gtk_widget_hide(basic.smtpserv_label);
		gtk_widget_hide(basic.smtpserv_entry);
		gtk_widget_hide(basic.localmbox_label);
		gtk_widget_hide(basic.localmbox_entry);
		gtk_widget_hide(basic.mailcmd_label);
		gtk_widget_hide(basic.mailcmd_entry);
		gtk_widget_hide(basic.mailcmd_chkbtn);
/*  		gtk_table_set_row_spacing (GTK_TABLE (basic.serv_table), 3, 0); */
		/* update userid/passwd sensitive state */
		prefs_account_nntpauth_toggled
			(GTK_TOGGLE_BUTTON(basic.nntpauth_chkbtn), NULL);
		gtk_widget_set_sensitive(receive.pop3_frame, FALSE);
		break;
	case A_LOCAL:
		gtk_widget_set_sensitive(basic.inbox_label, TRUE);
		gtk_widget_set_sensitive(basic.inbox_entry, TRUE);
		gtk_widget_hide(basic.nntpserv_label);
		gtk_widget_hide(basic.nntpserv_entry);
		gtk_widget_set_sensitive(basic.nntpauth_chkbtn, FALSE);
		gtk_widget_hide(basic.nntpauth_chkbtn);
		gtk_widget_set_sensitive(basic.recvserv_label, FALSE);
		gtk_widget_set_sensitive(basic.recvserv_entry, FALSE);
		gtk_widget_hide(basic.recvserv_label);
		gtk_widget_hide(basic.recvserv_entry);
		gtk_widget_show(basic.smtpserv_label);
		gtk_widget_show(basic.smtpserv_entry);
		gtk_widget_show(basic.localmbox_label);
		gtk_widget_show(basic.localmbox_entry);
		gtk_widget_show(basic.mailcmd_label);
		gtk_widget_show(basic.mailcmd_entry);
		gtk_widget_show(basic.mailcmd_chkbtn);
/*  		gtk_table_set_row_spacing */
/*  			(GTK_TABLE (basic.serv_table), 3, VSPACING_NARROW); */
		gtk_widget_set_sensitive(basic.uid_label,  TRUE);
		gtk_widget_set_sensitive(basic.pass_label, TRUE);
		gtk_widget_set_sensitive(basic.uid_entry,  TRUE);
		gtk_widget_set_sensitive(basic.pass_entry, TRUE);
		gtk_widget_set_sensitive(receive.pop3_frame, FALSE);
		prefs_account_mailcmd_toggled
			(GTK_TOGGLE_BUTTON(basic.mailcmd_chkbtn), NULL);
		break;
		/*
	case A_LOCAL_CMD:
		gtk_widget_set_sensitive(basic.inbox_label, TRUE);
		gtk_widget_set_sensitive(basic.inbox_entry, TRUE);
		gtk_widget_hide(basic.nntpserv_label);
		gtk_widget_hide(basic.nntpserv_entry);
		gtk_widget_hide(basic.nntpauth_chkbtn);
		gtk_widget_set_sensitive(basic.recvserv_label, FALSE);
		gtk_widget_set_sensitive(basic.recvserv_entry, FALSE);
		gtk_widget_hide(basic.recvserv_label);
		gtk_widget_hide(basic.recvserv_entry);
		gtk_widget_hide(basic.smtpserv_label);
		gtk_widget_hide(basic.smtpserv_entry);
		gtk_widget_show(basic.localmbox_label);
		gtk_widget_show(basic.localmbox_entry);
		gtk_widget_show(basic.mailcmd_label);
		gtk_widget_show(basic.mailcmd_entry);
		gtk_widget_hide(basic.mailcmd_chkbtn);
		gtk_table_set_row_spacing
			(GTK_TABLE (basic.serv_table), 3, VSPACING_NARROW);
		gtk_widget_set_sensitive(basic.uid_label,  FALSE);
		gtk_widget_set_sensitive(basic.pass_label, FALSE);
		gtk_widget_set_sensitive(basic.uid_entry,  FALSE);
		gtk_widget_set_sensitive(basic.pass_entry, FALSE);
		gtk_widget_set_sensitive(receive.pop3_frame, FALSE);
		break;
		*/
	case A_IMAP4:
		gtk_widget_set_sensitive(basic.inbox_label, TRUE);
		gtk_widget_set_sensitive(basic.inbox_entry, TRUE);
		gtk_widget_hide(basic.nntpserv_label);
		gtk_widget_hide(basic.nntpserv_entry);
		gtk_widget_set_sensitive(basic.nntpauth_chkbtn, FALSE);
		gtk_widget_hide(basic.nntpauth_chkbtn);
		gtk_widget_set_sensitive(basic.recvserv_label, TRUE);
		gtk_widget_set_sensitive(basic.recvserv_entry, TRUE);
		gtk_widget_show(basic.recvserv_label);
		gtk_widget_show(basic.recvserv_entry);
		gtk_widget_show(basic.smtpserv_label);
		gtk_widget_show(basic.smtpserv_entry);
		gtk_widget_hide(basic.localmbox_label);
		gtk_widget_hide(basic.localmbox_entry);
		gtk_widget_hide(basic.mailcmd_label);
		gtk_widget_hide(basic.mailcmd_entry);
		gtk_widget_hide(basic.mailcmd_chkbtn);
/*  		gtk_table_set_row_spacing */
/*  			(GTK_TABLE (basic.serv_table), 3, VSPACING_NARROW); */
		gtk_widget_set_sensitive(basic.uid_label,  TRUE);
		gtk_widget_set_sensitive(basic.pass_label, TRUE);
		gtk_widget_set_sensitive(basic.uid_entry,  TRUE);
		gtk_widget_set_sensitive(basic.pass_entry, TRUE);
		gtk_widget_set_sensitive(receive.pop3_frame, FALSE);
		gtk_widget_set_sensitive(basic.smtpserv_entry, TRUE);
		gtk_widget_set_sensitive(basic.smtpserv_label, TRUE);
		break;
	case A_POP3:
	default:
		gtk_widget_set_sensitive(basic.inbox_label, TRUE);
		gtk_widget_set_sensitive(basic.inbox_entry, TRUE);
		gtk_widget_set_sensitive(basic.nntpauth_chkbtn, FALSE);
		gtk_widget_hide(basic.nntpserv_label);
		gtk_widget_hide(basic.nntpserv_entry);
		gtk_widget_set_sensitive(basic.nntpauth_chkbtn, FALSE);
		gtk_widget_hide(basic.nntpauth_chkbtn);
		gtk_widget_set_sensitive(basic.recvserv_label, TRUE);
		gtk_widget_set_sensitive(basic.recvserv_entry, TRUE);
		gtk_widget_show(basic.recvserv_label);
		gtk_widget_show(basic.recvserv_entry);
		gtk_widget_show(basic.smtpserv_label);
		gtk_widget_show(basic.smtpserv_entry);
		gtk_widget_hide(basic.localmbox_label);
		gtk_widget_hide(basic.localmbox_entry);
		gtk_widget_hide(basic.mailcmd_label);
		gtk_widget_hide(basic.mailcmd_entry);
		gtk_widget_hide(basic.mailcmd_chkbtn);
/*  		gtk_table_set_row_spacing */
/*  			(GTK_TABLE (basic.serv_table), 3, VSPACING_NARROW); */
		gtk_widget_set_sensitive(basic.uid_label,  TRUE);
		gtk_widget_set_sensitive(basic.pass_label, TRUE);
		gtk_widget_set_sensitive(basic.uid_entry,  TRUE);
		gtk_widget_set_sensitive(basic.pass_entry, TRUE);
		gtk_widget_set_sensitive(receive.pop3_frame, TRUE);
		gtk_widget_set_sensitive(basic.smtpserv_entry, TRUE);
		gtk_widget_set_sensitive(basic.smtpserv_label, TRUE);
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
	gtk_widget_set_sensitive(basic.uid_label,  !use_mailcmd);
	gtk_widget_set_sensitive(basic.pass_label, !use_mailcmd);
	gtk_widget_set_sensitive(basic.uid_entry,  !use_mailcmd);
	gtk_widget_set_sensitive(basic.pass_entry, !use_mailcmd);
}
