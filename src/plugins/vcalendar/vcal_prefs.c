/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Colin Leroy <colin@colino.net> and 
 * the Claws Mail team
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <stddef.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "defs.h"

#include "mainwindow.h"
#include "prefs.h"
#include "prefs_gtk.h"
#include "prefswindow.h"
#include "common/utils.h"

#include "vcalendar.h"
#include "vcal_prefs.h"
#include "vcal_folder.h"

#define PREFS_BLOCK_NAME "VCalendar"

struct VcalendarPage
{
	PrefsPage page;
	
	GtkWidget *alert_enable_btn;
	GtkWidget *alert_delay_spinbtn;

	GtkWidget *export_enable_btn;
	GtkWidget *export_subs_btn;
	GtkWidget *export_path_entry;
	GtkWidget *export_command_entry;
	
	GtkWidget *export_user_label;
	GtkWidget *export_user_entry;
	GtkWidget *export_pass_label;
	GtkWidget *export_pass_entry;

	GtkWidget *export_freebusy_enable_btn;
	GtkWidget *export_freebusy_path_entry;
	GtkWidget *export_freebusy_command_entry;
	
	GtkWidget *export_freebusy_user_label;
	GtkWidget *export_freebusy_user_entry;
	GtkWidget *export_freebusy_pass_label;
	GtkWidget *export_freebusy_pass_entry;

	GtkWidget *freebusy_get_url_entry;
};

VcalendarPrefs vcalprefs;
static struct VcalendarPage vcal_prefs_page;

static PrefParam param[] = {
	{"alert_delay", "10", &vcalprefs.alert_delay, P_INT,
	 NULL, NULL, NULL},
	{"alert_enable", "FALSE", &vcalprefs.alert_enable, P_BOOL,
	 NULL, NULL, NULL},

	{"export_enable", "FALSE", &vcalprefs.export_enable, P_BOOL,
	 NULL, NULL, NULL},
	{"export_subs", "TRUE", &vcalprefs.export_subs, P_BOOL,
	 NULL, NULL, NULL},
	{"export_path", "", &vcalprefs.export_path, P_STRING,
	 NULL, NULL, NULL},
	{"export_command", NULL, &vcalprefs.export_command, P_STRING,
	 NULL, NULL, NULL},

	{"export_user", "", &vcalprefs.export_user, P_STRING,
	 NULL, NULL, NULL},
	{"export_pass", "", &vcalprefs.export_pass, P_PASSWORD,
	 NULL, NULL, NULL},

	{"orage_registered", "FALSE", &vcalprefs.orage_registered, P_BOOL,
	 NULL, NULL, NULL},

	{"export_freebusy_enable", "FALSE", &vcalprefs.export_freebusy_enable, P_BOOL,
	 NULL, NULL, NULL},
	{"export_freebusy_path", "", &vcalprefs.export_freebusy_path, P_STRING,
	 NULL, NULL, NULL},
	{"export_freebusy_command", NULL, &vcalprefs.export_freebusy_command, P_STRING,
	 NULL, NULL, NULL},
	{"freebusy_get_url", NULL, &vcalprefs.freebusy_get_url, P_STRING,
	 NULL, NULL, NULL},

	{"export_freebusy_user", "", &vcalprefs.export_freebusy_user, P_STRING,
	 NULL, NULL, NULL},
	{"export_freebusy_pass", "", &vcalprefs.export_freebusy_pass, P_PASSWORD,
	 NULL, NULL, NULL},

	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

static void set_auth_sensitivity(struct VcalendarPage *page)
{
	const gchar *export_path, *export_freebusy_path;
	gboolean export_enable, export_freebusy_enable;
	
	export_enable = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->export_enable_btn));
	export_freebusy_enable = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->export_freebusy_enable_btn));
	
	export_path = gtk_entry_get_text(GTK_ENTRY(page->export_path_entry));
	export_freebusy_path = gtk_entry_get_text(GTK_ENTRY(page->export_freebusy_path_entry));
	if (export_enable && export_path &&
	    (!strncmp(export_path, "http://", 7) ||
	     !strncmp(export_path, "ftp://", 6) ||
	     !strncmp(export_path, "https://", 8) ||
	     !strncmp(export_path, "sftp://", 5) ||
	     !strncmp(export_path, "webcal://", 9))) {
		gtk_widget_set_sensitive(page->export_user_label, TRUE);	
		gtk_widget_set_sensitive(page->export_user_entry, TRUE);	
		gtk_widget_set_sensitive(page->export_pass_label, TRUE);	
		gtk_widget_set_sensitive(page->export_pass_entry, TRUE);	
	} else {
		gtk_widget_set_sensitive(page->export_user_label, FALSE);	
		gtk_widget_set_sensitive(page->export_user_entry, FALSE);	
		gtk_widget_set_sensitive(page->export_pass_label, FALSE);	
		gtk_widget_set_sensitive(page->export_pass_entry, FALSE);	
	}
	if (export_freebusy_enable && export_freebusy_path &&
	    (!strncmp(export_freebusy_path, "http://", 7) ||
	     !strncmp(export_freebusy_path, "ftp://", 6) ||
	     !strncmp(export_freebusy_path, "https://", 8) ||
	     !strncmp(export_freebusy_path, "sftp://", 5) ||
	     !strncmp(export_freebusy_path, "webcal://", 9))) {
		gtk_widget_set_sensitive(page->export_freebusy_user_label, TRUE);	
		gtk_widget_set_sensitive(page->export_freebusy_user_entry, TRUE);	
		gtk_widget_set_sensitive(page->export_freebusy_pass_label, TRUE);	
		gtk_widget_set_sensitive(page->export_freebusy_pass_entry, TRUE);	
	} else {
		gtk_widget_set_sensitive(page->export_freebusy_user_label, FALSE);	
		gtk_widget_set_sensitive(page->export_freebusy_user_entry, FALSE);	
		gtk_widget_set_sensitive(page->export_freebusy_pass_label, FALSE);	
		gtk_widget_set_sensitive(page->export_freebusy_pass_entry, FALSE);	
	}	
}

