/*
 * acpi_notifier -- for Claws Mail
 *
 * Copyright (C) 2005-2019 Colin Leroy <colin@colino.net>
 * 	and the Claws Mail Team
 *
 * Sylpheed is a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2019 Hiroyuki Yamamoto and the Claws Mail Team
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

#include <glib.h>
#include <glib/gi18n.h>

#include <string.h>

#include <gtk/gtk.h>

#include "defs.h"
#include "claws.h"
#include "version.h"
#include "prefs.h"
#include "prefs_gtk.h"
#include "main.h"
#include "menu.h"
#include "hooks.h"
#include "plugin.h"
#include "alertpanel.h"
#include "utils.h"
#include "folder_item_prefs.h"
#include "gtk/gtkutils.h"
#include "gtk/combobox.h"
#include "file-utils.h"

#define PREFS_BLOCK_NAME "AcpiNotifier"
#define PLUGIN_NAME _("Acpi Notifier")

typedef struct _PredefinedAcpis {
	gchar *name;
	gchar *on_param;
	gchar *off_param;
	gchar *file_path;
	gboolean is_program;
	gchar *help;
} PredefinedAcpis;

/**
 * Add your implementation here (and send me the patch!) 
 */
char *acpi_help[] = {
	"",
	N_("Make sure that the kernel module 'acerhk' is loaded.\n"
	    "You can get it from http://www.cakey.de/acerhk/"),
	N_("Make sure that the kernel module 'acer_acpi' is loaded.\n"
	    "You can get it from http://code.google.com/p/aceracpi/"),
	N_("Make sure that the kernel module 'asus_laptop' is loaded."),
	N_("Make sure that the kernel module 'asus_acpi' is loaded."),
	N_("Make sure that the kernel module 'ibm_acpi' is loaded."),
	N_("Make sure that you have apanelc installed.\n"
	    "You can get it from http://apanel.sourceforge.net/"),
	NULL
};
PredefinedAcpis known_implementations[] = {
	{"Other file", "", "", "", FALSE, NULL},

	{"ACER (acerhk)", "1", "0", "/proc/driver/acerhk/led", FALSE, NULL},

	{"ACER (acer_acpi)", "1", "0", "/proc/acpi/acer/mailled", FALSE, NULL},

	{"ASUS (asus_laptop)", "1", "0", "/sys/class/leds/asus:mail/brightness", FALSE, NULL},

	{"ASUS (asus_acpi)", "1", "0", "/proc/acpi/asus/mled", FALSE, NULL},

	{"IBM (ibm_acpi)", "7 on", "7 off", "/proc/acpi/ibm/led", FALSE, NULL},

	{"Lenovo (tp_smapi)", "on", "off", "/proc/acpi/ibm/light", FALSE, NULL},

	{"Fujitsu (apanel)", "led on", "led off", "apanelc", TRUE, NULL},

	{NULL, NULL, NULL, NULL, FALSE, NULL}
};

static gulong folder_hook_id = HOOK_NONE;
static gulong alertpanel_hook_id = HOOK_NONE;

struct AcpiNotifierPage
{
	PrefsPage page;
	
	GtkWidget *no_mail_off_btn;
	GtkWidget *no_mail_blink_btn;
	GtkWidget *no_mail_on_btn;
	GtkWidget *unread_mail_off_btn;
	GtkWidget *unread_mail_blink_btn;
	GtkWidget *unread_mail_on_btn;
	GtkWidget *new_mail_off_btn;
	GtkWidget *new_mail_blink_btn;
	GtkWidget *new_mail_on_btn;
	GtkWidget *default_implementations_optmenu;
	GtkWidget *on_value_entry;
	GtkWidget *off_value_entry;
	GtkWidget *file_entry;
	GtkWidget *hbox_acpi_file;
	GtkWidget *hbox_acpi_values;
	GtkWidget *warning_label;
	GtkWidget *warning_box;
	GtkWidget *blink_on_err_chkbtn;
};

typedef struct _AcpiNotifierPrefs AcpiNotifierPrefs;

