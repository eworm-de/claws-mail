#include "config.h"

#include <glib.h>

#include <common/md5.h>

struct td_digest {
	gchar *input;
	gchar *expected_output;
};

struct td_digest td_rfc1321_1 = { "", "d41d8cd98f00b204e9800998ecf8427e" };
struct td_digest td_rfc1321_2 = { "a", "0cc175b9c0f1b6a831c399e269772661" };
struct td_digest td_rfc1321_3 = { "abc", "900150983cd24fb0d6963f7d28e17f72" };
struct td_digest td_rfc1321_4 = { "message digest", "f96b697d7cb7938d525a2f31aaf161d0" };
struct td_digest td_rfc1321_5 = { "abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b" };
struct td_digest td_rfc1321_6 = { "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789", "d174ab98d277d9f5a5611c2c9f419d9f" };
struct td_digest td_rfc1321_7 = { "12345678901234567890123456789012345678901234567890123456789012345678901234567890", "57edf4a22be3c955ac49da2e2107b67a" };

struct td_hmac {
	gchar *key;
	int key_len;
	gchar *data;
	int data_len;
	gchar *expected_output;
};

struct td_hmac td_hmac_null_key = {
	NULL, 50,
	"", 0,
	""
};
struct td_hmac td_hmac_negative_key_length = {
	"", -1,
	"", 0,
	""
};
struct td_hmac td_hmac_null_data = {
	"", 0,
	NULL, 50,
	""
};
struct td_hmac td_hmac_negative_data_length = {
	"abc", 3,
	"", -1,
	"",
};

struct td_hmac td_rfc2202_1 = {
	"\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b", 16,
	"Hi There", 8,
	"9294727a3638bb1c13f48ef8158bfc9d"
};
struct td_hmac td_rfc2202_2 = {
	"Jefe", 4,
	"what do ya want for nothing?", 28,
	"750c783e6ab0b503eaa86e310a5db738"
};
struct td_hmac td_rfc2202_3 = {
	"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa", 16,
	"\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd\xdd", 50,
	"56be34521d144c88dbb8c733f0e8b3f6"
};
struct td_hmac td_rfc2202_4 = {
	"\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19", 25,
	"\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd\xcd", 50,
	"697eaf0aca3a3aea3a75164746ffaa79"
};
struct td_hmac td_rfc2202_5 = {
	"\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c\x0c", 16,
	"Test With Truncation", 20,
	"56461ef2342edc00f9bab995690efd4c"
};
struct td_hmac td_rfc2202_6 = {
	"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa", 80,
	"Test Using Larger Than Block-Size Key - Hash Key First", 54,
	"6b1ab7fe4bd7bf8f0b62e6ce61b9d0cd"
};
struct td_hmac td_rfc2202_7 = {
	"\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa\xaa", 80,
	"Test Using Larger Than Block-Size Key and Larger Than One Block-Size Data", 73,
	"6f630fad67cda0ee1fb1f562db3aa53e"
};

static void
test_md5_hex_digest_null_input(void)
{
	char outbuf[33];

	outbuf[0] = '\0';

	if (!g_test_undefined())
		return;

	if (g_test_subprocess()) {
		md5_hex_digest(outbuf, NULL);
		return;
	}

	g_test_trap_subprocess(NULL, 0, 0);
	g_test_trap_assert_failed();
	g_test_trap_assert_stderr("*CRITICAL*md5_hex_digest: assertion*failed*");
	g_assert_cmpint(outbuf[0], ==, '\0');
}

static void
test_md5_hex_digest_null_output(void)
{
	if (!g_test_undefined())
		return;

	if (g_test_subprocess()) {
		md5_hex_digest(NULL, "");
		return;
	}

	g_test_trap_subprocess(NULL, 0, 0);
	g_test_trap_assert_failed();
	g_test_trap_assert_stderr("*CRITICAL*md5_hex_digest: assertion*failed*");
}

static void
test_md5_hex_digest(gconstpointer user_data)
{
	char outbuf[33];
	struct td_digest *data = (struct td_digest *)user_data;

	md5_hex_digest(outbuf, data->input);

	g_assert_cmpint(outbuf[32], ==, '\0');
	g_assert_cmpstr(outbuf, ==, data->expected_output);
}

