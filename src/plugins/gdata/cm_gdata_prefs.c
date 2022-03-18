/*
 * GData plugin for Claws-Mail
 * Claws Mail -- A GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2011-2018 Holger Berndt and the Claws Mail team
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
 */


#ifdef HAVE_CONFIG_H
#  include "config.h"
#  include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include "password.h"
#include "cm_gdata_prefs.h"
#include "gdata_plugin.h"
#include "cm_gdata_contacts.h"

#include "prefs_gtk.h"
#include "main.h"

#include <gtk/gtk.h>


typedef struct
{
  PrefsPage page;
  GtkWidget *entry_username;
  GtkWidget *spin_max_num_results;
  GtkWidget *spin_max_cache_age;
} CmGDataPage;

CmGDataPrefs cm_gdata_config;
CmGDataPage gdata_page;

PrefParam cm_gdata_param[] =
{
    {"username", NULL, &cm_gdata_config.username, P_STRING,
        &gdata_page.entry_username, prefs_set_data_from_entry, prefs_set_entry},

    { "max_num_results", "1000", &cm_gdata_config.max_num_results, P_INT,
        &gdata_page.spin_max_num_results, prefs_set_data_from_spinbtn, prefs_set_spinbtn},

    { "max_cache_age", "300", &cm_gdata_config.max_cache_age, P_INT,
        &gdata_page.spin_max_cache_age, prefs_set_data_from_spinbtn, prefs_set_spinbtn},

    {"oauth2_refresh_token", NULL, &cm_gdata_config.oauth2_refresh_token, P_PASSWORD,
        NULL, NULL, NULL},

    {NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL }
};

static void gdata_create_prefs_page(PrefsPage *page, GtkWindow *window, gpointer data)
{
  GtkWidget *vbox;
  GtkWidget *frame;
  GtkWidget *spinner;
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *entry;

  vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

  /* auth frame */
  frame = gtk_frame_new(_("Authentication"));
  gtk_container_set_border_width(GTK_CONTAINER(frame), 5);
  gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

  /* username */
  table = gtk_grid_new();
  label = gtk_label_new(_("Username:"));
  gtk_label_set_xalign(GTK_LABEL(label), 0.0);
  gtk_grid_attach(GTK_GRID(table), label, 0, 0, 1, 1);
  entry = gtk_entry_new();
  gtk_widget_set_size_request(entry, 250, -1);
  gtk_grid_attach(GTK_GRID(table), entry, 1, 0, 1, 1);
  gtk_widget_set_hexpand(entry, TRUE);
  gtk_widget_set_halign(entry, GTK_ALIGN_FILL);
  gdata_page.entry_username = entry;
  gtk_container_add(GTK_CONTAINER(frame), table);

  table = gtk_grid_new();
  gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
  label = gtk_label_new(_("Polling interval (seconds):"));
  gtk_grid_attach(GTK_GRID(table), label, 0, 0, 1, 1);
  gtk_label_set_xalign(GTK_LABEL(label), 0.0);
  spinner = gtk_spin_button_new_with_range(10, 10000, 10);
  gtk_grid_attach(GTK_GRID(table), spinner, 1, 0, 1, 1);
  gdata_page.spin_max_cache_age = spinner;

  label = gtk_label_new(_("Maximum number of results:"));
  gtk_grid_attach(GTK_GRID(table), label, 0, 1, 1, 1);
  gtk_label_set_xalign(GTK_LABEL(label), 0.0);
  spinner = gtk_spin_button_new_with_range(0, G_MAXINT, 50);
  gtk_grid_attach(GTK_GRID(table), spinner, 1, 1, 1, 1);
  gdata_page.spin_max_num_results = spinner;

  gtk_widget_show_all(vbox);
  page->widget = vbox;

  prefs_set_dialog(cm_gdata_param);
}

static void gdata_destroy_prefs_page(PrefsPage *page)
{
}

static void gdata_save_prefs(PrefsPage *page)
{
  int old_max_cache_age = cm_gdata_config.max_cache_age;

  if (!page->page_open)
    return;

  prefs_set_data_from_dialog(cm_gdata_param);

  cm_gdata_update_contacts_cache();
  if(old_max_cache_age != cm_gdata_config.max_cache_age)
    cm_gdata_update_contacts_update_timer();
}

void cm_gdata_prefs_init(void)
{
  static gchar *path[3];

  path[0] = _("Plugins");
  path[1] = _("GData");
  path[2] = NULL;

  gdata_page.page.path = path;
  gdata_page.page.create_widget = gdata_create_prefs_page;
  gdata_page.page.destroy_widget = gdata_destroy_prefs_page;
  gdata_page.page.save_page = gdata_save_prefs;
  gdata_page.page.weight = 40.0;
  prefs_gtk_register_page((PrefsPage*) &gdata_page);
}

void cm_gdata_prefs_done(void)
{
  if(!claws_is_exiting()) {
    prefs_gtk_unregister_page((PrefsPage*) &gdata_page);
  }
}