struct _AcpiNotifierPrefs
{
	gint		 no_mail_action;
	gint		 unread_mail_action;
	gint		 new_mail_action;
	gboolean	 blink_on_err;
	gchar 		*on_param;	
	gchar 		*off_param;	
	gchar 		*file_path;	
};

AcpiNotifierPrefs acpiprefs;

static struct AcpiNotifierPage acpi_prefs_page;

enum {
	OFF = 0,
	BLINK,
	ON
} BlinkType;

static PrefParam param[] = {
	{"no_mail_action", "0", &acpiprefs.no_mail_action, P_INT,
	 NULL, NULL, NULL},
	{"unread_mail_action", "0", &acpiprefs.unread_mail_action, P_INT,
	 NULL, NULL, NULL},
	{"new_mail_action", "1", &acpiprefs.new_mail_action, P_INT,
	 NULL, NULL, NULL},
	{"blink_on_err", "TRUE", &acpiprefs.blink_on_err, P_BOOL,
	 NULL, NULL, NULL},
	{"on_param", NULL, &acpiprefs.on_param, P_STRING,
	 NULL, NULL, NULL},
	{"off_param", NULL, &acpiprefs.off_param, P_STRING,
	 NULL, NULL, NULL},
	{"file_path", NULL, &acpiprefs.file_path, P_STRING,
	 NULL, NULL, NULL},
	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

static gboolean check_impl (const gchar *filepath)
{
	int i;
	for (i = 0; known_implementations[i].name != NULL; i++) {
		if (strcmp(known_implementations[i].file_path, filepath))
			continue;
		if (!known_implementations[i].is_program)
			return is_file_exist(filepath);
		else {
			gchar *cmd = g_strdup_printf("which %s", filepath);
			int found = system(cmd);
			g_free(cmd);
			return (found == 0);
		}
	}
	return is_file_exist(filepath);
}

static gboolean is_program (const gchar *filepath)
{
	int i;
	for (i = 0; known_implementations[i].name != NULL; i++) {
		if (strcmp(known_implementations[i].file_path, filepath))
			continue;
		return known_implementations[i].is_program;
	}
	return FALSE;
}

static void show_error (struct AcpiNotifierPage *page, const gchar *filepath)
{
	int i;
	if (!filepath) {
		gtk_widget_hide(page->warning_box);
		return;
	}
	for (i = 0; known_implementations[i].name != NULL; i++) {
		if (strcmp(known_implementations[i].file_path, filepath))
			continue;
		if (known_implementations[i].help) {
			gchar *tmp = g_strdup_printf("%s\n%s", 
					_("Control file doesn't exist."),
					known_implementations[i].help);
			gtk_label_set_text(GTK_LABEL(page->warning_label), tmp);
			g_free(tmp);
		} else {
			gtk_label_set_text(GTK_LABEL(page->warning_label), 
				_("Control file doesn't exist."));
		}
		gtk_widget_show_all(page->warning_box);
		return;
	}
}

static void type_changed(GtkComboBox *combobox, gpointer data)
{
	gint selected = gtk_combo_box_get_active(combobox);
	struct AcpiNotifierPage *page = (struct AcpiNotifierPage *)data;

	if (selected != 0) {
		gtk_widget_hide(page->hbox_acpi_file);
		gtk_widget_hide(page->hbox_acpi_values);
		gtk_entry_set_text(GTK_ENTRY(page->file_entry), 
			known_implementations[selected].file_path);
		gtk_entry_set_text(GTK_ENTRY(page->on_value_entry), 
			known_implementations[selected].on_param);
		gtk_entry_set_text(GTK_ENTRY(page->off_value_entry), 
			known_implementations[selected].off_param);
		if (!check_impl(known_implementations[selected].file_path))
			show_error(page, known_implementations[selected].file_path);
		else
			show_error(page, NULL);
	} else {
		gtk_widget_show_all(page->hbox_acpi_file);
		gtk_widget_show_all(page->hbox_acpi_values);
	}
}

static void file_entry_changed (GtkWidget *entry, gpointer data)
{
	struct AcpiNotifierPage *page = (struct AcpiNotifierPage *)data;
	if (!page->warning_box)
		return;

	if (!check_impl(gtk_entry_get_text(GTK_ENTRY(entry))))
		show_error(page, gtk_entry_get_text(GTK_ENTRY(entry)));
	else
		show_error(page, NULL);
}

static void acpi_prefs_create_widget_func(PrefsPage * _page,
					   GtkWindow * window,
					   gpointer data)
{
	struct AcpiNotifierPage *page = (struct AcpiNotifierPage *) _page;

	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *hbox_acpi_file;
	GtkWidget *hbox_acpi_values;
	GtkWidget *start_label;
	GtkWidget *no_mail_label;
	GtkWidget *no_mail_off_btn;
	GtkWidget *no_mail_blink_btn;
	GtkWidget *no_mail_on_btn;
	GtkWidget *unread_mail_label;
	GtkWidget *unread_mail_off_btn;
	GtkWidget *unread_mail_blink_btn;
	GtkWidget *unread_mail_on_btn;
	GtkWidget *new_mail_label;
	GtkWidget *new_mail_off_btn;
	GtkWidget *new_mail_blink_btn;
	GtkWidget *new_mail_on_btn;
	GtkWidget *default_implementations_optmenu;
	GtkListStore *menu;
	GtkWidget *on_value_entry;
	GtkWidget *off_value_entry;
	GtkWidget *file_entry;
	GtkWidget *warning_label;
	GtkWidget *warning_box;
	GtkWidget *image;
	GtkWidget *blink_on_err_chkbtn;
	GtkTreeIter iter; /* iter for the COMBOBOX_ADD macro */

	int i;
	int found = 0;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), VBOX_BORDER);
	
	no_mail_label = gtk_label_new(_(" : no new or unread mail"));
	unread_mail_label = gtk_label_new(_(" : unread mail"));
	new_mail_label = gtk_label_new(_(" : new mail"));

	no_mail_off_btn = gtk_radio_button_new_with_label(NULL, _("off"));
	no_mail_blink_btn = gtk_radio_button_new_with_label_from_widget(
		GTK_RADIO_BUTTON(no_mail_off_btn), _("blinking"));
	no_mail_on_btn = gtk_radio_button_new_with_label_from_widget(
		GTK_RADIO_BUTTON(no_mail_off_btn), _("on"));

	unread_mail_off_btn = gtk_radio_button_new_with_label(NULL, _("off"));
	unread_mail_blink_btn = gtk_radio_button_new_with_label_from_widget(
		GTK_RADIO_BUTTON(unread_mail_off_btn), _("blinking"));
	unread_mail_on_btn = gtk_radio_button_new_with_label_from_widget(
		GTK_RADIO_BUTTON(unread_mail_off_btn), _("on"));

	new_mail_off_btn = gtk_radio_button_new_with_label(NULL, _("off"));
	new_mail_blink_btn = gtk_radio_button_new_with_label_from_widget(
		GTK_RADIO_BUTTON(new_mail_off_btn), _("blinking"));
	new_mail_on_btn = gtk_radio_button_new_with_label_from_widget(
		GTK_RADIO_BUTTON(new_mail_off_btn), _("on"));

	on_value_entry = gtk_entry_new();
	off_value_entry = gtk_entry_new();
	file_entry = gtk_entry_new();
	gtk_widget_set_size_request(on_value_entry, 40, -1);
	gtk_widget_set_size_request(off_value_entry, 40, -1);

	/* Create the ACPI type menu */
	default_implementations_optmenu = gtkut_sc_combobox_create(NULL, FALSE);
	menu = GTK_LIST_STORE(gtk_combo_box_get_model(
				GTK_COMBO_BOX(default_implementations_optmenu)));
	g_signal_connect(G_OBJECT(default_implementations_optmenu), "changed",
			G_CALLBACK(type_changed), page);

	/* Populate it with known implementations */
	for (i = 0; known_implementations[i].name != NULL; i++) {
		COMBOBOX_ADD(menu, known_implementations[i].name, i);
	}
	
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	start_label = gtk_label_new(_("LED "));
	gtk_box_pack_start(GTK_BOX(hbox), start_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), no_mail_off_btn, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), no_mail_blink_btn, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), no_mail_on_btn, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), no_mail_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
	
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	start_label = gtk_label_new(_("LED "));
	gtk_box_pack_start(GTK_BOX(hbox), 
			start_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), 
			unread_mail_off_btn, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), 
			unread_mail_blink_btn, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), 
			unread_mail_on_btn, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), 
			unread_mail_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), 
			hbox, FALSE, FALSE, 0);
	
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	start_label = gtk_label_new(_("LED "));
	gtk_box_pack_start(GTK_BOX(hbox), 
			start_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), 
			new_mail_off_btn, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), 
			new_mail_blink_btn, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), 
			new_mail_on_btn, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), 
			new_mail_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), 
			hbox, FALSE, FALSE, 0);
	
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	start_label = gtk_label_new(_("ACPI type: "));
	gtk_box_pack_start(GTK_BOX(hbox), 
			start_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), 
			default_implementations_optmenu, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), 
			hbox, FALSE, FALSE, 0);

	hbox_acpi_file = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	start_label = gtk_label_new(_("ACPI file: "));
	gtk_box_pack_start(GTK_BOX(hbox_acpi_file), 
			start_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_acpi_file), 
			file_entry, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), 
			hbox_acpi_file, FALSE, FALSE, 0);
        g_signal_connect(G_OBJECT(file_entry), "changed",
                         G_CALLBACK(file_entry_changed), page);

	hbox_acpi_values = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	start_label = gtk_label_new(_("values - On: "));
	gtk_box_pack_start(GTK_BOX(hbox_acpi_values), 
			start_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_acpi_values), 
			on_value_entry, FALSE, FALSE, 0);
	start_label = gtk_label_new(_(" - Off: "));
	gtk_box_pack_start(GTK_BOX(hbox_acpi_values), 
			start_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_acpi_values), 
			off_value_entry, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), 
			hbox_acpi_values, FALSE, FALSE, 0);

	warning_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	
	image = gtk_button_new_from_icon_name("dialog-warning",
			GTK_ICON_SIZE_SMALL_TOOLBAR);
	gtk_box_pack_start(GTK_BOX(warning_box), image, FALSE, FALSE, 0);
	warning_label = gtk_label_new(
			_("Control file doesn't exist."));
	gtk_box_pack_start(GTK_BOX(warning_box), warning_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), warning_box, FALSE, FALSE, 0);

	gtk_widget_show_all(vbox);
	gtk_widget_hide(warning_box);

	blink_on_err_chkbtn = gtk_check_button_new_with_label(
		_("Blink when user interaction is required"));
	gtk_box_pack_start(GTK_BOX(vbox), blink_on_err_chkbtn, FALSE, FALSE, 0);
	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(blink_on_err_chkbtn),
			acpiprefs.blink_on_err);
	gtk_widget_show(blink_on_err_chkbtn);

	switch (acpiprefs.no_mail_action) {
	case OFF: 	
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(no_mail_off_btn), TRUE); 
		break;
	case BLINK:	
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(no_mail_blink_btn), TRUE); 
		break;
	case ON:	
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(no_mail_on_btn), TRUE); 
		break;
	}
	
	switch (acpiprefs.unread_mail_action) {
	case OFF: 	
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(unread_mail_off_btn), TRUE); 
		break;
	case BLINK:	
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(unread_mail_blink_btn), TRUE); 
		break;
	case ON:	
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(unread_mail_on_btn), TRUE); 
		break;
	}
	
	switch (acpiprefs.new_mail_action) {
	case OFF: 	
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(new_mail_off_btn), TRUE); 
		break;
	case BLINK:	
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(new_mail_blink_btn), TRUE); 
		break;
	case ON:	
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(new_mail_on_btn), TRUE); 
		break;
	}
	
	if (acpiprefs.file_path != NULL) {
		for (i = 0; known_implementations[i].name != NULL; i++) {
			if (!strcmp(acpiprefs.file_path, 
				    known_implementations[i].file_path)) {
				gtk_combo_box_set_active(
						GTK_COMBO_BOX(default_implementations_optmenu), i);
				found = i;
			}
		}
	}
	if (found == 0) {
		for (i = 0; known_implementations[i].name != NULL; i++) {
			if (check_impl(known_implementations[i].file_path)) {
				gtk_combo_box_set_active(
						GTK_COMBO_BOX(default_implementations_optmenu), i);
				found = i;
			}
		}
	}
	page->page.widget = vbox;

	page->no_mail_off_btn = no_mail_off_btn;
	page->no_mail_blink_btn = no_mail_blink_btn;
	page->no_mail_on_btn = no_mail_on_btn;
	page->unread_mail_off_btn = unread_mail_off_btn;
	page->unread_mail_blink_btn = unread_mail_blink_btn;
	page->unread_mail_on_btn = unread_mail_on_btn;
	page->new_mail_off_btn = new_mail_off_btn;
	page->new_mail_blink_btn = new_mail_blink_btn;
	page->new_mail_on_btn = new_mail_on_btn;
	page->default_implementations_optmenu = default_implementations_optmenu;
	page->on_value_entry = on_value_entry;
	page->off_value_entry = off_value_entry;
	page->file_entry = file_entry;
	page->hbox_acpi_file = hbox_acpi_file;
	page->hbox_acpi_values = hbox_acpi_values;
	page->warning_box = warning_box;
	page->warning_label = warning_label;
	page->blink_on_err_chkbtn = blink_on_err_chkbtn;

	if (found != 0) {
		gtk_widget_hide(hbox_acpi_file);
		gtk_widget_hide(hbox_acpi_values);
		gtk_entry_set_text(GTK_ENTRY(file_entry), 
			known_implementations[found].file_path);
		gtk_entry_set_text(GTK_ENTRY(on_value_entry), 
			known_implementations[found].on_param);
		gtk_entry_set_text(GTK_ENTRY(off_value_entry), 
			known_implementations[found].off_param);
		
		if (!check_impl(known_implementations[found].file_path))
			show_error(page, known_implementations[found].file_path);
	} else {
		gtk_combo_box_set_active(
				GTK_COMBO_BOX(default_implementations_optmenu), 0);
		gtk_widget_show_all(hbox_acpi_file);
		gtk_widget_show_all(hbox_acpi_values);
		if (acpiprefs.file_path != NULL)
			gtk_entry_set_text(GTK_ENTRY(file_entry), 
					acpiprefs.file_path);
		if (acpiprefs.on_param != NULL)
			gtk_entry_set_text(GTK_ENTRY(on_value_entry), 
					acpiprefs.on_param);
		if (acpiprefs.off_param != NULL)
			gtk_entry_set_text(GTK_ENTRY(off_value_entry), 
					acpiprefs.off_param);
		if (!acpiprefs.file_path || !check_impl(acpiprefs.file_path))
			show_error(page, acpiprefs.file_path);
	}
}

