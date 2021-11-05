#include "config.h"

#include <glib.h>

#include "date.h"


static void
test_createRFC822Date (void)
{
	gchar *buf;
	time_t t = 0;

	buf = createRFC822Date(&t);
	if (g_test_verbose())
		g_printerr("time_t %ld => '%s'\n", t, buf);
	g_assert_cmpstr(buf, ==, "Thu,  1 Jan 1970 00:00:00 GMT");
	g_free(buf);

	t = 1534240471;
	buf = createRFC822Date(&t);
	if (g_test_verbose())
		g_printerr("time_t %ld => '%s'\n", t, buf);
	g_assert_cmpstr(buf, ==, "Tue, 14 Aug 2018 09:54:31 GMT");
	g_free(buf);
}

int
main (int argc, char *argv[])
{
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/rssyl/libfeed/createRFC822Date", test_createRFC822Date);

	return g_test_run();
}
