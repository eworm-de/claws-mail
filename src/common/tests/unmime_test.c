#include "config.h"

#include <stdio.h>
#include <glib.h>

#include "unmime.h"
#include "quoted-printable.h"

#include "mock_prefs_common_get_use_shred.h"
#include "mock_prefs_common_get_flush_metadata.h"

struct td {
	gchar *input;
	gchar *expected_output;
};

struct td td_rfc2047_space1 = {
	"(=?ISO-8859-1?Q?a?=)",
	"(a)"
};
struct td td_rfc2047_space2 = {
	"(=?ISO-8859-1?Q?a?= b)",
	"(a b)"
};
struct td td_rfc2047_space3 = {
	"(=?ISO-8859-1?Q?a?= =?ISO-8859-1?Q?b?=)",
	"(ab)"
};
struct td td_rfc2047_space4 = {
	"(=?ISO-8859-1?Q?a?=  =?ISO-8859-1?Q?b?=)",
	"(ab)"
};
struct td td_rfc2047_space5 = {
	"(=?ISO-8859-1?Q?a?=\r\n =?ISO-8859-1?Q?b?=)",
	"(ab)"
};
struct td td_rfc2047_space6 = {
	"(=?ISO-8859-1?Q?a_b?=)",
	"(a b)"
};
struct td td_rfc2047_space7 = {
	"(=?ISO-8859-1?Q?a?= =?ISO-8859-2?Q?_b?=)",
	"(a b)"
};

static void
test_unmime_header_null()
{
	if (!g_test_undefined())
		return;

	if (g_test_subprocess()) {
		gchar *out;

		out = unmime_header(NULL, FALSE);
		g_assert_null(out);
		return;
	}

	g_test_trap_subprocess(NULL, 0, 0);
	g_test_trap_assert_stdout("*Condition*failed*");
	g_test_trap_assert_passed();
}

static void
test_unmime_header(gconstpointer user_data)
{
	struct td *data = (struct td *)user_data;
	gchar *out = unmime_header(data->input, FALSE);

	g_assert_nonnull(out);
	g_assert_cmpstr(out, ==, data->expected_output);
}

int
main(int argc, char *argv[])
{
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/common/unmime/null",
			test_unmime_header_null);

	g_test_add_data_func("/common/unmime/rfc2047_space1",
			&td_rfc2047_space1,
			test_unmime_header);
	g_test_add_data_func("/common/unmime/rfc2047_space2",
			&td_rfc2047_space2,
			test_unmime_header);
	g_test_add_data_func("/common/unmime/rfc2047_space3",
			&td_rfc2047_space3,
			test_unmime_header);
	g_test_add_data_func("/common/unmime/rfc2047_space4",
			&td_rfc2047_space4,
			test_unmime_header);
	g_test_add_data_func("/common/unmime/rfc2047_space5",
			&td_rfc2047_space5,
			test_unmime_header);
	g_test_add_data_func("/common/unmime/rfc2047_space6",
			&td_rfc2047_space6,
			test_unmime_header);
	g_test_add_data_func("/common/unmime/rfc2047_space7",
			&td_rfc2047_space7,
			test_unmime_header);

	return g_test_run();
}