static void path_changed(GtkWidget *widget, gpointer data)
{
	set_auth_sensitivity((struct VcalendarPage *)data);
}

static gboolean orage_available(void)
{
	gchar *tmp = g_find_program_in_path("orage");
	if (tmp) {
		g_free(tmp);
		return TRUE;
	}
	return FALSE;
}

static void orage_register(gboolean reg)
{
	if (orage_available()) {
		gchar *orage_argv[4];
		gchar *cmdline = g_strdup_printf("%s%svcalendar%sinternal.ics",
				get_rc_dir(), G_DIR_SEPARATOR_S, G_DIR_SEPARATOR_S);

		debug_print("telling Orage %s us ...\n", reg?"about":"to forget");
		orage_argv[0] = "orage";
		orage_argv[1] = reg ? "--add-foreign":"--remove-foreign";
		orage_argv[2] = cmdline;
		orage_argv[3] = NULL;
		g_spawn_async(NULL, (gchar **)orage_argv, NULL, 
			G_SPAWN_SEARCH_PATH|G_SPAWN_STDOUT_TO_DEV_NULL|
				G_SPAWN_STDERR_TO_DEV_NULL,
			NULL, NULL, NULL, FALSE);
		g_free(cmdline);
	}
}

void register_orage_checkbtn_toggled(GtkToggleButton	*toggle_btn,
				 	GtkWidget		*widget)
{
	orage_register(gtk_toggle_button_get_active(toggle_btn));
	vcalprefs.orage_registered = gtk_toggle_button_get_active(toggle_btn);
}

