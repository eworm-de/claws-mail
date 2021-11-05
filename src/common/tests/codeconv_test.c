#include "config.h"

#include <glib.h>

#include "codeconv.h"

#include "mock_prefs_common_get_use_shred.h"
#include "mock_prefs_common_get_flush_metadata.h"

struct td {
	gchar *pre; /* Input string */
	gchar *post; /* Expected output */
};

struct td from_utf8_empty = { "", "" };
/* TODO: more tests */

struct td to_utf8_empty = { "", "" };
/* TODO: more tests */

static void
test_filename_from_utf8_null()
{
	if (!g_test_undefined())
		return;

	if (g_test_subprocess()) {
		gchar *out;

		out = conv_filename_from_utf8(NULL);
		g_assert_null(out);
		return;
	}

	g_test_trap_subprocess(NULL, 0, 0);
	g_test_trap_assert_stdout("*Condition*failed*");
	g_test_trap_assert_passed();
}

static void
test_filename_from_utf8(gconstpointer user_data)
{
	struct td *data = (struct td *)user_data;

	if (!g_test_undefined())
		return;

	if (g_test_subprocess()) {
		gchar *out;

		out = conv_filename_from_utf8(data->pre);
		g_assert_cmpstr(out, ==, data->post);

		g_free(out);
		return;
	}

	g_test_trap_subprocess(NULL, 0, 0);
	g_test_trap_assert_passed();
}

static void
test_filename_to_utf8(gconstpointer user_data)
{
	struct td *data = (struct td *)user_data;

	if (!g_test_undefined())
		return;

	if (g_test_subprocess()) {
		gchar *out;

		out = conv_filename_to_utf8(data->pre);
		g_assert_cmpstr(out, ==, data->post);

		g_free(out);
		return;
	}

	g_test_trap_subprocess(NULL, 0, 0);
	g_test_trap_assert_passed();
}

int
main(int argc, char *argv[])
{
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/common/codeconv/filename_from_utf8/null",
			test_filename_from_utf8_null);
	g_test_add_data_func("/common/codeconv/filename_from_utf8/empty",
			&from_utf8_empty,
			test_filename_from_utf8);

	g_test_add_func("/common/codeconv/filename_to_utf8/null",
			test_filename_from_utf8_null);
	g_test_add_data_func("/common/codeconv/filename_to_utf8/empty",
			&to_utf8_empty,
			test_filename_to_utf8);

	/* TODO: more tests */

	return g_test_run();
}
