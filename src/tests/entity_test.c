#include "config.h"

#include <glib.h>
#include <stdio.h>

#include "mock_debug_print.h"

#include "entity.h"

static void
test_entity_invalid(void)
{
	gchar *result;

	/* Invalid entity strings */
	result = entity_decode(NULL);
	g_assert_null(result);
	result = entity_decode("");
	g_assert_null(result);
	result = entity_decode("foo");
	g_assert_null(result);
	result = entity_decode("&");
	g_assert_null(result);
	result = entity_decode("&;");
	g_assert_null(result);
	result = entity_decode("&#");
	g_assert_null(result);
	result = entity_decode("&#;");
	g_assert_null(result);

	/* Valid entity string, but with missing semicolon */
	result = entity_decode("&Aacute");
	g_assert_null(result);
	result = entity_decode("&#123");
	g_assert_null(result);
}

static void
test_entity_toolong(void)
{
	gchar *result;

	/* Largest unicode code point is 0x10ffff, let's test that,
	 * and one past it */
	result = entity_decode("&#1114111;");
	g_assert_nonnull(result);
	g_free(result);
	result = entity_decode("&#1114112;");
	g_assert_null(result);

	/* ENTITY_MAX_LEN is 8, test 8- and 9-char entity strings
	 * for possible buffer overflows */
	result = entity_decode("&#88888888;");
	g_assert_null(result);
	result = entity_decode("&#999999999;");
	g_assert_null(result);
}

static void
test_entity_unprintable(void)
{
	gchar *result, numstr[6]; /* "&#XX;" */
	gint i;

	for (i = 0; i < 32; i++) {
		sprintf(numstr, "&#%d;", i);
		result = entity_decode(numstr);
		g_assert_nonnull(result);
		g_assert_cmpstr(result, ==, "\xef\xbf\xbd");
		g_free(result);
	}
}

static void
test_entity_valid(void)
{
	gchar *result;

	result = entity_decode("&Aacute;");
	g_assert_nonnull(result);
	if (g_test_verbose())
		g_printerr("result '%s'\n", result);
	g_assert_cmpstr(result, ==, "Á");
	g_free(result);

	result = entity_decode("&#123;");
	g_assert_nonnull(result);
	if (g_test_verbose())
		g_printerr("result '%s'\n", result);
	g_assert_cmpstr(result, ==, "{");
	g_free(result);

}

int
main(int argc, char *argv[])
{
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/core/entity/invalid", test_entity_invalid);
	g_test_add_func("/core/entity/toolong", test_entity_toolong);
	g_test_add_func("/core/entity/unprintable", test_entity_unprintable);
	g_test_add_func("/core/entity/valid", test_entity_valid);

	return g_test_run();
}