static void vcal_prefs_create_widget_func(PrefsPage * _page,
					   GtkWindow * window,
					   gpointer data)
{
	struct VcalendarPage *page = (struct VcalendarPage *) _page;

	GtkWidget *vbox1, *vbox2, *vbox3;
	GtkWidget *hbox1, *hbox2, *hbox3;
	
	GtkWidget *frame_alert;
	GtkWidget *alert_enable_checkbtn;
	GtkObject *alert_enable_spinbtn_adj;
	GtkWidget *alert_enable_spinbtn;
	GtkWidget *label_alert_enable;

	GtkWidget *frame_export;
	GtkWidget *export_enable_checkbtn;
	GtkWidget *export_subs_checkbtn;
	GtkWidget *export_path_entry;
	GtkWidget *export_command_label;
	GtkWidget *export_command_entry;
	GtkWidget *register_orage_checkbtn;

	GtkWidget *export_user_label;
	GtkWidget *export_user_entry;
	GtkWidget *export_pass_label;
	GtkWidget *export_pass_entry;

	GtkWidget *frame_freebusy_export;
	GtkWidget *export_freebusy_enable_checkbtn;
	GtkWidget *export_freebusy_path_entry;
	GtkWidget *export_freebusy_command_label;
	GtkWidget *export_freebusy_command_entry;

	GtkWidget *export_freebusy_user_label;
	GtkWidget *export_freebusy_user_entry;
	GtkWidget *export_freebusy_pass_label;
	GtkWidget *export_freebusy_pass_entry;

	GtkWidget *freebusy_get_url_label;
	GtkWidget *freebusy_get_url_entry;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_vbox_new (FALSE, 4);
	gtk_widget_show (vbox2);
	gtk_box_pack_start(GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);


/* alert stuff */
	PACK_FRAME(vbox2, frame_alert, _("Reminders"));
	vbox3 = gtk_vbox_new (FALSE, 8);
	gtk_widget_show (vbox3);
	gtk_container_add (GTK_CONTAINER (frame_alert), vbox3);
	gtk_container_set_border_width (GTK_CONTAINER (vbox3), VBOX_BORDER);
	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start(GTK_BOX (vbox3), hbox1, TRUE, TRUE, 0);

	alert_enable_checkbtn = gtk_check_button_new_with_label(_("Alert me"));
	gtk_widget_show (alert_enable_checkbtn);
	gtk_box_pack_start(GTK_BOX (hbox1), alert_enable_checkbtn, FALSE, FALSE, 0);

	alert_enable_spinbtn_adj = gtk_adjustment_new (10, 1, 24*60, 1, 10, 0);
	alert_enable_spinbtn = gtk_spin_button_new
		(GTK_ADJUSTMENT (alert_enable_spinbtn_adj), 1, 0);
	gtk_widget_set_size_request (alert_enable_spinbtn, 64, -1);
	gtk_widget_show (alert_enable_spinbtn);
	gtk_box_pack_start(GTK_BOX (hbox1), alert_enable_spinbtn, FALSE, FALSE, 0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (alert_enable_spinbtn), TRUE);

	label_alert_enable = gtk_label_new(_("minutes before an event"));
	gtk_widget_show (label_alert_enable);
	gtk_box_pack_start(GTK_BOX (hbox1), label_alert_enable, FALSE, FALSE, 0);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(alert_enable_checkbtn), 
			vcalprefs.alert_enable);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(alert_enable_spinbtn),
			vcalprefs.alert_delay);
	SET_TOGGLE_SENSITIVITY(alert_enable_checkbtn, alert_enable_spinbtn);

/* calendar export */
/* export enable + path stuff */
	PACK_FRAME(vbox2, frame_export, _("Calendar export"));
	vbox3 = gtk_vbox_new (FALSE, 8);
	gtk_widget_show (vbox3);
	gtk_container_add (GTK_CONTAINER (frame_export), vbox3);
	gtk_container_set_border_width (GTK_CONTAINER (vbox3), VBOX_BORDER);

/* export path */
	hbox2 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox2);
	gtk_box_pack_start(GTK_BOX (vbox3), hbox2, TRUE, TRUE, 0);

	export_enable_checkbtn = gtk_check_button_new_with_label(_("Automatically export calendar to"));
	gtk_widget_show(export_enable_checkbtn);
	gtk_box_pack_start(GTK_BOX (hbox2), export_enable_checkbtn, FALSE, FALSE, 0);

	export_path_entry = gtk_entry_new();
	gtk_widget_show(export_path_entry);
	gtk_box_pack_start(GTK_BOX(hbox2), export_path_entry, TRUE, TRUE, 0);
	SET_TOGGLE_SENSITIVITY(export_enable_checkbtn, export_path_entry);
	CLAWS_SET_TIP(export_enable_checkbtn, 
			    _("You can export to a local file or URL"));
	CLAWS_SET_TIP(export_path_entry, 
			    _("Specify a local file or URL "
			      "(http://server/path/file.ics)"));

