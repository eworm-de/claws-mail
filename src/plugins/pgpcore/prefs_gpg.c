/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2004 Hiroyuki Yamamoto & the Sylpheed-Claws team
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

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "defs.h"
#include "utils.h"
#include "prefs.h"
#include "prefs_gtk.h"
#include "prefs_gpg.h"

struct GPGConfig prefs_gpg;

static PrefParam param[] = {
	/* Privacy */
	{"auto_check_signatures", "FALSE",
	 &prefs_gpg.auto_check_signatures, P_BOOL,
	 NULL, NULL, NULL},
	{"store_passphrase", "FALSE", &prefs_gpg.store_passphrase, P_BOOL,
	 NULL, NULL, NULL},
	{"store_passphrase_timeout", "0",
	 &prefs_gpg.store_passphrase_timeout, P_INT,
	 NULL, NULL, NULL},
	{"passphrase_grab", "FALSE", &prefs_gpg.passphrase_grab, P_BOOL,
	 NULL, NULL, NULL},
	{"gpg_warning", "TRUE", &prefs_gpg.gpg_warning, P_BOOL,
	 NULL, NULL, NULL},

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

struct GPGPage
{
	PrefsPage page;

	GtkWidget *checkbtn_auto_check_signatures;
        GtkWidget *checkbtn_store_passphrase;  
        GtkWidget *spinbtn_store_passphrase;  
        GtkWidget *checkbtn_passphrase_grab;  
        GtkWidget *checkbtn_gpg_warning;
};

static void prefs_gpg_create_widget_func(PrefsPage *_page,
					 GtkWindow *window,
					 gpointer data)
{
	struct GPGPage *page = (struct GPGPage *) _page;
	struct GPGConfig *config;

	GtkWidget *checkbtn_passphrase_grab;
	GtkWidget *checkbtn_store_passphrase;
	GtkWidget *checkbtn_auto_check_signatures;
	GtkWidget *checkbtn_gpg_warning;
	GtkWidget *hbox1;
	GtkWidget *vbox1, *vbox2;
	GtkWidget *label_expire1;
	GtkObject *spinbtn_store_passphrase_adj;
	GtkWidget *spinbtn_store_passphrase;
	GtkWidget *label_expire2;
	GtkWidget *frame_passphrase;
	GtkTooltips *tooltips;

	tooltips = gtk_tooltips_new();

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (vbox2, checkbtn_auto_check_signatures,
			_("Automatically check signatures"));

	PACK_FRAME(vbox1, frame_passphrase, _("Passphrase"));

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox2);
	gtk_container_add (GTK_CONTAINER (frame_passphrase), vbox2);
	gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);

	PACK_CHECK_BUTTON (vbox2, checkbtn_store_passphrase,
			_("Store passphrase in memory"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

	label_expire1 = gtk_label_new(_("Expire after"));
	gtk_widget_show (label_expire1);
	gtk_box_pack_start (GTK_BOX (hbox1), label_expire1, FALSE, FALSE, 0);

	spinbtn_store_passphrase_adj =
	    gtk_adjustment_new(1, 0, 1440, 1, 10, 10);
	spinbtn_store_passphrase =
	    gtk_spin_button_new(GTK_ADJUSTMENT
				(spinbtn_store_passphrase_adj), 1, 0);
	gtk_widget_show(spinbtn_store_passphrase);
	gtk_box_pack_start(GTK_BOX(hbox1), spinbtn_store_passphrase, FALSE,
			   FALSE, 0);
	gtk_widget_set_size_request(spinbtn_store_passphrase, 64, -1);
	gtk_tooltips_set_tip(tooltips, spinbtn_store_passphrase,
			     _
			     ("Setting to '0' will store the passphrase for the whole session"),
			     NULL);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON
				    (spinbtn_store_passphrase), TRUE);

	label_expire2 = gtk_label_new(_("minute(s)"));
	gtk_widget_show(label_expire2);
	gtk_box_pack_start(GTK_BOX(hbox1), label_expire2, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(label_expire2), 0.0, 0.5);

	SET_TOGGLE_SENSITIVITY (checkbtn_store_passphrase, label_expire1);
	SET_TOGGLE_SENSITIVITY (checkbtn_store_passphrase, spinbtn_store_passphrase);
	SET_TOGGLE_SENSITIVITY (checkbtn_store_passphrase, label_expire2);

	PACK_CHECK_BUTTON (vbox2, checkbtn_passphrase_grab,
			_("Grab input while entering a passphrase"));

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (vbox2, checkbtn_gpg_warning,
			_("Display warning on startup if GnuPG doesn't work"));

	config = prefs_gpg_get_config();

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_auto_check_signatures), config->auto_check_signatures);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_store_passphrase), config->store_passphrase);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbtn_store_passphrase), (float) config->store_passphrase_timeout);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_passphrase_grab), config->passphrase_grab);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_gpg_warning), config->gpg_warning);

	page->checkbtn_auto_check_signatures = checkbtn_auto_check_signatures;
	page->checkbtn_store_passphrase = checkbtn_store_passphrase;
	page->spinbtn_store_passphrase = spinbtn_store_passphrase;
	page->checkbtn_passphrase_grab = checkbtn_passphrase_grab;
	page->checkbtn_gpg_warning = checkbtn_gpg_warning;

	page->page.widget = vbox1;
}