static void acpi_prefs_destroy_widget_func(PrefsPage *_page)
{
}

static void acpi_prefs_save_func(PrefsPage * _page)
{
	struct AcpiNotifierPage *page = (struct AcpiNotifierPage *) _page;
	PrefFile *pfile;
	gchar *rcpath;
	gint selected = 0;

	g_free(acpiprefs.file_path);
	acpiprefs.file_path = gtk_editable_get_chars(
			GTK_EDITABLE(page->file_entry), 0, -1);
	g_free(acpiprefs.on_param);
	acpiprefs.on_param = gtk_editable_get_chars(
			GTK_EDITABLE(page->on_value_entry), 0, -1);
	g_free(acpiprefs.off_param);
	acpiprefs.off_param = gtk_editable_get_chars(
			GTK_EDITABLE(page->off_value_entry), 0, -1);

	if (gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->no_mail_off_btn)))
		acpiprefs.no_mail_action = OFF;
	else if (gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->no_mail_blink_btn)))
		acpiprefs.no_mail_action = BLINK;
	else if (gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->no_mail_on_btn)))
		acpiprefs.no_mail_action = ON;
	
	if (gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->unread_mail_off_btn)))
		acpiprefs.unread_mail_action = OFF;
	else if (gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->unread_mail_blink_btn)))
		acpiprefs.unread_mail_action = BLINK;
	else if (gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->unread_mail_on_btn)))
		acpiprefs.unread_mail_action = ON;
	
	if (gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->new_mail_off_btn)))
		acpiprefs.new_mail_action = OFF;
	else if (gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->new_mail_blink_btn)))
		acpiprefs.new_mail_action = BLINK;
	else if (gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->new_mail_on_btn)))
		acpiprefs.new_mail_action = ON;
	
	acpiprefs.blink_on_err = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->blink_on_err_chkbtn));

	selected = gtk_combo_box_get_active(GTK_COMBO_BOX(page->default_implementations_optmenu));

	if (selected != 0) {
		g_free(acpiprefs.file_path);
		acpiprefs.file_path = g_strdup(
				known_implementations[selected].file_path);
		g_free(acpiprefs.on_param);
		acpiprefs.on_param = g_strdup(
				known_implementations[selected].on_param);
		g_free(acpiprefs.off_param);
		acpiprefs.off_param = g_strdup(
				known_implementations[selected].off_param);
	}

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	pfile = prefs_write_open(rcpath);
	g_free(rcpath);
	if (!pfile || (prefs_set_block_label(pfile, PREFS_BLOCK_NAME) < 0))
		return;

	if (prefs_write_param(param, pfile->fp) < 0) {
		g_warning("failed to write " PREFS_BLOCK_NAME 
			  " configuration to file");
		prefs_file_close_revert(pfile);
		return;
	}
        if (fprintf(pfile->fp, "\n") < 0) {
		FILE_OP_ERROR(rcpath, "fprintf");
		prefs_file_close_revert(pfile);
	} else
	        prefs_file_close(pfile);
}