/* export auth */
	hbox2 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox2);
	gtk_box_pack_start(GTK_BOX (vbox3), hbox2, TRUE, TRUE, 0);

	export_user_label = gtk_label_new(_("User ID"));
	gtk_widget_show(export_user_label);
	gtk_box_pack_start(GTK_BOX (hbox2), export_user_label, FALSE, FALSE, 0);

	export_user_entry = gtk_entry_new();
	gtk_widget_show(export_user_entry);
	gtk_box_pack_start(GTK_BOX (hbox2), export_user_entry, FALSE, FALSE, 0);

	export_pass_label = gtk_label_new(_("Password"));
	gtk_widget_show(export_pass_label);
	gtk_box_pack_start(GTK_BOX (hbox2), export_pass_label, FALSE, FALSE, 0);

	export_pass_entry = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(export_pass_entry), FALSE);
	gtk_widget_show(export_pass_entry);
	gtk_box_pack_start(GTK_BOX (hbox2), export_pass_entry, FALSE, FALSE, 0);

/* export subscriptions too */
	hbox2 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox2);
	gtk_box_pack_start(GTK_BOX (vbox3), hbox2, TRUE, TRUE, 0);

	export_subs_checkbtn = gtk_check_button_new_with_label(_("Include webcal subscriptions in export"));
	gtk_widget_show(export_subs_checkbtn);
	gtk_box_pack_start(GTK_BOX (hbox2), export_subs_checkbtn, FALSE, FALSE, 0);
	SET_TOGGLE_SENSITIVITY(export_enable_checkbtn, export_subs_checkbtn);

/* run-command after export stuff */
	hbox3 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox3);
	gtk_box_pack_start(GTK_BOX (vbox3), hbox3, TRUE, TRUE, 0);

	export_command_label = gtk_label_new(_("Command to run after calendar export"));
	gtk_widget_show(export_command_label);
	gtk_box_pack_start(GTK_BOX (hbox3), export_command_label, FALSE, FALSE, 0);
	SET_TOGGLE_SENSITIVITY(export_enable_checkbtn, export_command_label);

	export_command_entry = gtk_entry_new();
	gtk_widget_show(export_command_entry);
	gtk_box_pack_start(GTK_BOX (hbox3), export_command_entry, TRUE, TRUE, 0);
	SET_TOGGLE_SENSITIVITY(export_enable_checkbtn, export_command_entry);


	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(export_enable_checkbtn), 
			vcalprefs.export_enable);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(export_subs_checkbtn), 
			vcalprefs.export_subs);
	if (vcalprefs.export_path == NULL || *vcalprefs.export_path == '\0')
		vcalprefs.export_path = g_strconcat(get_rc_dir(), 
					G_DIR_SEPARATOR_S,
                                        "claws-mail.ics", NULL);
	if (vcalprefs.export_command == NULL)
		vcalprefs.export_command = g_strdup("");
	gtk_entry_set_text(GTK_ENTRY(export_path_entry), 
			vcalprefs.export_path);
	gtk_entry_set_text(GTK_ENTRY(export_command_entry), 
			vcalprefs.export_command);

	hbox3 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox3);
	gtk_box_pack_start(GTK_BOX (vbox3), hbox3, TRUE, TRUE, 0);
	register_orage_checkbtn = gtk_check_button_new_with_label(_("Register Claws' calendar in XFCE's Orage clock"));
	CLAWS_SET_TIP(register_orage_checkbtn, 
			    _("Allows Orage (version greater than 4.4) to see Claws Mail's calendar"));

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(register_orage_checkbtn), 
			vcalprefs.orage_registered);
	gtk_widget_set_sensitive(register_orage_checkbtn, orage_available());
	g_signal_connect(G_OBJECT(register_orage_checkbtn), "toggled",
			 G_CALLBACK(register_orage_checkbtn_toggled), NULL); 
	gtk_widget_show (register_orage_checkbtn);
	gtk_box_pack_start(GTK_BOX (hbox3), register_orage_checkbtn, TRUE, TRUE, 0);

