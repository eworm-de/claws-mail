/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2003-2023 the Claws Mail Team
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
#  include "config.h"
#  include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include "defs.h"

#include <gtk/gtk.h>
#include <gtk/gtkutils.h>

#include "common/claws.h"
#include "common/version.h"
#include "plugin.h"
#include "utils.h"
#include "prefs.h"
#include "folder.h"
#include "prefs_gtk.h"
#include "foldersel.h"
#include "clamav_plugin.h"
#include "statusbar.h"
#include "alertpanel.h"
#include "clamd-plugin.h"

struct ClamAvPage
{
	PrefsPage page;
	
	GtkWidget *enable_clamav;
/*	GtkWidget *enable_arc;*/
	GtkWidget *max_size;
	GtkWidget *recv_infected;
	GtkWidget *save_folder;
	GtkWidget *config_type;
	GtkWidget *config_folder;
	GtkWidget *config_host;
	GtkWidget *config_port;
};

static GtkWidget *hbox_auto1, *hbox_auto2, *hbox_manual1, *hbox_manual2;

static void foldersel_cb(GtkWidget *widget, gpointer data)
{
	struct ClamAvPage *page = (struct ClamAvPage *) data;
	FolderItem *item;
	gchar *item_id;
	gint newpos = 0;
	
	item = foldersel_folder_sel(NULL, FOLDER_SEL_MOVE, NULL, FALSE,
			_("Select folder to store infected messages in"));
	if (item && (item_id = folder_item_get_identifier(item)) != NULL) {
		gtk_editable_delete_text(GTK_EDITABLE(page->save_folder), 0, -1);
		gtk_editable_insert_text(GTK_EDITABLE(page->save_folder), item_id, strlen(item_id), &newpos);
		g_free(item_id);
	}
}

static void clamd_folder_cb(GtkWidget *widget, gpointer data)
{
	GtkFileChooserNative *dialog;
	gchar* file;
	gint newpos = 0;
	struct ClamAvPage *page = (struct ClamAvPage *) data;

	dialog = gtk_file_chooser_native_new(
					"Select file with clamd configuration [clamd.conf]",
					NULL,
					GTK_FILE_CHOOSER_ACTION_OPEN,
					_("_Apply"),
					_("_Cancel"));

	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), "/etc");
	if (gtk_native_dialog_run (GTK_NATIVE_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
		file = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		debug_print("New clamd.conf: %s\n", file);
		if (file) {
			gtk_editable_delete_text(GTK_EDITABLE(page->config_folder), 0, -1);
			gtk_editable_insert_text(GTK_EDITABLE(page->config_folder), file, strlen(file), &newpos);
			g_free(file);
		}
	}
	g_object_unref(dialog);
}

static void check_permission(gchar* folder) {
	GStatBuf info;

	if (g_stat(folder, &info) < 0)
		return;
	mode_t perm = info.st_mode & ~(S_IFMT);
	debug_print("%s: Old file permission: %05o\n", folder, perm);
	if ((perm & S_IXOTH) != S_IXOTH) {
		perm = perm | S_IXOTH;
		g_chmod(folder, perm);
	}
	debug_print("%s: New file permission: %05o\n", folder, perm);
}

static void folder_permission_cb(GtkWidget *widget, gpointer data) {
	static gchar* folders[] = {
			".claws-mail",
			".claws-mail/mimetmp",
			".claws-mail/tmp",
			NULL};
	const gchar* home = get_home_dir();
	int i;

	check_permission((gchar *) home);
	for (i = 0; folders[i]; i++) {
		gchar* file = g_strdup_printf("%s/%s", home, folders[i]);
		check_permission(file);
		g_free(file);
	}
}

static void clamav_show_config(Config* config) {
	if (config) {
		if (config->ConfigType == MANUAL) {
			gtk_widget_hide(hbox_auto1);
			gtk_widget_hide(hbox_auto2);
			gtk_widget_show(hbox_manual1);
			gtk_widget_show(hbox_manual2);
		}
		else {
			gtk_widget_hide(hbox_manual1);
			gtk_widget_hide(hbox_manual2);
			gtk_widget_show(hbox_auto1);
			gtk_widget_show(hbox_auto2);
		}
	}
}

