/*
 * == Fancy Plugin ==
 * Claws Mail -- A GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2019 Salvatore De Paolis and the Claws Mail Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include "common/version.h"
#include "defs.h"
#include "claws.h"
#include "plugin.h"
#include "gtkutils.h"
#include "file-utils.h"
#include "utils.h"
#include "prefs.h"
#include "prefs_common.h"
#include "prefs_gtk.h"
#include "prefswindow.h"
#include "combobox.h"
#include "addressbook.h"
#include "filesel.h"

#include "fancy_prefs.h"

#define PREFS_BLOCK_NAME "fancy"

FancyPrefs fancy_prefs;

// static void prefs_set_proxy_entry_sens(GtkWidget *button, GtkEntry *entry_str);

typedef struct _FancyPrefsPage FancyPrefsPage;

struct _FancyPrefsPage {
	PrefsPage page;
	GtkWidget *enable_images;
	GtkWidget *enable_remote_content;
	GtkWidget *enable_scripts;
	GtkWidget *enable_plugins;
	GtkWidget *enable_java;
	GtkWidget *open_external;
/*	GtkWidget *proxy_checkbox;
	GtkWidget *proxy_str; */
	GtkWidget *stylesheet;
};

static PrefParam param[] = {
		{"enable_images", "TRUE", &fancy_prefs.enable_images, P_BOOL,
		NULL, NULL, NULL},
		{"enable_remote_content", "FALSE", &fancy_prefs.enable_remote_content, P_BOOL,
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
/*		{"enable_proxy", "FALSE", &fancy_prefs.enable_proxy, P_BOOL,
		NULL, NULL, NULL},
		{"proxy_server", "http://SERVERNAME:PORT", &fancy_prefs.proxy_str, P_STRING,
		NULL, NULL, NULL}, */
		{"stylesheet", "", &fancy_prefs.stylesheet, P_STRING, NULL, NULL, NULL},
		{0,0,0,0,0,0,0}
};

static FancyPrefsPage fancy_prefs_page;

static void fancy_prefs_stylesheet_browse_cb	(GtkWidget *widget, gpointer data);
static void fancy_prefs_stylesheet_edit_cb	(GtkWidget *widget, gpointer data);
static void fancy_prefs_stylesheet_changed_cb	(GtkWidget *widget, gpointer data);

static void create_fancy_prefs_page     (PrefsPage *page, GtkWindow *window, gpointer   data);
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

static void remote_content_set_labels_cb(GtkWidget *button, FancyPrefsPage *prefs_page)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean remote_enabled = gtk_toggle_button_get_active(
					GTK_TOGGLE_BUTTON(prefs_page->enable_remote_content));

	/* Enable images */
	gtk_button_set_label(GTK_BUTTON(prefs_page->enable_images),
			     remote_enabled ? _("Display images")
					    : _("Display embedded images"));

	/* Enable Javascript */
	gtk_button_set_label(GTK_BUTTON(prefs_page->enable_scripts),
			     remote_enabled ? _("Execute javascript")
					    : _("Execute embedded javascript"));

	/* Enable java */
	gtk_button_set_label(GTK_BUTTON(prefs_page->enable_java),
			     remote_enabled ? _("Execute Java applets")
					    : _("Execute embedded Java applets"));

	/* Enable plugins */
	gtk_button_set_label(GTK_BUTTON(prefs_page->enable_plugins),
			     remote_enabled ? _("Render objects using plugins")
					    : _("Render embedded objects using plugins"));

	/* Open links */
	model = gtk_combo_box_get_model(GTK_COMBO_BOX(prefs_page->open_external));
	if (gtk_tree_model_get_iter_first (model, &iter)) {
		if (remote_enabled)
			gtk_list_store_set(GTK_LIST_STORE(model), &iter, COMBOBOX_TEXT,
					   _("Open in Viewer (remote content is enabled)"), -1);
		else
			gtk_list_store_set(GTK_LIST_STORE(model), &iter, COMBOBOX_TEXT,
					   _("Do nothing (remote content is disabled)"), -1);
	}

}
static void create_fancy_prefs_page(PrefsPage *page, GtkWindow *window,
									gpointer data)
{
	FancyPrefsPage *prefs_page = (FancyPrefsPage *) page;

	GtkWidget *vbox;
/*	GtkWidget *proxy_checkbox;
	GtkWidget *proxy_str;
	GtkWidget *vbox_proxy;
	GtkWidget *frame_proxy;
*/
	GtkWidget *frame_remote;
	GtkWidget *vbox_remote;
	GtkWidget *remote_label;
	GtkWidget *enable_remote_content;
	GtkWidget *enable_images;
	GtkWidget *enable_scripts;
	GtkWidget *enable_plugins;
	GtkWidget *enable_java;
	GtkWidget *stylesheet_label;
	GtkWidget *stylesheet;
	GtkWidget *stylesheet_browse_button;
	GtkWidget *stylesheet_edit_button;

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), VBOX_BORDER);
	gtk_widget_show(vbox);
