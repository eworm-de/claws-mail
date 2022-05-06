/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2004-2015 the Claws Mail team
 * Copyright (C) 2014-2015 Charles Lehner
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
 * 
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#include "claws-features.h"
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "defs.h"
#include "gtk/gtkutils.h"
#include "gtk/combobox.h"
#include "alertpanel.h"
#include "passcrypt.h"
#include "password.h"
#include "passwordstore.h"
#include "utils.h"
#include "prefs.h"
#include "prefs_gtk.h"
#include "sieve_prefs.h"
#include "managesieve.h"

#define PREFS_BLOCK_NAME "ManageSieve"

SieveConfig sieve_config;

static PrefParam prefs[] = {
        {"manager_win_width", "-1", &sieve_config.manager_win_width,
		P_INT, NULL, NULL, NULL},
        {"manager_win_height", "-1", &sieve_config.manager_win_height,
		P_INT, NULL, NULL, NULL},
        {0,0,0,0,0,0,0}
};

#define PACK_HBOX(hbox, vbox) \
{ \
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5); \
	gtk_widget_show (hbox); \
	gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0); \
}

#define RADIO_ADD(radio, group, hbox, vbox, label) \
{ \
	PACK_HBOX(hbox, vbox); \
	gtk_container_set_border_width(GTK_CONTAINER (hbox), 0); \
	radio = gtk_radio_button_new_with_label(group, label); \
	group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio)); \
	gtk_widget_show(radio); \
	gtk_box_pack_start(GTK_BOX(hbox), radio, FALSE, FALSE, 0); \
}

struct SieveAccountPage
{
	PrefsPage page;

	GtkWidget *enable_checkbtn;
	GtkWidget *serv_frame;
	GtkWidget *auth_frame;
	GtkWidget *host_checkbtn, *host_entry;
	GtkWidget *port_checkbtn, *port_spinbtn;
	GtkWidget *tls_radio_no, *tls_radio_maybe, *tls_radio_yes;
	GtkWidget *auth_radio_noauth, *auth_radio_reuse, *auth_radio_custom;
	GtkWidget *auth_custom_vbox, *auth_method_hbox;
	GtkWidget *uid_entry;
	GtkWidget *pass_entry;
	GtkWidget *auth_menu;

	PrefsAccount *account;
};

static struct SieveAccountPage account_page;

static void update_auth_sensitive(struct SieveAccountPage *page)
{
	gboolean use_auth, custom;

	custom = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->auth_radio_custom));
	use_auth = custom || gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->auth_radio_reuse));

	gtk_widget_set_sensitive(GTK_WIDGET(page->auth_custom_vbox), custom);
	gtk_widget_set_sensitive(GTK_WIDGET(page->auth_method_hbox), use_auth);
}

static void auth_toggled(GtkToggleButton *togglebutton,
		gpointer user_data)
{
	struct SieveAccountPage *page = (struct SieveAccountPage *) user_data;
	update_auth_sensitive(page);
}

