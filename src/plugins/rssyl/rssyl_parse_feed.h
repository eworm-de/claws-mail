#ifndef __RSSYL_PARSE_FEED
#define __RSSYL_PARSE_FEED

#include <glib.h>

#include "rssyl.h"
#include "libfeed/feed.h"

#define RSSYL_TMP_TEMPLATE	"rssylXXXXXX"

#define RSSYL_TEXT_START    "<!-- RSSyl text start -->"
#define RSSYL_TEXT_END      "<!-- RSSyl text end -->"

gboolean rssyl_parse_feed(RFolderItem *ritem, Feed *feed);

#endif /* __RSSYL_PARSE_FEED */
