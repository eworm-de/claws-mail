#ifndef __FEED_H
#define __FEED_H

#include <libxml/parser.h>

#include "procmsg.h"

#include "rssyl.h"

#define RSSYL_TMP_TEMPLATE	"curltmpXXXXXX"

#define RSSYL_XPATH_ROOT		"/rssyl"
#define RSSYL_XPATH_TITLE		RSSYL_XPATH_ROOT"/title"
#define RSSYL_XPATH_LINK		RSSYL_XPATH_ROOT"/link"
#define RSSYL_XPATH_TEXT		RSSYL_XPATH_ROOT"/text"

#define RSSYL_TEXT_START		"<!-- RSSyl text start -->"
#define RSSYL_TEXT_END			"<!-- RSSyl text end -->"

#define RSSYL_LOG_ERROR_TIMEOUT		_("Time out connecting to URL %s\n")
#define RSSYL_LOG_ERROR_FETCH		_("Couldn't fetch URL %s\n")
#define RSSYL_LOG_ERROR_PARSE		_("Error parsing feed from URL %s\n")
#define RSSYL_LOG_ERROR_UNKNOWN		_("Unsupported feed type at URL %s\n")

#define RSSYL_LOG_UPDATING		_("RSSyl: Updating feed %s\n")
#define RSSYL_LOG_UPDATED 		_("RSSyl: Feed update finished: %s\n")
#define RSSYL_LOG_EXITING			_("RSSyl: Feed update aborted, application is exiting.\n")

struct _RSSylFeedItemMedia {
	gchar *url;
	gchar *type;
	gulong size;
};

typedef struct _RSSylFeedItemMedia RSSylFeedItemMedia;

struct _RSSylFeedItem {
	gchar *title;
	gchar *text;
	gchar *link;
	gchar *parent_link;
	gchar *comments_link;
	gchar *author;
	gchar *id;
	gboolean id_is_permalink;

	RSSylFeedItemMedia *media;

#ifdef RSSYL_DEBUG
	long int debug_fetched;
#endif	/* RSSYL_DEBUG */

	gchar *realpath;
	time_t date;
	time_t date_published;
};

typedef struct _RSSylFeedItem RSSylFeedItem;

struct _RSSylFindByUrlCtx {
	gchar *url;
	RSSylFolderItem *ritem;
};

typedef struct _RSSylFindByUrlCtx RSSylFindByUrlCtx;

struct _RSSylParseCtx {
	RSSylFolderItem *ritem;
	gboolean ready;
};

typedef struct _RSSylParseCtx RSSylParseCtx;

xmlDocPtr rssyl_fetch_feed(const gchar *url, time_t last_update, gchar **title, gchar **error);
void rssyl_parse_feed(xmlDocPtr doc, RSSylFolderItem *ritem, gchar *parent);
gboolean rssyl_add_feed_item(RSSylFolderItem *ritem, RSSylFeedItem *fitem);
MsgInfo *rssyl_parse_feed_item_to_msginfo(gchar *file, MsgFlags flags,
		gboolean a, gboolean b, FolderItem *item);
void rssyl_remove_feed_cache(FolderItem *item);
void rssyl_update_feed(RSSylFolderItem *ritem);
void rssyl_read_existing(RSSylFolderItem *ritem);

void rssyl_start_refresh_timeout(RSSylFolderItem *ritem);
void rssyl_expire_items(RSSylFolderItem *ritem);

FolderItem *rssyl_subscribe_new_feed(FolderItem *parent, const gchar *url, gboolean verbose);
void rssyl_free_feeditem(RSSylFeedItem *item);
gchar *rssyl_format_string(const gchar *str, gboolean replace_html, gboolean replace_returns);

void rssyl_refresh_all_func(FolderItem *item, gpointer data);
void rssyl_refresh_all_feeds(void);
gchar *rssyl_feed_title_to_dir(const gchar *title);

#endif /* __FEED_H */
