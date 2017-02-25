#ifndef __RSSYL_SUBSCRIBE_H
#define __RSSYL_SUBSCRIBE_H

#include <rssyl_feed.h>

FolderItem *rssyl_subscribe(FolderItem *parent, const gchar *url,
		RSSylVerboseFlags verbose);

#endif /* __RSSYL_SUBSCRIBE_H */
