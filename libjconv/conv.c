
/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include "jconv.h"

#define DEBUG_DO(x)

#undef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))

/*
 * Applies iconv() to the text in buffer and stores the result in
 * *buffer_r. *buffer_r is newly allocated, and user is responsible
 * to free() it.
 */
int
jconv_alloc_apply_iconv (iconv_t cd,
			 const char *buffer,
			 size_t len,
			 char **buffer_r,
			 size_t *len_r,
			 size_t *error_pos_r)
{
	char *to;
	const char *from_p, *from;
	size_t from_len, to_len, to_alloc;
	int error_code;

	DEBUG_DO(printf ("jconv_alloc_apply_iconv\n"));
	*buffer_r = NULL;
	*len_r = 0;
	*error_pos_r = 0;
	
	from_p = from = buffer;
	from_len = len;
	to_alloc = 4096;
	to = malloc(to_alloc);
	if (to == NULL)
		return errno;
	to_len = 0;
	for (;;) {
		char *to_p;
		const char *from_p_old;
		size_t s, from_left, to_left;
		
		if (to_alloc < to_len + 4096) {
			to_alloc += 4096;
			to = realloc(to, to_alloc);
			if (to == NULL)
				return errno;
		}
		from_left = MIN(from + from_len - from_p, 256);
		to_p = to + to_len;
		to_left = 4096;
		error_code = 0;
		from_p_old = from_p;
		s = iconv(cd, &from_p, &from_left, &to_p, &to_left);
		if (s == (size_t)-1)
			error_code = errno;
		switch (error_code) {
		case 0:
			if (from_p < from + from_len)
				break;
			/* write a reset sequence */
			s = iconv(cd, NULL, NULL, &to_p, &to_left);
			if (s == (size_t)-1)
				error_code = errno;
			to_len = to_p - to;
			goto break_loop;
		case EILSEQ:
			goto break_loop;
		case EINVAL:
			if (from_p + from_left < from + from_len)
				break;
			goto break_loop;
		case E2BIG:
		case EBADF:
		default:
			// abort();
			// error_code = EINVAL;
			// goto break_loop;
			;
		}
		if (from_p_old == from_p) {
			/* I believe iconv() is buggy if we reach here.
			 * We stop calling iconv() and return E2BIG in
			 * order not to go into an infinite loop. */
			error_code = E2BIG;
			break;
		}
		to_len = to_p - to;
	}
break_loop:

	to = realloc(to, to_len + 1); /* truncate */
	to[to_len] = 0;
	
	if (error_code)
		*error_pos_r = from_p - from;
	*len_r = to_len;
	*buffer_r = to;
	return error_code;
}

/*
 * Converts the text in src according to the specified codesets and
 * stores the result in dest. The src_codesets are candidate names
 * for codeset of src. At first, this function presumes src is coded in
 * src_codesets[0] and tries to convert. If it failes, tries the next
 * codeset, and so on. This function returns 0 if src is a valid text
 * of one of specified codesets, and nonzero otherwise.
 */