static void setting_type_cb(GtkWidget *widget, gpointer data) {
	gboolean state = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	struct ClamAvPage *page = (struct ClamAvPage *) data;
	Config* c;
	gint newpos = 0;
	gboolean tmp_conf = FALSE;

	if (page && page->page.widget) {
		/* Reset configuration */
		debug_print("Resetting configuration\n");
		gtk_editable_delete_text(GTK_EDITABLE(page->config_folder), 0, -1);
		gtk_editable_delete_text(GTK_EDITABLE(page->config_host), 0, -1);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(page->config_port), (gdouble) 0);
		clamav_save_config();
	
		c = clamd_get_config();
		if (!c) {
			c = clamd_config_new();
			tmp_conf = TRUE;
		}
		if (state) {
			/* Automatic configuration */
			debug_print("Setting clamd to automatic configuration\n");
			if (clamd_find_socket()) {
				if (tmp_conf) {
					Config* clamd_conf = clamd_get_config();
					if (clamd_conf->automatic.folder)
						c->automatic.folder = g_strdup(clamd_conf->automatic.folder);
					else
						c->automatic.folder = g_strdup("");
				}
				if (c->ConfigType == AUTOMATIC) {
					gtk_editable_insert_text(GTK_EDITABLE(page->config_folder),
						c->automatic.folder, strlen(c->automatic.folder), &newpos);
					clamav_save_config();
				}
			}
			c->ConfigType = AUTOMATIC;
			if (page->config_type)
			    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(page->config_type), TRUE);
		}
		else {
			/* Manual configuration */
			debug_print("Setting clamd to manual configuration\n");
			c->ConfigType = MANUAL;
			if (page->config_type)
			    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(page->config_type), FALSE);
		}
		clamav_show_config(c);
		if (tmp_conf)
			clamd_config_free(c);
	}
}

static void clamav_create_widget_func(PrefsPage * _page, GtkWindow *window, gpointer data)
{
	struct ClamAvPage *page = (struct ClamAvPage *) _page;
	ClamAvConfig *config;
	Config		 *clamd_config;
 	 
	GtkWidget *vbox1, *vbox2;
	GtkWidget *enable_clamav;
  	GtkWidget *label1;
/*  	GtkWidget *enable_arc;*/
  	GtkWidget *label2;
  	GtkAdjustment *max_size_adj;
  	GtkWidget *max_size;
	GtkWidget *hbox1;
  	GtkWidget *recv_infected;
  	GtkWidget *save_folder;
  	GtkWidget *save_folder_select;
	GtkWidget *clamd_conf_label;
	GtkWidget *config_folder;
	GtkWidget *config_host;
	GtkWidget *config_port;
	GtkWidget *config_folder_select;
	GtkWidget *blank;
	GtkWidget *permission_label;
	GtkWidget *permission_select;
	GtkWidget *host_label;
	GtkWidget *port_label;
	GtkWidget *setting_type;

	enable_clamav = page->enable_clamav;

	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (vbox2, enable_clamav, _("Enable virus scanning"));
/*	PACK_CHECK_BUTTON (vbox2, enable_arc, _("Scan archive contents"));

 	SET_TOGGLE_SENSITIVITY (enable_clamav, enable_arc);*/

 	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);
