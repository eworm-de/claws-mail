#ifndef __RSSYL_GTK
#define __RSSYL_GTK

#include <gtk/gtk.h>

#include <folder.h>

#include "rssyl.h"

struct _RSSylFeedProp {
	GtkWidget *window;
	GtkWidget *url;
	GtkWidget *default_refresh_interval;
	GtkWidget *refresh_interval;
	GtkWidget *default_expired_num;
	GtkWidget *expired_num;
	GtkWidget *fetch_comments;
	GtkWidget *fetch_comments_for;
	GtkWidget *silent_update;
};

typedef struct _RSSylFeedProp RSSylFeedProp;

void rssyl_gtk_init(void);
void rssyl_gtk_done(void);

void rssyl_gtk_prop(RSSylFolderItem *ritem);
void rssyl_gtk_prop_store(RSSylFolderItem *ritem);

GtkWidget *rssyl_feed_removal_dialog(gchar *name, GtkWidget **rmcache_widget);

#endif /* __RSSYL_GTK */