/* freebusy export */
/* export enable + path stuff */
	PACK_FRAME(vbox2, frame_freebusy_export, _("Free/Busy information"));
	vbox3 = gtk_vbox_new (FALSE, 8);
	gtk_widget_show (vbox3);
	gtk_container_add (GTK_CONTAINER (frame_freebusy_export), vbox3);
	gtk_container_set_border_width (GTK_CONTAINER (vbox3), VBOX_BORDER);

/* export */
	hbox2 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox2);
	gtk_box_pack_start(GTK_BOX (vbox3), hbox2, TRUE, TRUE, 0);

	export_freebusy_enable_checkbtn = gtk_check_button_new_with_label(
		_("Automatically export free/busy status to"));
	gtk_widget_show(export_freebusy_enable_checkbtn);
	gtk_box_pack_start(GTK_BOX (hbox2), export_freebusy_enable_checkbtn, FALSE, FALSE, 0);

	export_freebusy_path_entry = gtk_entry_new();
	gtk_widget_show(export_freebusy_path_entry);
	gtk_box_pack_start(GTK_BOX(hbox2), export_freebusy_path_entry, TRUE, TRUE, 0);
	SET_TOGGLE_SENSITIVITY(export_freebusy_enable_checkbtn, export_freebusy_path_entry);
	CLAWS_SET_TIP(export_freebusy_enable_checkbtn, 
			    _("You can export to a local file or URL"));
	CLAWS_SET_TIP(export_freebusy_path_entry, 
			    _("Specify a local file or URL "
			      "(http://server/path/file.ifb)"));

/* auth */
	hbox2 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox2);
	gtk_box_pack_start(GTK_BOX (vbox3), hbox2, TRUE, TRUE, 0);

	export_freebusy_user_label = gtk_label_new(_("User ID"));
	gtk_widget_show(export_freebusy_user_label);
	gtk_box_pack_start(GTK_BOX (hbox2), export_freebusy_user_label, FALSE, FALSE, 0);

	export_freebusy_user_entry = gtk_entry_new();
	gtk_widget_show(export_freebusy_user_entry);
	gtk_box_pack_start(GTK_BOX (hbox2), export_freebusy_user_entry, FALSE, FALSE, 0);

	export_freebusy_pass_label = gtk_label_new(_("Password"));
	gtk_widget_show(export_freebusy_pass_label);
	gtk_box_pack_start(GTK_BOX (hbox2), export_freebusy_pass_label, FALSE, FALSE, 0);

	export_freebusy_pass_entry = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(export_freebusy_pass_entry), FALSE);
	gtk_widget_show(export_freebusy_pass_entry);
	gtk_box_pack_start(GTK_BOX (hbox2), export_freebusy_pass_entry, FALSE, FALSE, 0);

/* run-command after export stuff */
	hbox3 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox3);
	gtk_box_pack_start(GTK_BOX (vbox3), hbox3, TRUE, TRUE, 0);

	export_freebusy_command_label = gtk_label_new(_("Command to run after free/busy status export"));
	gtk_widget_show(export_freebusy_command_label);
	gtk_box_pack_start(GTK_BOX (hbox3), export_freebusy_command_label, FALSE, FALSE, 0);
	SET_TOGGLE_SENSITIVITY(export_freebusy_enable_checkbtn, export_freebusy_command_label);
	export_freebusy_command_entry = gtk_entry_new();
	gtk_widget_show(export_freebusy_command_entry);
	gtk_box_pack_start(GTK_BOX (hbox3), export_freebusy_command_entry, TRUE, TRUE, 0);
	SET_TOGGLE_SENSITIVITY(export_freebusy_enable_checkbtn, export_freebusy_command_entry);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(export_freebusy_enable_checkbtn), 
			vcalprefs.export_freebusy_enable);
	if (vcalprefs.export_freebusy_path == NULL || 
	    *vcalprefs.export_freebusy_path == '\0')
		vcalprefs.export_freebusy_path = g_strconcat(get_rc_dir(), 
					G_DIR_SEPARATOR_S,
                                        "claws-mail.ifb", NULL);
	if (vcalprefs.export_freebusy_command == NULL)
		vcalprefs.export_freebusy_command = g_strdup("");
	if (vcalprefs.freebusy_get_url == NULL)
		vcalprefs.freebusy_get_url = g_strdup("");

