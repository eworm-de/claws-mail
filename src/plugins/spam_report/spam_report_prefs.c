/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Colin Leroy <colin@colino.net>
 * and the Claws Mail Team
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "gtkutils.h"
#include "password.h"
#include "prefs.h"
#include "prefs_gtk.h"
#include "prefswindow.h"
#include "alertpanel.h"
#include "utils.h"

#include "spam_report_prefs.h"

#define PREFS_BLOCK_NAME "SpamReport"

SpamReportPrefs spamreport_prefs;
void spamreport_clear_cache(void);

typedef struct _SpamReportPage SpamReportPage;

struct _SpamReportPage {
        PrefsPage page;
        GtkWidget *frame[INTF_LAST];
	GtkWidget *enabled_chkbtn[INTF_LAST];
	GtkWidget *user_entry[INTF_LAST];
	GtkWidget *pass_entry[INTF_LAST];
};

static PrefParam param[] = {
        {"signalspam_enabled", "FALSE", &spamreport_prefs.enabled[INTF_SIGNALSPAM], P_BOOL, NULL, NULL, NULL},
        {"signalspam_user", "", &spamreport_prefs.user[INTF_SIGNALSPAM], P_STRING, NULL, NULL, NULL},
        {"signalspam_pass", "", &spamreport_prefs.pass[INTF_SIGNALSPAM], P_PASSWORD, NULL, NULL, NULL},
	{"spamcop_enabled",    "FALSE", &spamreport_prefs.enabled[INTF_SPAMCOP], P_BOOL, NULL, NULL, NULL},
	{"spamcop_user",    "", &spamreport_prefs.user[INTF_SPAMCOP], P_STRING, NULL, NULL, NULL},
	{"debianspam_enabled", "FALSE", &spamreport_prefs.enabled[INTF_DEBIANSPAM], P_BOOL, NULL, NULL, NULL},
       {0,0,0,0,0,0,0}
};

static SpamReportPage spamreport_prefs_page;

static void create_spamreport_prefs_page	(PrefsPage *page,
					 GtkWindow *window,
					 gpointer   data);
static void destroy_spamreport_prefs_page	(PrefsPage *page);
static void save_spamreport_prefs		(PrefsPage *page);

void spamreport_prefs_init(void)
{
	static gchar *path[3];
	gchar *rcpath;
	guint i;
	gboolean passwords_migrated = FALSE;

	path[0] = _("Plugins");
	path[1] = _("SpamReport");
	path[2] = NULL;

        prefs_set_default(param);
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
        prefs_read_config(param, PREFS_BLOCK_NAME, rcpath, NULL);
	g_free(rcpath);

	/* Move passwords that are still in main config to password store. */
	for (i = 0; i < INTF_LAST; i++) {
		if (spamreport_prefs.pass[i] != NULL) {
			passwd_store_set(PWS_PLUGIN, "SpamReport",
					spam_interfaces[i].name, spamreport_prefs.pass[i], TRUE);
			passwords_migrated = TRUE;
		}
	}
	if (passwords_migrated)
		passwd_store_write_config();

        spamreport_prefs_page.page.path = path;
        spamreport_prefs_page.page.create_widget = create_spamreport_prefs_page;
        spamreport_prefs_page.page.destroy_widget = destroy_spamreport_prefs_page;
        spamreport_prefs_page.page.save_page = save_spamreport_prefs;
	spamreport_prefs_page.page.weight = 30.0;
        
        prefs_gtk_register_page((PrefsPage *) &spamreport_prefs_page);
}

void spamreport_prefs_done(void)
{
        prefs_gtk_unregister_page((PrefsPage *) &spamreport_prefs_page);
}

