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

        /*
         * BEGIN GLADE CODE 
         * DO NOT EDIT 
         */
	GtkWidget *table;
	GtkWidget *checkbtn_passphrase_grab;
	GtkWidget *checkbtn_store_passphrase;
	GtkWidget *checkbtn_auto_check_signatures;
	GtkWidget *checkbtn_gpg_warning;
	GtkWidget *label7;
	GtkWidget *label6;
	GtkWidget *label9;
	GtkWidget *label10;
	GtkWidget *hbox1;
	GtkWidget *label11;
	GtkObject *spinbtn_store_passphrase_adj;
	GtkWidget *spinbtn_store_passphrase;
	GtkWidget *label12;
	GtkTooltips *tooltips;

	tooltips = gtk_tooltips_new();

	table = gtk_table_new(5, 2, FALSE);
	gtk_widget_show(table);
	gtk_container_set_border_width(GTK_CONTAINER(table), 8);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	checkbtn_passphrase_grab = gtk_check_button_new_with_label("");
	gtk_widget_show(checkbtn_passphrase_grab);
	gtk_table_attach(GTK_TABLE(table), checkbtn_passphrase_grab, 0, 1,
			 3, 4, (GtkAttachOptions) (GTK_SHRINK | GTK_FILL),
			 (GtkAttachOptions) (GTK_SHRINK), 0, 0);

	checkbtn_store_passphrase = gtk_check_button_new_with_label("");
	gtk_widget_show(checkbtn_store_passphrase);
	gtk_table_attach(GTK_TABLE(table), checkbtn_store_passphrase, 0, 1,
			 1, 2, (GtkAttachOptions) (GTK_SHRINK | GTK_FILL),
			 (GtkAttachOptions) (GTK_SHRINK), 0, 0);

	checkbtn_auto_check_signatures =
	    gtk_check_button_new_with_label("");
	gtk_widget_show(checkbtn_auto_check_signatures);
	gtk_table_attach(GTK_TABLE(table), checkbtn_auto_check_signatures,
			 0, 1, 0, 1,
			 (GtkAttachOptions) (GTK_SHRINK | GTK_FILL),
			 (GtkAttachOptions) (GTK_SHRINK), 0, 0);

	checkbtn_gpg_warning = gtk_check_button_new_with_label("");
	gtk_widget_show(checkbtn_gpg_warning);
	gtk_table_attach(GTK_TABLE(table), checkbtn_gpg_warning, 0, 1, 4,
			 5, (GtkAttachOptions) (GTK_SHRINK | GTK_FILL),
			 (GtkAttachOptions) (GTK_SHRINK), 0, 0);

	label7 = gtk_label_new(_("Store passphrase in memory"));
	gtk_widget_show(label7);
	gtk_table_attach(GTK_TABLE(table), label7, 1, 2, 1, 2,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (GTK_SHRINK), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label7), 0, 0.5);

	label6 = gtk_label_new(_("Automatically check signatures"));
	gtk_widget_show(label6);
	gtk_table_attach(GTK_TABLE(table), label6, 1, 2, 0, 1,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (GTK_SHRINK), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label6), 0, 0.5);

	label9 =
	    gtk_label_new(_("Grab input while entering a passphrase"));
	gtk_widget_show(label9);
	gtk_table_attach(GTK_TABLE(table), label9, 1, 2, 3, 4,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (GTK_SHRINK), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label9), 0, 0.5);

	label10 =
	    gtk_label_new(_
			  ("Display warning on startup if GnuPG doesn't work"));
	gtk_widget_show(label10);
	gtk_table_attach(GTK_TABLE(table), label10, 1, 2, 4, 5,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (GTK_SHRINK), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label10), 0, 0.5);

	hbox1 = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox1);
	gtk_table_attach(GTK_TABLE(table), hbox1, 1, 2, 2, 3,
			 (GtkAttachOptions) (GTK_SHRINK | GTK_FILL),
			 (GtkAttachOptions) (GTK_SHRINK), 0, 0);

	label11 = gtk_label_new(_("Expire after"));
	gtk_widget_show(label11);
	gtk_box_pack_start(GTK_BOX(hbox1), label11, FALSE, FALSE, 0);

	spinbtn_store_passphrase_adj =
	    gtk_adjustment_new(1, 0, 1440, 1, 10, 10);
	spinbtn_store_passphrase =
	    gtk_spin_button_new(GTK_ADJUSTMENT
				(spinbtn_store_passphrase_adj), 1, 0);
	gtk_widget_show(spinbtn_store_passphrase);
	gtk_box_pack_start(GTK_BOX(hbox1), spinbtn_store_passphrase, FALSE,
			   FALSE, 0);
	gtk_widget_set_usize(spinbtn_store_passphrase, 64, -2);
	gtk_tooltips_set_tip(tooltips, spinbtn_store_passphrase,
			     _
			     ("Setting to '0' will store the passphrase for the whole session"),
			     NULL);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON
				    (spinbtn_store_passphrase), TRUE);

	label12 = gtk_label_new(_("minute(s)"));
	gtk_widget_show(label12);
	gtk_box_pack_start(GTK_BOX(hbox1), label12, TRUE, TRUE, 0);
	gtk_misc_set_alignment(GTK_MISC(label12), 0.0, 0.5);
        /* 
         * END GLADE CODE
         */

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

	page->page.widget = table;
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

static struct GPGPage gpg_page;

void prefs_gpg_init()
{
	static gchar *path[3];

	prefs_set_default(param);
	prefs_read_config(param, "GPG", COMMON_RC);

        path[0] = _("Privacy");
        path[1] = _("GPG");
        path[2] = NULL;

        gpg_page.page.path = path;
        gpg_page.page.create_widget = prefs_gpg_create_widget_func;
        gpg_page.page.destroy_widget = prefs_gpg_destroy_widget_func;
        gpg_page.page.save_page = prefs_gpg_save_func;
        gpg_page.page.weight = 30.0;

        prefs_gtk_register_page((PrefsPage *) &gpg_page);
}

void prefs_gpg_done()
{
	prefs_gtk_unregister_page((PrefsPage *) &gpg_page);
}