/*
	GtkWidget *block = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

	vbox_proxy = gtkut_get_options_frame(vbox, &frame_proxy, _("Proxy"));
	proxy_checkbox = gtk_check_button_new_with_label(_("Use proxy"));
	proxy_str = gtk_entry_new();
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(proxy_checkbox),
				     fancy_prefs.enable_proxy);
	prefs_set_proxy_entry_sens(proxy_checkbox, GTK_ENTRY(proxy_str));
	g_signal_connect(G_OBJECT(proxy_checkbox), "toggled",
			 G_CALLBACK(prefs_set_proxy_entry_sens), proxy_str);
	pref_set_entry_from_pref(GTK_ENTRY(proxy_str), fancy_prefs.proxy_str);

	gtk_box_pack_start(GTK_BOX(block), proxy_checkbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(block), proxy_str, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox_proxy), block, FALSE, FALSE, 0);
	gtk_widget_show_all(vbox_proxy);
*/
	vbox_remote = gtkut_get_options_frame(vbox, &frame_remote, _("Remote resources"));
	remote_label = gtk_label_new(_("Loading remote resources can lead to some privacy issues.\n"
					"When remote content loading is disabled, nothing will be requested\n"
					"from the network. Rendering of images, scripts, plugin objects or\n"
					"Java applets can still be enabled for content that is attached\n"
					"in the email."));
	gtk_label_set_xalign(GTK_LABEL(remote_label), 0.0);
	gtk_label_set_yalign(GTK_LABEL(remote_label), 0.0);
	enable_remote_content = gtk_check_button_new_with_label(_("Enable loading of remote content"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_remote_content),
				     fancy_prefs.enable_remote_content);
	gtk_box_pack_start (GTK_BOX (vbox_remote), remote_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox_remote), enable_remote_content, FALSE, FALSE, 0);
	gtk_widget_show_all(vbox_remote);
	
	enable_images = gtk_check_button_new_with_label(("IMAGES"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_images),
				     fancy_prefs.enable_images);
	gtk_box_pack_start(GTK_BOX(vbox), enable_images, FALSE, FALSE, 0);
	gtk_widget_show(enable_images);

	enable_scripts = gtk_check_button_new_with_label("SCRIPTS");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_scripts),
				     fancy_prefs.enable_scripts);
	gtk_box_pack_start(GTK_BOX(vbox), enable_scripts, FALSE, FALSE, 0);
	gtk_widget_show(enable_scripts);

	enable_java = gtk_check_button_new_with_label("JAVA");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_java),
				     fancy_prefs.enable_java);
	gtk_box_pack_start(GTK_BOX(vbox), enable_java, FALSE, FALSE, 0);
	gtk_widget_show(enable_java);

	enable_plugins = gtk_check_button_new_with_label("PLUGINS");
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_plugins),
				     fancy_prefs.enable_plugins);
	gtk_box_pack_start(GTK_BOX(vbox), enable_plugins, FALSE, FALSE, 0);
	gtk_widget_show(enable_plugins);

	GtkWidget *hbox_ext = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	GtkWidget *open_external_label = gtk_label_new(_("When clicking on a link, by default"));
	GtkWidget *optmenu_open_external = gtkut_sc_combobox_create(NULL, FALSE);
	GtkListStore *menu = GTK_LIST_STORE(gtk_combo_box_get_model(
				GTK_COMBO_BOX(optmenu_open_external)));
	gtk_widget_show (optmenu_open_external);
	GtkTreeIter iter;

	COMBOBOX_ADD (menu, "DEFAULT_ACTION", FALSE);
	COMBOBOX_ADD (menu, _("Open in External Browser"), TRUE);

	gtk_box_pack_start(GTK_BOX(hbox_ext), open_external_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_ext), optmenu_open_external, FALSE, FALSE, 0);
	gtk_widget_show_all(hbox_ext);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_ext, FALSE, FALSE, 0);

	combobox_select_by_data(GTK_COMBO_BOX(optmenu_open_external),
			fancy_prefs.open_external);
	
	GtkWidget *hbox_css = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show(hbox_css);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_css, FALSE, FALSE, 0);

	CLAWS_SET_TIP(hbox_css, _("The CSS in this file will be applied to all HTML parts"));

	stylesheet_label = gtk_label_new(_("Stylesheet"));
	gtk_widget_show(stylesheet_label);
	gtk_box_pack_start(GTK_BOX(hbox_css), stylesheet_label, FALSE, FALSE, 0);
	
	stylesheet = gtk_entry_new();
	gtk_widget_show(stylesheet);
	gtk_box_pack_start(GTK_BOX(hbox_css), stylesheet, TRUE, TRUE, 0);

	stylesheet_browse_button = gtkut_get_browse_file_btn(_("Bro_wse"));
	gtk_widget_show(stylesheet_browse_button);
	gtk_box_pack_start(GTK_BOX(hbox_css), stylesheet_browse_button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(stylesheet_browse_button), "clicked",
			 G_CALLBACK(fancy_prefs_stylesheet_browse_cb), stylesheet);

	stylesheet_edit_button = gtk_button_new_with_mnemonic("_Edit");
	gtk_widget_show (stylesheet_edit_button);
	gtk_box_pack_start(GTK_BOX(hbox_css), stylesheet_edit_button, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(stylesheet_edit_button), "clicked",
			 G_CALLBACK(fancy_prefs_stylesheet_edit_cb), stylesheet);
	g_signal_connect(G_OBJECT(stylesheet), "changed",
			 G_CALLBACK(fancy_prefs_stylesheet_changed_cb), stylesheet_edit_button);
	pref_set_entry_from_pref(GTK_ENTRY(stylesheet), fancy_prefs.stylesheet);
	g_signal_emit_by_name(G_OBJECT(stylesheet), "changed", stylesheet_edit_button);