int
jconv_alloc_conv (const char *src,
		  size_t src_len,
		  char **dest_r,
		  size_t *dest_len_r,
		  const char *const *src_codesets,
		  int num_src_codesets,
		  int *actual_codeset_r,
		  const char *dest_codeset)
{
	int i;
	char *new_buffer = NULL;
	size_t new_buffer_len = 0, error_pos = 0;
	int error_code = 0;

	*dest_r = NULL;
	*dest_len_r = 0;
	*actual_codeset_r = num_src_codesets;
	
	for (i = 0; i < num_src_codesets; i++) {
		iconv_t cd;
		DEBUG_DO(printf("jconv_alloc_conv: try %s\n", src_codesets[i]));
		cd = iconv_open(dest_codeset, src_codesets[i]);
		if (cd == (iconv_t)-1) {
			/* EMFILE, ENFILE, ENOMEM, or EINVAL */
			error_code = errno;
			break;
		}
		error_code = jconv_alloc_apply_iconv (cd, src, src_len,
						      &new_buffer,
						      &new_buffer_len,
						      &error_pos);
		/*
		 * Glibc don't reject paticular 8-bit strings when
		 * from_codeset is ISO-2022-JP. We reject them by
		 * hand.
		 */
		/******** DIRTY HACK ON ********/
		if (error_code == 0 &&
		    strcasecmp(src_codesets[i], "ISO-2022-JP") == 0)
		{
			int j;
			for (j = 0; j < src_len; j++) {
				if (src[j] & 0x80) {
					error_code = EILSEQ;
					break;
				}
			}
		}
		/******** DIRTY HACK OFF ********/
		iconv_close(cd);
		if (error_code) {
			if (new_buffer)
				free(new_buffer);
			new_buffer = NULL;
		}
		if (error_code != EILSEQ)
			break;
	}

	if (num_src_codesets > 0 && i >= num_src_codesets)
		i = num_src_codesets - 1;

	DEBUG_DO(printf("FROM: %s\n", src));
	DEBUG_DO(printf("CODESET: %s\n", src_codesets[i]));
	*dest_r = new_buffer;
	*dest_len_r = new_buffer_len;
	*actual_codeset_r = i;
	return error_code;
}

/*
 * Converts the text in src according to the specified codesets and
 * stores the result in dest. If dest_codeset is NULL, codeset of
 * the current locale obtained by jconv_info_get_current_codeset() is
 * used. The src_codesets  are candidate names for codeset of src.
 * If src_codesets is of zero-length, codeset names obtained by
 * jconv_info_get_pref_codesets() are used. This function returns 0
 * if src is a valid text of one of specified codesets, and nonzero
 * otherwise.
 */
int
jconv_alloc_conv_autodetect (const char *src,
			     size_t src_len,
			     char **dest_r,
			     size_t *dest_len_r,
			     const char *const *src_codesets,
			     int num_src_codesets,
			     int *actual_codeset_r,
			     const char *dest_codeset)
{
	if (dest_codeset == NULL)
		dest_codeset = jconv_info_get_current_codeset();
	if (num_src_codesets == 0)
		src_codesets = jconv_info_get_pref_codesets(&num_src_codesets);
	return jconv_alloc_conv(src, src_len, dest_r, dest_len_r,
				src_codesets, num_src_codesets,
				actual_codeset_r, dest_codeset);
}

/*
 * Mostly same as jconv_alloc_conv_autodetect() except that src must be
 * a null-terminated string, and that this function simply plays as
 * strdup() if convertion is failed.
 */
char *
jconv_strdup_conv_autodetect (const char *src,
			      const char *dest_codeset,
			      const char *src_codeset,
			      ...)
{
	int n = 0, actual_codeset;
	const char **cs = NULL;
	va_list ap;
	char *newstr;
	size_t newstr_len;
	int error_code;

	if (src_codeset) {
		va_start(ap, src_codeset);
		while (src_codeset) {
			cs = realloc(cs, (n + 1) * sizeof(*cs));
			cs[n] = src_codeset;
			n++;
			src_codeset = va_arg(ap, const char *);
		}
	}
	error_code = jconv_alloc_conv_autodetect(src, strlen(src),
						 &newstr, &newstr_len,
						 cs, n,
						 &actual_codeset,
						 dest_codeset);
	if (cs) free(cs);
	if (error_code) {
		if (newstr) free(newstr);
		newstr = strdup(src);
	}
	return newstr;
}

char *
jconv_strdup_conv_fullauto (const char *src)
{
	DEBUG_DO ({
	int i;
	for (i = 0; i <= strlen (src); i++) {
		printf ("%02x ", ((unsigned char *)src)[i]);
	}
	printf ("\n");
	})

	return jconv_strdup_conv_autodetect (src, NULL, NULL);
}

