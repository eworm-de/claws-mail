#include "config.h"

#include <stdio.h>
#include <glib.h>

#include "utils.h"

#include "mock_prefs_common_get_use_shred.h"
#include "mock_prefs_common_get_flush_metadata.h"

struct td_get_uri_part {
	gchar *str;
	gboolean ret;
	guint uri_length;
};

struct td_get_uri_part td_get_uri_basic = {
	"http://www.example.com", TRUE, 22
};
struct td_get_uri_part td_get_uri_slash = {
	"http://www.example.com/", TRUE, 23
};
struct td_get_uri_part td_get_uri_question = {
	"http://www.example.com/foo?", TRUE, 27
};
struct td_get_uri_part td_get_uri_parenthesis = {
	"http://www.example.com/f(o)o", TRUE, 28
};
struct td_get_uri_part td_get_uri_brace = {
	"http://www.example.com/f[oo", TRUE, 24
};
struct td_get_uri_part td_get_uri_umlaut = {
	"http://www.examöple.com", TRUE, 24
};
struct td_get_uri_part td_get_uri_kanji = {
	"http://www.漢字.com", TRUE, 21
};
struct td_get_uri_part td_get_uri_nonprintable = {
	"http://www.exam\x01ple.com", TRUE, 15
};

#define URI "http://www.example.com"
static void
test_utils_get_uri_part_nowhitespace()
{
	gboolean ret;
	gchar *str = g_strdup("Nowhitespace"URI"nowhitespace");
	const gchar *bp, *ep;

	ret = get_uri_part(str, str + 12, &bp, &ep, FALSE);

	g_assert_true(ret);
	g_assert_true(ep == str + strlen(str));

	g_free(str);
}

static void
test_utils_get_uri_part_whitespace()
{
	gboolean ret;
	gchar *str = g_strdup("Whitespace "URI" whitespace");
	const gchar *bp, *ep;

	ret = get_uri_part(str, str + 11, &bp, &ep, FALSE);

	g_assert_true(ret);
	g_assert_true(ep == bp + strlen(URI));

	g_free(str);
}
#undef URI

static void
test_utils_get_uri_part(gconstpointer user_data)
{
	const struct td_get_uri_part *data = (struct td_get_uri_part *)user_data;
	gboolean ret;
	const gchar *bp, *ep;

	ret = get_uri_part(data->str, data->str, &bp, &ep, FALSE);

	g_assert_nonnull(bp);
	g_assert_nonnull(ep);

	g_assert_true(ret == data->ret);
	g_assert_true(ep >= bp);
	g_assert_true(bp + data->uri_length == ep);
}

int
main(int argc, char *argv[])
{
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/common/utils/get_uri_part/nowhitespace",
			test_utils_get_uri_part_nowhitespace);
	g_test_add_func("/common/utils/get_uri_part/whitespace",
			test_utils_get_uri_part_whitespace);

	g_test_add_data_func("/common/utils/get_uri_part/basic",
			&td_get_uri_basic,
			test_utils_get_uri_part);
	g_test_add_data_func("/common/utils/get_uri_part/slash",
			&td_get_uri_slash,
			test_utils_get_uri_part);
	g_test_add_data_func("/common/utils/get_uri_part/question",
			&td_get_uri_question,
			test_utils_get_uri_part);
	g_test_add_data_func("/common/utils/get_uri_part/parenthesis",
			&td_get_uri_parenthesis,
			test_utils_get_uri_part);
	g_test_add_data_func("/common/utils/get_uri_part/brace",
			&td_get_uri_brace,
			test_utils_get_uri_part);
	g_test_add_data_func("/common/utils/get_uri_part/umlaut",
			&td_get_uri_umlaut,
			test_utils_get_uri_part);
	g_test_add_data_func("/common/utils/get_uri_part/kanji",
			&td_get_uri_kanji,
			test_utils_get_uri_part);
	g_test_add_data_func("/common/utils/get_uri_part/nonprintable",
			&td_get_uri_nonprintable,
			test_utils_get_uri_part);

	return g_test_run();
}
