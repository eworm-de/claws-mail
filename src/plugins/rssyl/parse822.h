#ifndef __RSSYL_PARSE822_H
#define __RSSYL_PARSE822_H

#include <glib.h>

#include "libfeed/feeditem.h"
#include "rssyl.h"

struct _RFeedCtx {
	gchar *path;
	time_t last_seen;
};

typedef struct _RFeedCtx RFeedCtx;

FeedItem *rssyl_parse_folder_item_file(gchar *path);
void rssyl_folder_read_existing(RFolderItem *ritem);

#endif /* __RSSYL_PARSE822_H */