static void sieve_prefs_account_create_widget_func(PrefsPage *_page,
						 GtkWindow *window,
						 gpointer data)
{
	struct SieveAccountPage *page = (struct SieveAccountPage *) _page;
	PrefsAccount *account = (PrefsAccount *) data;
	SieveAccountConfig *config;
	gchar *pass;

	GtkWidget *page_vbox, *sieve_vbox;
	GtkWidget *hbox;
	GtkWidget *hbox_spc;

	GtkWidget *enable_checkbtn;
	GtkWidget *serv_vbox, *tls_frame;
	GtkWidget *tls_vbox, *serv_frame;
	GtkWidget *auth_vbox, *auth_frame;
	GtkWidget *auth_custom_vbox, *auth_method_hbox;
	GtkSizeGroup *size_group;
	GtkWidget *host_checkbtn, *host_entry;
	GtkWidget *port_checkbtn, *port_spinbtn;
	GSList *tls_group = NULL;
	GSList *auth_group = NULL;
	GtkWidget *tls_radio_no, *tls_radio_maybe, *tls_radio_yes;
	GtkWidget *auth_radio_noauth, *auth_radio_reuse, *auth_radio_custom;
	GtkWidget *label;
	GtkWidget *uid_entry;
	GtkWidget *pass_entry;
	GtkWidget *auth_menu;
	GtkListStore *menu;
	GtkTreeIter iter;

	page_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING);
	gtk_widget_show (page_vbox);
	gtk_container_set_border_width (GTK_CONTAINER (page_vbox), VBOX_BORDER);

	/* Enable/disable */
	PACK_CHECK_BUTTON (page_vbox, enable_checkbtn,
			   _("Enable Sieve"));

	sieve_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING);
	gtk_widget_show (sieve_vbox);
	gtk_box_pack_start (GTK_BOX (page_vbox), sieve_vbox, FALSE, FALSE, 0);

	/* Server info */
	serv_vbox = gtkut_get_options_frame(sieve_vbox, &serv_frame, _("Server information"));

	SET_TOGGLE_SENSITIVITY (enable_checkbtn, sieve_vbox);
	size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

	/* Server name */
	PACK_HBOX (hbox, serv_vbox);
	PACK_CHECK_BUTTON (hbox, host_checkbtn, _("Server name"));
	gtk_size_group_add_widget(size_group, host_checkbtn);

	host_entry = gtk_entry_new();
	gtk_entry_set_max_length(GTK_ENTRY(host_entry), 255);
	gtk_widget_show (host_entry);
	gtk_box_pack_start (GTK_BOX (hbox), host_entry, TRUE, TRUE, 0);
	SET_TOGGLE_SENSITIVITY (host_checkbtn, host_entry);
	CLAWS_SET_TIP(hbox,
		_("Connect to this host instead of the host used for receiving mail"));

	/* Port */
	PACK_HBOX (hbox, serv_vbox);
	PACK_CHECK_BUTTON (hbox, port_checkbtn, _("Server port"));
	port_spinbtn = gtk_spin_button_new_with_range(1, 65535, 1);
	gtk_widget_show (port_spinbtn);
	gtk_box_pack_start (GTK_BOX (hbox), port_spinbtn, FALSE, FALSE, 0);
	SET_TOGGLE_SENSITIVITY (port_checkbtn, port_spinbtn);
	gtk_size_group_add_widget(size_group, port_checkbtn);
	CLAWS_SET_TIP(hbox,
		_("Connect to this port instead of the default"));

	/* Encryption */

	tls_vbox = gtkut_get_options_frame(sieve_vbox, &tls_frame, _("Encryption"));

	RADIO_ADD(tls_radio_no, tls_group, hbox, tls_vbox,
			_("No encryption"));
	RADIO_ADD(tls_radio_maybe, tls_group, hbox, tls_vbox,
			_("Use STARTTLS when available"));
	RADIO_ADD(tls_radio_yes, tls_group, hbox, tls_vbox,
			_("Require STARTTLS"));

	/* Authentication */

	auth_vbox = gtkut_get_options_frame(sieve_vbox, &auth_frame,
			_("Authentication"));

	RADIO_ADD(auth_radio_noauth, auth_group, hbox, auth_vbox,
		_("No authentication"));
	RADIO_ADD(auth_radio_reuse, auth_group, hbox, auth_vbox,
		_("Use same authentication as for receiving mail"));
	RADIO_ADD(auth_radio_custom, auth_group, hbox, auth_vbox,
		_("Specify authentication"));

	g_signal_connect(G_OBJECT(auth_radio_custom), "toggled",
			G_CALLBACK(auth_toggled), page);
	g_signal_connect(G_OBJECT(auth_radio_reuse), "toggled",
			G_CALLBACK(auth_toggled), page);

	/* Custom Auth Settings */

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (auth_vbox), hbox, FALSE, FALSE, 0);

	hbox_spc = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (hbox), hbox_spc, FALSE, FALSE, 0);
	gtk_widget_set_size_request (hbox_spc, 12, -1);

	auth_custom_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING/2);
	gtk_widget_show (auth_custom_vbox);
	gtk_container_set_border_width (GTK_CONTAINER (auth_custom_vbox), 0);
	gtk_box_pack_start (GTK_BOX (hbox), auth_custom_vbox, TRUE, TRUE, 0);

	/* User ID + Password */

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (auth_custom_vbox), hbox, FALSE, FALSE, 0);

	/* User ID*/
	label = gtk_label_new (_("User ID"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	uid_entry = gtk_entry_new ();
	gtk_widget_show (uid_entry);
	gtk_widget_set_size_request (uid_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_box_pack_start (GTK_BOX (hbox), uid_entry, TRUE, TRUE, 0);

	/* Password */
	label = gtk_label_new (_("Password"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	pass_entry = gtk_entry_new ();
	gtk_widget_show (pass_entry);
	gtk_widget_set_size_request (pass_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_entry_set_visibility (GTK_ENTRY (pass_entry), FALSE);
	gtk_box_pack_start (GTK_BOX (hbox), pass_entry, TRUE, TRUE, 0);

	/* Authentication method */

	auth_method_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (auth_method_hbox);
	gtk_box_pack_start (GTK_BOX (auth_vbox), auth_method_hbox, FALSE, FALSE, 0);

	label = gtk_label_new (_("Authentication method"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (auth_method_hbox), label, FALSE, FALSE, 0);

	auth_menu = gtkut_sc_combobox_create(NULL, FALSE);
	menu = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(auth_menu)));
	gtk_widget_show (auth_menu);
	gtk_box_pack_start (GTK_BOX (auth_method_hbox), auth_menu, FALSE, FALSE, 0);

	COMBOBOX_ADD (menu, _("Automatic"), SIEVEAUTH_AUTO);
	COMBOBOX_ADD (menu, NULL, 0);
	COMBOBOX_ADD (menu, "PLAIN", SIEVEAUTH_PLAIN);
	COMBOBOX_ADD (menu, "LOGIN", SIEVEAUTH_LOGIN);
	COMBOBOX_ADD (menu, "CRAM-MD5", SIEVEAUTH_CRAM_MD5);

	/* Populate config */

	config = sieve_prefs_account_get_config(account);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_checkbtn), config->enable);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(host_checkbtn), config->use_host);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(port_checkbtn), config->use_port);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(port_spinbtn), (float) config->port);

	if (config->host != NULL)
		gtk_entry_set_text(GTK_ENTRY(host_entry), config->host);
	if (config->userid != NULL)
		gtk_entry_set_text(GTK_ENTRY(uid_entry), config->userid);
	if ((pass = passwd_store_get_account(account->account_id,
				     "sieve")) != NULL) {
		gtk_entry_set_text(GTK_ENTRY(pass_entry), pass);
		memset(pass, 0, strlen(pass));
		g_free(pass);
	}

	combobox_select_by_data(GTK_COMBO_BOX(auth_menu), config->auth_type);

	/* Add items to page struct */
	page->account = account;
	page->enable_checkbtn = enable_checkbtn;
	page->serv_frame = serv_frame;
	page->auth_frame = auth_frame;
	page->auth_custom_vbox = auth_custom_vbox;
	page->auth_method_hbox = auth_method_hbox;
	page->host_checkbtn = host_checkbtn;
	page->host_entry = host_entry;
	page->port_checkbtn = port_checkbtn;
	page->port_spinbtn = port_spinbtn;
	page->auth_radio_noauth = auth_radio_noauth;
	page->auth_radio_reuse = auth_radio_reuse;
	page->auth_radio_custom = auth_radio_custom;
	page->tls_radio_no = tls_radio_no;
	page->tls_radio_maybe = tls_radio_maybe;
	page->tls_radio_yes = tls_radio_yes;
	page->uid_entry = uid_entry;
	page->pass_entry = pass_entry;
	page->auth_menu = auth_menu;
	page->page.widget = page_vbox;

	/* Update things */

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
				config->tls_type == SIEVE_TLS_NO ? tls_radio_no :
				config->tls_type == SIEVE_TLS_MAYBE ? tls_radio_maybe :
				tls_radio_yes), TRUE);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(
				config->auth == SIEVEAUTH_REUSE ? auth_radio_reuse :
				config->auth == SIEVEAUTH_CUSTOM ? auth_radio_custom :
				auth_radio_noauth), TRUE);

	update_auth_sensitive(page);

	/* Free things */
	g_object_unref(G_OBJECT(size_group));

	sieve_prefs_account_free_config(config);
}