/*	SET_TOGGLE_SENSITIVITY (enable_arc, hbox1);*/

  	label1 = gtk_label_new(_("Maximum attachment size"));
  	gtk_widget_show (label1);
  	gtk_box_pack_start (GTK_BOX (hbox1), label1, FALSE, FALSE, 0);
 	SET_TOGGLE_SENSITIVITY (enable_clamav, label1);

  	max_size_adj = gtk_adjustment_new (1, 1, 1024, 1, 10, 0);
  	max_size = gtk_spin_button_new (GTK_ADJUSTMENT (max_size_adj), 1, 0);
	gtk_widget_show (max_size);
  	gtk_box_pack_start (GTK_BOX (hbox1), max_size, FALSE, FALSE, 0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (max_size), TRUE);
	gtk_widget_set_tooltip_text(max_size,
			     _("Message attachments larger than this will not be scanned"));
 	SET_TOGGLE_SENSITIVITY (enable_clamav, max_size);

  	label2 = gtk_label_new(_("MB"));
	gtk_widget_show (label2);
  	gtk_box_pack_start (GTK_BOX (hbox1), label2, FALSE, FALSE, 0);
 	SET_TOGGLE_SENSITIVITY (enable_clamav, label2);

  	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

 	recv_infected = gtk_check_button_new_with_label(_("Save infected mail in"));
	gtk_widget_show (recv_infected);
	gtk_box_pack_start (GTK_BOX (hbox1), recv_infected, FALSE, FALSE, 0);
	gtk_widget_set_tooltip_text(recv_infected,
			     _("Save mail that contains viruses"));
 	SET_TOGGLE_SENSITIVITY (enable_clamav, recv_infected);

  	save_folder = gtk_entry_new ();
	gtk_widget_show (save_folder);
	gtk_box_pack_start (GTK_BOX (hbox1), save_folder, TRUE, TRUE, 0);
	gtk_widget_set_tooltip_text(save_folder,
			     _("Folder for storing infected mail. Leave empty to use the default trash folder"));
 	SET_TOGGLE_SENSITIVITY (enable_clamav, save_folder);

	save_folder_select = gtkut_get_browse_directory_btn(_("_Browse"));
	gtk_widget_show (save_folder_select);
  	gtk_box_pack_start (GTK_BOX (hbox1), save_folder_select, FALSE, FALSE, 0);
	gtk_widget_set_tooltip_text(save_folder_select,
			     _("Click this button to select a folder for storing infected mail"));
 	SET_TOGGLE_SENSITIVITY (enable_clamav, save_folder_select);

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

 	setting_type = gtk_check_button_new_with_label(_("Automatic configuration"));
 	/*gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(setting_type), TRUE);*/
	gtk_widget_show (setting_type);
	gtk_box_pack_start (GTK_BOX (hbox1), setting_type, FALSE, FALSE, 0);
	gtk_widget_set_tooltip_text(setting_type,
			     _("Should configuration be done automatic or manual"));
 	SET_TOGGLE_SENSITIVITY (enable_clamav, setting_type);
	
  	hbox_auto1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox_auto1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_auto1, FALSE, FALSE, 0);

 	clamd_conf_label = gtk_label_new(_("Where is clamd.conf"));
	gtk_widget_show (clamd_conf_label);
	gtk_box_pack_start (GTK_BOX (hbox_auto1), clamd_conf_label, FALSE, FALSE, 0);

	config_folder = gtk_entry_new ();
	gtk_widget_show (config_folder);
	gtk_box_pack_start (GTK_BOX (hbox_auto1), config_folder, TRUE, TRUE, 0);
	gtk_widget_set_tooltip_text(config_folder,
			     _("Full path to clamd.conf. If this field is not empty then the plugin has been able to locate the file automatically"));
 	SET_TOGGLE_SENSITIVITY (enable_clamav, config_folder);

	config_folder_select = gtkut_get_browse_directory_btn(_("Br_owse"));
	gtk_widget_show (config_folder_select);
  	gtk_box_pack_start (GTK_BOX (hbox_auto1), config_folder_select, FALSE, FALSE, 0);
	gtk_widget_set_tooltip_text(config_folder_select,
			     _("Click this button to select full path to clamd.conf"));
 	SET_TOGGLE_SENSITIVITY (enable_clamav, config_folder_select);

  	hbox_auto2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox_auto2);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_auto2, FALSE, FALSE, 0);

 	permission_label = gtk_label_new(_("Check permission for folders and adjust if necessary"));
	gtk_widget_show (permission_label);
	gtk_box_pack_start (GTK_BOX (hbox_auto2), permission_label, FALSE, FALSE, 0);

	blank = gtk_label_new("");
	gtk_widget_show (blank);
	gtk_box_pack_start (GTK_BOX (hbox_auto2), blank, TRUE, TRUE, 0);

	permission_select = gtkut_stock_button("edit-find-replace", _("Find and _Replace"));
			/*gtk_button_new_with_mnemonic(_("_Check Permission"));*/
	gtk_widget_show (permission_select);
  	gtk_box_pack_start (GTK_BOX (hbox_auto2), permission_select, FALSE, FALSE, 0);
	gtk_widget_set_tooltip_text(permission_select,
			     _("Click this button to check and adjust folder permissions"));
 	SET_TOGGLE_SENSITIVITY (enable_clamav, permission_select);

  	hbox_manual1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox_manual1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_manual1, FALSE, FALSE, 0);

 	host_label = gtk_label_new(_("Remote Host"));
	gtk_widget_show (host_label);
	gtk_box_pack_start (GTK_BOX (hbox_manual1), host_label, FALSE, FALSE, 0);

	config_host = gtk_entry_new ();
	gtk_widget_show (config_host);
	gtk_box_pack_start (GTK_BOX (hbox_manual1), config_host, FALSE, FALSE, 0);
	gtk_widget_set_tooltip_text(config_host,
			     _("Hostname or IP for remote host running clamav daemon"));
 	SET_TOGGLE_SENSITIVITY (enable_clamav, config_host);

	blank = gtk_label_new("");
	gtk_widget_show (blank);
	gtk_box_pack_start (GTK_BOX (hbox_manual1), blank, TRUE, TRUE, 0);

  	hbox_manual2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (hbox_manual2);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_manual2, FALSE, FALSE, 0);

 	port_label = gtk_label_new(_("Port"));
	gtk_widget_show (port_label);
	gtk_box_pack_start (GTK_BOX (hbox_manual2), port_label, FALSE, FALSE, 0);

	config_port = gtk_spin_button_new_with_range(0, 65535, 1);
	gtk_widget_show (config_port);
	gtk_box_pack_start (GTK_BOX (hbox_manual2), config_port, FALSE, FALSE, 0);
	gtk_widget_set_tooltip_text(config_port,
			     _("Port number where clamav daemon is listening"));

	blank = gtk_label_new("");
	gtk_widget_show (blank);
	gtk_box_pack_start (GTK_BOX (hbox_manual2), blank, TRUE, TRUE, 0);

 	SET_TOGGLE_SENSITIVITY (enable_clamav, config_port);

	config = clamav_get_config();

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_clamav), config->clamav_enable);
/*	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_arc), config->clamav_enable_arc);*/
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(max_size), (float) config->clamav_max_size);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(recv_infected), config->clamav_recv_infected);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(setting_type), config->clamd_config_type);

        g_signal_connect(G_OBJECT(save_folder_select), "clicked",
			 G_CALLBACK(foldersel_cb), page);
	g_signal_connect(G_OBJECT(config_folder_select), "clicked",
			 G_CALLBACK(clamd_folder_cb), page);
	g_signal_connect(G_OBJECT(permission_select), "clicked",
			 G_CALLBACK(folder_permission_cb), page);
	g_signal_connect(G_OBJECT(setting_type), "clicked",
			 G_CALLBACK(setting_type_cb), page);

	clamd_config = clamd_get_config();
		
	if (config->clamav_save_folder != NULL)
		gtk_entry_set_text(GTK_ENTRY(save_folder), config->clamav_save_folder);
	if (!config->clamd_config_type) {
	/*if (config->clamd_host && strlen(config->clamd_host) > 0 && config->clamd_port > 0) {*/
		gtk_entry_set_text(GTK_ENTRY(config_host), config->clamd_host);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(config_port), (gdouble) config->clamd_port);
		/* activate manual checkbox and blind folder */
		debug_print("Showing manual configuration and hiding automatic configuration\n");
		if (! clamd_config) {
			clamd_config = clamd_config_new();
			clamd_config->ConfigType = MANUAL;
			clamav_show_config(clamd_config);
			clamd_config_free(clamd_config);
		}
		else
			clamav_show_config(clamd_config);
	}
	else {
	//else if (config->clamd_config_folder == NULL || strlen(config->clamd_config_folder) == 0) {
		if (clamd_find_socket()) {
			Config* c = clamd_get_config();
			if (c && c->ConfigType == AUTOMATIC) {
				config->clamd_config_folder = g_strdup(c->automatic.folder);
				/* deactivate manual checkbox and blind host and port */
				debug_print("Showing automatic configuration and hiding manual configuration\n");
				clamav_show_config(c);
				gint newpos = 0;
				gtk_editable_delete_text(GTK_EDITABLE(config_folder), 0, -1);
				gtk_editable_insert_text(GTK_EDITABLE(config_folder), 
					config->clamd_config_folder, strlen(config->clamd_config_folder), &newpos);
			}
			else if (c && c->ConfigType == MANUAL) {
				/* deactivate automatic automatic configuration */
				debug_print("Showing manual configuration and hiding automatic configuration\n");
				clamav_show_config(c);
			}
		}	
	}
