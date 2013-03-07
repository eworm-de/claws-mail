/* 
 * Claws Mail -- A GTK+ based, lightweight, and fast e-mail client
 * Copyright(C) 1999-2013 the Claws Mail Team
 * == Fancy Plugin ==
 * This file Copyright (C) 2009-2013 Salvatore De Paolis
 * <iwkse@claws-mail.org> and the Claws Mail Team
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write tothe Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include "defs.h"
#include "version.h"
#include "claws.h"
#include "plugin.h"


#include "gtkutils.h"
#include "utils.h"
#include "prefs.h"
#include "prefs_common.h"
#include "prefs_gtk.h"
#include "prefswindow.h"
#include "combobox.h"
#include "addressbook.h"

#include "fancy_prefs.h"

#define PREFS_BLOCK_NAME "fancy"

FancyPrefs fancy_prefs;

static void prefs_set_proxy_entry_sens(GtkWidget *button, GtkEntry *entry_str);

#ifdef HAVE_LIBSOUP_GNOME
static void prefs_disable_fancy_proxy(GtkWidget *checkbox, GtkWidget *block);
#endif
typedef struct _FancyPrefsPage FancyPrefsPage;

struct _FancyPrefsPage {
	PrefsPage page;
	GtkWidget *auto_load_images;
	GtkWidget *enable_inner_navigation;
	GtkWidget *enable_scripts;
	GtkWidget *enable_plugins;
	GtkWidget *enable_java;
	GtkWidget *open_external;
#ifdef HAVE_LIBSOUP_GNOME    
	GtkWidget *gnome_proxy_checkbox;
#endif
	GtkWidget *proxy_checkbox;
	GtkWidget *proxy_str;
};

static PrefParam param[] = {
		{"auto_load_images", "FALSE", &fancy_prefs.auto_load_images, P_BOOL, 
		NULL, NULL, NULL},
		{"enable_inner_navigation", "FALSE", &fancy_prefs.enable_inner_navigation, P_BOOL, 
		NULL, NULL, NULL},
		{"enable_scripts", "FALSE", &fancy_prefs.enable_scripts, P_BOOL, 
		NULL, NULL, NULL},
		{"enable_plugins", "FALSE", &fancy_prefs.enable_plugins, P_BOOL, 
		NULL, NULL, NULL},
		{"open_external", "TRUE", &fancy_prefs.open_external, P_BOOL, 
		NULL, NULL, NULL},
		{"zoom_level", "100", &fancy_prefs.zoom_level, P_INT, 
		NULL, NULL, NULL},
		{"enable_java", "FALSE", &fancy_prefs.enable_java, P_BOOL, 
		NULL, NULL, NULL},
#ifdef HAVE_LIBSOUP_GNOME    
		{"enable_gnome_proxy","FALSE", &fancy_prefs.enable_gnome_proxy, P_BOOL, 
		NULL, NULL, NULL},
#endif
		{"enable_proxy", "FALSE", &fancy_prefs.enable_proxy, P_BOOL, 
		NULL, NULL, NULL},
		{"proxy_server", "http://SERVERNAME:PORT", &fancy_prefs.proxy_str, P_STRING, 
		NULL, NULL, NULL},
		{0,0,0,0}
};

static FancyPrefsPage fancy_prefs_page;

static void create_fancy_prefs_page     (PrefsPage *page, GtkWindow *window, 
										 gpointer   data);
static void destroy_fancy_prefs_page    (PrefsPage *page);
static void save_fancy_prefs_page       (PrefsPage *page);
static void save_fancy_prefs            (PrefsPage *page);

void fancy_prefs_init(void)
{
	static gchar *path[3];
	gchar *rcpath;

	path[0] = _("Plugins");
	path[1] = "Fancy";
	path[2] = NULL;

	prefs_set_default(param);
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	prefs_read_config(param, PREFS_BLOCK_NAME, rcpath, NULL);
	g_free(rcpath);
	    
	fancy_prefs_page.page.path = path;
	fancy_prefs_page.page.create_widget = create_fancy_prefs_page;
	fancy_prefs_page.page.destroy_widget = destroy_fancy_prefs_page;
	fancy_prefs_page.page.save_page = save_fancy_prefs_page;
	fancy_prefs_page.page.weight = 30.0;
	prefs_gtk_register_page((PrefsPage *) &fancy_prefs_page);
}

void fancy_prefs_done(void)
{
	save_fancy_prefs((PrefsPage *) &fancy_prefs_page);
	prefs_gtk_unregister_page((PrefsPage *) &fancy_prefs_page);
}

static void open_external_set_label_cb(GtkWidget *button, FancyPrefsPage *prefs_page)
{
	GtkTreeModel *model = gtk_combo_box_get_model(GTK_COMBO_BOX(prefs_page->open_external));
	GtkTreeIter iter;
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(prefs_page->enable_inner_navigation)))
			gtk_list_store_set(model, &iter, COMBOBOX_TEXT, _("Open in viewer"), -1);
		else
			gtk_list_store_set(model, &iter, COMBOBOX_TEXT, _("Do nothing"), -1);
	}

}
static void create_fancy_prefs_page(PrefsPage *page, GtkWindow *window, 
									gpointer data)
{
	FancyPrefsPage *prefs_page = (FancyPrefsPage *) page;

	GtkWidget *vbox;
#ifdef HAVE_LIBSOUP_GNOME    
	GtkWidget *gnome_proxy_checkbox;
#endif
	GtkWidget *proxy_checkbox;
	GtkWidget *proxy_str;
	GtkWidget *checkbox1;
	GtkWidget *checkbox2;
	GtkWidget *checkbox3;
	GtkWidget *checkbox4;
	GtkWidget *checkbox6;

	vbox = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), VBOX_BORDER);
	gtk_widget_show(vbox);
	    
	GtkWidget *block = gtk_vbox_new(FALSE, FALSE);
	proxy_checkbox = gtk_check_button_new_with_label(_("Proxy Setting"));
	proxy_str = gtk_entry_new();
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(proxy_checkbox),
								 fancy_prefs.enable_proxy);
	prefs_set_proxy_entry_sens(proxy_checkbox, GTK_ENTRY(proxy_str));
	g_signal_connect(G_OBJECT(proxy_checkbox), "toggled",
					 G_CALLBACK(prefs_set_proxy_entry_sens), proxy_str);
	pref_set_entry_from_pref(GTK_ENTRY(proxy_str), fancy_prefs.proxy_str);

	gtk_box_pack_start(GTK_BOX(block), proxy_checkbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(block), proxy_str, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), block, FALSE, FALSE, 0);
	gtk_widget_show(proxy_checkbox);
	gtk_widget_show(proxy_str);
	gtk_widget_show(block);
#ifdef HAVE_LIBSOUP_GNOME
	gnome_proxy_checkbox = gtk_check_button_new_with_label(_("Use GNOME proxy setting"));	
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(gnome_proxy_checkbox),
								 fancy_prefs.enable_gnome_proxy);
	if (fancy_prefs.enable_gnome_proxy) 
		gtk_widget_set_sensitive(proxy_checkbox, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), gnome_proxy_checkbox, FALSE, FALSE, 0);
	gtk_widget_show(gnome_proxy_checkbox);
	g_signal_connect(G_OBJECT(gnome_proxy_checkbox), "toggled",
					 G_CALLBACK(prefs_disable_fancy_proxy), block);
#endif
	checkbox1 = gtk_check_button_new_with_label(_("Auto-Load images"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox1),
								 fancy_prefs.auto_load_images);
	gtk_box_pack_start(GTK_BOX(vbox), checkbox1, FALSE, FALSE, 0);
	gtk_widget_show(checkbox1);

	checkbox2 = gtk_check_button_new_with_label(_("Enable inner navigation"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox2),
								 fancy_prefs.enable_inner_navigation);
	gtk_box_pack_start(GTK_BOX(vbox), checkbox2, FALSE, FALSE, 0);
	gtk_widget_show(checkbox2);
	
	checkbox3 = gtk_check_button_new_with_label(_("Enable Javascript"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox3),
								 fancy_prefs.enable_scripts);
	gtk_box_pack_start(GTK_BOX(vbox), checkbox3, FALSE, FALSE, 0);
	gtk_widget_show(checkbox3);

	checkbox4 = gtk_check_button_new_with_label(_("Enable Plugins"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox4),
								 fancy_prefs.enable_plugins);
	gtk_box_pack_start(GTK_BOX(vbox), checkbox4, FALSE, FALSE, 0);
	gtk_widget_show(checkbox4);
	checkbox6 = gtk_check_button_new_with_label(_("Enable Java"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox6),
								 fancy_prefs.enable_java);
	gtk_box_pack_start(GTK_BOX(vbox), checkbox6, FALSE, FALSE, 0);
	gtk_widget_show(checkbox6);

	GtkWidget *hbox_ext = gtk_hbox_new(FALSE, 8);
	GtkWidget *open_external_label = gtk_label_new(_("When clicking on a link, by default:"));
	GtkWidget *optmenu_open_external = gtkut_sc_combobox_create(NULL, FALSE);
	GtkListStore *menu = GTK_LIST_STORE(gtk_combo_box_get_model(
				GTK_COMBO_BOX(optmenu_open_external)));
	gtk_widget_show (optmenu_open_external);
	GtkTreeIter iter;

	COMBOBOX_ADD (menu, "DEFAULT_ACTION", FALSE);
	COMBOBOX_ADD (menu, _("Open in external browser"), TRUE);

	gtk_box_pack_start(GTK_BOX(hbox_ext), open_external_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_ext), optmenu_open_external, FALSE, FALSE, 0);
	gtk_widget_show_all(hbox_ext);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_ext, FALSE, FALSE, 0);

	combobox_select_by_data(GTK_COMBO_BOX(optmenu_open_external),
			fancy_prefs.open_external);

#ifdef HAVE_LIBSOUP_GNOME    
	prefs_page->gnome_proxy_checkbox = gnome_proxy_checkbox;
#endif
	prefs_page->proxy_checkbox = proxy_checkbox;
	prefs_page->proxy_str = proxy_str;
	prefs_page->auto_load_images = checkbox1;
	prefs_page->enable_inner_navigation = checkbox2;
	prefs_page->enable_scripts = checkbox3;
	prefs_page->enable_plugins = checkbox4;
	prefs_page->enable_java = checkbox6;
	prefs_page->open_external = optmenu_open_external;
	prefs_page->page.widget = vbox;

	g_signal_connect(G_OBJECT(prefs_page->enable_inner_navigation), "toggled",
					 G_CALLBACK(open_external_set_label_cb), prefs_page);
	open_external_set_label_cb(NULL, prefs_page);
}

static void prefs_set_proxy_entry_sens(GtkWidget *button, GtkEntry *entry_str) {
	gtk_widget_set_sensitive(GTK_WIDGET(entry_str), 
							   gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)));
}

#ifdef HAVE_LIBSOUP_GNOME    
static void prefs_disable_fancy_proxy(GtkWidget *checkbox, GtkWidget *block) {
	gboolean toggle = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checkbox));
	gtk_widget_set_sensitive(block, !toggle);
	GList *list = g_list_first(gtk_container_get_children(GTK_CONTAINER(block)));
	if (toggle) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(list->data), FALSE);
	}
	else {
		gtk_widget_set_sensitive(GTK_WIDGET(list->data), TRUE);
	}
}
#endif
static void destroy_fancy_prefs_page(PrefsPage *page)
{
	/* Do nothing! */
}
static void save_fancy_prefs(PrefsPage *page)
{
	PrefFile *pref_file;
	gchar *rc_file_path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
									  COMMON_RC, NULL);
	pref_file = prefs_write_open(rc_file_path);
	g_free(rc_file_path);
	if (!(pref_file) ||
	(prefs_set_block_label(pref_file, PREFS_BLOCK_NAME) < 0))
		return;
	
	if (prefs_write_param(param, pref_file->fp) < 0) {
		g_warning("failed to write Fancy Plugin configuration\n");
		prefs_file_close_revert(pref_file);
		return;
	}

	if (fprintf(pref_file->fp, "\n") < 0) {
	FILE_OP_ERROR(rc_file_path, "fprintf");
	prefs_file_close_revert(pref_file);
	} else
		prefs_file_close(pref_file);
}