static void sieve_prefs_account_destroy_widget_func(PrefsPage *_page)
{
}

static gint sieve_prefs_account_apply(struct SieveAccountPage *page)
{
	SieveAccountConfig *config;

	config = sieve_prefs_account_get_config(page->account);

	config->enable = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->enable_checkbtn));
	config->use_port = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->port_checkbtn));
	config->use_host = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->host_checkbtn));
	config->port = (gushort)gtk_spin_button_get_value_as_int
			(GTK_SPIN_BUTTON(page->port_spinbtn));

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->auth_radio_noauth)))
		config->auth = SIEVEAUTH_NONE;
	else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->auth_radio_reuse)))
		config->auth = SIEVEAUTH_REUSE;
	else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->auth_radio_custom)))
		config->auth = SIEVEAUTH_CUSTOM;

	config->tls_type =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->tls_radio_no)) ?
			SIEVE_TLS_NO :
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->tls_radio_maybe)) ?
			SIEVE_TLS_MAYBE :
			SIEVE_TLS_YES;

	g_free(config->host);
	g_free(config->userid);

	config->host = gtk_editable_get_chars(GTK_EDITABLE(page->host_entry), 0, -1);
	config->userid = gtk_editable_get_chars(GTK_EDITABLE(page->uid_entry), 0, -1);
	gchar *pwd = gtk_editable_get_chars(GTK_EDITABLE(page->pass_entry), 0, -1);
	passwd_store_set_account(page->account->account_id, "sieve", pwd, FALSE);
	memset(pwd, 0, strlen(pwd));
	g_free(pwd);
	config->auth_type = combobox_get_active_data(GTK_COMBO_BOX(page->auth_menu));

	sieve_prefs_account_set_config(page->account, config);
	sieve_prefs_account_free_config(config);
	return TRUE;
}