/*	else {
		gtk_entry_set_text(GTK_ENTRY(config_folder), config->clamd_config_folder);
		// deactivate manual checkbox and blind host and port
		debug_print("Showing automatic configuration and hiding manual configuration\n");
		if (! clamd_config) {
			clamd_config = clamd_config_new();
			clamd_config->ConfigType = AUTOMATIC;
			clamav_show_config(clamd_config);
			clamd_config_free(clamd_config);
		}
		else
			clamav_show_config(clamd_config);
	}*/

	page->enable_clamav = enable_clamav;
/*	page->enable_arc = enable_arc;*/
	page->max_size = max_size;
	page->recv_infected = recv_infected;
	page->save_folder = save_folder;
	page->config_type = setting_type;
	page->config_folder = config_folder;
	page->config_host = config_host;
	page->config_port = config_port;
	page->page.widget = vbox1;
	
	clamav_save_config();
}

static void clamav_destroy_widget_func(PrefsPage *_page)
{
	debug_print("Destroying Clamd widget\n");
}

static void clamav_save_func(PrefsPage *_page)
{
	struct ClamAvPage *page = (struct ClamAvPage *) _page;
	ClamAvConfig *config;

	debug_print("Saving Clamd Page\n");

	config = clamav_get_config();

	config->clamav_enable = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->enable_clamav));
