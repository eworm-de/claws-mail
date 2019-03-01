#include "pgp_utils.h"

#define HEADER "HEADER"
struct td {
	gchar *input;
	gchar *expected_output;
};

static void
test_pgp_locate_armor_header_null()
{
	if (!g_test_undefined())
		return;

	if (g_test_subprocess()) {
		gchar *out = pgp_locate_armor_header(NULL, HEADER);
		g_assert_null(out);
		return;
	}

	g_test_trap_subprocess(NULL, 0, 0);
	g_test_trap_assert_stderr("*assertion*failed*");
	g_test_trap_assert_failed();
}

static void
test_pgp_locate_armor_header(gconstpointer user_data)
{
	struct td *data = (struct td *)user_data;
	gchar *out = pgp_locate_armor_header(data->input, HEADER);

	g_assert_cmpstr(out, ==, data->expected_output);
}

struct td td_justheader = {
	"HEADER",
	"HEADER"
};

struct td td_leading = {
	"leadingHEADER",
	NULL
};

struct td td_trailingspaces = {
	"HEADER     ",
	"HEADER     "
};

struct td td_trailingtext1 = {
	"HEADERblah",
	NULL
};

struct td td_trailingtext2 = {
	"HEADER   blah",
	NULL
};

struct td td_leadinglines = {
	"foo\nHEADER\nbar",
	"HEADER\nbar"
};

#define TEST(name, data) \
	g_test_add_data_func("/plugins/pgpcore/pgp_locate_armor_header_"name, \
			&data, \
			test_pgp_locate_armor_header)
int
main (int argc, char *argv[])
{
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/plugins/pgpcore/pgp_locate_armor_header_null",
			test_pgp_locate_armor_header_null);

	TEST("justheader", td_justheader);
	TEST("leading", td_leading);
	TEST("trailingspaces", td_trailingspaces);
	TEST("trailingtext1", td_trailingtext1);
	TEST("trailingtext2", td_trailingtext2);
	TEST("leadinglines", td_leadinglines);

	return g_test_run();
}