/* free/busy import */
	hbox2 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox2);
	gtk_box_pack_start(GTK_BOX (vbox3), hbox2, TRUE, TRUE, 0);

	freebusy_get_url_label = gtk_label_new(
		_("Get free/busy status of others from"));
	gtk_widget_show(freebusy_get_url_label);
	gtk_box_pack_start(GTK_BOX (hbox2), freebusy_get_url_label, FALSE, FALSE, 0);

	freebusy_get_url_entry = gtk_entry_new();
	gtk_widget_show(freebusy_get_url_entry);
	gtk_box_pack_start(GTK_BOX(hbox2), freebusy_get_url_entry, TRUE, TRUE, 0);
	CLAWS_SET_TIP(freebusy_get_url_entry, 
			    _("Specify a local file or URL "
			      "(http://server/path/file.ifb). Use %u "
			      "for the left part of the email address, %d for "
			      "the domain"));

	gtk_entry_set_text(GTK_ENTRY(export_freebusy_path_entry), 
			vcalprefs.export_freebusy_path);
	gtk_entry_set_text(GTK_ENTRY(export_freebusy_command_entry), 
			vcalprefs.export_freebusy_command);

	gtk_entry_set_text(GTK_ENTRY(freebusy_get_url_entry), 
			vcalprefs.freebusy_get_url);

	if (!vcalprefs.export_user)
		vcalprefs.export_user = g_strdup("");
	if (!vcalprefs.export_pass)
		vcalprefs.export_pass = g_strdup("");
	if (!vcalprefs.export_freebusy_user)
		vcalprefs.export_freebusy_user = g_strdup("");
	if (!vcalprefs.export_freebusy_pass)
		vcalprefs.export_freebusy_pass = g_strdup("");

	gtk_entry_set_text(GTK_ENTRY(export_user_entry), vcalprefs.export_user);
	gtk_entry_set_text(GTK_ENTRY(export_pass_entry), vcalprefs.export_pass);
	gtk_entry_set_text(GTK_ENTRY(export_freebusy_user_entry), vcalprefs.export_freebusy_user);
	gtk_entry_set_text(GTK_ENTRY(export_freebusy_pass_entry), vcalprefs.export_freebusy_pass);

	g_signal_connect(G_OBJECT(export_enable_checkbtn),
			 "toggled", G_CALLBACK(path_changed), page);
	g_signal_connect(G_OBJECT(export_freebusy_enable_checkbtn),
			 "toggled", G_CALLBACK(path_changed), page);
	g_signal_connect(G_OBJECT(export_path_entry),
			 "changed", G_CALLBACK(path_changed), page);
	g_signal_connect(G_OBJECT(export_freebusy_path_entry),
			 "changed", G_CALLBACK(path_changed), page);

	page->alert_enable_btn = alert_enable_checkbtn;
	page->alert_delay_spinbtn = alert_enable_spinbtn;

	page->export_enable_btn = export_enable_checkbtn;
	page->export_subs_btn = export_subs_checkbtn;
	page->export_path_entry = export_path_entry;
	page->export_command_entry = export_command_entry;

	page->export_freebusy_enable_btn = export_freebusy_enable_checkbtn;
	page->export_freebusy_path_entry = export_freebusy_path_entry;
	page->export_freebusy_command_entry = export_freebusy_command_entry;

	page->export_user_label = export_user_label;
	page->export_user_entry = export_user_entry;
	page->export_pass_label = export_pass_label;
	page->export_pass_entry = export_pass_entry;

	page->export_freebusy_user_label = export_freebusy_user_label;
	page->export_freebusy_user_entry = export_freebusy_user_entry;
	page->export_freebusy_pass_label = export_freebusy_pass_label;
	page->export_freebusy_pass_entry = export_freebusy_pass_entry;

	set_auth_sensitivity(page);

	page->freebusy_get_url_entry = freebusy_get_url_entry;

	page->page.widget = vbox1;
}

