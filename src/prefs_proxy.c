/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2018-2019 the Claws Mail team
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

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "common/defs.h"
#include "common/proxy.h"

#include "gtk/menu.h"

#include "prefs_common.h"
#include "prefs_gtk.h"
#include "passwordstore.h"

typedef struct _ProxyPage
{
	PrefsPage page;

	GtkWidget *proxy_checkbtn;
	GtkWidget *socks4_radiobtn;
	GtkWidget *socks5_radiobtn;
	GtkWidget *proxy_host_entry;
	GtkWidget *proxy_port_spinbtn;
	GtkWidget *proxy_auth_checkbtn;
	GtkWidget *proxy_name_entry;
	GtkWidget *proxy_pass_entry;
} ProxyPage;

static void showpwd_checkbtn_toggled(GtkToggleButton *button,
		gpointer user_data);

static void prefs_proxy_create_widget(PrefsPage *_page, GtkWindow *window,
		gpointer data)
{
	ProxyPage *page = (ProxyPage *)_page;

	GtkWidget *vbox0, *vbox1, *vbox2;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *proxy_checkbtn;
	GtkWidget *socks4_radiobtn, *socks5_radiobtn;
	GtkWidget *proxy_auth_checkbtn;
	GtkWidget *proxy_frame;
	GtkWidget *proxy_host_entry;
	GtkWidget *proxy_port_spinbtn;
	GtkWidget *proxy_name_entry;
	GtkWidget *proxy_pass_entry;
	GtkWidget *button;
	GtkWidget *table;
	gchar *buf;

	vbox0 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING);
	gtk_container_set_border_width(GTK_CONTAINER(vbox0), VBOX_BORDER);

	proxy_checkbtn = gtk_check_button_new_with_label(_("Use proxy server"));
	PACK_FRAME(vbox0, proxy_frame, NULL);
	gtk_frame_set_label_widget(GTK_FRAME(proxy_frame), proxy_checkbtn);

	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING_NARROW);
	gtk_container_set_border_width(GTK_CONTAINER(vbox1), 8);
	gtk_container_add(GTK_CONTAINER(proxy_frame), vbox1);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, FALSE, 0);

	socks4_radiobtn = gtk_radio_button_new_with_label(NULL, "SOCKS4");
	gtk_box_pack_start(GTK_BOX(hbox), socks4_radiobtn, FALSE, FALSE, 0);
	g_object_set_data(G_OBJECT(socks4_radiobtn), MENU_VAL_ID,
			GINT_TO_POINTER(PROXY_SOCKS4));

	socks5_radiobtn = gtk_radio_button_new_with_label_from_widget(
			GTK_RADIO_BUTTON(socks4_radiobtn), "SOCKS5");
	gtk_box_pack_start(GTK_BOX(hbox), socks5_radiobtn, FALSE, FALSE, 0);
	g_object_set_data(G_OBJECT(socks5_radiobtn), MENU_VAL_ID,
			GINT_TO_POINTER(PROXY_SOCKS5));

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(_("Hostname"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	proxy_host_entry = gtk_entry_new();
	gtk_widget_set_size_request(proxy_host_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_box_pack_start(GTK_BOX(hbox), proxy_host_entry, TRUE, TRUE, 0);

	label = gtk_label_new(_("Port"));
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

	proxy_port_spinbtn = gtk_spin_button_new_with_range(0, 65535, 1080);
	gtk_widget_set_size_request(proxy_port_spinbtn, 64, -1);
	gtk_box_pack_start(GTK_BOX(hbox), proxy_port_spinbtn, FALSE, FALSE, 0);

	vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, VSPACING_NARROW);
	gtk_box_pack_start(GTK_BOX(vbox1), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON(vbox2, proxy_auth_checkbtn, _("Use authentication"));

	table = gtk_grid_new();

	gtk_grid_set_row_spacing(GTK_GRID(table), VSPACING_NARROW);
	gtk_grid_set_column_spacing(GTK_GRID(table), 9);
	gtk_box_pack_start(GTK_BOX(vbox2), table, FALSE, FALSE, 0);

	label = gtk_label_new(_("Username"));
	gtk_grid_attach(GTK_GRID(table), label, 0, 0, 1, 1);

	proxy_name_entry = gtk_entry_new();
	gtk_widget_set_size_request(proxy_name_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_grid_attach(GTK_GRID(table), proxy_name_entry, 1, 0, 1, 1);
	gtk_widget_set_hexpand(proxy_name_entry, TRUE);
	gtk_widget_set_halign(proxy_name_entry, GTK_ALIGN_FILL);

	label = gtk_label_new(_("Password"));
	gtk_grid_attach(GTK_GRID(table), label, 2, 0, 1, 1);

	proxy_pass_entry = gtk_entry_new();
	gtk_widget_set_size_request(proxy_pass_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_entry_set_visibility(GTK_ENTRY(proxy_pass_entry), FALSE);
	gtk_grid_attach(GTK_GRID(table), proxy_pass_entry, 3, 0, 1, 1);
	gtk_widget_set_hexpand(proxy_pass_entry, TRUE);
	gtk_widget_set_halign(proxy_pass_entry, GTK_ALIGN_FILL);

	button = gtk_check_button_new_with_label(_("Show password"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), FALSE);
	gtk_grid_attach(GTK_GRID(table), button, 3, 1, 1, 1);
	g_signal_connect(G_OBJECT(button), "toggled",
			G_CALLBACK(showpwd_checkbtn_toggled), proxy_pass_entry);

	gtk_widget_show_all(vbox0);

	SET_TOGGLE_SENSITIVITY(proxy_checkbtn, vbox1);
	SET_TOGGLE_SENSITIVITY(proxy_auth_checkbtn, table);
	SET_TOGGLE_SENSITIVITY(socks5_radiobtn, vbox2);

	/* Set widgets to their correct states, based on prefs. */
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(proxy_checkbtn),
			prefs_common.use_proxy);
	if (prefs_common.proxy_info.proxy_type == PROXY_SOCKS4)
		button = socks4_radiobtn;
	else
		button = socks5_radiobtn;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
	gtk_entry_set_text(GTK_ENTRY(proxy_host_entry),
			prefs_common.proxy_info.proxy_host);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(proxy_port_spinbtn),
			(gdouble)prefs_common.proxy_info.proxy_port);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(proxy_auth_checkbtn),
			prefs_common.proxy_info.use_proxy_auth);
	gtk_entry_set_text(GTK_ENTRY(proxy_name_entry),
			prefs_common.proxy_info.proxy_name);

	buf =
		passwd_store_get(PWS_CORE, PWS_CORE_PROXY, PWS_CORE_PROXY_PASS);
	gtk_entry_set_text(GTK_ENTRY(proxy_pass_entry), buf != NULL ? buf : "");
	if (buf != NULL) {
		memset(buf, 0, strlen(buf));
		g_free(buf);
	}

	page->proxy_checkbtn = proxy_checkbtn;
	page->socks4_radiobtn = socks4_radiobtn;
	page->socks5_radiobtn = socks5_radiobtn;
	page->proxy_host_entry = proxy_host_entry;
	page->proxy_port_spinbtn = proxy_port_spinbtn;
	page->proxy_auth_checkbtn = proxy_auth_checkbtn;
	page->proxy_name_entry = proxy_name_entry;
	page->proxy_pass_entry = proxy_pass_entry;
	page->page.widget = vbox0;
}


static void prefs_proxy_destroy_widget(PrefsPage *_page)
{
}


static void prefs_proxy_save(PrefsPage *_page)
{
	ProxyPage *page = (ProxyPage *)_page;

	prefs_common.use_proxy = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->proxy_checkbtn));

	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->socks4_radiobtn)))
		prefs_common.proxy_info.proxy_type = PROXY_SOCKS4;
	else
		prefs_common.proxy_info.proxy_type = PROXY_SOCKS5;

	g_free(prefs_common.proxy_info.proxy_host);
	prefs_common.proxy_info.proxy_host = g_strdup(gtk_entry_get_text(
				GTK_ENTRY(page->proxy_host_entry)));

	prefs_common.proxy_info.proxy_port = gtk_spin_button_get_value_as_int(
			GTK_SPIN_BUTTON(page->proxy_port_spinbtn));

	prefs_common.proxy_info.use_proxy_auth = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->proxy_auth_checkbtn));

	g_free(prefs_common.proxy_info.proxy_name);
	prefs_common.proxy_info.proxy_name = g_strdup(gtk_entry_get_text(
				GTK_ENTRY(page->proxy_name_entry)));

	passwd_store_set(PWS_CORE, PWS_CORE_PROXY, PWS_CORE_PROXY_PASS,
			gtk_entry_get_text(GTK_ENTRY(page->proxy_pass_entry)), FALSE);
}


ProxyPage *prefs_proxy;

void prefs_proxy_init(void)
{
	ProxyPage *page;
	static gchar *path[3];

	path[0] = _("Mail Handling");
	path[1] = _("Proxy");
	path[2] = NULL;

	page = g_new0(ProxyPage, 1);
	page->page.path = path;
	page->page.create_widget = prefs_proxy_create_widget;
	page->page.destroy_widget = prefs_proxy_destroy_widget;
	page->page.save_page = prefs_proxy_save;
	page->page.weight = 190.0;

	prefs_gtk_register_page((PrefsPage *)page);
	prefs_proxy = page;
}

void prefs_proxy_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *)prefs_proxy);
	g_free(prefs_proxy);
}

static void showpwd_checkbtn_toggled(GtkToggleButton *button,
    gpointer user_data)
{
	gboolean active = gtk_toggle_button_get_active(button);
	GtkWidget *entry = GTK_WIDGET(user_data);

	gtk_entry_set_visibility(GTK_ENTRY(entry), active);
}