static void prefs_gpg_destroy_widget_func(PrefsPage *_page)
{
}

static void prefs_gpg_save_func(PrefsPage *_page)
{
	struct GPGPage *page = (struct GPGPage *) _page;
	GPGConfig *config = prefs_gpg_get_config();

	config->auto_check_signatures =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_auto_check_signatures));
	config->store_passphrase = 
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_store_passphrase));
	config->store_passphrase_timeout = 
		gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(page->spinbtn_store_passphrase));
	config->passphrase_grab = 
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_passphrase_grab));
	config->gpg_warning = 
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_gpg_warning));

	prefs_gpg_save_config();
}

struct GPGAccountPage
{
	PrefsPage page;

	GtkWidget *key_default;
	GtkWidget *key_by_from;
	GtkWidget *key_custom;
	GtkWidget *keyid;

	PrefsAccount *account;
};

void key_custom_toggled(GtkToggleButton *togglebutton, gpointer user_data)
{
	struct GPGAccountPage *page = (struct GPGAccountPage *) user_data;
	gboolean active;

	active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->key_custom));
	gtk_widget_set_sensitive(GTK_WIDGET(page->keyid), active);
	if (!active)
		gtk_editable_delete_text(GTK_EDITABLE(page->keyid), 0, -1);
}

static void prefs_gpg_account_create_widget_func(PrefsPage *_page,
						 GtkWindow *window,
						 gpointer data)
{
	struct GPGAccountPage *page = (struct GPGAccountPage *) _page;
	PrefsAccount *account = (PrefsAccount *) data;
	GPGAccountConfig *config;

	GtkWidget *vbox;
	GtkWidget *frame1;
	GtkWidget *table1;
	GtkWidget *table2;
	GSList *key_group = NULL;
	GtkWidget *key_default;
	GtkWidget *key_by_from;
	GtkWidget *key_custom;
	GtkWidget *label1;
	GtkWidget *keyid;

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);

	frame1 = gtk_frame_new(_("Sign key"));
	gtk_widget_show(frame1);
	gtk_box_pack_start(GTK_BOX(vbox), frame1, FALSE, FALSE, 0);
	gtk_frame_set_label_align(GTK_FRAME(frame1), 0.0, 0.5);

	table1 = gtk_table_new(4, 3, FALSE);
	gtk_widget_show(table1);
	gtk_container_add(GTK_CONTAINER(frame1), table1);
	gtk_container_set_border_width(GTK_CONTAINER(table1), 8);
	gtk_table_set_row_spacings(GTK_TABLE(table1), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table1), 4);

	key_default = gtk_radio_button_new_with_label(key_group,
			_("Use default GnuPG key"));
	key_group = gtk_radio_button_group(GTK_RADIO_BUTTON(key_default));
	gtk_widget_show(key_default);
	gtk_table_attach(GTK_TABLE(table1), key_default, 0, 3, 0, 1,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);

	key_by_from = gtk_radio_button_new_with_label(key_group,
		_("Select key by your email address"));
	key_group = gtk_radio_button_group(GTK_RADIO_BUTTON(key_by_from));
	gtk_widget_show(key_by_from);
	gtk_table_attach(GTK_TABLE(table1), key_by_from, 0, 3, 1, 2,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);

	key_custom = gtk_radio_button_new_with_label(key_group,
		_("Specify key manually"));
	key_group = gtk_radio_button_group(GTK_RADIO_BUTTON(key_custom));
	gtk_widget_show(key_custom);
	gtk_table_attach(GTK_TABLE(table1), key_custom, 0, 3, 2, 3,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);

	label1 = gtk_label_new(_("User or key ID:"));
	gtk_widget_show(label1);
	gtk_table_attach(GTK_TABLE(table1), label1, 0, 2, 3, 4,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify(GTK_LABEL(label1), GTK_JUSTIFY_LEFT);

	keyid = gtk_entry_new();
	gtk_widget_show(keyid);
	gtk_table_attach(GTK_TABLE(table1), keyid, 2, 3, 3, 4,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);

	config = prefs_gpg_account_get_config(account);
	switch (config->sign_key) {
	case SIGN_KEY_DEFAULT:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(key_default), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(keyid), FALSE);
		break;
	case SIGN_KEY_BY_FROM:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(key_by_from), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(keyid), FALSE);
		break;
	case SIGN_KEY_CUSTOM:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(key_custom), TRUE);
		gtk_widget_set_sensitive(GTK_WIDGET(keyid), TRUE);
		break;
	}

	if (config->sign_key_id != NULL)
		gtk_entry_set_text(GTK_ENTRY(keyid), config->sign_key_id);

	g_signal_connect(G_OBJECT(key_custom), "toggled", G_CALLBACK(key_custom_toggled), page);

	page->key_default = key_default;
	page->key_by_from = key_by_from;
	page->key_custom = key_custom;
	page->keyid = keyid;

	page->page.widget = vbox;
	page->account = account;
}