static void acpi_set(gboolean on)
{
	FILE *fp = NULL;

	if (!acpiprefs.file_path) {
		debug_print("acpiprefs.file_path NULL\n");
		return;
	}
	if (!check_impl(acpiprefs.file_path)) {
		debug_print("acpiprefs.file_path not implemented\n");
		return;
	}
	if (!acpiprefs.on_param || !acpiprefs.off_param) {
		debug_print("no param\n");
		return;
	}

	if (!is_program(acpiprefs.file_path)) {
		fp = claws_fopen(acpiprefs.file_path, "wb");
		if (fp == NULL)
			return;

		if (on) {
			claws_fwrite(acpiprefs.on_param, 1, strlen(acpiprefs.on_param), fp);
		} else {
			claws_fwrite(acpiprefs.off_param, 1, strlen(acpiprefs.off_param), fp);
		}
		claws_safe_fclose(fp);
	} else {
		gchar *cmd = g_strdup_printf("%s %s", 
				acpiprefs.file_path,
				on ? acpiprefs.on_param:acpiprefs.off_param);
		execute_command_line(cmd, TRUE, NULL);
		g_free(cmd);
	}
}

static guint should_quit = FALSE;
static int last_blink = 0;

static gint acpi_blink(gpointer data)
{
	if (!should_quit) {
		acpi_set(last_blink); 
		last_blink = !last_blink; 
		return TRUE;
	} else {
		acpi_set(FALSE);
		return FALSE;
	}
}

