#ifndef __RSSYL_MENU_CB
#define __RSSYL_MENU_CB

#include <glib.h>
#include <gtk/gtk.h>

#include "folderview.h"

void rssyl_new_feed_cb(GtkAction *action, gpointer data);
void rssyl_rename_cb(GtkAction *action, gpointer data);
void rssyl_new_folder_cb(GtkAction *action, gpointer data);
void rssyl_remove_folder_cb(GtkAction *action, gpointer data);
void rssyl_refresh_feed_cb(GtkAction *action, gpointer data);
void rssyl_prop_cb(GtkAction *action, gpointer data);
void rssyl_update_all_cb(GtkAction *action, gpointer data);
void rssyl_remove_mailbox_cb(GtkAction *action, gpointer data);
void rssyl_import_feed_list_cb(GtkAction *action, gpointer data);

#endif /* __RSSYL_MENU_FEED */
