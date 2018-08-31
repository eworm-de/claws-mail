#include <glib.h>
#include "mock_debug_print.h"

#include "entity.h"

static void
test_entity(void)
{
	gchar *result;

	result = entity_decode(NULL);
	g_assert_null(result);

	result = entity_decode("foo");
	g_assert_null(result);

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

	g_test_add_func("/core/entity", test_entity);

	return g_test_run();
}