static int blink_timeout_id = 0;
static int alertpanel_blink_timeout_id = 0;
static gint my_new = -1, my_unread = -1;
static int my_action = -1;

static gboolean acpi_update_hook(gpointer source, gpointer data)
{
	int action = 0;
	guint new, unread, unreadmarked, marked, total;
	guint replied, forwarded, locked, ignored, watched;
	
	if (alertpanel_blink_timeout_id)
		return FALSE;

	folder_count_total_msgs(&new, &unread, &unreadmarked, &marked, &total,
				&replied, &forwarded, &locked, &ignored, &watched);

	if (my_new != new || my_unread != unread) {
		my_new = new;
		my_unread = unread;
		if (my_new > 0) {
			action = acpiprefs.new_mail_action;
		} else if (my_unread > 0) {
			action = acpiprefs.unread_mail_action;
		} else {
			action = acpiprefs.no_mail_action;
		}
		
		if (action != my_action) {
			my_action = action;
			
			if (action != BLINK && blink_timeout_id != 0) {
				g_source_remove(blink_timeout_id);
				blink_timeout_id = 0;
			}

			switch (action) {
			case ON: 
				acpi_set(TRUE); 
				break;
			case BLINK:	
				acpi_set(TRUE); 
				last_blink = FALSE; 
				blink_timeout_id = g_timeout_add(1000, acpi_blink, NULL);
				break;
			case OFF:	
				acpi_set(FALSE); 
				break;
			}
		}
	}

	return FALSE;
}

