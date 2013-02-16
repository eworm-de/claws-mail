#ifndef __RSSYL_GTK_CB
#define __RSSYL_GTK_CB

#include <glib.h>
#include <gtk/gtk.h>

gboolean rssyl_props_cancel_cb(GtkWidget *widget, gpointer data);
gboolean rssyl_props_ok_cb(GtkWidget *widget, gpointer data);
gboolean rssyl_refresh_timeout_cb(gpointer data);
gboolean rssyl_default_refresh_interval_toggled_cb(GtkToggleButton *default_ri,
		gpointer data);
gboolean rssyl_default_expired_num_toggled_cb(GtkToggleButton *default_ex,
		gpointer data);
gboolean rssyl_fetch_comments_toggled_cb(GtkToggleButton *fetch_comments,
		gpointer data);
gboolean rssyl_props_key_press_cb(GtkWidget *widget, GdkEventKey *event,
		gpointer data);

#endif /* __RSSYL_GTK_CB */
