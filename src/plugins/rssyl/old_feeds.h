#ifndef __RSSYL_OLD_FEEDS
#define __RSSYL_OLD_FEEDS

struct _OldRFeed {
	gchar *name;
	gchar *official_name;
	gchar *url;
	gint default_refresh_interval;
	gint refresh_interval;
	gint expired_num;
	gint fetch_comments;
	gint fetch_comments_for;
	gint silent_update;
	gint ssl_verify_peer;
};

typedef struct _OldRFeed OldRFeed;

GSList *rssyl_old_feed_metadata_parse(gchar *filepath);
void rssyl_old_feed_metadata_free(GSList *oldfeeds);
OldRFeed *rssyl_old_feed_get_by_name(GSList *oldfeeds, gchar *name);

#endif /* __RSSYL_OLD_FEEDS */
