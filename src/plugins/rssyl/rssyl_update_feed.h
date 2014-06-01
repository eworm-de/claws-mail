#ifndef __RSSYL_UPDATE_FEED
#define __RSSYL_UPDATE_FEED

#include <glib.h>

#include "rssyl.h"

void rssyl_fetch_feed(RFetchCtx *ctx, gboolean verbose);

RFetchCtx *rssyl_prep_fetchctx_from_url(gchar *url);
RFetchCtx *rssyl_prep_fetchctx_from_item(RFolderItem *ritem);

gboolean rssyl_update_feed(RFolderItem *ritem, gboolean verbose);

void rssyl_update_recursively(FolderItem *item);

void rssyl_update_all_feeds(void);

#endif /* __RSSYL_UPDATE_FEED */