static void vcal_prefs_destroy_widget_func(PrefsPage *_page)
{
}

void vcal_prefs_save(void)
{
	PrefFile *pfile;
	gchar *rcpath;
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	pfile = prefs_write_open(rcpath);
	g_free(rcpath);
	if (!pfile || (prefs_set_block_label(pfile, PREFS_BLOCK_NAME) < 0))
		return;

	if (prefs_write_param(param, pfile->fp) < 0) {
		g_warning("failed to write Vcalendar configuration to file\n");
		prefs_file_close_revert(pfile);
		return;
	}
        if (fprintf(pfile->fp, "\n") < 0) {
		FILE_OP_ERROR(rcpath, "fprintf");
		prefs_file_close_revert(pfile);
	} else
	        prefs_file_close(pfile);
}

static void vcal_prefs_save_func(PrefsPage * _page)
{
	struct VcalendarPage *page = (struct VcalendarPage *) _page;

/* alert */
	vcalprefs.alert_enable =
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
					 (page->alert_enable_btn));
	vcalprefs.alert_delay =
		gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON
						 (page->alert_delay_spinbtn));

/* calendar export */
	vcalprefs.export_enable = 
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
					 (page->export_enable_btn));
	
	vcalprefs.export_subs = 
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
					 (page->export_subs_btn));
	
	g_free(vcalprefs.export_path);
	vcalprefs.export_path =
	    gtk_editable_get_chars(GTK_EDITABLE(page->export_path_entry), 0, -1);

	g_free(vcalprefs.export_command);
	vcalprefs.export_command =
	    gtk_editable_get_chars(GTK_EDITABLE(page->export_command_entry), 0, -1);
	
	g_free(vcalprefs.export_user);
	vcalprefs.export_user =
	    gtk_editable_get_chars(GTK_EDITABLE(page->export_user_entry), 0, -1);
	g_free(vcalprefs.export_pass);
	vcalprefs.export_pass =
	    gtk_editable_get_chars(GTK_EDITABLE(page->export_pass_entry), 0, -1);
	
/* free/busy export */
	vcalprefs.export_freebusy_enable = 
	    gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON
					 (page->export_freebusy_enable_btn));
	
	g_free(vcalprefs.export_freebusy_path);
	vcalprefs.export_freebusy_path =
	    gtk_editable_get_chars(GTK_EDITABLE(page->export_freebusy_path_entry), 0, -1);

	g_free(vcalprefs.export_freebusy_command);
	vcalprefs.export_freebusy_command =
	    gtk_editable_get_chars(GTK_EDITABLE(page->export_freebusy_command_entry), 0, -1);

	g_free(vcalprefs.export_freebusy_user);
	vcalprefs.export_freebusy_user =
	    gtk_editable_get_chars(GTK_EDITABLE(page->export_freebusy_user_entry), 0, -1);
	g_free(vcalprefs.export_freebusy_pass);
	vcalprefs.export_freebusy_pass =
	    gtk_editable_get_chars(GTK_EDITABLE(page->export_freebusy_pass_entry), 0, -1);
	

/* free/busy import */
	g_free(vcalprefs.freebusy_get_url);
	vcalprefs.freebusy_get_url =
	    gtk_editable_get_chars(GTK_EDITABLE(page->freebusy_get_url_entry), 0, -1);


	vcal_prefs_save();
	vcal_folder_export(NULL);
}

void vcal_prefs_init(void)
{
	static gchar *path[3];
	gchar *rcpath;

	path[0] = _("Plugins");
	path[1] = _("vCalendar");
	path[2] = NULL;

	prefs_set_default(param);
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	prefs_read_config(param, PREFS_BLOCK_NAME, rcpath, NULL);
	g_free(rcpath);

	vcal_prefs_page.page.path = path;
	vcal_prefs_page.page.create_widget = vcal_prefs_create_widget_func;
	vcal_prefs_page.page.destroy_widget = vcal_prefs_destroy_widget_func;
	vcal_prefs_page.page.save_page = vcal_prefs_save_func;

	prefs_gtk_register_page((PrefsPage *) &vcal_prefs_page);
}

void vcal_prefs_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) &vcal_prefs_page);
}