static gboolean acpi_alertpanel_hook(gpointer source, gpointer data)
{
	gboolean *opened = (gboolean *)source;

	if (*opened == TRUE) {
		if (blink_timeout_id)
			g_source_remove(blink_timeout_id);
		blink_timeout_id = 0;
		
		if (alertpanel_blink_timeout_id)
			return FALSE;
		
		acpi_set(TRUE); 
		last_blink = FALSE; 
		alertpanel_blink_timeout_id = g_timeout_add(250, acpi_blink, NULL);
	} else {
		if (alertpanel_blink_timeout_id)
			g_source_remove(alertpanel_blink_timeout_id);
		alertpanel_blink_timeout_id = 0;
		my_new = -1;
		my_unread = -1;
		my_action = -1;
		acpi_update_hook(NULL, NULL);
	}
	return FALSE;
}

void acpi_prefs_init(void)
{
	static gchar *path[3];
	gchar *rcpath;

	path[0] = _("Plugins");
	path[1] = PLUGIN_NAME;
	path[2] = NULL;

	prefs_set_default(param);
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	prefs_read_config(param, PREFS_BLOCK_NAME, rcpath, NULL);
	g_free(rcpath);

	acpi_prefs_page.page.path = path;
	acpi_prefs_page.page.create_widget = acpi_prefs_create_widget_func;
	acpi_prefs_page.page.destroy_widget = acpi_prefs_destroy_widget_func;
	acpi_prefs_page.page.save_page = acpi_prefs_save_func;
	acpi_prefs_page.page.weight = 40.0;

	prefs_gtk_register_page((PrefsPage *) &acpi_prefs_page);
	folder_hook_id = hooks_register_hook (FOLDER_ITEM_UPDATE_HOOKLIST, 
			acpi_update_hook, NULL);
	alertpanel_hook_id = hooks_register_hook (ALERTPANEL_OPENED_HOOKLIST, 
			acpi_alertpanel_hook, NULL);
	should_quit = FALSE;
}