static gboolean sieve_prefs_account_check(struct SieveAccountPage *page)
{
	if (strchr(gtk_entry_get_text(GTK_ENTRY(page->host_entry)), ' ')) {
		alertpanel_error(_("Sieve server must not contain a space."));
		return FALSE;
	}

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->host_checkbtn)) &&
			*gtk_entry_get_text(GTK_ENTRY(page->host_entry)) == '\0') {
		alertpanel_error(_("Sieve server is not entered."));
		return FALSE;
	}

	return TRUE;
}

static void sieve_prefs_account_save_func(PrefsPage *_page)
{
	struct SieveAccountPage *page = (struct SieveAccountPage *) _page;
	if (sieve_prefs_account_check(page)) {
		sieve_prefs_account_apply(page);
	}
}

static gboolean sieve_prefs_account_can_close(PrefsPage *_page)
{
	struct SieveAccountPage *page = (struct SieveAccountPage *) _page;
	return sieve_prefs_account_check(page);
}

void sieve_prefs_init()
{
	gchar *rcpath;

	/* Account prefs */
	static gchar *path[3];
	path[0] = _("Plugins");
	path[1] = _("Sieve");
	path[2] = NULL;

	account_page.page.path = path;
	account_page.page.create_widget = sieve_prefs_account_create_widget_func;
	account_page.page.destroy_widget = sieve_prefs_account_destroy_widget_func;
	account_page.page.save_page = sieve_prefs_account_save_func;
	account_page.page.can_close = sieve_prefs_account_can_close;
	account_page.page.weight = 30.0;
	prefs_account_register_page((PrefsPage *) &account_page);

	/* Common prefs */
	prefs_set_default(prefs);
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	prefs_read_config(prefs, PREFS_BLOCK_NAME, rcpath, NULL);
	g_free(rcpath);
}

void sieve_prefs_done(void)
{
	PrefFile *pref_file;
	gchar *rc_file_path;

	prefs_account_unregister_page((PrefsPage *) &account_page);

	rc_file_path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
				   COMMON_RC, NULL);
	pref_file = prefs_write_open(rc_file_path);
	g_free(rc_file_path);

	if (!pref_file || prefs_set_block_label(pref_file, PREFS_BLOCK_NAME) < 0)
		return;

	if (prefs_write_param(prefs, pref_file->fp) < 0) {
		g_warning("failed to write ManageSieve plugin configuration");
		prefs_file_close_revert(pref_file);
		return;
	}

	if (fprintf(pref_file->fp, "\n") < 0) {
		FILE_OP_ERROR(rc_file_path, "fprintf");
		prefs_file_close_revert(pref_file);
	} else
		prefs_file_close(pref_file);
}

