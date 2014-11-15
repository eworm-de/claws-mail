#ifndef __RSSYL_FEED_PROPS
#define __RSSYL_FEED_PROPS

#include "rssyl.h"

void rssyl_gtk_prop(RFolderItem *ritem);

struct _RFeedProp {
	GtkWidget *window;
	GtkWidget *url;
	GtkWidget *default_refresh_interval;
	GtkWidget *refresh_interval;
	GtkWidget *keep_old;
	GtkWidget *fetch_comments;
	GtkWidget *fetch_comments_max_age;
	GtkWidget *silent_update;
	GtkWidget *write_heading;
	GtkWidget *ignore_title_rename;
	GtkWidget *ssl_verify_peer;
	GtkWidget *auth_type;
	GtkWidget *auth_username;
	GtkWidget *auth_password;
};

typedef struct _RFeedProp RFeedProp;

#endif /* __RSSYL_FEED_PROPS */
