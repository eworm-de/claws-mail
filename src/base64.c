/*
 * base64.c -- base-64 conversion routines.
 *
 * For license terms, see the file COPYING in this directory.
 *
 * This base 64 encoding is defined in RFC2045 section 6.8,
 * "Base64 Content-Transfer-Encoding", but lines must not be broken in the
 * scheme used here.
 *
 * Modified by Hiroyuki Yamamoto <hiro-y@kcn.ne.jp>
 */

#include "defs.h"

#include <ctype.h>
#include <string.h>
#include <glib.h>

#include "base64.h"

static const char base64digits[] =
   "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

#define BAD	-1
static const char base64val[] = {
    BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD,
    BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD,
    BAD,BAD,BAD,BAD, BAD,BAD,BAD,BAD, BAD,BAD,BAD, 62, BAD,BAD,BAD, 63,
     52, 53, 54, 55,  56, 57, 58, 59,  60, 61,BAD,BAD, BAD,BAD,BAD,BAD,
    BAD,  0,  1,  2,   3,  4,  5,  6,   7,  8,  9, 10,  11, 12, 13, 14,
     15, 16, 17, 18,  19, 20, 21, 22,  23, 24, 25,BAD, BAD,BAD,BAD,BAD,
    BAD, 26, 27, 28,  29, 30, 31, 32,  33, 34, 35, 36,  37, 38, 39, 40,
     41, 42, 43, 44,  45, 46, 47, 48,  49, 50, 51,BAD, BAD,BAD,BAD,BAD
};
#define DECODE64(c)  (isascii(c) ? base64val[c] : BAD)

void to64frombits(unsigned char *out, const unsigned char *in, int inlen)
/* raw bytes in quasi-big-endian order to base 64 string (NUL-terminated) */
{
    for (; inlen >= 3; inlen -= 3)
    {
	*out++ = base64digits[in[0] >> 2];
	*out++ = base64digits[((in[0] << 4) & 0x30) | (in[1] >> 4)];
	*out++ = base64digits[((in[1] << 2) & 0x3c) | (in[2] >> 6)];
	*out++ = base64digits[in[2] & 0x3f];
	in += 3;
    }
    if (inlen > 0)
    {
	unsigned char fragment;
    
	*out++ = base64digits[in[0] >> 2];
	fragment = (in[0] << 4) & 0x30;
	if (inlen > 1)
	    fragment |= in[1] >> 4;
	*out++ = base64digits[fragment];
	*out++ = (inlen < 2) ? '=' : base64digits[(in[1] << 2) & 0x3c];
	*out++ = '=';
    }
    *out = '\0';
}

int from64tobits(char *out, const char *in)
/* base 64 to raw bytes in quasi-big-endian order, returning count of bytes */
{
    int len = 0;
    register unsigned char digit1, digit2, digit3, digit4;

    if (in[0] == '+' && in[1] == ' ')
	in += 2;
    if (*in == '\r' || *in == '\n')
	return 0;

    do {
	digit1 = in[0];
	if (DECODE64(digit1) == BAD)
	    return -1;
	digit2 = in[1];
	if (DECODE64(digit2) == BAD)
	    return -1;
	digit3 = in[2];
	if (digit3 != '=' && DECODE64(digit3) == BAD)
	    return -1; 
	digit4 = in[3];
	if (digit4 != '=' && DECODE64(digit4) == BAD)
	    return -1;
	in += 4;
	*out++ = (DECODE64(digit1) << 2) | ((DECODE64(digit2) >> 4) & 0x03);
	++len;
	if (digit3 != '=')
	{
	    *out++ = ((DECODE64(digit2) << 4) & 0xf0) | (DECODE64(digit3) >> 2);
	    ++len;
	    if (digit4 != '=')
	    {
		*out++ = ((DECODE64(digit3) << 6) & 0xc0) | DECODE64(digit4);
		++len;
	    }
	}
    } while (*in && *in != '\r' && *in != '\n' && digit4 != '=');

    return len;
}

struct _Base64Decoder
{
	int buf_len;
	unsigned char buf[4];
};

Base64Decoder *
base64_decoder_new (void)
{
	Base64Decoder *decoder;

	decoder = g_new0 (Base64Decoder, 1);
	return decoder;
}

void
base64_decoder_free (Base64Decoder *decoder)
{
	g_free (decoder);
}

int
base64_decoder_decode (Base64Decoder *decoder,
		       const char    *in, 
		       char          *out)
{
	int len = 0;
	int buf_len;
	unsigned char buf[4];

	g_return_val_if_fail (decoder != NULL, -1);
	g_return_val_if_fail (in != NULL, -1);
	g_return_val_if_fail (out != NULL, -1);

	buf_len = decoder->buf_len;
	memcpy (buf, decoder->buf, sizeof(buf));
	while (1) {
		while (buf_len < 4) {
			int c = *(unsigned char *)in++;
			if (c == '\0') break;
			if (c == '\r' || c == '\n') continue;
			if (c != '=' && DECODE64(c) == BAD)
				return -1;
			buf[buf_len++] = c;
		}
		if (buf_len < 4 || buf[0] == '=' || buf[1] == '=') {
			decoder->buf_len = buf_len;
			memcpy (decoder->buf, buf, sizeof(buf));
			return len;
		}
		*out++ = ((DECODE64(buf[0]) << 2)
			  | ((DECODE64(buf[1]) >> 4) & 0x03));
		++len;
		if (buf[2] != '=') {
			*out++ = (((DECODE64(buf[1]) << 4) & 0xf0)
				  | (DECODE64(buf[2]) >> 2));
			++len;
			if (buf[3] != '=') {
				*out++ = (((DECODE64(buf[2]) << 6) & 0xc0)
					  | DECODE64(buf[3]));
				++len;
			}
		}
		buf_len = 0;
		if (buf[2] == '=' || buf[3] == '=') {
			decoder->buf_len = 0;
			return len;
		}
	}
}

/* base64.c ends here */
