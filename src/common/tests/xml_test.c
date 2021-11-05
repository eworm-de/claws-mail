#include "config.h"

#include <glib.h>

#include "xml.h"

#include "mock_prefs_common_get_use_shred.h"
#include "mock_prefs_common_get_flush_metadata.h"

#define DATADIR "data/"

static void
test_xml_open_file_missing(void)
{
	XMLFile *xf = xml_open_file(DATADIR "missing.xml");
	g_assert_null(xf);
}

static void
test_xml_open_file_empty(void)
{
	XMLFile *xf = xml_open_file(DATADIR "empty.xml");
	g_assert_nonnull(xf);
	g_assert_nonnull(xf->buf);
	g_assert_nonnull(xf->bufp);
	g_assert_null(xf->dtd);
	g_assert_null(xf->encoding);
	g_assert_null(xf->tag_stack);
	g_assert_cmpint(xf->level, ==, 0);
	g_assert_false(xf->is_empty_element);
}

int
main(int argc, char *argv[])
{
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/common/xml_open_file_missing", test_xml_open_file_missing);
	g_test_add_func("/common/xml_open_file_empty", test_xml_open_file_empty);

	return g_test_run();
}
