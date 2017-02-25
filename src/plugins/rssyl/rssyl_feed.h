#ifndef __RSSYL_FEED_H
#define __RSSYL_FEED_H

#include <glib.h>

#include <folder.h>

#include "rssyl.h"

#define RSSYL_LOG_SUBSCRIBING  _("RSSyl: Subscribing new feed: %s\n")
#define RSSYL_LOG_SUBSCRIBED   _("RSSyl: New feed subscribed: '%s' (%s)\n")
#define RSSYL_LOG_UPDATING     _("RSSyl: Updating feed: %s\n")
#define RSSYL_LOG_UPDATED      _("RSSyl: Feed update finished: %s\n")
#define RSSYL_LOG_ERROR_FETCH  _("RSSyl: Error fetching feed at '%s': %s\n")
#define RSSYL_LOG_ERROR_NOFEED _("RSSyl: No valid feed found at '%s'\n")
#define RSSYL_LOG_ERROR_PROC   _("RSSyl: Couldn't process feed at '%s'\n")
#define RSSYL_LOG_ABORTED_EXITING _("RSSyl: Application is exiting, couldn't finish updating feed at '%s'\n")

typedef enum
{
	RSSYL_SHOW_ERRORS = 1 << 0,
	RSSYL_SHOW_RENAME_DIALOG = 1 << 1
} RSSylVerboseFlags;

MsgInfo *rssyl_feed_parse_item_to_msginfo(gchar *file, MsgFlags flags,
		gboolean a, gboolean b, FolderItem *item);

void rssyl_feed_start_refresh_timeout(RFolderItem *ritem);

#endif /* __RSSYL_FEED_H */
