#include "config.h"

#include <glib.h>

#include "feed.h"

#include "mock_procheader_date_parse.h"

#define FEED_URL "http://example.com/feed.xml"

static void
test_Feed_create(void)
{
	Feed *feed = feed_new(FEED_URL);

	g_assert_nonnull(feed);
	g_assert_cmpstr(feed->url, ==, FEED_URL);
	g_assert_true(feed->is_valid);
	g_assert_true(feed->ssl_verify_peer);

	feed_free(feed);
}

int
main(int argc, char *argv[])
{
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/rssyl/libfeed/Feed_create", test_Feed_create);

	return g_test_run();
}