struct SieveAccountConfig *sieve_prefs_account_get_config(
		PrefsAccount *account)
{
	SieveAccountConfig *config;
	const gchar *confstr;
	gchar enc_userid[256], enc_passwd[256];
	gchar enable, use_host, use_port;
	guchar tls_type, auth, auth_type;
	gsize len;
	gint num;
#if defined(G_OS_WIN32) || defined(__OpenBSD__) || defined(__FreeBSD__)
	/* Non-GNU sscanf() does not understand the %ms format, so we
	 * have to do the allocation of target buffer ourselves before
	 * calling sscanf(), and copy the host string to config->host.
	 */
	gchar tmphost[256];
#endif

	config = g_new0(SieveAccountConfig, 1);

	config->enable = FALSE;
	config->use_host = FALSE;
	config->host = NULL;
	config->use_port = FALSE;
	config->port = 4190;
	config->tls_type = SIEVE_TLS_YES;
	config->auth = SIEVEAUTH_REUSE;
	config->auth_type = SIEVEAUTH_AUTO;
	config->userid = NULL;

	confstr = prefs_account_get_privacy_prefs(account, "sieve");
	if (confstr == NULL)
		return config;

	enc_userid[0] = '\0';
	enc_passwd[0] = '\0';
#if defined(G_OS_WIN32) || defined(__OpenBSD__) || defined(__FreeBSD__)
	if ((num = sscanf(confstr, "%c%c %255s %c%hu %hhu %hhu %hhu %255s %255s",
#else
	if ((num = sscanf(confstr, "%c%c %ms %c%hu %hhu %hhu %hhu %255s %255s",
#endif
			&enable, &use_host,
#if defined(G_OS_WIN32) || defined(__OpenBSD__) || defined(__FreeBSD__)
			tmphost,
#else
			&config->host,
#endif
			&use_port, &config->port,
			&tls_type,
			&auth,
			&auth_type,
			enc_userid,
			enc_passwd)) != 10) {
			/* This (10th element missing) will happen on any recent
			 * configuration, where the password is already in
			 * passwordstore, and not in this config string. We have
			 * to read the 10th element in order not to break older
			 * configurations, and to move the password to password
			 * store.
			 * The userid may be missing if it is unset.
			 * If there are not 10, 9 or 8 elements, something is wrong. */
		if (num != 9 && num != 8) {
			g_warning("failed reading Sieve config elements");
		}
	}
	debug_print("Read %d Sieve config elements\n", num);

	/* Scan enums separately, for endian purposes */
	config->tls_type = tls_type;
	config->auth = auth;
	config->auth_type = auth_type;

#if defined(G_OS_WIN32) || defined(__OpenBSD__) || defined(__FreeBSD__)
	config->host = g_strndup(tmphost, 255);
#endif

	config->enable = enable == 'y';
	config->use_host = use_host == 'y';
	config->use_port = use_port == 'y';

	if (config->host != NULL && config->host[0] == '!' && !config->host[1]) {
		g_free(config->host);
		config->host = NULL;
	}

	config->userid = g_base64_decode(enc_userid, &len);

	/* migrate password from passcrypt to passwordstore, if
	 * it's not there yet */
	if (enc_passwd[0] != '\0' &&
			!passwd_store_has_password_account(account->account_id, "sieve")) {
		gchar *pass = g_base64_decode(enc_passwd, &len);
		passcrypt_decrypt(pass, len);
		passwd_store_set_account(account->account_id, "sieve",
				pass, FALSE);
		g_free(pass);
	}

	return config;
}

void sieve_prefs_account_set_config(
		PrefsAccount *account, SieveAccountConfig *config)
{
	gchar *confstr = NULL;
	gchar *enc_userid = NULL;
	gsize len;

	if (config->userid) {
		len = strlen(config->userid);
		enc_userid = g_base64_encode(config->userid, len);
	}

	confstr = g_strdup_printf("%c%c %s %c%hu %hu %hu %hu %s",
			config->enable ? 'y' : 'n',
			config->use_host ? 'y' : 'n',
			config->host && config->host[0] ? config->host : "!",
			config->use_port ? 'y' : 'n',
			config->port,
			(unsigned short)config->tls_type,
			(unsigned short)config->auth,
			(unsigned short)config->auth_type,
			enc_userid ? enc_userid : "");

	if (enc_userid)
		g_free(enc_userid);

	prefs_account_set_privacy_prefs(account, "sieve", confstr);

	g_free(confstr);

	sieve_account_prefs_updated(account);
}

void sieve_prefs_account_free_config(SieveAccountConfig *config)
{
	g_free(config->host);
	g_free(config->userid);
	g_free(config);
}

