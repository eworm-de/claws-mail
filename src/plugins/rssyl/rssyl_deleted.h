#ifndef __RSSYL_DELETED_H
#define __RSSYL_DELETED_H

#include <glib.h>

#include "rssyl.h"

void rssyl_deleted_update(RFolderItem *ritem);
void rssyl_deleted_free(RFolderItem *ritem);
void rssyl_deleted_add(RFolderItem *ritem, gchar *path);
void rssyl_deleted_store(RFolderItem *ritem);
gboolean rssyl_deleted_check(RFolderItem *ritem, FeedItem *fitem);
void rssyl_deleted_expire(RFolderItem *ritem, Feed *feed);

#endif /* __RSSYL_DELETED_H */
