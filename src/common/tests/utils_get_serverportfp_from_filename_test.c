#include "config.h"

#include <stdio.h>
#include <glib.h>

#include "utils.h"

#include "mock_prefs_common_get_use_shred.h"
#include "mock_prefs_common_get_flush_metadata.h"

struct td {
	gchar *str;
	gboolean ret;
	gchar *assertmsg;
	gchar *host;
	gchar *port;
	gchar *fp;
};

#define ASSERTMSG_NULLINPUT "*CRITICAL* 'str != NULL' failed*"
#define ASSERTMSG_FAILED "*could not parse filename*"

struct td td_null = {
	NULL,
	FALSE, ASSERTMSG_NULLINPUT,
	NULL, NULL, NULL
};
struct td td_empty = {
	"",
	FALSE, ASSERTMSG_FAILED,
	NULL, NULL, NULL
};
struct td td_suffixonly = {
	".cert",
	FALSE, ASSERTMSG_FAILED,
	NULL, NULL, NULL
};
struct td td_hostonly = {
	"foo.cert",
	FALSE, ASSERTMSG_FAILED,
	"foo", NULL, NULL
};
struct td td_short_nofp = {
	"shorthost.10.cert",
	TRUE, NULL,
	"shorthost", "10", NULL
};
struct td td_short_fp = {
	"shorthost.10.ab:cd:ef:gh.cert",
	TRUE, NULL,
	"shorthost", "10", "ab:cd:ef:gh"
};
struct td td_ip_nofp = {
	"10.20.30.40.10.cert",
	TRUE, NULL,
	"10.20.30.40", "10", NULL
};
struct td td_ip_fp = {
	"10.20.30.40.10.ab:cd:ef:gh.cert",
	TRUE, NULL,
	"10.20.30.40", "10", "ab:cd:ef:gh"
};
struct td td_long_nofp = {
	"longer.host.name.10.cert",
	TRUE, NULL,
	"longer.host.name", "10", NULL
};
struct td td_long_fp = {
	"longer.host.name.10.ab:cd:ef:gh.cert",
	TRUE, NULL,
	"longer.host.name", "10", "ab:cd:ef:gh"
};
struct td td_cert_starts_with_digits = {
	"longer.host.name.10.20:cd:ef:gh.cert",
	TRUE, NULL,
	"longer.host.name", "10", "20:cd:ef:gh"
};

void
test_utils_get_serverportfp_from_filename_nulloutput()
{
	if (!g_test_undefined())
		return;

	if (g_test_subprocess()) {
		gboolean ret = get_serverportfp_from_filename("valid.host.10.ab:cd:ef:gh.cert",
				NULL, NULL, NULL);
		g_assert_true(ret);
		return;
	}

	g_test_trap_subprocess(NULL, 0, 0);
	g_test_trap_assert_failed();
}

void
test_utils_get_serverportfp_from_filename(gconstpointer user_data)
{
	const struct td *data = (const struct td *)user_data;

	if (!g_test_undefined())
		return;

	if (g_test_subprocess()) {
		gchar *host, *port, *fp;
		gboolean ret = get_serverportfp_from_filename(data->str, &host, &port, &fp);

		g_assert_true(ret == data->ret);
		g_assert_cmpstr(host, ==, data->host);
		g_assert_cmpstr(port, ==, data->port);
		g_assert_cmpstr(fp, ==, data->fp);
		return;
	}

	g_test_trap_subprocess(NULL, 0, 0);

	if (!data->ret && data->assertmsg != NULL) {
		g_test_trap_assert_stderr(data->assertmsg);
		g_test_trap_assert_failed();
	} else {
		g_test_trap_assert_passed();
	}
}

int
main(int argc, char *argv[])
{
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/common/utils/get_serverportfp_from_filename/nulloutput",
			test_utils_get_serverportfp_from_filename_nulloutput);

	g_test_add_data_func("/common/utils/get_serverportfp_from_filename/null",
			&td_null, test_utils_get_serverportfp_from_filename);
	g_test_add_data_func("/common/utils/get_serverportfp_from_filename/empty",
			&td_empty, test_utils_get_serverportfp_from_filename);
	g_test_add_data_func("/common/utils/get_serverportfp_from_filename/suffixonly",
			&td_suffixonly, test_utils_get_serverportfp_from_filename);
	g_test_add_data_func("/common/utils/get_serverportfp_from_filename/hostonly",
			&td_hostonly, test_utils_get_serverportfp_from_filename);
	g_test_add_data_func("/common/utils/get_serverportfp_from_filename/short_nofp",
			&td_short_nofp, test_utils_get_serverportfp_from_filename);
	g_test_add_data_func("/common/utils/get_serverportfp_from_filename/short_fp",
			&td_short_fp, test_utils_get_serverportfp_from_filename);
	g_test_add_data_func("/common/utils/get_serverportfp_from_filename/ip_nofp",
			&td_ip_nofp, test_utils_get_serverportfp_from_filename);
	g_test_add_data_func("/common/utils/get_serverportfp_from_filename/ip_fp",
			&td_ip_fp, test_utils_get_serverportfp_from_filename);
	g_test_add_data_func("/common/utils/get_serverportfp_from_filename/long_nofp",
			&td_long_nofp, test_utils_get_serverportfp_from_filename);
	g_test_add_data_func("/common/utils/get_serverportfp_from_filename/long_fp",
			&td_long_fp, test_utils_get_serverportfp_from_filename);
	g_test_add_data_func("/common/utils/get_serverportfp_from_filename/cert_starts_with_digits",
			&td_cert_starts_with_digits, test_utils_get_serverportfp_from_filename);

	return g_test_run();
}