void acpi_prefs_done(void)
{
	should_quit = TRUE;
	acpi_set(FALSE); 
	if (claws_is_exiting())
		return;
	prefs_gtk_unregister_page((PrefsPage *) &acpi_prefs_page);
	hooks_unregister_hook(FOLDER_ITEM_UPDATE_HOOKLIST, folder_hook_id);
	hooks_unregister_hook(ALERTPANEL_OPENED_HOOKLIST, alertpanel_hook_id);
}


void acpi_init(void)
{
	gint i;
	for (i = 0; acpi_help[i] != NULL; i++)
		known_implementations[i].help = 
			*acpi_help[i] ? _(acpi_help[i]) : "";
	acpi_prefs_init();
}

void acpi_done(void)
{
	acpi_prefs_done();
}

gint plugin_init(gchar **error)
{
	if( !check_plugin_version(MAKE_NUMERIC_VERSION(3,8,1,46),
				VERSION_NUMERIC, PLUGIN_NAME, error) )
		return -1;

	acpi_init();
	return 0;
}

gboolean plugin_done(void)
{
	if (blink_timeout_id)
		g_source_remove(blink_timeout_id);
	if (alertpanel_blink_timeout_id)
		g_source_remove(alertpanel_blink_timeout_id);

	acpi_done();
	return TRUE;
}

const gchar *plugin_name(void)
{
	return PLUGIN_NAME;
}

const gchar *plugin_desc(void)
{
	return _("This plugin handles various ACPI mail LEDs.");
}

const gchar *plugin_type(void)
{
	return "GTK3";
}

const gchar *plugin_licence(void)
{
	return "GPL3+";
}

const gchar *plugin_version(void)
{
	return VERSION;
}

struct PluginFeature *plugin_provides(void)
{
	static struct PluginFeature features[] = 
		{ {PLUGIN_NOTIFIER, N_("Laptop LED")},
		  {PLUGIN_NOTHING, NULL}};
	return features;
}
