#ifndef __RSSYL_GTK
#define __RSSYL_GTK

#include <gtk/gtk.h>

#include <folder.h>

#include "rssyl.h"

void rssyl_gtk_init(void);
void rssyl_gtk_done(void);

GtkWidget *rssyl_feed_removal_dialog(gchar *name, GtkWidget **rmcache_widget);

void rssyl_gtk_prop(RFolderItem *ritem);

#endif /* __RSSYL_GTK */