static void
test_md5_hex_hmac_null_output(void)
{
	if (!g_test_undefined())
		return;

	if (g_test_subprocess()) {
		md5_hex_hmac(NULL, "", 0, "", 0);
		return;
	}

	g_test_trap_subprocess(NULL, 0, 0);
	g_test_trap_assert_failed();
	g_test_trap_assert_stderr("*CRITICAL*md5_hex_hmac: assertion*failed*");
}

/* We expect all test cases using this function to fail with
 * failed assertion */
static void
test_md5_hex_hmac_fails(gconstpointer user_data)
{
	char outbuf[33];
	struct td_hmac *data = (struct td_hmac *)user_data;

	if (!g_test_undefined())
		return;

	if (g_test_subprocess()) {
		md5_hex_hmac(outbuf,
				data->data, data->data_len,
				data->key, data->key_len);
		return;
	}

	g_test_trap_subprocess(NULL, 0, 0);
	g_test_trap_assert_failed();
	g_test_trap_assert_stderr("*CRITICAL*md5_hex_hmac: assertion*failed*");
}

static void
test_md5_hex_hmac(gconstpointer user_data)
{
	char outbuf[33];
	struct td_hmac *data = (struct td_hmac *)user_data;

	md5_hex_hmac(outbuf,
			data->data, data->data_len,
			data->key, data->key_len);

	g_assert_cmpint(outbuf[32], ==, '\0');
	g_assert_cmpstr(outbuf, ==, data->expected_output);
}


int
main(int argc, char *argv[])
{
	g_test_init(&argc, &argv, NULL);

	g_test_add_func("/common/md5/hex_digest/null_input",
			test_md5_hex_digest_null_input);
	g_test_add_func("/common/md5/hex_digest/null_output",
			test_md5_hex_digest_null_output);

	g_test_add_data_func("/common/md5/hex_digest/rfc1321_1",
			&td_rfc1321_1,
			test_md5_hex_digest);
	g_test_add_data_func("/common/md5/hex_digest/rfc1321_2",
			&td_rfc1321_2,
			test_md5_hex_digest);
	g_test_add_data_func("/common/md5/hex_digest/rfc1321_3",
			&td_rfc1321_3,
			test_md5_hex_digest);
	g_test_add_data_func("/common/md5/hex_digest/rfc1321_4",
			&td_rfc1321_4,
			test_md5_hex_digest);
	g_test_add_data_func("/common/md5/hex_digest/rfc1321_5",
			&td_rfc1321_5,
			test_md5_hex_digest);
	g_test_add_data_func("/common/md5/hex_digest/rfc1321_6",
			&td_rfc1321_6,
			test_md5_hex_digest);
	g_test_add_data_func("/common/md5/hex_digest/rfc1321_7",
			&td_rfc1321_7,
			test_md5_hex_digest);

	g_test_add_data_func("/common/md5/hex_hmac/null_key",
			&td_hmac_null_key,
			test_md5_hex_hmac_fails);
	g_test_add_data_func("/common/md5/hex_hmac/negative_key_length",
			&td_hmac_negative_key_length,
			test_md5_hex_hmac_fails);
	g_test_add_data_func("/common/md5/hex_hmac/null_data",
			&td_hmac_null_data,
			test_md5_hex_hmac_fails);
	g_test_add_data_func("/common/md5/hex_hmac/negative_data_length",
			&td_hmac_negative_data_length,
			test_md5_hex_hmac_fails);
	g_test_add_func("/common/md5/hex_hmac/null_output",
			test_md5_hex_hmac_null_output);

	g_test_add_data_func("/common/md5/hex_hmac/rfc2202_1",
			&td_rfc2202_1,
			test_md5_hex_hmac);
	g_test_add_data_func("/common/md5/hex_hmac/rfc2202_2",
			&td_rfc2202_2,
			test_md5_hex_hmac);
	g_test_add_data_func("/common/md5/hex_hmac/rfc2202_3",
			&td_rfc2202_3,
			test_md5_hex_hmac);
	g_test_add_data_func("/common/md5/hex_hmac/rfc2202_4",
			&td_rfc2202_4,
			test_md5_hex_hmac);
	g_test_add_data_func("/common/md5/hex_hmac/rfc2202_5",
			&td_rfc2202_5,
			test_md5_hex_hmac);
	g_test_add_data_func("/common/md5/hex_hmac/rfc2202_6",
			&td_rfc2202_6,
			test_md5_hex_hmac);
	g_test_add_data_func("/common/md5/hex_hmac/rfc2202_7",
			&td_rfc2202_7,
			test_md5_hex_hmac);

	return g_test_run();
}
