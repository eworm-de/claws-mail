#ifndef __FEEDPROPS_H
#define __FEEDPROPS_H

#include "rssyl.h"

#define RSSYL_PROPS_FILE	"feeds.xml"
#define RSSYL_PROPS_XPATH	"/feeds/feed"

#define RSSYL_PROP_URL					"url"
#define RSSYL_PROP_NAME					"name"
#define RSSYL_PROP_OFFICIAL_NAME			"official_name"
#define RSSYL_PROP_DEF_REFRESH	"default_refresh_interval"
#define RSSYL_PROP_REFRESH			"refresh_interval"
#define RSSYL_PROP_DEF_EXPIRED	"default_expired_num"
#define RSSYL_PROP_EXPIRED			"expired_num"
#define RSSYL_PROP_FETCH_COMMENTS		"fetch_comments"
#define RSSYL_PROP_FETCH_COMMENTS_FOR		"fetch_comments_for"
#define RSSYL_PROP_SILENT_UPDATE "silent_update"

void rssyl_store_feed_props(RSSylFolderItem *ritem);
void rssyl_get_feed_props(RSSylFolderItem *ritem);
void rssyl_remove_feed_props(RSSylFolderItem *ritem);
void rssyl_props_update_name(RSSylFolderItem *ritem, gchar *new_name);

#endif /* __FEEDPROPS_H */
