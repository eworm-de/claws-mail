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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>

#include "defs.h"
#include "intl.h"
#include "utils.h"
#include "prefs.h"
#include "prefs_gtk.h"
#include "prefs_gpg.h"

struct GPGConfig prefs_gpg;

static PrefParam param[] = {
	/* Privacy */
	{"auto_check_signatures", "TRUE",
	 &prefs_gpg.auto_check_signatures, P_BOOL,
	 NULL, NULL, NULL},
	{"store_passphrase", "FALSE", &prefs_gpg.store_passphrase, P_BOOL,
	 NULL, NULL, NULL},
	{"store_passphrase_timeout", "0",
	 &prefs_gpg.store_passphrase_timeout, P_INT,
	 NULL, NULL, NULL},
#ifndef __MINGW32__
	{"passphrase_grab", "FALSE", &prefs_gpg.passphrase_grab, P_BOOL,
	 NULL, NULL, NULL},
#endif /* __MINGW32__ */
	{"gpg_warning", "TRUE", &prefs_gpg.gpg_warning, P_BOOL,
	 NULL, NULL, NULL},

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

static void prefs_privacy_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *vbox3;
	GtkWidget *hbox1;
	GtkWidget *hbox_spc;
	GtkWidget *label;
	GtkWidget *checkbtn_auto_check_signatures;
	GtkWidget *checkbtn_store_passphrase;
	GtkObject *spinbtn_store_passphrase_adj;
	GtkWidget *spinbtn_store_passphrase;
	GtkTooltips *store_tooltip;
	GtkWidget *checkbtn_passphrase_grab;
	GtkWidget *checkbtn_gpg_warning;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (vbox2, checkbtn_auto_check_signatures,
			   _("Automatically check signatures"));

	PACK_CHECK_BUTTON (vbox2, checkbtn_store_passphrase,
			   _("Store passphrase in memory temporarily"));

	vbox3 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox3);
	gtk_box_pack_start (GTK_BOX (vbox2), vbox3, FALSE, FALSE, 0);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox3), hbox1, FALSE, FALSE, 0);

	hbox_spc = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (hbox1), hbox_spc, FALSE, FALSE, 0);
	gtk_widget_set_usize (hbox_spc, 12, -1);

	label = gtk_label_new (_("Expire after"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);

	store_tooltip = gtk_tooltips_new();

	spinbtn_store_passphrase_adj = gtk_adjustment_new (0, 0, 1440, 1, 5, 5);
	spinbtn_store_passphrase = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_store_passphrase_adj), 1, 0);
	gtk_widget_show (spinbtn_store_passphrase);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(store_tooltip), spinbtn_store_passphrase,
			     _("Setting to '0' will store the passphrase"
			       " for the whole session"),
			     NULL);
 	gtk_box_pack_start (GTK_BOX (hbox1), spinbtn_store_passphrase, FALSE, FALSE, 0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_store_passphrase),
				     TRUE);
	gtk_widget_set_usize (spinbtn_store_passphrase, 64, -1);

	label = gtk_label_new (_("minute(s) "));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox1), label, FALSE, FALSE, 0);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox3), hbox1, FALSE, FALSE, 0);

	hbox_spc = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox_spc);
	gtk_box_pack_start (GTK_BOX (hbox1), hbox_spc, FALSE, FALSE, 0);
	gtk_widget_set_usize (hbox_spc, 12, -1);

	SET_TOGGLE_SENSITIVITY (checkbtn_store_passphrase, vbox3);

#ifndef __MINGW32__
	PACK_CHECK_BUTTON (vbox2, checkbtn_passphrase_grab,
			   _("Grab input while entering a passphrase"));
#endif

	PACK_CHECK_BUTTON
		(vbox2, checkbtn_gpg_warning,
		 _("Display warning on startup if GnuPG doesn't work"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

/*
	privacy.checkbtn_auto_check_signatures
					     = checkbtn_auto_check_signatures;
	privacy.checkbtn_store_passphrase    = checkbtn_store_passphrase;
	privacy.spinbtn_store_passphrase     = spinbtn_store_passphrase;
	privacy.spinbtn_store_passphrase_adj = spinbtn_store_passphrase_adj;
	privacy.checkbtn_passphrase_grab     = checkbtn_passphrase_grab;
	privacy.checkbtn_gpg_warning         = checkbtn_gpg_warning;
*/
}

GPGConfig *prefs_gpg_get_config(void)
{
	return &prefs_gpg;
}

void prefs_gpg_save_config(void)
{
	PrefFile *pfile;
	gchar *rcpath;

	debug_print("Saving GPGME config\n");

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	pfile = prefs_write_open(rcpath);
	g_free(rcpath);
	if (!pfile || (prefs_set_block_label(pfile, "GPGME") < 0))
		return;

	if (prefs_write_param(param, pfile->fp) < 0) {
		g_warning("failed to write GPGME configuration to file\n");
		prefs_file_close_revert(pfile);
		return;
	}
	fprintf(pfile->fp, "\n");

	prefs_file_close(pfile);
}

void prefs_gpg_init()
{
	prefs_set_default(param);
	prefs_read_config(param, "GPGME", COMMON_RC);
}

void prefs_gpg_done()
{
}