static void save_fancy_prefs_page(PrefsPage *page)
{
		FancyPrefsPage *prefs_page = (FancyPrefsPage *) page;
	    
#ifdef HAVE_LIBSOUP_GNOME    
		fancy_prefs.enable_gnome_proxy = gtk_toggle_button_get_active
				(GTK_TOGGLE_BUTTON(prefs_page->gnome_proxy_checkbox));
#endif
		fancy_prefs.auto_load_images = gtk_toggle_button_get_active
				(GTK_TOGGLE_BUTTON(prefs_page->auto_load_images));
		fancy_prefs.enable_inner_navigation = gtk_toggle_button_get_active
				(GTK_TOGGLE_BUTTON(prefs_page->enable_inner_navigation));
		fancy_prefs.enable_scripts = gtk_toggle_button_get_active
				(GTK_TOGGLE_BUTTON(prefs_page->enable_scripts));
		fancy_prefs.enable_plugins = gtk_toggle_button_get_active
				(GTK_TOGGLE_BUTTON(prefs_page->enable_plugins));
		fancy_prefs.enable_java = gtk_toggle_button_get_active
				(GTK_TOGGLE_BUTTON(prefs_page->enable_java));
		fancy_prefs.open_external = combobox_get_active_data
				(GTK_COMBO_BOX(prefs_page->open_external));
		fancy_prefs.enable_proxy = gtk_toggle_button_get_active
				(GTK_TOGGLE_BUTTON(prefs_page->proxy_checkbox));
		fancy_prefs.proxy_str = pref_get_pref_from_entry(GTK_ENTRY(prefs_page->proxy_str));

		save_fancy_prefs(page);
}
