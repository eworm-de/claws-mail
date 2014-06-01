#ifndef __RSSYL_DELETED_H
#define __RSSYL_DELETED_H

#include <glib.h>

#include "rssyl.h"

GSList *rssyl_deleted_update(RFolderItem *ritem);
void *rssyl_deleted_free(GSList *deleted_items);
void rssyl_deleted_add(RFolderItem *ritem, gchar *path);
gboolean rssyl_deleted_check(GSList *deleted_items, FeedItem *fitem);
void rssyl_deleted_expire(RFolderItem *ritem, Feed *feed);

#endif /* __RSSYL_DELETED_H */
