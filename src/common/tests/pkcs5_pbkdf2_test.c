#include "config.h"

#include <glib.h>

#include <common/pkcs5_pbkdf2.h>

struct td {
	gchar *input;
	size_t input_length;
	gchar *salt;
	size_t salt_length;
	size_t length;
	guint rounds;
	gint expected_return;
	gchar *expected_output;
};

/* Test vectors from RFC 6070 */
struct td td_rfc6070_1 = { "password", 8,
	"salt", 4,
	20, 1, 0,
	"\x0c\x60\xc8\x0f\x96\x1f\x0e\x71\xf3\xa9\xb5\x24\xaf\x60\x12\x06\x2f\xe0\x37\xa6" };
struct td td_rfc6070_2 = { "password", 8,
	"salt", 4,
	20, 2, 0,
	"\xea\x6c\x01\x4d\xc7\x2d\x6f\x8c\xcd\x1e\xd9\x2a\xce\x1d\x41\xf0\xd8\xde\x89\x57" };
struct td td_rfc6070_3 = { "password", 8,
	"salt", 4,
	20, 4096, 0,
	"\x4b\x00\x79\x01\xb7\x65\x48\x9a\xbe\xad\x49\xd9\x26\xf7\x21\xd0\x65\xa4\x29\xc1" };
struct td td_rfc6070_4 = { "password", 8,
	"salt", 4,
	20, 16777216, 0,
	"\xee\xfe\x3d\x61\xcd\x4d\xa4\xe4\xe9\x94\x5b\x3d\x6b\xa2\x15\x8c\x26\x34\xe9\x84" };
struct td td_rfc6070_5 = { "passwordPASSWORDpassword", 24,
	"saltSALTsaltSALTsaltSALTsaltSALTsalt", 36,
	25, 4096, 0,
	"\x3d\x2e\xec\x4f\xe4\x1c\x84\x9b\x80\xc8\xd8\x36\x62\xc0\xe4\x4a\x8b\x29\x1a\x96\x4c\xf2\xf0\x70\x38" };
struct td td_rfc6070_6 = { "pass\0word", 9,
	"sa\0lt", 5,
	16, 4096, 0,
	"\x56\xfa\x6a\xa7\x55\x48\x09\x9d\xcc\x37\xd7\xf0\x34\x25\xe0\xc3" };

struct td td_zero_rounds = { "abc", 3, "abc", 3, 3, 0, -1, "" };
struct td td_zero_output_length = { "abc", 3, "abc", 3, 0, 1, -1, NULL };
struct td td_null_input = { NULL, 10, "", 10, 10, 100, -1, "" };
struct td td_null_salt = { "", 10, NULL, 10, 10, 100, -1, "" };

static void
test_pkcs5_pbkdf2(gconstpointer user_data)
{
	struct td *data = (struct td *)user_data;
	guchar *kd = g_malloc0(data->length);
	gint ret;

	if (data->rounds > 10000 && !g_test_slow()) {
		gchar *msg = "Time-intensive test, rerun in slow mode to run it.";
		g_test_skip(msg);
		g_test_message(msg);
		return;
	}

	if (g_test_verbose()) {
		g_printerr("input '%s' (%ld)\nsalt '%s' (%ld)\nlength %ld\nrounds %d\nexpected_return %d\n",
				data->input, data->input_length,
				data->salt, data->salt_length,
				data->length,
				data->rounds,
				data->expected_return);
	}

	ret = pkcs5_pbkdf2(
			data->input,
			data->input_length,
			data->salt,
			data->salt_length,
			kd,
			data->length,
			data->rounds);

	g_assert_cmpint(ret, ==, data->expected_return);
	g_assert_cmpstr(kd, ==, data->expected_output);
}

static void
test_pkcs5_pbkdf2_null_output_buffer(void)
{
	gint ret = pkcs5_pbkdf2("abc", 3, "abc", 3, NULL, 10, 1);

	g_assert_cmpint(ret, ==, -1);
}

int
main(int argc, char *argv[])
{
	g_test_init(&argc, &argv, NULL);

	g_test_add_data_func("/common/pkcs5_pbkdf2/zero_rounds",
			&td_zero_rounds, test_pkcs5_pbkdf2);
	g_test_add_data_func("/common/pkcs5_pbkdf2/zero_output_length",
			&td_zero_output_length, test_pkcs5_pbkdf2);
	g_test_add_data_func("/common/pkcs5_pbkdf2/null_salt",
			&td_null_salt, test_pkcs5_pbkdf2);
	g_test_add_data_func("/common/pkcs5_pbkdf2/null_input",
			&td_null_input, test_pkcs5_pbkdf2);
	g_test_add_func("/common/pkcs5_pbkdf2/null_output_buffer",
			test_pkcs5_pbkdf2_null_output_buffer);

	g_test_add_data_func("/common/pkcs5_pbkdf2/rfc6070_1",
			&td_rfc6070_1, test_pkcs5_pbkdf2);
	g_test_add_data_func("/common/pkcs5_pbkdf2/rfc6070_2",
			&td_rfc6070_2, test_pkcs5_pbkdf2);
	g_test_add_data_func("/common/pkcs5_pbkdf2/rfc6070_3",
			&td_rfc6070_3, test_pkcs5_pbkdf2);
	g_test_add_data_func("/common/pkcs5_pbkdf2/rfc6070_4",
			&td_rfc6070_4, test_pkcs5_pbkdf2);
	g_test_add_data_func("/common/pkcs5_pbkdf2/rfc6070_5",
			&td_rfc6070_5, test_pkcs5_pbkdf2);
	g_test_add_data_func("/common/pkcs5_pbkdf2/rfc6070_6",
			&td_rfc6070_6, test_pkcs5_pbkdf2);

	return g_test_run();
}
