
/* $Id$ */

#ifndef JCONV_H
#define JCONV_H

#include <iconv.h>

#ifdef __cplusplus
extern "C" {
#endif

/************************************************************************/
void jconv_info_init (const char *conffile);
void jconv_info_maybe_init (void);
void jconv_info_set_locale (void);
const char *jconv_info_get_current_codeset (void);
const char *const *jconv_info_get_pref_codesets (int *num_codesets_r);

/************************************************************************/
int
jconv_alloc_apply_iconv (iconv_t cd,
			 const char *buffer,
			 size_t len,
			 char **buffer_r,
			 size_t *len_r,
			 size_t *error_pos_r);
int
jconv_alloc_conv (const char *src,
		  size_t src_len,
		  char **dest_r,
		  size_t *dest_len_r,
		  const char *const *src_codesets,
		  int num_src_codesets,
		  int *actual_codeset_r,
		  const char *dest_codeset);
int
jconv_alloc_conv_autodetect (const char *src,
			     size_t src_len,
			     char **dest_r,
			     size_t *dest_len_r,
			     const char *const *src_codesets,
			     int num_src_codesets,
			     int *actual_codeset_r,
			     const char *dest_codeset);
char *
jconv_strdup_conv_autodetect (const char *src,
			      const char *dest_codeset,
			      const char *src_codeset,
			      ...);
char *
jconv_strdup_conv_fullauto (const char *src);


/************************************************************************/
char *
convert_kanji_auto (const char *src);
char *
convert_kanji (const char *src, const char *dest_codeset);
char *
convert_kanji_strict (const char *src,
		      const char *dest_codeset,
		      const char *src_codeset);

#ifdef __cplusplus
}
#endif

#endif /* JCONV_H */