static void create_spamreport_prefs_page(PrefsPage *page,
				    GtkWindow *window,
                                    gpointer data)
{
        SpamReportPage *prefs_page = (SpamReportPage *) page;

        GtkWidget *vbox, *table;
	GtkWidget *tmp;
	int i = 0;
	
        vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING_NARROW);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), VBOX_BORDER);
 	gtk_widget_show(vbox);
       
	for (i = 0; i < INTF_LAST; i++) {
		gchar *pass;
		prefs_page->frame[i] = gtk_frame_new(spam_interfaces[i].name);
		gtk_frame_set_label_align(GTK_FRAME(prefs_page->frame[i]), 0.01, 0.5);
		gtk_box_pack_start(GTK_BOX(vbox), prefs_page->frame[i], FALSE, FALSE, 6);

		prefs_page->user_entry[i] = gtk_entry_new();
		prefs_page->pass_entry[i] = gtk_entry_new();
		prefs_page->enabled_chkbtn[i] = gtk_check_button_new_with_label(_("Enabled"));

		gtk_entry_set_visibility(GTK_ENTRY(prefs_page->pass_entry[i]), FALSE);

		gtk_entry_set_text(GTK_ENTRY(prefs_page->user_entry[i]),
			spamreport_prefs.user[i] ? spamreport_prefs.user[i]:"");

		pass = spamreport_passwd_get(spam_interfaces[i].name);
		gtk_entry_set_text(GTK_ENTRY(prefs_page->pass_entry[i]), pass ? pass:"");
		if (pass != NULL) {
			memset(pass, 0, strlen(pass));
		}
		g_free(pass);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_page->enabled_chkbtn[i]),
			spamreport_prefs.enabled[i]);

		table = gtk_grid_new();
		gtk_container_set_border_width(GTK_CONTAINER(table), 8);
		gtk_grid_set_row_spacing(GTK_GRID(table), 4);
		gtk_grid_set_column_spacing(GTK_GRID(table), 8);
	
		gtk_container_add(GTK_CONTAINER(prefs_page->frame[i]), table);
		gtk_widget_show(prefs_page->frame[i]);
		gtk_widget_show(table);

		gtk_grid_attach(GTK_GRID(table), prefs_page->enabled_chkbtn[i], 0, 0, 1, 1);
		gtk_widget_show(prefs_page->enabled_chkbtn[i]);

		switch(spam_interfaces[i].type) {
		case INTF_MAIL:
			tmp = gtk_label_new(_("Forward to:"));
			break;
		default:
			tmp = gtk_label_new(_("Username:"));
		}
		gtk_grid_attach(GTK_GRID(table), tmp, 0, 1, 1, 1);
		gtk_grid_attach(GTK_GRID(table), prefs_page->user_entry[i], 1, 1, 1, 1);
		gtk_widget_set_hexpand(prefs_page->user_entry[i], TRUE);
		gtk_widget_set_halign(prefs_page->user_entry[i], GTK_ALIGN_FILL);
		if (spam_interfaces[i].type != INTF_HTTP_GET) {
			gtk_widget_show(tmp);
			gtk_widget_show(prefs_page->user_entry[i]);
		}

		tmp = gtk_label_new(_("Password:"));
		gtk_grid_attach(GTK_GRID(table), tmp, 0, 2, 1, 1);
		gtk_grid_attach(GTK_GRID(table), prefs_page->pass_entry[i], 1, 2, 1, 1);
		gtk_widget_set_hexpand(prefs_page->pass_entry[i], TRUE);
		gtk_widget_set_halign(prefs_page->pass_entry[i], GTK_ALIGN_FILL);
		
		if (spam_interfaces[i].type != INTF_MAIL && spam_interfaces[i].type != INTF_HTTP_GET) {
			gtk_widget_show(tmp);
			gtk_widget_show(prefs_page->pass_entry[i]);
		}
	}
        prefs_page->page.widget = vbox;
}

static void destroy_spamreport_prefs_page(PrefsPage *page)
{
	/* Do nothing! */
}

static void save_spamreport_prefs(PrefsPage *page)
{
        SpamReportPage *prefs_page = (SpamReportPage *) page;
        PrefFile *pref_file;
        gchar *rc_file_path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
                                          COMMON_RC, NULL);
        int i = 0;
	
	for (i = 0; i < INTF_LAST; i++) {
		gchar *pass;

        	g_free(spamreport_prefs.user[i]);
		g_free(spamreport_prefs.pass[i]);

		spamreport_prefs.enabled[i] = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(prefs_page->enabled_chkbtn[i]));
		spamreport_prefs.user[i] = gtk_editable_get_chars(
			GTK_EDITABLE(prefs_page->user_entry[i]), 0, -1);

		pass = gtk_editable_get_chars(GTK_EDITABLE(prefs_page->pass_entry[i]), 0, -1);
		spamreport_passwd_set(spam_interfaces[i].name, pass);
		memset(pass, 0, strlen(pass));
		g_free(pass);
	}

        pref_file = prefs_write_open(rc_file_path);
        g_free(rc_file_path);
        
        if (!(pref_file) ||
	    (prefs_set_block_label(pref_file, PREFS_BLOCK_NAME) < 0))
          return;
        
        if (prefs_write_param(param, pref_file->fp) < 0) {
          g_warning("failed to write SpamReport plugin configuration");
          prefs_file_close_revert(pref_file);
          return;
        }
        if (fprintf(pref_file->fp, "\n") < 0) {
		FILE_OP_ERROR(rc_file_path, "fprintf");
		prefs_file_close_revert(pref_file);
	} else
	        prefs_file_close(pref_file);

	passwd_store_write_config();
}
