
/* $Id$ */

#include <stdio.h>
#include <stdlib.h>
#include <locale.h>
#include <errno.h>
#include "jconv.h"

void
usage (const char *argv0)
{
	fprintf(stderr,
		"Usage: %s [-v] [-c config_file] "
		"[to_codeset [from_codeset ...]]\n",
		argv0);
	exit(1);
}

int
main (int argc, char **argv)
{
	int i, num_src_codesets = 0, verbose = 0, actual_codeset;
	char *conffile = NULL;
	char **src_codesets = NULL;
	char *dest_codeset = NULL;
	int error_code;
        char *src, *dest;
        size_t len, dlen;

        setlocale(LC_ALL, "");
	
	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-c") == 0) {
			i++;
			if (i >= argc)
				usage(argv[0]);
			conffile = argv[2];
		} else if (strcmp(argv[i], "-v") == 0) {
			verbose = 1;
		} else if (argv[i][0] == '-') {
			usage(argv[0]);
		} else if (dest_codeset == NULL) {
			dest_codeset = argv[i];
		} else {
			src_codesets = realloc(src_codesets,
					       (num_src_codesets + 1) *
					       sizeof (*src_codesets));
			src_codesets[num_src_codesets++] = argv[i];
		}
	}
	if (conffile)
		jconv_info_init(conffile);
	
        for (len = 0, src = NULL; !feof(stdin); ) {
                src = realloc(src, len + 4096);
                len += fread(src + len, 1, 4096, stdin);
        }
	if (ferror(stdin)) {
		perror("jconv(stdin)");
		exit(1);
	}
        src[len] = 0;
	
	error_code = jconv_alloc_conv_autodetect(src, len, &dest, &dlen,
						 (const char **)src_codesets,
						 num_src_codesets,
						 &actual_codeset,
						 dest_codeset);
	if (verbose) {
		if (num_src_codesets == 0)
			src_codesets = (void *)jconv_info_get_pref_codesets (
				&num_src_codesets);
		fprintf(stderr, "jconv: actual codeset: %s\n",
			src_codesets[actual_codeset]);
	}
	if (error_code) {
		errno = error_code;
		perror("jconv(jconv_alloc_conv_autodetect)");
		exit(1);
	}
	fwrite(dest, dlen, 1, stdout);
	fflush(stdout);
	if (ferror(stdout)) {
		perror("jconv(stdout)");
		exit(1);
	}
	return 0;
}