/*	prefs_page->proxy_checkbox = proxy_checkbox;
	prefs_page->proxy_str = proxy_str; */
	prefs_page->enable_remote_content = enable_remote_content;
	prefs_page->enable_images = enable_images;
	prefs_page->enable_scripts = enable_scripts;
	prefs_page->enable_plugins = enable_plugins;
	prefs_page->enable_java = enable_java;
	prefs_page->open_external = optmenu_open_external;
	prefs_page->stylesheet = stylesheet;
	prefs_page->page.widget = vbox;

	g_signal_connect(G_OBJECT(prefs_page->enable_remote_content), "toggled",
			 G_CALLBACK(remote_content_set_labels_cb), prefs_page);
	remote_content_set_labels_cb(NULL, prefs_page);
}

static void fancy_prefs_stylesheet_browse_cb(GtkWidget *widget, gpointer data)
{
	gchar *filename;
	gchar *utf8_filename;
	GtkEntry *dest = GTK_ENTRY(data);

	filename = filesel_select_file_open(_("Select stylesheet"), NULL);
	if (!filename) return;

	utf8_filename = g_filename_to_utf8(filename, -1, NULL, NULL, NULL);
	if (!utf8_filename) {
		g_warning("fancy_prefs_stylesheet_browse_cb(): failed to convert character set");
		utf8_filename = g_strdup(filename);
	}
	gtk_entry_set_text(GTK_ENTRY(dest), utf8_filename);
	g_free(utf8_filename);
}

static void fancy_prefs_stylesheet_edit_cb(GtkWidget *widget, gpointer data)
{
	const gchar *stylesheet = gtk_entry_get_text(GTK_ENTRY(data));
	if (!is_file_exist(stylesheet))
		str_write_to_file(stylesheet, "", TRUE);
	open_txt_editor(stylesheet, prefs_common_get_ext_editor_cmd());
}

static void fancy_prefs_stylesheet_changed_cb(GtkWidget *widget, gpointer data)
{
	const gchar *stylesheet = gtk_entry_get_text(GTK_ENTRY(widget));
	gtk_widget_set_sensitive(GTK_WIDGET(data), (*stylesheet)? TRUE: FALSE);
}
/*
static void prefs_set_proxy_entry_sens(GtkWidget *button, GtkEntry *entry_str) {
	gtk_widget_set_sensitive(GTK_WIDGET(entry_str),
				 gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)));
}
*/
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
		g_warning("failed to write Fancy plugin configuration");
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
	
		fancy_prefs.enable_images = gtk_toggle_button_get_active
				(GTK_TOGGLE_BUTTON(prefs_page->enable_images));
		fancy_prefs.enable_remote_content = gtk_toggle_button_get_active
				(GTK_TOGGLE_BUTTON(prefs_page->enable_remote_content));
		fancy_prefs.enable_scripts = gtk_toggle_button_get_active
				(GTK_TOGGLE_BUTTON(prefs_page->enable_scripts));
		fancy_prefs.enable_plugins = gtk_toggle_button_get_active
				(GTK_TOGGLE_BUTTON(prefs_page->enable_plugins));
		fancy_prefs.enable_java = gtk_toggle_button_get_active
				(GTK_TOGGLE_BUTTON(prefs_page->enable_java));
		fancy_prefs.open_external = combobox_get_active_data
				(GTK_COMBO_BOX(prefs_page->open_external));
/*		fancy_prefs.enable_proxy = gtk_toggle_button_get_active
				(GTK_TOGGLE_BUTTON(prefs_page->proxy_checkbox));
		fancy_prefs.proxy_str = pref_get_pref_from_entry(GTK_ENTRY(prefs_page->proxy_str)); */
#ifdef G_OS_WIN32
		/* pref_get_pref_from_entry() escapes the backslashes in strings,
		 * we do not want that, since this entry contains a Windows path.
		 * Let's just strdup it. */
		fancy_prefs.stylesheet = g_strdup(gtk_entry_get_text(
					GTK_ENTRY(prefs_page->stylesheet)));
#else
		fancy_prefs.stylesheet = pref_get_pref_from_entry(GTK_ENTRY(prefs_page->stylesheet));
#endif

		save_fancy_prefs(page);
}