/*	config->clamav_enable_arc = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->enable_arc));*/

	config->clamav_max_size = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(page->max_size));
	config->clamav_recv_infected = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->recv_infected));
	g_free(config->clamav_save_folder);
	config->clamav_save_folder = gtk_editable_get_chars(GTK_EDITABLE(page->save_folder), 0, -1);
	config->clamd_config_type = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->config_type));
	g_free(config->clamd_config_folder);
	config->clamd_config_folder = gtk_editable_get_chars(GTK_EDITABLE(page->config_folder), 0, -1);
	g_free(config->clamd_host);
	config->clamd_host = gtk_editable_get_chars(GTK_EDITABLE(page->config_host), 0, -1);
	config->clamd_port = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(page->config_port));

	if (config->clamav_enable) {
		Clamd_Stat status = clamd_prepare();
		switch (status) {
			case NO_SOCKET: 
				g_warning("[New config] no socket information");
				alertpanel_error(_("New config\nNo socket information.\nAntivirus disabled."));
				break;
			case NO_CONNECTION:
				g_warning("[New config] Clamd does not respond to ping");
				alertpanel_warning(_("New config\nClamd does not respond to ping.\nIs clamd running?"));
				break;
			default:
				break;
		}
	}
	clamav_save_config();
}

static struct ClamAvPage clamav_page;

static void gtk_message_callback(gchar *message)
{
	statusbar_print_all("%s", message);
}

gint clamav_gtk_init(void)
{
	static gchar *path[3];

	path[0] = _("Plugins");
	path[1] = _("Clam AntiVirus");
	path[2] = NULL;

	clamav_page.page.path = path;
	clamav_page.page.create_widget = clamav_create_widget_func;
	clamav_page.page.destroy_widget = clamav_destroy_widget_func;
	clamav_page.page.save_page = clamav_save_func;
	clamav_page.page.weight = 35.0;
	
	prefs_gtk_register_page((PrefsPage *) &clamav_page);
	clamav_set_message_callback(gtk_message_callback);

	debug_print("Clamd GTK plugin loaded\n");
	return 0;	
}

void clamav_gtk_done(void)
{
        prefs_gtk_unregister_page((PrefsPage *) &clamav_page);
}
