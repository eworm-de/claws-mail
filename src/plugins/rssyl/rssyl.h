#ifndef __RSSYL_H
#define __RSSYL_H

#include <glib.h>

#include <folder.h>

#include "libfeed/feed.h"

#define PLUGIN_NAME "RSSyl"

/* Name of directory in rcdir where RSSyl will store its data. */
#define RSSYL_DIR		PLUGIN_NAME

/* Folder name for a new feed, before it is parsed for the first time. */
#define RSSYL_NEW_FOLDER_NAME		"NewFeed"

/* Default RSSyl mailbox name */
#define RSSYL_DEFAULT_MAILBOX	_("My Feeds")

/* Default feed to be added when creating mailbox for the first time */
#define RSSYL_DEFAULT_FEED	"http://planet.claws-mail.org/rss20.xml"

/* File where info about user-deleted feed items is stored */
#define RSSYL_DELETED_FILE ".deleted"

struct _RFolderItem {
	FolderItem item;
	gchar *url;
	FeedAuth *auth;
	gchar *official_title;
	gchar *source_id;

	gboolean keep_old;

	gboolean default_refresh_interval;
	gint refresh_interval;

	gboolean fetch_comments;
	gint fetch_comments_max_age;

	gint silent_update;
	gboolean write_heading;
	gboolean ignore_title_rename;
	gboolean ssl_verify_peer;

	guint refresh_id;
	gboolean fetching_comments;
	time_t last_update;

	struct _RFeedProp *feedprop;

	GSList *items;
	GSList *deleted_items;
};

typedef struct _RFolderItem RFolderItem;

struct _RRefreshCtx {
	RFolderItem *ritem;
	guint id;
};

typedef struct _RRefreshCtx RRefreshCtx;

struct _RFetchCtx {
	Feed *feed;
	guint response_code;
	gchar *error;
	gboolean success;
	gboolean ready;
};

typedef struct _RFetchCtx RFetchCtx;

struct _RParseCtx {
	RFolderItem *ritem;
	gboolean ready;
};

typedef struct _RParseCtx RParseCtx;

struct _RDeletedItem {
	gchar *id;
	gchar *title;
	time_t date_published;
	time_t date_modified;
};

typedef struct _RDeletedItem RDeletedItem;

void rssyl_init(void);
void rssyl_done(void);

FolderClass *rssyl_folder_get_class(void);

FolderItem *rssyl_get_root_folderitem(FolderItem *item);

#define IS_RSSYL_FOLDER_ITEM(item) \
	(item->folder->klass == rssyl_folder_get_class())

#endif /* __RSSYL_H */
