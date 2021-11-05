#include "config.h"

#include "strutils.h"

/* It's safe to mock this out here, we are interested in
 * rssyl_strreplace(), which doesn't use the real function. */
gchar *entity_decode(gchar *str)
{
	return g_strdup("mocked_entity_decode");
}

struct test {
	gchar *string;
	gchar *pattern;
	gchar *replacement;
	gchar *result;
};

static void
test_strreplace(void)
{
	gint i;
	static struct test strings[] = {
		{ "simplestring", "foo", "bar", "simplestring" },
		{ "foobarzot", "foo", "", "barzot" },
		{ NULL, NULL }
	};

	for (i = 0; strings[i].string != NULL; i++) {
		gchar *result = rssyl_strreplace(
				strings[i].string,
				strings[i].pattern,
				strings[i].replacement);

		if (g_test_verbose()) {
			g_printerr("string '%s', pattern '%s', replacement '%s' => result '%s'\n",
					strings[i].string,
					strings[i].pattern,
					strings[i].replacement,
					result);
		}
		g_assert_cmpstr(result, ==, strings[i].result);
		g_free(result);
	}
}

int
main (int argc, char *argv[])
{
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/rssyl/strreplace", test_strreplace);

	return g_test_run();
}