static void prefs_gpg_account_destroy_widget_func(PrefsPage *_page)
{
	/* nothing to do here */
}

static void prefs_gpg_account_save_func(PrefsPage *_page)
{
	struct GPGAccountPage *page = (struct GPGAccountPage *) _page;
	GPGAccountConfig *config;

	config = prefs_gpg_account_get_config(page->account);

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->key_default)))
		config->sign_key = SIGN_KEY_DEFAULT;
	else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->key_by_from)))
		config->sign_key = SIGN_KEY_BY_FROM;
	else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->key_custom))) {
		config->sign_key = SIGN_KEY_CUSTOM;
		g_free(config->sign_key_id);
		config->sign_key_id = gtk_editable_get_chars(GTK_EDITABLE(page->keyid), 0, -1);
	}

	prefs_gpg_account_set_config(page->account, config);
	prefs_gpg_account_free_config(config);
}

GPGConfig *prefs_gpg_get_config(void)
{
	return &prefs_gpg;
}

void prefs_gpg_save_config(void)
{
	PrefFile *pfile;
	gchar *rcpath;

	debug_print("Saving GPG config\n");

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	pfile = prefs_write_open(rcpath);
	g_free(rcpath);
	if (!pfile || (prefs_set_block_label(pfile, "GPG") < 0))
		return;

	if (prefs_write_param(param, pfile->fp) < 0) {
		g_warning("failed to write GPG configuration to file\n");
		prefs_file_close_revert(pfile);
		return;
	}
	fprintf(pfile->fp, "\n");

	prefs_file_close(pfile);
}

struct GPGAccountConfig *prefs_gpg_account_get_config(PrefsAccount *account)
{
	GPGAccountConfig *config;
	const gchar *confstr;
	gchar **strv;

	config = g_new0(GPGAccountConfig, 1);
	config->sign_key = SIGN_KEY_DEFAULT;
	config->sign_key_id = NULL;

	confstr = prefs_account_get_privacy_prefs(account, "gpg");
	if (confstr == NULL)
		return config;

	strv = g_strsplit(confstr, ";", 0);
	if (strv[0] != NULL) {
		if (!strcmp(strv[0], "DEFAULT"))
			config->sign_key = SIGN_KEY_DEFAULT;
		if (!strcmp(strv[0], "BY_FROM"))
			config->sign_key = SIGN_KEY_BY_FROM;
		if (!strcmp(strv[0], "CUSTOM")) {
			if (strv[1] != NULL) {
				config->sign_key = SIGN_KEY_CUSTOM;
				config->sign_key_id = g_strdup(strv[1]);
			} else
				config->sign_key = SIGN_KEY_DEFAULT;
		}
	}
	g_strfreev(strv);

	return config;
}

void prefs_gpg_account_set_config(PrefsAccount *account, GPGAccountConfig *config)
{
	gchar *confstr = NULL;

	switch (config->sign_key) {
	case SIGN_KEY_DEFAULT:
		confstr = g_strdup("DEFAULT");
		break;
	case SIGN_KEY_BY_FROM:
		confstr = g_strdup("BY_FROM");
		break;
	case SIGN_KEY_CUSTOM:
		confstr = g_strdup_printf("CUSTOM;%s", config->sign_key_id);
		break;
	default:
		confstr = g_strdup("");
		g_warning("prefs_gpg_account_set_config: bad sign_key val\n");
	}

	prefs_account_set_privacy_prefs(account, "gpg", confstr);

	g_free(confstr);
}

void prefs_gpg_account_free_config(GPGAccountConfig *config)
{
	g_free(config->sign_key_id);
	g_free(config);
}

static struct GPGPage gpg_page;
static struct GPGAccountPage gpg_account_page;

void prefs_gpg_init()
{
	static gchar *path[3];
	gchar *rcpath;

	prefs_set_default(param);
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	prefs_read_config(param, "GPG", rcpath, NULL);
	g_free(rcpath);

        path[0] = _("Plugins");
        path[1] = _("GPG");
        path[2] = NULL;

        gpg_page.page.path = path;
        gpg_page.page.create_widget = prefs_gpg_create_widget_func;
        gpg_page.page.destroy_widget = prefs_gpg_destroy_widget_func;
        gpg_page.page.save_page = prefs_gpg_save_func;
        gpg_page.page.weight = 30.0;

        prefs_gtk_register_page((PrefsPage *) &gpg_page);

        gpg_account_page.page.path = path;
        gpg_account_page.page.create_widget = prefs_gpg_account_create_widget_func;
        gpg_account_page.page.destroy_widget = prefs_gpg_account_destroy_widget_func;
        gpg_account_page.page.save_page = prefs_gpg_account_save_func;
        gpg_account_page.page.weight = 30.0;

        prefs_account_register_page((PrefsPage *) &gpg_account_page);
}

void prefs_gpg_done()
{
	prefs_gtk_unregister_page((PrefsPage *) &gpg_page);
	prefs_account_unregister_page((PrefsPage *) &gpg_account_page);
}
