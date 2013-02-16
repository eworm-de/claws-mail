#ifndef __RSSYL_H
#define __RSSYL_H

#include <glib.h>

#include <folder.h>

#define PLUGIN_NAME		(_("RSSyl"))

/* Name of directory in rcdir where RSSyl will store its data. */
#define RSSYL_DIR "RSSyl"

/* Default RSSyl mailbox name */
#define RSSYL_DEFAULT_MAILBOX	_("My Feeds")

/* Default feed to be added when creating mailbox for the first time */
#define RSSYL_DEFAULT_FEED	"http://planet.claws-mail.org/rss20.xml"

struct _RSSylFolderItem {
	FolderItem item;
	GSList *contents;
	gint last_count;

	gchar *url;
	gchar *official_name;

	gboolean default_refresh_interval;
	gint refresh_interval;

	gboolean default_expired_num;
	gint expired_num;

	guint refresh_id;	
	gboolean fetch_comments;
	gint fetch_comments_for;
	gint silent_update;

	struct _RSSylFeedProp *feedprop;
};

typedef struct _RSSylFolderItem RSSylFolderItem;

struct _RSSylRefreshCtx {
	RSSylFolderItem *ritem;
	guint id;
};

typedef struct _RSSylRefreshCtx RSSylRefreshCtx;

void rssyl_init(void);
void rssyl_done(void);

FolderClass *rssyl_folder_get_class(void);

#define IS_RSSYL_FOLDER_ITEM(item) \
	(item->folder->klass == rssyl_folder_get_class())

#endif /* __RSSYL_H */
